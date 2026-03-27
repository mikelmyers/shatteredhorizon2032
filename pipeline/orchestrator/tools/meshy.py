"""Meshy.ai tool handler — text-to-3D mesh generation."""

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

_BASE_URL = "https://api.meshy.ai/v2"
_COST_PER_MESH = 0.10


class MeshyHandler(ToolHandler):
    """Generate 3D meshes via the Meshy.ai text-to-3D API."""

    tool_id: str = "meshy"
    tool_name: str = "Meshy.ai"

    def is_available(self) -> bool:
        return bool(os.environ.get("MESHY_API_KEY"))

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        return _COST_PER_MESH

    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        api_key = os.environ.get("MESHY_API_KEY", "")
        if not api_key:
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": "MESHY_API_KEY environment variable not set",
            }

        headers = {
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        }
        payload = {
            "mode": "preview",
            "prompt": request.prompt,
            "art_style": "realistic",
        }

        try:
            with httpx.Client(timeout=180.0) as client:
                # Submit text-to-3D task.
                resp = client.post(
                    f"{_BASE_URL}/text-to-3d",
                    headers=headers,
                    json=payload,
                )
                resp.raise_for_status()
                task_data = resp.json()
                task_id = task_data.get("result", "")

                if not task_id:
                    return {
                        "success": False,
                        "file_path": "",
                        "cost": 0.0,
                        "metadata": {"response": task_data},
                        "error": "No task ID returned from Meshy",
                    }

                # Poll for completion.
                model_url: str = ""
                for _ in range(60):
                    time.sleep(5)
                    poll = client.get(
                        f"{_BASE_URL}/text-to-3d/{task_id}",
                        headers=headers,
                    )
                    poll.raise_for_status()
                    poll_data = poll.json()
                    status = poll_data.get("status", "")

                    if status == "SUCCEEDED":
                        model_urls = poll_data.get("model_urls", {})
                        model_url = model_urls.get("glb", "") or model_urls.get("obj", "")
                        break
                    elif status in ("FAILED", "EXPIRED"):
                        return {
                            "success": False,
                            "file_path": "",
                            "cost": _COST_PER_MESH,
                            "metadata": {"task_id": task_id, "status": status},
                            "error": f"Meshy task {status}: {poll_data.get('message', '')}",
                        }

                if not model_url:
                    return {
                        "success": False,
                        "file_path": "",
                        "cost": _COST_PER_MESH,
                        "metadata": {"task_id": task_id},
                        "error": "Meshy task timed out waiting for completion",
                    }

                # Download the mesh file.
                ext = "glb" if "glb" in model_url else "obj"
                mesh_resp = client.get(model_url)
                mesh_resp.raise_for_status()

                out_path = Path(output_dir) / f"{request.id}.{ext}"
                out_path.write_bytes(mesh_resp.content)

        except httpx.HTTPStatusError as exc:
            logger.error("Meshy API error: %s", exc)
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": f"HTTP {exc.response.status_code}: {exc.response.text[:200]}",
            }
        except httpx.RequestError as exc:
            logger.error("Meshy request failed: %s", exc)
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
            "cost": _COST_PER_MESH,
            "metadata": {
                "tool": self.tool_id,
                "task_id": task_id,
                "prompt": request.prompt,
                "format": ext,
            },
            "error": "",
        }
