"""
3D Model Generator — AI-powered mesh generation via Meshy/Tripo APIs.
"""

import json
import os
import time
from abc import ABC, abstractmethod
from pathlib import Path
from typing import Optional

import requests


class ModelProvider(ABC):
    @abstractmethod
    def generate_model(self, prompt: str, poly_target: int) -> Optional[Path]:
        pass

    @abstractmethod
    def is_available(self) -> bool:
        pass


class MeshyProvider(ModelProvider):
    """Meshy AI — text to 3D model generation."""

    def __init__(self):
        self._api_key = os.environ.get("MESHY_API_KEY")
        self.base_url = "https://api.meshy.ai/v2"

    def is_available(self) -> bool:
        return bool(self._api_key)

    def generate_model(self, prompt: str, poly_target: int,
                        output_dir: Path = Path(".")) -> Optional[Path]:
        if not self.is_available():
            return None

        headers = {"Authorization": f"Bearer {self._api_key}"}

        # Create text-to-3D task
        payload = {
            "mode": "preview",
            "prompt": prompt,
            "art_style": "realistic",
            "negative_prompt": "low quality, cartoon, anime, stylized",
            "topology": "triangle",
            "target_polycount": poly_target,
        }

        try:
            resp = requests.post(f"{self.base_url}/text-to-3d",
                                 headers=headers, json=payload, timeout=30)
            resp.raise_for_status()
            task_id = resp.json()["result"]
            print(f"[Meshy] Task created: {task_id}")

            # Poll for completion
            for _ in range(120):
                time.sleep(5)
                status_resp = requests.get(
                    f"{self.base_url}/text-to-3d/{task_id}",
                    headers=headers, timeout=15,
                )
                status_resp.raise_for_status()
                data = status_resp.json()

                if data["status"] == "SUCCEEDED":
                    model_url = data["model_urls"]["glb"]
                    output_dir.mkdir(parents=True, exist_ok=True)
                    out_path = output_dir / f"{task_id}.glb"

                    dl_resp = requests.get(model_url, timeout=120)
                    with open(out_path, "wb") as f:
                        f.write(dl_resp.content)
                    print(f"[Meshy] Downloaded: {out_path.name}")
                    return out_path

                elif data["status"] == "FAILED":
                    print(f"[Meshy] Task failed: {data.get('task_error', 'unknown')}")
                    return None

            print("[Meshy] Timed out waiting for model")
            return None
        except requests.RequestException as e:
            print(f"[Meshy] Error: {e}")
            return None


class TripoProvider(ModelProvider):
    """Tripo3D API — alternative 3D generation."""

    def __init__(self):
        self._api_key = os.environ.get("TRIPO_API_KEY")
        self.base_url = "https://api.tripo3d.ai/v2"

    def is_available(self) -> bool:
        return bool(self._api_key)

    def generate_model(self, prompt: str, poly_target: int,
                        output_dir: Path = Path(".")) -> Optional[Path]:
        if not self.is_available():
            return None

        headers = {"Authorization": f"Bearer {self._api_key}"}
        payload = {"type": "text_to_model", "prompt": prompt}

        try:
            resp = requests.post(f"{self.base_url}/openapi/task",
                                 headers=headers, json=payload, timeout=30)
            resp.raise_for_status()
            task_id = resp.json()["data"]["task_id"]

            for _ in range(120):
                time.sleep(5)
                status_resp = requests.get(
                    f"{self.base_url}/openapi/task/{task_id}",
                    headers=headers, timeout=15,
                )
                status_resp.raise_for_status()
                data = status_resp.json()["data"]

                if data["status"] == "success":
                    model_url = data["output"]["model"]
                    output_dir.mkdir(parents=True, exist_ok=True)
                    out_path = output_dir / f"{task_id}.glb"

                    dl_resp = requests.get(model_url, timeout=120)
                    with open(out_path, "wb") as f:
                        f.write(dl_resp.content)
                    return out_path

                elif data["status"] == "failed":
                    print(f"[Tripo] Failed")
                    return None

            return None
        except requests.RequestException as e:
            print(f"[Tripo] Error: {e}")
            return None


class ModelGenerator:
    """Manages 3D model generation providers."""

    def __init__(self, manifest_path: str):
        with open(manifest_path) as f:
            self.manifest = json.load(f)
        self.providers: list[ModelProvider] = [MeshyProvider(), TripoProvider()]

    def get_available_provider(self) -> Optional[ModelProvider]:
        for p in self.providers:
            if p.is_available():
                return p
        return None

    def generate_from_manifest(self, output_dir: Path,
                                category: Optional[str] = None) -> dict[str, Path]:
        """Generate all models from manifest."""
        provider = self.get_available_provider()
        if not provider:
            print("[ModelGenerator] No providers available (set MESHY_API_KEY or TRIPO_API_KEY)")
            return {}

        generated = {}
        categories = [category] if category else list(self.manifest.keys())

        for cat in categories:
            items = self.manifest.get(cat, [])
            cat_dir = output_dir / cat
            for item in items:
                print(f"[ModelGenerator] Generating {item['id']}...")
                path = provider.generate_model(
                    item["description"], item["poly_target"], cat_dir
                )
                if path:
                    generated[item["id"]] = path
                time.sleep(2)

        print(f"[ModelGenerator] Generated {len(generated)} models")
        return generated
