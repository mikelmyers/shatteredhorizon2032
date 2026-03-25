"""
Texture Processor — Deterministic PBR texture generation from source images.

Takes raw photos or AI-generated images and produces game-ready PBR texture
sets: albedo (color-graded), normal map, roughness map, ambient occlusion,
and height map. All processing is pure code — no AI model needed.

Also handles seamless tiling, color grading to match the SH2032 milsim
palette, and resolution scaling.
"""

import json
from pathlib import Path
from typing import Optional

import numpy as np
from PIL import Image, ImageFilter, ImageEnhance, ImageOps
from scipy import ndimage


class TextureProcessor:
    """Processes raw images into PBR texture sets for UE5."""

    def __init__(self, manifest_path: str):
        with open(manifest_path) as f:
            self.manifest = json.load(f)
        self.color_profile = self.manifest["color_profile"]

    def process_full_pbr_set(self, source_image: Path, output_dir: Path,
                              asset_name: str, texture_size: tuple[int, int] = None
                              ) -> dict[str, Path]:
        """Generate a complete PBR texture set from a single source image."""
        if texture_size is None:
            texture_size = tuple(self.manifest["texture_sizes"]["terrain"])

        output_dir.mkdir(parents=True, exist_ok=True)
        img = Image.open(source_image).convert("RGB")
        img = img.resize(texture_size, Image.LANCZOS)

        suffixes = self.manifest["ue5_naming_convention"]["suffixes"]
        prefix = self.manifest["ue5_naming_convention"]["texture_prefix"]
        results = {}

        # Albedo (color-graded)
        albedo = self.color_grade(img)
        albedo_path = output_dir / f"{prefix}{asset_name}{suffixes['albedo']}.png"
        albedo.save(albedo_path, "PNG")
        results["albedo"] = albedo_path

        # Normal map
        normal = self.generate_normal_map(img)
        normal_path = output_dir / f"{prefix}{asset_name}{suffixes['normal']}.png"
        normal.save(normal_path, "PNG")
        results["normal"] = normal_path

        # Roughness map
        roughness = self.generate_roughness_map(img)
        roughness_path = output_dir / f"{prefix}{asset_name}{suffixes['roughness']}.png"
        roughness.save(roughness_path, "PNG")
        results["roughness"] = roughness_path

        # Ambient Occlusion
        ao = self.generate_ao_map(img)
        ao_path = output_dir / f"{prefix}{asset_name}{suffixes['ao']}.png"
        ao.save(ao_path, "PNG")
        results["ao"] = ao_path

        # Height map
        height = self.generate_height_map(img)
        height_path = output_dir / f"{prefix}{asset_name}{suffixes['height']}.png"
        height.save(height_path, "PNG")
        results["height"] = height_path

        print(f"[TextureProcessor] Generated PBR set for {asset_name}: "
              f"{len(results)} maps at {texture_size[0]}x{texture_size[1]}")
        return results

    def color_grade(self, img: Image.Image) -> Image.Image:
        """Apply the SH2032 milsim color grade — desaturated, muted, cool tones."""
        profile = self.color_profile

        # Desaturate
        enhancer = ImageEnhance.Color(img)
        img = enhancer.enhance(profile["saturation_scale"])

        # Adjust brightness
        enhancer = ImageEnhance.Brightness(img)
        img = enhancer.enhance(profile["brightness_scale"])

        # Boost contrast
        enhancer = ImageEnhance.Contrast(img)
        img = enhancer.enhance(profile["contrast"])

        # Apply tint (cool blue-grey military look)
        tint = profile["tint_rgb"]
        arr = np.array(img, dtype=np.float32)
        arr[:, :, 0] *= tint[0]
        arr[:, :, 1] *= tint[1]
        arr[:, :, 2] *= tint[2]
        arr = np.clip(arr, 0, 255).astype(np.uint8)

        return Image.fromarray(arr)

    def generate_normal_map(self, img: Image.Image, strength: float = 2.0) -> Image.Image:
        """Generate a tangent-space normal map from a source image using Sobel filters."""
        # Convert to grayscale height representation
        gray = np.array(img.convert("L"), dtype=np.float32) / 255.0

        # Sobel gradients
        sobel_x = ndimage.sobel(gray, axis=1)
        sobel_y = ndimage.sobel(gray, axis=0)

        # Scale by strength
        sobel_x *= strength
        sobel_y *= strength

        # Compute normal vectors
        normal_x = -sobel_x
        normal_y = -sobel_y
        normal_z = np.ones_like(gray)

        # Normalize
        length = np.sqrt(normal_x**2 + normal_y**2 + normal_z**2)
        normal_x /= length
        normal_y /= length
        normal_z /= length

        # Map from [-1,1] to [0,255]
        r = ((normal_x + 1.0) * 0.5 * 255).astype(np.uint8)
        g = ((normal_y + 1.0) * 0.5 * 255).astype(np.uint8)
        b = ((normal_z + 1.0) * 0.5 * 255).astype(np.uint8)

        return Image.fromarray(np.stack([r, g, b], axis=-1))

    def generate_roughness_map(self, img: Image.Image) -> Image.Image:
        """Generate a roughness map — high frequency detail = rough, smooth areas = low."""
        gray = img.convert("L")
        arr = np.array(gray, dtype=np.float32)

        # High-pass filter to extract surface detail
        blurred = ndimage.gaussian_filter(arr, sigma=8)
        high_pass = np.abs(arr - blurred)

        # Normalize to 0-255 range
        if high_pass.max() > 0:
            high_pass = (high_pass / high_pass.max() * 200).astype(np.float32)

        # Bias toward rough (milsim surfaces are generally rough)
        roughness = np.clip(high_pass + 80, 0, 255).astype(np.uint8)

        return Image.fromarray(roughness, mode="L")

    def generate_ao_map(self, img: Image.Image) -> Image.Image:
        """Generate an ambient occlusion map — darker in crevices, bright on exposed surfaces."""
        gray = np.array(img.convert("L"), dtype=np.float32)

        # Multi-scale blur to simulate light occlusion
        ao_small = ndimage.gaussian_filter(gray, sigma=2)
        ao_medium = ndimage.gaussian_filter(gray, sigma=8)
        ao_large = ndimage.gaussian_filter(gray, sigma=16)

        # Combine scales — darker areas in the original stay dark (occluded)
        ao = (ao_small * 0.3 + ao_medium * 0.4 + ao_large * 0.3)

        # Normalize and brighten (AO is mostly white with dark crevices)
        ao_min, ao_max = ao.min(), ao.max()
        if ao_max > ao_min:
            ao = (ao - ao_min) / (ao_max - ao_min)
        ao = (ao * 0.6 + 0.4) * 255  # Bias toward bright
        ao = np.clip(ao, 0, 255).astype(np.uint8)

        return Image.fromarray(ao, mode="L")

    def generate_height_map(self, img: Image.Image) -> Image.Image:
        """Generate a height/displacement map from luminance."""
        gray = img.convert("L")
        arr = np.array(gray, dtype=np.float32)

        # Smooth slightly to reduce noise
        arr = ndimage.gaussian_filter(arr, sigma=1.5)

        # Normalize full range
        arr_min, arr_max = arr.min(), arr.max()
        if arr_max > arr_min:
            arr = (arr - arr_min) / (arr_max - arr_min) * 255
        arr = arr.astype(np.uint8)

        return Image.fromarray(arr, mode="L")

    def make_seamless(self, img: Image.Image, blend_width: int = 64) -> Image.Image:
        """Make a texture seamlessly tileable by blending edges."""
        arr = np.array(img, dtype=np.float32)
        h, w = arr.shape[:2]

        # Create blend masks for horizontal and vertical seams
        blend_h = np.linspace(1, 0, blend_width).reshape(1, -1, 1)
        blend_v = np.linspace(1, 0, blend_width).reshape(-1, 1, 1)

        if len(arr.shape) == 2:
            blend_h = blend_h[:, :, 0]
            blend_v = blend_v[:, :, 0]
            arr_expanded = arr
        else:
            arr_expanded = arr

        # Horizontal seam blending
        left = arr_expanded[:, :blend_width]
        right = arr_expanded[:, -blend_width:]
        blended_h = left * (1 - blend_h) + right * blend_h
        arr_expanded[:, :blend_width] = blended_h
        arr_expanded[:, -blend_width:] = left * blend_h + right * (1 - blend_h)

        # Vertical seam blending
        top = arr_expanded[:blend_width, :]
        bottom = arr_expanded[-blend_width:, :]
        blended_v = top * (1 - blend_v) + bottom * blend_v
        arr_expanded[:blend_width, :] = blended_v
        arr_expanded[-blend_width:, :] = top * blend_v + bottom * (1 - blend_v)

        result = np.clip(arr_expanded, 0, 255).astype(np.uint8)
        if len(img.getbands()) == 1:
            return Image.fromarray(result, mode="L")
        return Image.fromarray(result)
