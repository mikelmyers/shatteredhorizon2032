"""API call dispatcher — loads tool plugins and dispatches generation requests."""

from __future__ import annotations

import importlib
import inspect
import logging
from dataclasses import dataclass, field
from pathlib import Path
from typing import TYPE_CHECKING

from pipeline.orchestrator.base_tool import ToolHandler

if TYPE_CHECKING:
    from pipeline.inventory.models import AssetRequest

logger = logging.getLogger(__name__)

# Default location for built-in tool plugins.
_DEFAULT_TOOLS_DIR = Path(__file__).parent / "tools"


# ---------------------------------------------------------------------------
# Result dataclass
# ---------------------------------------------------------------------------

@dataclass
class DispatchResult:
    """Outcome of a single tool dispatch."""

    success: bool
    tool_id: str
    asset_path: str | None = None
    cost: float = 0.0
    error: str = ""
    metadata: dict = field(default_factory=dict)


# ---------------------------------------------------------------------------
# Dispatcher
# ---------------------------------------------------------------------------

class Dispatcher:
    """Discover tool handlers and dispatch generation requests to them."""

    def __init__(self, tools_dir: str | Path | None = None) -> None:
        self._tools_dir = Path(tools_dir) if tools_dir else _DEFAULT_TOOLS_DIR
        self._handlers: dict[str, ToolHandler] = {}
        self._load_tools()

    # ------------------------------------------------------------------
    # Tool discovery
    # ------------------------------------------------------------------

    def _load_tools(self) -> None:
        """Import every Python module in the tools directory and register
        any :class:`ToolHandler` subclasses found inside."""
        if not self._tools_dir.is_dir():
            logger.warning("Tools directory does not exist: %s", self._tools_dir)
            return

        for py_file in sorted(self._tools_dir.glob("*.py")):
            if py_file.name.startswith("_"):
                continue

            module_name = f"pipeline.orchestrator.tools.{py_file.stem}"
            try:
                module = importlib.import_module(module_name)
            except Exception:
                logger.exception("Failed to import tool module %s", module_name)
                continue

            for _name, obj in inspect.getmembers(module, inspect.isclass):
                if (
                    issubclass(obj, ToolHandler)
                    and obj is not ToolHandler
                    and getattr(obj, "tool_id", "")
                ):
                    instance = obj()
                    self._handlers[instance.tool_id] = instance
                    logger.debug("Registered tool handler: %s", instance.tool_id)

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def get_handler(self, tool_id: str) -> ToolHandler | None:
        """Return the handler instance for *tool_id*, or ``None``."""
        return self._handlers.get(tool_id)

    def dispatch(
        self,
        tool_id: str,
        request: "AssetRequest",
        output_dir: str | Path,
    ) -> DispatchResult:
        """Run a generation through the identified tool handler.

        Args:
            tool_id: Registered identifier for the tool.
            request: The asset request to fulfil.
            output_dir: Directory where generated files will be saved.

        Returns:
            A :class:`DispatchResult` describing the outcome.
        """
        handler = self._handlers.get(tool_id)
        if handler is None:
            return DispatchResult(
                success=False,
                tool_id=tool_id,
                error=f"No handler registered for tool '{tool_id}'",
            )

        if not handler.is_available():
            return DispatchResult(
                success=False,
                tool_id=tool_id,
                error=f"Tool '{tool_id}' is not currently available (missing API key or service down)",
            )

        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)

        try:
            result = handler.generate(request, str(output_path))
        except Exception as exc:
            logger.exception("Tool %s raised an exception", tool_id)
            return DispatchResult(
                success=False,
                tool_id=tool_id,
                error=str(exc),
            )

        return DispatchResult(
            success=result.get("success", False),
            tool_id=tool_id,
            asset_path=result.get("file_path"),
            cost=result.get("cost", 0.0),
            error=result.get("error", ""),
            metadata=result.get("metadata", {}),
        )

    def list_available(self) -> list[str]:
        """Return the tool_ids of all registered handlers."""
        return list(self._handlers.keys())
