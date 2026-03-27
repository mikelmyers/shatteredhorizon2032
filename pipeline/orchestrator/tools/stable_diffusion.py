"""Stable Diffusion tool handler — local Automatic1111 / ComfyUI instance."""

from __future__ import annotations

import base64
import logging
import os
from pathlib import Path
from typing import TYPE_CHECKING

import httpx

from pipeline.orchestrator.base_tool import ToolHandler

if TYPE_CHECKING:
    from pipeline.inventory.models import AssetRequest

logger = logging.getLogger(__name__)


class StableDiffusionHandler(ToolHandler):
    """Generate concept art via a local Stable Diffusion WebUI API."""

    tool_id: str = "stable_diffusion"
    tool_name: str = "Stable Diffusion (local)"

    def is_available(self) -> bool:
        return bool(os.environ.get("STABLE_DIFFUSION_HOST"))

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        return 0.0  # Always free — runs locally.

    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        host = os.environ.get("STABLE_DIFFUSION_HOST", "")
        if not host:
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": "STABLE_DIFFUSION_HOST environment variable not set",
            }

        url = f"{host.rstrip('/')}/sdapi/v1/txt2img"
        payload = {
            "prompt": request.prompt,
            "negative_prompt": "",
            "steps": 30,
            "width": 1024,
            "height": 1024,
            "cfg_scale": 7,
            "sampler_name": "DPM++ 2M Karras",
        }

        try:
            with httpx.Client(timeout=120.0) as client:
                response = client.post(url, json=payload)
                response.raise_for_status()
        except httpx.ConnectError as exc:
            logger.error("Cannot reach Stable Diffusion at %s: %s", host, exc)
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": f"Connection failed: {exc}",
            }
        except httpx.HTTPStatusError as exc:
            logger.error("Stable Diffusion API error: %s", exc)
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": f"HTTP {exc.response.status_code}: {exc.response.text[:200]}",
            }

        data = response.json()
        images = data.get("images", [])
        if not images:
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": "No images returned from Stable Diffusion",
            }

        # Save the first image.
        out_path = Path(output_dir) / f"{request.id}.png"
        out_path.write_bytes(base64.b64decode(images[0]))

        return {
            "success": True,
            "file_path": str(out_path),
            "cost": 0.0,
            "metadata": {
                "tool": self.tool_id,
                "prompt": request.prompt,
                "parameters": {k: v for k, v in payload.items() if k != "prompt"},
            },
            "error": "",
        }
