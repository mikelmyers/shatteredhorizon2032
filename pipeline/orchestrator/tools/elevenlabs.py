"""ElevenLabs tool handler — AI sound effect generation."""

from __future__ import annotations

import logging
import os
from pathlib import Path
from typing import TYPE_CHECKING

import httpx

from pipeline.orchestrator.base_tool import ToolHandler

if TYPE_CHECKING:
    from pipeline.inventory.models import AssetRequest

logger = logging.getLogger(__name__)

_BASE_URL = "https://api.elevenlabs.io/v1"
_COST_PER_GENERATION = 0.05


class ElevenLabsHandler(ToolHandler):
    """Generate sound effects via the ElevenLabs API."""

    tool_id: str = "elevenlabs"
    tool_name: str = "ElevenLabs"

    def is_available(self) -> bool:
        return bool(os.environ.get("ELEVENLABS_API_KEY"))

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        return _COST_PER_GENERATION

    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        api_key = os.environ.get("ELEVENLABS_API_KEY", "")
        if not api_key:
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": "ELEVENLABS_API_KEY environment variable not set",
            }

        headers = {
            "xi-api-key": api_key,
            "Content-Type": "application/json",
        }
        payload = {
            "text": request.prompt,
            "duration_seconds": 10.0,
            "prompt_influence": 0.5,
        }

        try:
            with httpx.Client(timeout=60.0) as client:
                resp = client.post(
                    f"{_BASE_URL}/sound-generation",
                    headers=headers,
                    json=payload,
                )
                resp.raise_for_status()

                out_path = Path(output_dir) / f"{request.id}.mp3"
                out_path.write_bytes(resp.content)

        except httpx.HTTPStatusError as exc:
            logger.error("ElevenLabs API error: %s", exc)
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": f"HTTP {exc.response.status_code}: {exc.response.text[:200]}",
            }
        except httpx.RequestError as exc:
            logger.error("ElevenLabs request failed: %s", exc)
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": f"Request error: {exc}",
            }

        return {
            "success": True,
            "file_path": str(out_path),
            "cost": _COST_PER_GENERATION,
            "metadata": {
                "tool": self.tool_id,
                "prompt": request.prompt,
                "duration_seconds": 10.0,
            },
            "error": "",
        }
