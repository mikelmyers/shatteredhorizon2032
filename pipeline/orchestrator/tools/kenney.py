"""Kenney.nl tool handler — free game assets (UI elements, sprites, etc.)."""

from __future__ import annotations

import logging
from pathlib import Path
from typing import TYPE_CHECKING

import httpx

from pipeline.orchestrator.base_tool import ToolHandler

if TYPE_CHECKING:
    from pipeline.inventory.models import AssetRequest

logger = logging.getLogger(__name__)

_BASE_URL = "https://kenney.nl"


class KenneyHandler(ToolHandler):
    """Download free UI and game assets from the Kenney.nl asset library."""

    tool_id: str = "kenney"
    tool_name: str = "Kenney.nl"

    def is_available(self) -> bool:
        return True  # Free assets, always available.

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        return 0.0

    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        # Kenney does not have a public search API.  In production this would
        # index the Kenney asset packs locally.  For now we produce a stub
        # result that documents which pack/asset would be selected.

        query = request.prompt.strip()
        logger.info(
            "[STUB] Kenney would search asset library for: %s", query
        )

        # Generate a placeholder descriptor so the pipeline knows what
        # was "selected" and the file slot is populated.
        out_path = Path(output_dir) / f"{request.id}.json"

        import json

        descriptor = {
            "source": "kenney.nl",
            "query": query,
            "stub": True,
            "note": (
                "Replace this placeholder with the actual downloaded asset. "
                "Browse https://kenney.nl/assets to find a matching pack."
            ),
        }
        out_path.write_text(json.dumps(descriptor, indent=2))

        return {
            "success": True,
            "file_path": str(out_path),
            "cost": 0.0,
            "metadata": {
                "tool": self.tool_id,
                "query": query,
                "stub": True,
            },
            "error": "",
        }
