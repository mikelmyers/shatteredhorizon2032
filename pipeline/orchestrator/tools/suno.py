"""Suno tool handler — AI music generation."""

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

_BASE_URL = "https://studio-api.suno.ai/api"
_COST_PER_TRACK = 0.10


class SunoHandler(ToolHandler):
    """Generate music tracks via the Suno API."""

    tool_id: str = "suno"
    tool_name: str = "Suno"

    def is_available(self) -> bool:
        return bool(os.environ.get("SUNO_API_KEY"))

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        return _COST_PER_TRACK

    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        api_key = os.environ.get("SUNO_API_KEY", "")
        if not api_key:
            # Stub mode: log what we would do and return a placeholder.
            logger.info(
                "[STUB] Suno would generate music for prompt: %s", request.prompt
            )
            out_path = Path(output_dir) / f"{request.id}.mp3"
            out_path.write_text(
                f"[STUB] Suno music placeholder for prompt: {request.prompt}"
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
            "make_instrumental": False,
            "wait_audio": False,
        }

        try:
            with httpx.Client(timeout=180.0) as client:
                resp = client.post(
                    f"{_BASE_URL}/generate/v2",
                    headers=headers,
                    json=payload,
                )
                resp.raise_for_status()
                data = resp.json()
                clips = data if isinstance(data, list) else data.get("clips", [])

                if not clips:
                    return {
                        "success": False,
                        "file_path": "",
                        "cost": _COST_PER_TRACK,
                        "metadata": {"response": data},
                        "error": "No clips returned from Suno",
                    }

                clip_id = clips[0].get("id", "")

                # Poll for completion.
                audio_url: str = ""
                for _ in range(60):
                    time.sleep(5)
                    poll = client.get(
                        f"{_BASE_URL}/feed/{clip_id}",
                        headers=headers,
                    )
                    poll.raise_for_status()
                    clip_data = poll.json()
                    if isinstance(clip_data, list):
                        clip_data = clip_data[0] if clip_data else {}
                    status = clip_data.get("status", "")
                    if status == "complete":
                        audio_url = clip_data.get("audio_url", "")
                        break

                if not audio_url:
                    return {
                        "success": False,
                        "file_path": "",
                        "cost": _COST_PER_TRACK,
                        "metadata": {"clip_id": clip_id},
                        "error": "Suno generation timed out",
                    }

                # Download audio.
                audio_resp = client.get(audio_url)
                audio_resp.raise_for_status()

                out_path = Path(output_dir) / f"{request.id}.mp3"
                out_path.write_bytes(audio_resp.content)

        except httpx.HTTPStatusError as exc:
            logger.error("Suno API error: %s", exc)
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": f"HTTP {exc.response.status_code}: {exc.response.text[:200]}",
            }
        except httpx.RequestError as exc:
            logger.error("Suno request failed: %s", exc)
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
                "clip_id": clip_id,
                "prompt": request.prompt,
            },
            "error": "",
        }
