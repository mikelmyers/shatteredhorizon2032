"""
SFX Sourcer — Sources reference audio from Freesound.org API.
Creative Commons licensed audio for reference and fallback use.
"""

import json
import os
import time
from pathlib import Path
from typing import Optional

import requests


class SFXSourcer:
    """Sources reference sound effects from free audio APIs."""

    def __init__(self, config_path: str, output_dir: str):
        with open(config_path) as f:
            self.config = json.load(f)
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self._freesound_key = os.environ.get("FREESOUND_API_KEY")

    def search_freesound(self, query: str, count: int = 5) -> list[dict]:
        """Search Freesound.org for sounds matching query."""
        if not self._freesound_key:
            print("[SFXSourcer] FREESOUND_API_KEY not set, skipping")
            return []

        cfg = self.config["sfx_sourcing"]["freesound"]
        url = f"{cfg['base_url']}/search/text/"
        params = {
            "query": query,
            "token": self._freesound_key,
            "page_size": count,
            "fields": "id,name,previews,duration,license,username,tags",
            "filter": "duration:[0.1 TO 30]",
        }

        try:
            resp = requests.get(url, params=params, timeout=30)
            resp.raise_for_status()
            data = resp.json()
            results = []
            for sound in data.get("results", []):
                previews = sound.get("previews", {})
                results.append({
                    "source": "freesound",
                    "id": sound["id"],
                    "name": sound["name"],
                    "url": previews.get("preview-hq-mp3", previews.get("preview-lq-mp3", "")),
                    "duration": sound["duration"],
                    "license": sound.get("license", ""),
                    "username": sound.get("username", ""),
                })
            return results
        except requests.RequestException as e:
            print(f"[SFXSourcer] Freesound search failed: {e}")
            return []

    def download_sound(self, url: str, filename: str) -> Optional[Path]:
        """Download a sound file."""
        filepath = self.output_dir / filename
        if filepath.exists():
            return filepath
        try:
            resp = requests.get(url, timeout=60, stream=True)
            resp.raise_for_status()
            with open(filepath, "wb") as f:
                for chunk in resp.iter_content(8192):
                    f.write(chunk)
            print(f"[SFXSourcer] Downloaded: {filename}")
            return filepath
        except requests.RequestException as e:
            print(f"[SFXSourcer] Download failed: {e}")
            return None

    def source_category(self, query: str, asset_id: str, count: int = 3) -> list[Path]:
        """Search and download reference sounds for an asset."""
        results = self.search_freesound(query, count)
        paths = []
        for i, r in enumerate(results[:count]):
            if r["url"]:
                fname = f"ref_{asset_id}_{r['source']}_{i}.mp3"
                path = self.download_sound(r["url"], fname)
                if path:
                    paths.append(path)
            time.sleep(0.3)
        return paths

    def source_all(self, manifest_path: str) -> dict[str, list[Path]]:
        """Source references for all assets in manifest."""
        with open(manifest_path) as f:
            manifest = json.load(f)

        downloaded = {}

        # Projectiles
        for item in manifest.get("projectiles", []):
            refs = self.source_category(item["description"], item["id"])
            downloaded[item["id"]] = refs

        # Destruction
        for item in manifest.get("destruction", []):
            refs = self.source_category(item["description"], item["id"])
            downloaded[item["id"]] = refs

        # Drones
        for item in manifest.get("drones", []):
            refs = self.source_category(item["description"], item["id"])
            downloaded[item["id"]] = refs

        print(f"[SFXSourcer] Sourced {sum(len(v) for v in downloaded.values())} total references")
        return downloaded
