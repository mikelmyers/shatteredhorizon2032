"""AIVA tool handler — AI music composition."""

from __future__ import annotations

import logging
import os
import time
from pathlib import Path
from typing import TYPE_CHECKING

import httpx

from pipeline.orchestrator.base_tool import ToolHandler

if TYPE_CHECKING:
    from pipeline.inventory.models import AssetRequest

logger = logging.getLogger(__name__)

_BASE_URL = "https://api.aiva.ai/v1"
_COST_PER_TRACK = 0.15


class AIVAHandler(ToolHandler):
    """Generate music compositions via the AIVA API."""

    tool_id: str = "aiva"
    tool_name: str = "AIVA"

    def is_available(self) -> bool:
        return bool(os.environ.get("AIVA_API_KEY"))

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        return _COST_PER_TRACK

    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        api_key = os.environ.get("AIVA_API_KEY", "")
        if not api_key:
            # Stub mode: produce a placeholder file.
            logger.info(
                "[STUB] AIVA would compose music for prompt: %s", request.prompt
            )
            out_path = Path(output_dir) / f"{request.id}.mp3"
            out_path.write_text(
                f"[STUB] AIVA music placeholder for prompt: {request.prompt}"
            )
            return {
                "success": True,
                "file_path": str(out_path),
                "cost": 0.0,
                "metadata": {"tool": self.tool_id, "stub": True},
                "error": "",
            }

        headers = {
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        }
        payload = {
            "prompt": request.prompt,
            "format": "mp3",
            "duration": 120,
        }

        try:
            with httpx.Client(timeout=180.0) as client:
                resp = client.post(
                    f"{_BASE_URL}/compositions",
                    headers=headers,
                    json=payload,
                )
                resp.raise_for_status()
                data = resp.json()
                composition_id = data.get("id", "")

                if not composition_id:
                    return {
                        "success": False,
                        "file_path": "",
                        "cost": 0.0,
                        "metadata": {"response": data},
                        "error": "No composition ID returned from AIVA",
                    }

                # Poll for completion.
                download_url: str = ""
                for _ in range(60):
                    time.sleep(5)
                    poll = client.get(
                        f"{_BASE_URL}/compositions/{composition_id}",
                        headers=headers,
                    )
                    poll.raise_for_status()
                    poll_data = poll.json()
                    status = poll_data.get("status", "")
                    if status == "completed":
                        download_url = poll_data.get("download_url", "")
                        break
                    elif status == "failed":
                        return {
                            "success": False,
                            "file_path": "",
                            "cost": _COST_PER_TRACK,
                            "metadata": {"composition_id": composition_id},
                            "error": f"AIVA composition failed: {poll_data.get('error', '')}",
                        }

                if not download_url:
                    return {
                        "success": False,
                        "file_path": "",
                        "cost": _COST_PER_TRACK,
                        "metadata": {"composition_id": composition_id},
                        "error": "AIVA composition timed out",
                    }

                audio_resp = client.get(download_url)
                audio_resp.raise_for_status()

                out_path = Path(output_dir) / f"{request.id}.mp3"
                out_path.write_bytes(audio_resp.content)

        except httpx.HTTPStatusError as exc:
            logger.error("AIVA API error: %s", exc)
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": f"HTTP {exc.response.status_code}: {exc.response.text[:200]}",
            }
        except httpx.RequestError as exc:
            logger.error("AIVA request failed: %s", exc)
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
            "cost": _COST_PER_TRACK,
            "metadata": {
                "tool": self.tool_id,
                "composition_id": composition_id,
                "prompt": request.prompt,
            },
            "error": "",
        }
