"""Midjourney tool handler — AI image generation via Midjourney API."""

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

_BASE_URL = "https://api.midjourney.com/v1"
_COST_PER_IMAGE = 0.10


class MidjourneyHandler(ToolHandler):
    """Generate concept art via the Midjourney API."""

    tool_id: str = "midjourney"
    tool_name: str = "Midjourney"

    def is_available(self) -> bool:
        return bool(os.environ.get("MIDJOURNEY_API_KEY"))

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        return _COST_PER_IMAGE

    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        api_key = os.environ.get("MIDJOURNEY_API_KEY", "")
        if not api_key:
            # Stub mode.
            logger.info(
                "[STUB] Midjourney would generate image for prompt: %s",
                request.prompt,
            )
            out_path = Path(output_dir) / f"{request.id}.png"
            out_path.write_text(
                f"[STUB] Midjourney image placeholder for prompt: {request.prompt}"
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
            "aspect_ratio": "1:1",
        }

        try:
            with httpx.Client(timeout=180.0) as client:
                resp = client.post(
                    f"{_BASE_URL}/imagine",
                    headers=headers,
                    json=payload,
                )
                resp.raise_for_status()
                data = resp.json()
                task_id = data.get("taskId", "")

                if not task_id:
                    return {
                        "success": False,
                        "file_path": "",
                        "cost": 0.0,
                        "metadata": {"response": data},
                        "error": "No task ID returned from Midjourney",
                    }

                # Poll for result.
                image_url: str = ""
                for _ in range(60):
                    time.sleep(5)
                    poll = client.get(
                        f"{_BASE_URL}/task/{task_id}",
                        headers=headers,
                    )
                    poll.raise_for_status()
                    poll_data = poll.json()
                    status = poll_data.get("status", "")
                    if status == "completed":
                        image_url = poll_data.get("imageUrl", "")
                        break
                    elif status == "failed":
                        return {
                            "success": False,
                            "file_path": "",
                            "cost": _COST_PER_IMAGE,
                            "metadata": {"task_id": task_id},
                            "error": f"Midjourney task failed: {poll_data.get('error', '')}",
                        }

                if not image_url:
                    return {
                        "success": False,
                        "file_path": "",
                        "cost": _COST_PER_IMAGE,
                        "metadata": {"task_id": task_id},
                        "error": "Midjourney task timed out",
                    }

                img_resp = client.get(image_url)
                img_resp.raise_for_status()

                out_path = Path(output_dir) / f"{request.id}.png"
                out_path.write_bytes(img_resp.content)

        except httpx.HTTPStatusError as exc:
            logger.error("Midjourney API error: %s", exc)
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": f"HTTP {exc.response.status_code}: {exc.response.text[:200]}",
            }
        except httpx.RequestError as exc:
            logger.error("Midjourney request failed: %s", exc)
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
            "cost": _COST_PER_IMAGE,
            "metadata": {
                "tool": self.tool_id,
                "task_id": task_id,
                "prompt": request.prompt,
            },
            "error": "",
        }
