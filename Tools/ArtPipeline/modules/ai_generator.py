"""
AI Generator — Pluggable AI image generation for game textures.

Supports multiple backends:
  - Stability AI API (cloud, SD3.5)
  - Replicate API (cloud, SDXL)
  - ComfyUI (local GPU inference)

Generates textures from text prompts or transforms reference photos
via img2img. Each backend is a pluggable provider — enable whichever
you have access to in api_config.json.
"""

import base64
import io
import json
import os
from abc import ABC, abstractmethod
from pathlib import Path
from typing import Optional

import requests
from PIL import Image


class AIProvider(ABC):
    """Base class for AI image generation providers."""

    @abstractmethod
    def generate(self, prompt: str, width: int, height: int,
                 reference_image: Optional[Path] = None) -> Optional[Image.Image]:
        pass

    @abstractmethod
    def is_available(self) -> bool:
        pass


class StabilityAIProvider(AIProvider):
    """Stability AI API provider (SD3.5 Large)."""

    def __init__(self, config: dict):
        self.config = config
        self._api_key = os.environ.get(config.get("env_key", "STABILITY_API_KEY"))
        self.base_url = config.get("base_url", "https://api.stability.ai/v2beta")

    def is_available(self) -> bool:
        return bool(self._api_key) and self.config.get("enabled", False)

    def generate(self, prompt: str, width: int = 1024, height: int = 1024,
                 reference_image: Optional[Path] = None) -> Optional[Image.Image]:
        if not self.is_available():
            return None

        headers = {
            "Authorization": f"Bearer {self._api_key}",
            "Accept": "image/*",
        }

        if reference_image and reference_image.exists():
            return self._img2img(prompt, reference_image, width, height, headers)
        else:
            return self._txt2img(prompt, width, height, headers)

    def _txt2img(self, prompt: str, width: int, height: int,
                 headers: dict) -> Optional[Image.Image]:
        url = f"{self.base_url}/stable-image/generate/sd3"

        data = {
            "prompt": prompt,
            "model": self.config.get("model", "sd3.5-large"),
            "cfg_scale": self.config.get("cfg_scale", 7.0),
            "steps": self.config.get("steps", 40),
            "output_format": "png",
        }

        # SD3 API expects specific resolutions
        data["aspect_ratio"] = "1:1"

        try:
            resp = requests.post(url, headers=headers, data=data,
                                 files={"none": ""}, timeout=120)
            resp.raise_for_status()
            return Image.open(io.BytesIO(resp.content))
        except requests.RequestException as e:
            print(f"[StabilityAI] txt2img failed: {e}")
            return None

    def _img2img(self, prompt: str, ref_path: Path, width: int, height: int,
                 headers: dict) -> Optional[Image.Image]:
        url = f"{self.base_url}/stable-image/generate/sd3"

        # Prepare reference image
        ref_img = Image.open(ref_path).convert("RGB")
        ref_img = ref_img.resize((width, height), Image.LANCZOS)
        buf = io.BytesIO()
        ref_img.save(buf, format="PNG")
        buf.seek(0)

        data = {
            "prompt": prompt,
            "model": self.config.get("model", "sd3.5-large"),
            "strength": self.config.get("img2img_strength", 0.65),
            "cfg_scale": self.config.get("cfg_scale", 7.0),
            "output_format": "png",
            "mode": "image-to-image",
        }

        files = {"image": ("reference.png", buf, "image/png")}

        try:
            resp = requests.post(url, headers=headers, data=data,
                                 files=files, timeout=120)
            resp.raise_for_status()
            return Image.open(io.BytesIO(resp.content))
        except requests.RequestException as e:
            print(f"[StabilityAI] img2img failed: {e}")
            return None


class ReplicateProvider(AIProvider):
    """Replicate API provider."""

    def __init__(self, config: dict):
        self.config = config
        self._api_token = os.environ.get(config.get("env_key", "REPLICATE_API_TOKEN"))

    def is_available(self) -> bool:
        return bool(self._api_token) and self.config.get("enabled", False)

    def generate(self, prompt: str, width: int = 1024, height: int = 1024,
                 reference_image: Optional[Path] = None) -> Optional[Image.Image]:
        if not self.is_available():
            return None

        import time

        headers = {
            "Authorization": f"Bearer {self._api_token}",
            "Content-Type": "application/json",
        }

        model = self.config.get("model", "stability-ai/sdxl")
        payload = {
            "version": model,
            "input": {
                "prompt": prompt,
                "width": min(width, 1024),
                "height": min(height, 1024),
                "num_outputs": 1,
            },
        }

        if reference_image and reference_image.exists():
            with open(reference_image, "rb") as f:
                b64 = base64.b64encode(f.read()).decode()
            payload["input"]["image"] = f"data:image/png;base64,{b64}"
            payload["input"]["prompt_strength"] = self.config.get("img2img_strength", 0.6)

        try:
            resp = requests.post(
                "https://api.replicate.com/v1/predictions",
                headers=headers, json=payload, timeout=30,
            )
            resp.raise_for_status()
            prediction = resp.json()

            # Poll for completion
            poll_url = prediction["urls"]["get"]
            for _ in range(60):
                time.sleep(2)
                poll_resp = requests.get(poll_url, headers=headers, timeout=15)
                poll_resp.raise_for_status()
                status = poll_resp.json()
                if status["status"] == "succeeded":
                    img_url = status["output"][0]
                    img_resp = requests.get(img_url, timeout=60)
                    return Image.open(io.BytesIO(img_resp.content))
                elif status["status"] == "failed":
                    print(f"[Replicate] Generation failed: {status.get('error')}")
                    return None

            print("[Replicate] Generation timed out")
            return None
        except requests.RequestException as e:
            print(f"[Replicate] Generation failed: {e}")
            return None


class ComfyUIProvider(AIProvider):
    """Local ComfyUI provider — no API key needed, runs on local GPU."""

    def __init__(self, config: dict):
        self.config = config
        self.base_url = config.get("base_url", "http://127.0.0.1:8188")

    def is_available(self) -> bool:
        if not self.config.get("enabled", False):
            return False
        try:
            resp = requests.get(f"{self.base_url}/system_stats", timeout=5)
            return resp.status_code == 200
        except requests.RequestException:
            return False

    def generate(self, prompt: str, width: int = 1024, height: int = 1024,
                 reference_image: Optional[Path] = None) -> Optional[Image.Image]:
        if not self.is_available():
            return None

        import time

        # Build a basic ComfyUI workflow
        workflow = self._build_workflow(prompt, width, height, reference_image)

        try:
            # Queue the prompt
            resp = requests.post(
                f"{self.base_url}/prompt",
                json={"prompt": workflow},
                timeout=30,
            )
            resp.raise_for_status()
            prompt_id = resp.json()["prompt_id"]

            # Poll for completion
            for _ in range(120):
                time.sleep(2)
                hist_resp = requests.get(
                    f"{self.base_url}/history/{prompt_id}", timeout=15
                )
                hist_resp.raise_for_status()
                history = hist_resp.json()
                if prompt_id in history:
                    outputs = history[prompt_id]["outputs"]
                    for node_id, output in outputs.items():
                        if "images" in output:
                            img_info = output["images"][0]
                            img_resp = requests.get(
                                f"{self.base_url}/view",
                                params={
                                    "filename": img_info["filename"],
                                    "subfolder": img_info.get("subfolder", ""),
                                    "type": img_info.get("type", "output"),
                                },
                                timeout=30,
                            )
                            return Image.open(io.BytesIO(img_resp.content))

            print("[ComfyUI] Generation timed out")
            return None
        except requests.RequestException as e:
            print(f"[ComfyUI] Generation failed: {e}")
            return None

    def _build_workflow(self, prompt: str, width: int, height: int,
                        reference_image: Optional[Path] = None) -> dict:
        """Build a minimal ComfyUI workflow JSON."""
        workflow = {
            "3": {
                "class_type": "KSampler",
                "inputs": {
                    "seed": -1,
                    "steps": 40,
                    "cfg": 7.0,
                    "sampler_name": "euler_ancestral",
                    "scheduler": "normal",
                    "denoise": 1.0 if not reference_image else 0.65,
                    "model": ["4", 0],
                    "positive": ["6", 0],
                    "negative": ["7", 0],
                    "latent_image": ["5", 0],
                },
            },
            "4": {
                "class_type": "CheckpointLoaderSimple",
                "inputs": {"ckpt_name": "sd_xl_base_1.0.safetensors"},
            },
            "5": {
                "class_type": "EmptyLatentImage",
                "inputs": {"width": width, "height": height, "batch_size": 1},
            },
            "6": {
                "class_type": "CLIPTextEncode",
                "inputs": {"text": prompt, "clip": ["4", 1]},
            },
            "7": {
                "class_type": "CLIPTextEncode",
                "inputs": {
                    "text": "blurry, low quality, watermark, text, logo, cartoon, illustration",
                    "clip": ["4", 1],
                },
            },
            "8": {
                "class_type": "VAEDecode",
                "inputs": {"samples": ["3", 0], "vae": ["4", 2]},
            },
            "9": {
                "class_type": "SaveImage",
                "inputs": {"images": ["8", 0], "filename_prefix": "SH2032"},
            },
        }
        return workflow


class AIGenerator:
    """Manages multiple AI providers and generates game textures."""

    def __init__(self, config_path: str):
        with open(config_path) as f:
            config = json.load(f)

        ai_cfg = config["ai_generation"]
        self.providers: list[AIProvider] = []

        # Register providers in priority order
        if "stability_ai" in ai_cfg:
            self.providers.append(StabilityAIProvider(ai_cfg["stability_ai"]))
        if "replicate" in ai_cfg:
            self.providers.append(ReplicateProvider(ai_cfg["replicate"]))
        if "comfyui_local" in ai_cfg:
            self.providers.append(ComfyUIProvider(ai_cfg["comfyui_local"]))

    def get_available_provider(self) -> Optional[AIProvider]:
        """Return the first available provider."""
        for provider in self.providers:
            if provider.is_available():
                name = provider.__class__.__name__
                print(f"[AIGenerator] Using provider: {name}")
                return provider
        print("[AIGenerator] No AI providers available")
        return None

    def generate_texture(self, prompt: str, width: int = 1024, height: int = 1024,
                         reference_image: Optional[Path] = None) -> Optional[Image.Image]:
        """Generate a texture using the first available AI provider."""
        provider = self.get_available_provider()
        if not provider:
            return None
        return provider.generate(prompt, width, height, reference_image)

    def generate_terrain_set(self, terrain_config: dict, manifest: dict,
                             output_dir: Path,
                             reference_image: Optional[Path] = None) -> list[Path]:
        """Generate all destruction stages for a terrain type."""
        output_dir.mkdir(parents=True, exist_ok=True)
        generated = []

        base_prompt = terrain_config["ai_prompt"]

        for stage in manifest.get("destruction_stages", [{"stage": 0, "name": "Pristine", "ai_modifier": ""}]):
            modifier = stage["ai_modifier"]
            if modifier:
                full_prompt = f"{base_prompt}, {modifier}"
            else:
                full_prompt = base_prompt

            # Add quality boosters
            full_prompt += ", 8K, highly detailed, photorealistic, seamless tileable texture"

            tid = terrain_config["id"]
            stage_num = stage["stage"]
            size = manifest["texture_sizes"]["terrain"]

            print(f"[AIGenerator] Generating {tid} stage {stage_num}: {stage['name']}")

            img = self.generate_texture(full_prompt, size[0], size[1], reference_image)
            if img:
                fname = f"T_{tid}_stage{stage_num}_ai_raw.png"
                path = output_dir / fname
                img.save(path, "PNG")
                generated.append(path)
                print(f"[AIGenerator] Saved: {fname}")
            else:
                print(f"[AIGenerator] Failed to generate {tid} stage {stage_num}")

        return generated
