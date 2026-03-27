"""Leonardo.ai tool handler — AI image generation via Leonardo API."""

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

_BASE_URL = "https://cloud.leonardo.ai/api/rest/v1"
_COST_PER_IMAGE = 0.12


class LeonardoHandler(ToolHandler):
    """Generate concept art and UI assets via the Leonardo.ai API."""

    tool_id: str = "leonardo"
    tool_name: str = "Leonardo.ai"

    def is_available(self) -> bool:
        return bool(os.environ.get("LEONARDO_API_KEY"))

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        return _COST_PER_IMAGE

    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        api_key = os.environ.get("LEONARDO_API_KEY", "")
        if not api_key:
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": "LEONARDO_API_KEY environment variable not set",
            }

        headers = {
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        }
        payload = {
            "prompt": request.prompt,
            "num_images": 1,
            "width": 1024,
            "height": 1024,
        }

        try:
            with httpx.Client(timeout=120.0) as client:
                # Submit generation request.
                resp = client.post(
                    f"{_BASE_URL}/generations",
                    headers=headers,
                    json=payload,
                )
                resp.raise_for_status()
                gen_data = resp.json()
                generation_id = gen_data.get("sdGenerationJob", {}).get("generationId", "")

                if not generation_id:
                    return {
                        "success": False,
                        "file_path": "",
                        "cost": 0.0,
                        "metadata": {"response": gen_data},
                        "error": "No generation ID returned from Leonardo",
                    }

                # Poll until generation completes.
                image_url: str = ""
                for _ in range(30):
                    time.sleep(5)
                    poll = client.get(
                        f"{_BASE_URL}/generations/{generation_id}",
                        headers=headers,
                    )
                    poll.raise_for_status()
                    poll_data = poll.json()
                    status = poll_data.get("generations_by_pk", {}).get("status")
                    if status == "COMPLETE":
                        images = poll_data["generations_by_pk"].get("generated_images", [])
                        if images:
                            image_url = images[0].get("url", "")
                        break

                if not image_url:
                    return {
                        "success": False,
                        "file_path": "",
                        "cost": _COST_PER_IMAGE,
                        "metadata": {"generation_id": generation_id},
                        "error": "Generation timed out or produced no images",
                    }

                # Download the image.
                img_resp = client.get(image_url)
                img_resp.raise_for_status()

                out_path = Path(output_dir) / f"{request.id}.png"
                out_path.write_bytes(img_resp.content)

        except httpx.HTTPStatusError as exc:
            logger.error("Leonardo API error: %s", exc)
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": f"HTTP {exc.response.status_code}: {exc.response.text[:200]}",
            }
        except httpx.RequestError as exc:
            logger.error("Leonardo request failed: %s", exc)
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
                "generation_id": generation_id,
                "prompt": request.prompt,
            },
            "error": "",
        }
