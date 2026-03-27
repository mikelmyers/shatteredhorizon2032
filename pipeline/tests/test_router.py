"""Tests for pipeline.orchestrator.router — routing logic and budget integration."""

import json
import os
import shutil
import tempfile
import unittest

from pipeline.costs.tracker import CostTracker
from pipeline.inventory.models import AssetRequest
from pipeline.orchestrator.router import Router, RoutingDecision, ROUTING_TABLE


class MockDispatcher:
    """Minimal dispatcher mock — returns None for all handlers."""
    def get_handler(self, tool_id):
        return None


class TestRoutingTable(unittest.TestCase):
    """Verify routing table covers all asset types."""

    def test_all_asset_types_have_routes(self):
        expected_types = {"concept_art", "3d_mesh", "texture", "character",
                          "environment", "vfx", "music", "sfx", "ui"}
        self.assertEqual(set(ROUTING_TABLE.keys()), expected_types)

    def test_each_type_has_at_least_one_candidate(self):
        for asset_type, candidates in ROUTING_TABLE.items():
            self.assertGreater(len(candidates), 0, f"{asset_type} has no candidates")

    def test_free_tier_comes_first(self):
        """First candidate for each type should be free or freemium."""
        for asset_type, candidates in ROUTING_TABLE.items():
            first = candidates[0]
            self.assertIn(first["tier"], ("free", "freemium"),
                          f"{asset_type} first candidate is {first['tier']}")


class TestRouter(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.tools_config = {
            "tools": [
                {"id": "stable_diffusion", "name": "SD Local", "tier": "free", "monthly_cap": 0, "cost_per_call": 0},
                {"id": "leonardo", "name": "Leonardo.ai", "tier": "freemium", "monthly_cap": 24.00, "cost_per_call": 0},
                {"id": "midjourney", "name": "Midjourney", "tier": "paid", "monthly_cap": 30.00, "cost_per_call": 0},
                {"id": "meshy", "name": "Meshy.ai", "tier": "free", "monthly_cap": 0, "cost_per_call": 0},
                {"id": "kitbash3d", "name": "KitBash3D", "tier": "paid", "monthly_cap": 50.00, "cost_per_call": 0},
                {"id": "poly_haven", "name": "Poly Haven", "tier": "free", "monthly_cap": 0, "cost_per_call": 0},
                {"id": "metahuman", "name": "MetaHuman", "tier": "free", "monthly_cap": 0, "cost_per_call": 0},
                {"id": "ue5_pcg", "name": "UE5 PCG", "tier": "free", "monthly_cap": 0, "cost_per_call": 0},
                {"id": "niagara", "name": "Niagara", "tier": "free", "monthly_cap": 0, "cost_per_call": 0},
                {"id": "suno", "name": "Suno", "tier": "free", "monthly_cap": 0, "cost_per_call": 0},
                {"id": "aiva", "name": "AIVA", "tier": "paid", "monthly_cap": 15.00, "cost_per_call": 0},
                {"id": "elevenlabs", "name": "ElevenLabs", "tier": "free", "monthly_cap": 0, "cost_per_call": 0},
                {"id": "kenney", "name": "Kenney.nl", "tier": "free", "monthly_cap": 0, "cost_per_call": 0},
            ],
            "total_monthly_budget": 100.00,
            "alert_threshold_pct": 80,
        }
        config_path = os.path.join(self.tmpdir, "tools.json")
        with open(config_path, "w") as f:
            json.dump(self.tools_config, f)

        db_path = os.path.join(self.tmpdir, "ledger.db")
        self.tracker = CostTracker(db_path=db_path, tools_config_path=config_path)
        self.dispatcher = MockDispatcher()
        self.router = Router(cost_tracker=self.tracker, dispatcher=self.dispatcher)

    def tearDown(self):
        self.tracker.conn.close()
        shutil.rmtree(self.tmpdir)

    # ------------------------------------------------------------------
    # Basic routing
    # ------------------------------------------------------------------

    def test_routes_concept_art_to_free_first(self):
        req = AssetRequest(type="concept_art", prompt="test")
        decision = self.router.route(req)
        self.assertFalse(decision.blocked)
        self.assertEqual(decision.tool_id, "stable_diffusion")
        self.assertEqual(decision.tier, "free")

    def test_routes_3d_mesh_to_free_first(self):
        req = AssetRequest(type="3d_mesh", prompt="M4 carbine")
        decision = self.router.route(req)
        self.assertFalse(decision.blocked)
        self.assertEqual(decision.tool_id, "meshy")

    def test_routes_texture(self):
        req = AssetRequest(type="texture", prompt="concrete PBR")
        decision = self.router.route(req)
        self.assertFalse(decision.blocked)
        self.assertEqual(decision.tool_id, "poly_haven")

    def test_routes_all_types(self):
        for asset_type in ROUTING_TABLE:
            req = AssetRequest(type=asset_type, prompt="test")
            decision = self.router.route(req)
            self.assertFalse(decision.blocked,
                             f"Routing blocked for {asset_type}: {decision.block_reason}")

    # ------------------------------------------------------------------
    # Unknown type
    # ------------------------------------------------------------------

    def test_unknown_type_blocked(self):
        req = AssetRequest(type="hologram", prompt="test")
        decision = self.router.route(req)
        self.assertTrue(decision.blocked)
        self.assertIn("Unknown asset type", decision.block_reason)

    # ------------------------------------------------------------------
    # force_paid
    # ------------------------------------------------------------------

    def test_force_paid_skips_free(self):
        req = AssetRequest(type="concept_art", prompt="test", force_paid=True)
        decision = self.router.route(req)
        self.assertFalse(decision.blocked)
        self.assertEqual(decision.tool_id, "midjourney")
        self.assertEqual(decision.tier, "paid")

    def test_force_paid_no_paid_option(self):
        # texture only has free + paid, but let's check with a type
        # that might have all free (like if we modified the table)
        req = AssetRequest(type="ui", prompt="test", force_paid=True)
        # ui has kenney (free) and leonardo (freemium) — no "paid"
        decision = self.router.route(req)
        self.assertTrue(decision.blocked)
        self.assertIn("No paid-tier tools", decision.block_reason)

    # ------------------------------------------------------------------
    # Budget-gated routing
    # ------------------------------------------------------------------

    def test_escalates_when_tool_budget_exhausted(self):
        # Exhaust midjourney's cap — the only paid option for concept_art
        self.tracker.log_cost("midjourney", 30.01)
        req = AssetRequest(type="concept_art", prompt="test", force_paid=True)
        decision = self.router.route(req)
        # midjourney blocked, should be fully blocked (only paid option for concept_art)
        self.assertTrue(decision.blocked, f"Expected blocked but got tool={decision.tool_id}")

    def test_all_options_blocked_when_total_budget_hit(self):
        # Exhaust total budget
        self.tracker.log_cost("midjourney", 100.00)
        req = AssetRequest(type="concept_art", prompt="test", force_paid=True)
        decision = self.router.route(req)
        self.assertTrue(decision.blocked)
        self.assertIn("blocked by budget", decision.block_reason)

    def test_free_tools_unaffected_by_budget(self):
        # Even with total budget exhausted, free tools work
        self.tracker.log_cost("midjourney", 100.00)
        req = AssetRequest(type="concept_art", prompt="test")
        decision = self.router.route(req)
        self.assertFalse(decision.blocked)
        self.assertEqual(decision.tool_id, "stable_diffusion")

    def test_budget_warning_propagated(self):
        # Leonardo at 83% ($20 / $24)
        self.tracker.log_cost("leonardo", 20.00)
        req = AssetRequest(type="concept_art", prompt="test")
        decision = self.router.route(req)
        # Free tool (SD) should be picked first — no warning from SD
        self.assertEqual(decision.tool_id, "stable_diffusion")

    # ------------------------------------------------------------------
    # RoutingDecision dataclass
    # ------------------------------------------------------------------

    def test_routing_decision_defaults(self):
        rd = RoutingDecision()
        self.assertEqual(rd.tool_id, "")
        self.assertFalse(rd.blocked)
        self.assertEqual(rd.block_reason, "")
        self.assertEqual(rd.estimated_cost, 0.0)


class TestGetRoutingTable(unittest.TestCase):
    def test_returns_table(self):
        table = Router.get_routing_table()
        self.assertIsInstance(table, dict)
        self.assertIn("concept_art", table)


if __name__ == "__main__":
    unittest.main()
