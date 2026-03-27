"""Core routing logic for the asset pipeline orchestrator.

The router examines an AssetRequest, consults the routing table and cost
tracker, then selects the best tool to fulfil the request within budget.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from pipeline.costs.tracker import CostTracker
    from pipeline.inventory.models import AssetRequest
    from pipeline.orchestrator.dispatcher import Dispatcher


# ---------------------------------------------------------------------------
# Routing table — ordered lists of candidate tools per asset type.
# Within each list the entries are sorted free → freemium → paid so that the
# cheapest option is attempted first (unless the caller forces paid).
# ---------------------------------------------------------------------------

ROUTING_TABLE: dict[str, list[dict[str, str]]] = {
    "concept_art": [
        {"tool": "stable_diffusion", "tier": "free"},
        {"tool": "leonardo", "tier": "freemium"},
        {"tool": "midjourney", "tier": "paid"},
    ],
    "3d_mesh": [
        {"tool": "meshy", "tier": "free"},
        {"tool": "meshy", "tier": "freemium"},
        {"tool": "kitbash3d", "tier": "paid"},
    ],
    "texture": [
        {"tool": "poly_haven", "tier": "free"},
        {"tool": "adobe_substance", "tier": "paid"},
    ],
    "character": [
        {"tool": "metahuman", "tier": "free"},
        {"tool": "rokoko", "tier": "paid"},
    ],
    "environment": [
        {"tool": "ue5_pcg", "tier": "free"},
        {"tool": "promethean", "tier": "paid"},
    ],
    "vfx": [
        {"tool": "niagara", "tier": "free"},
        {"tool": "houdini", "tier": "paid"},
    ],
    "music": [
        {"tool": "suno", "tier": "free"},
        {"tool": "aiva", "tier": "paid"},
        {"tool": "suno", "tier": "paid"},
    ],
    "sfx": [
        {"tool": "elevenlabs", "tier": "free"},
        {"tool": "elevenlabs", "tier": "paid"},
    ],
    "ui": [
        {"tool": "kenney", "tier": "free"},
        {"tool": "leonardo", "tier": "freemium"},
    ],
}

# Tier ordering used when filtering for force_paid requests.
_PAID_TIERS = {"paid"}


# ---------------------------------------------------------------------------
# Data class returned by every routing decision
# ---------------------------------------------------------------------------

@dataclass
class RoutingDecision:
    """Result of a routing attempt."""

    tool_id: str = ""
    tool_name: str = ""
    tier: str = ""
    blocked: bool = False
    block_reason: str = ""
    budget_warning: str = ""
    estimated_cost: float = 0.0


# ---------------------------------------------------------------------------
# Router
# ---------------------------------------------------------------------------

class Router:
    """Select the best tool for an incoming asset request.

    Iterates through the routing table from cheapest tier upward, checking
    budget constraints via the CostTracker.  If the request has
    ``force_paid=True`` the free and freemium tiers are skipped entirely.
    """

    def __init__(self, cost_tracker: "CostTracker", dispatcher: "Dispatcher") -> None:
        self.cost_tracker = cost_tracker
        self.dispatcher = dispatcher

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def route(self, request: "AssetRequest") -> RoutingDecision:
        """Determine the best tool for *request*.

        Returns a :class:`RoutingDecision` — check ``decision.blocked`` to
        know whether the pipeline can proceed.
        """
        candidates = ROUTING_TABLE.get(request.type)
        if not candidates:
            return RoutingDecision(
                blocked=True,
                block_reason=f"Unknown asset type: {request.type}",
            )

        # Filter candidates when force_paid is requested.
        if request.force_paid:
            candidates = [c for c in candidates if c["tier"] in _PAID_TIERS]
            if not candidates:
                return RoutingDecision(
                    blocked=True,
                    block_reason=(
                        f"No paid-tier tools configured for asset type '{request.type}'"
                    ),
                )

        block_reasons: list[str] = []

        for candidate in candidates:
            tool_id = candidate["tool"]
            tier = candidate["tier"]

            # Try to get a handler so we can estimate cost accurately.
            handler = self.dispatcher.get_handler(tool_id)
            if handler is not None:
                estimated_cost = handler.get_estimated_cost(request)
                tool_name = handler.tool_name
            else:
                # No handler loaded — use a zero-cost estimate and the raw id.
                estimated_cost = 0.0
                tool_name = tool_id

            # Ask the cost tracker whether budget allows this dispatch.
            budget = self.cost_tracker.check_budget(tool_id, estimated_cost)
            if budget["allowed"]:
                return RoutingDecision(
                    tool_id=tool_id,
                    tool_name=tool_name,
                    tier=tier,
                    blocked=False,
                    budget_warning=budget.get("warning") or "",
                    estimated_cost=estimated_cost,
                )
            else:
                block_reasons.append(f"{tool_id} ({tier}): {budget['reason']}")

        # All candidates exhausted.
        return RoutingDecision(
            blocked=True,
            block_reason=(
                "All routing options blocked by budget: "
                + "; ".join(block_reasons)
            ),
        )

    @staticmethod
    def get_routing_table() -> dict[str, list[dict[str, str]]]:
        """Return the full routing table (useful for CLI display)."""
        return ROUTING_TABLE
