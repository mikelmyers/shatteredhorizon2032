"""Poly Haven tool handler — free PBR textures and HDRIs."""

from __future__ import annotations

import logging
from pathlib import Path
from typing import TYPE_CHECKING

import httpx

from pipeline.orchestrator.base_tool import ToolHandler

if TYPE_CHECKING:
    from pipeline.inventory.models import AssetRequest

logger = logging.getLogger(__name__)

_BASE_URL = "https://api.polyhaven.com"


class PolyHavenHandler(ToolHandler):
    """Download PBR textures from the free Poly Haven library."""

    tool_id: str = "poly_haven"
    tool_name: str = "Poly Haven"

    def is_available(self) -> bool:
        return True  # Always available — free public API.

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        return 0.0

    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        # Use the prompt as a search query for the Poly Haven asset library.
        query = request.prompt.strip().lower().replace(" ", "_")

        try:
            with httpx.Client(timeout=60.0) as client:
                # Search for matching textures.
                resp = client.get(
                    f"{_BASE_URL}/assets",
                    params={"t": "textures"},
                )
                resp.raise_for_status()
                assets = resp.json()

                # Find the best matching asset by checking if query terms
                # appear in the asset name.
                query_terms = request.prompt.strip().lower().split()
                best_match: str = ""
                best_score: int = 0

                for asset_name in assets:
                    name_lower = asset_name.lower()
                    score = sum(1 for term in query_terms if term in name_lower)
                    if score > best_score:
                        best_score = score
                        best_match = asset_name

                if not best_match:
                    # Fall back to the first available asset if no match found.
                    if assets:
                        best_match = next(iter(assets))
                    else:
                        return {
                            "success": False,
                            "file_path": "",
                            "cost": 0.0,
                            "metadata": {},
                            "error": "No textures found on Poly Haven",
                        }

                # Get download link for the texture.
                files_resp = client.get(f"{_BASE_URL}/files/{best_match}")
                files_resp.raise_for_status()
                files_data = files_resp.json()

                # Try to download the diffuse/color map at 2K resolution.
                texture_url: str = ""
                for res in ("2k", "1k", "4k"):
                    diffuse = (
                        files_data.get("Diffuse", {})
                        .get(res, {})
                        .get("png", {})
                        .get("url", "")
                    )
                    if diffuse:
                        texture_url = diffuse
                        break

                if not texture_url:
                    # Try any available file.
                    for category in files_data.values():
                        if isinstance(category, dict):
                            for res_data in category.values():
                                if isinstance(res_data, dict):
                                    for fmt_data in res_data.values():
                                        if isinstance(fmt_data, dict) and "url" in fmt_data:
                                            texture_url = fmt_data["url"]
                                            break
                                if texture_url:
                                    break
                        if texture_url:
                            break

                if not texture_url:
                    return {
                        "success": False,
                        "file_path": "",
                        "cost": 0.0,
                        "metadata": {"asset": best_match},
                        "error": f"No downloadable texture file found for '{best_match}'",
                    }

                # Download the texture.
                dl_resp = client.get(texture_url)
                dl_resp.raise_for_status()

                ext = Path(texture_url).suffix or ".png"
                out_path = Path(output_dir) / f"{request.id}{ext}"
                out_path.write_bytes(dl_resp.content)

        except httpx.HTTPStatusError as exc:
            logger.error("Poly Haven API error: %s", exc)
            return {
                "success": False,
                "file_path": "",
                "cost": 0.0,
                "metadata": {},
                "error": f"HTTP {exc.response.status_code}: {exc.response.text[:200]}",
            }
        except httpx.RequestError as exc:
            logger.error("Poly Haven request failed: %s", exc)
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
            "cost": 0.0,
            "metadata": {
                "tool": self.tool_id,
                "asset_name": best_match,
                "query": request.prompt,
            },
            "error": "",
        }
