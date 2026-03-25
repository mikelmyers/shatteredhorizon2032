"""
Image Sourcer — Automated reference photo acquisition via licensed APIs.

Searches multiple free/licensed photo APIs for reference images matching
terrain types, destruction states, and environment categories defined
in the asset manifest. No scraping — all sources are API-based with
proper licensing (Unsplash, Pexels, public domain satellite imagery).
"""

import json
import os
import time
import hashlib
from pathlib import Path
from typing import Optional

import requests


class ImageSourcer:
    """Acquires reference photos from licensed image APIs."""

    def __init__(self, config_path: str, output_dir: str):
        with open(config_path) as f:
            self.config = json.load(f)

        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

        self._unsplash_key = os.environ.get("UNSPLASH_ACCESS_KEY")
        self._pexels_key = os.environ.get("PEXELS_API_KEY")

    def search_unsplash(self, query: str, count: int = 5) -> list[dict]:
        """Search Unsplash for photos matching query."""
        if not self._unsplash_key:
            print("[ImageSourcer] UNSPLASH_ACCESS_KEY not set, skipping Unsplash")
            return []

        cfg = self.config["image_sourcing"]["unsplash"]
        url = f"{cfg['base_url']}/search/photos"
        headers = {"Authorization": f"Client-ID {self._unsplash_key}"}
        params = {
            "query": query,
            "per_page": count,
            "orientation": cfg.get("preferred_orientation", "squarish"),
        }

        try:
            resp = requests.get(url, headers=headers, params=params, timeout=30)
            resp.raise_for_status()
            data = resp.json()
            results = []
            for photo in data.get("results", []):
                results.append({
                    "source": "unsplash",
                    "id": photo["id"],
                    "url": photo["urls"]["raw"] + "&w=2048&h=2048&fit=crop",
                    "download_url": photo["links"]["download"],
                    "description": photo.get("description", ""),
                    "photographer": photo["user"]["name"],
                    "license": "Unsplash License (free for commercial use)",
                })
            return results
        except requests.RequestException as e:
            print(f"[ImageSourcer] Unsplash search failed: {e}")
            return []

    def search_pexels(self, query: str, count: int = 5) -> list[dict]:
        """Search Pexels for photos matching query."""
        if not self._pexels_key:
            print("[ImageSourcer] PEXELS_API_KEY not set, skipping Pexels")
            return []

        cfg = self.config["image_sourcing"]["pexels"]
        url = f"{cfg['base_url']}/search"
        headers = {"Authorization": self._pexels_key}
        params = {
            "query": query,
            "per_page": count,
            "orientation": cfg.get("preferred_orientation", "square"),
        }

        try:
            resp = requests.get(url, headers=headers, params=params, timeout=30)
            resp.raise_for_status()
            data = resp.json()
            results = []
            for photo in data.get("photos", []):
                results.append({
                    "source": "pexels",
                    "id": photo["id"],
                    "url": photo["src"]["original"],
                    "description": photo.get("alt", ""),
                    "photographer": photo["photographer"],
                    "license": "Pexels License (free for commercial use)",
                })
            return results
        except requests.RequestException as e:
            print(f"[ImageSourcer] Pexels search failed: {e}")
            return []

    def search_all(self, query: str, count_per_source: int = 3) -> list[dict]:
        """Search all enabled sources and return combined results."""
        results = []
        cfg = self.config["image_sourcing"]

        if cfg["unsplash"]["enabled"]:
            results.extend(self.search_unsplash(query, count_per_source))

        if cfg["pexels"]["enabled"]:
            results.extend(self.search_pexels(query, count_per_source))

        print(f"[ImageSourcer] Found {len(results)} images for '{query}'")
        return results

    def download_image(self, url: str, filename: str) -> Optional[Path]:
        """Download an image to the output directory."""
        filepath = self.output_dir / filename

        if filepath.exists():
            print(f"[ImageSourcer] Already downloaded: {filename}")
            return filepath

        try:
            resp = requests.get(url, timeout=60, stream=True)
            resp.raise_for_status()
            with open(filepath, "wb") as f:
                for chunk in resp.iter_content(chunk_size=8192):
                    f.write(chunk)
            print(f"[ImageSourcer] Downloaded: {filename}")
            return filepath
        except requests.RequestException as e:
            print(f"[ImageSourcer] Download failed for {filename}: {e}")
            return None

    def source_terrain_references(self, manifest_path: str) -> dict[str, list[Path]]:
        """Source reference photos for all terrain types in the manifest."""
        with open(manifest_path) as f:
            manifest = json.load(f)

        downloaded = {}
        for terrain in manifest["terrain_types"]:
            tid = terrain["id"]
            downloaded[tid] = []

            for term in terrain["search_terms"]:
                results = self.search_all(term, count_per_source=2)
                for i, result in enumerate(results[:3]):
                    ext = "jpg"
                    src = result["source"]
                    fname = f"ref_{tid}_{src}_{i}.{ext}"
                    path = self.download_image(result["url"], fname)
                    if path:
                        downloaded[tid].append(path)

                # Rate limit courtesy
                time.sleep(0.5)

        return downloaded

    def source_environment_references(self, manifest_path: str) -> dict[str, list[Path]]:
        """Source reference photos for environment asset categories."""
        with open(manifest_path) as f:
            manifest = json.load(f)

        downloaded = {}
        for category in manifest["environment_assets"]:
            cat_name = category["category"]
            for item in category["items"]:
                key = f"{cat_name}/{item}"
                downloaded[key] = []

                query = item.replace("_", " ")
                results = self.search_all(f"{query} photorealistic", count_per_source=2)
                for i, result in enumerate(results[:2]):
                    fname = f"ref_{cat_name}_{item}_{result['source']}_{i}.jpg"
                    path = self.download_image(result["url"], fname)
                    if path:
                        downloaded[key].append(path)

                time.sleep(0.5)

        return downloaded

    def generate_attribution_log(self, results: list[dict], log_path: str):
        """Write a license attribution log for all sourced images."""
        with open(log_path, "w") as f:
            f.write("# Image Attribution Log\n")
            f.write("# Shattered Horizon 2032 — Primordia Games\n\n")
            for r in results:
                f.write(f"Source: {r['source']}\n")
                f.write(f"ID: {r['id']}\n")
                f.write(f"Photographer: {r.get('photographer', 'Unknown')}\n")
                f.write(f"License: {r.get('license', 'See source')}\n")
                f.write(f"Description: {r.get('description', 'N/A')}\n")
                f.write("---\n")
