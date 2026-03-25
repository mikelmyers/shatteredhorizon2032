"""
UI Generator — Creates HUD textures and tactical icons via code + AI.

Code-generates clean geometric UI elements (compass, stance icons, formations).
Uses AI for concept art (menu backgrounds, loading screens).
"""

import json
import math
import os
from pathlib import Path
from typing import Optional

import numpy as np
from PIL import Image, ImageDraw, ImageFont

import requests


# Military green HUD color
HUD_GREEN = (153, 204, 153, 217)  # (0.6, 0.8, 0.6, 0.85) * 255
HUD_RED = (230, 51, 51, 230)
HUD_YELLOW = (230, 179, 51, 230)
HUD_WHITE = (220, 220, 220, 240)


class CodeUIGenerator:
    """Generates UI elements programmatically with PIL."""

    def generate_damage_indicator(self, size: int = 512) -> Image.Image:
        """Directional damage indicator — red gradient wedge."""
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        cx, cy = size // 2, size // 2
        # Red wedge pointing up (north), semi-transparent
        points = [
            (cx, int(cy * 0.15)),
            (int(cx - size * 0.12), int(cy * 0.4)),
            (int(cx + size * 0.12), int(cy * 0.4)),
        ]
        draw.polygon(points, fill=(220, 30, 30, 180))
        # Softer outer glow
        for i in range(5):
            offset = (i + 1) * 3
            alpha = max(0, 100 - i * 25)
            pts = [
                (cx, int(cy * 0.15) - offset),
                (int(cx - size * 0.12) - offset, int(cy * 0.4) + offset),
                (int(cx + size * 0.12) + offset, int(cy * 0.4) + offset),
            ]
            draw.polygon(pts, fill=(220, 30, 30, alpha))
        return img

    def generate_suppression_vignette(self, width: int = 1920,
                                       height: int = 1080) -> Image.Image:
        """Dark edges vignette with red tint for suppression effect."""
        img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        arr = np.zeros((height, width, 4), dtype=np.uint8)

        cy, cx = height / 2, width / 2
        max_dist = math.sqrt(cx**2 + cy**2)

        y_coords = np.arange(height).reshape(-1, 1)
        x_coords = np.arange(width).reshape(1, -1)
        dist = np.sqrt((x_coords - cx)**2 + (y_coords - cy)**2) / max_dist

        # Vignette: transparent center, dark red edges
        alpha = np.clip((dist - 0.4) * 2.0, 0, 1)
        arr[:, :, 0] = (alpha * 40).astype(np.uint8)   # Red tint
        arr[:, :, 1] = 0
        arr[:, :, 2] = 0
        arr[:, :, 3] = (alpha * 180).astype(np.uint8)

        return Image.fromarray(arr, "RGBA")

    def generate_stance_icon(self, stance: str, size: int = 64) -> Image.Image:
        """Simple soldier silhouette for stance display."""
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        color = HUD_GREEN
        cx = size // 2
        head_r = size // 10

        if stance == "standing":
            # Head
            draw.ellipse([cx - head_r, 8, cx + head_r, 8 + head_r * 2], fill=color)
            # Body
            draw.rectangle([cx - 4, 8 + head_r * 2, cx + 4, size - 20], fill=color)
            # Legs
            draw.line([(cx, size - 20), (cx - 8, size - 4)], fill=color, width=3)
            draw.line([(cx, size - 20), (cx + 8, size - 4)], fill=color, width=3)
            # Arms
            draw.line([(cx, 22), (cx - 10, 32)], fill=color, width=2)
            draw.line([(cx, 22), (cx + 10, 32)], fill=color, width=2)

        elif stance == "crouching":
            # Head (lower)
            draw.ellipse([cx - head_r, 18, cx + head_r, 18 + head_r * 2], fill=color)
            # Body (angled)
            draw.rectangle([cx - 4, 18 + head_r * 2, cx + 4, size - 14], fill=color)
            # Bent legs
            draw.line([(cx, size - 14), (cx - 10, size - 4)], fill=color, width=3)
            draw.line([(cx - 10, size - 4), (cx - 4, size - 4)], fill=color, width=3)

        elif stance == "prone":
            # Horizontal body
            y = size // 2 + 5
            draw.ellipse([8, y - head_r, 8 + head_r * 2, y + head_r], fill=color)
            draw.rectangle([8 + head_r * 2, y - 3, size - 8, y + 3], fill=color)

        return img

    def generate_body_diagram(self, size: int = 256) -> Image.Image:
        """Body silhouette with zone regions for injury display."""
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        cx = size // 2
        color = (100, 160, 100, 150)  # Dim green base

        # Head
        draw.ellipse([cx - 15, 15, cx + 15, 45], outline=color, width=2)
        # Torso
        draw.rectangle([cx - 25, 48, cx + 25, 130], outline=color, width=2)
        # Left arm
        draw.rectangle([cx - 45, 50, cx - 28, 120], outline=color, width=2)
        # Right arm
        draw.rectangle([cx + 28, 50, cx + 45, 120], outline=color, width=2)
        # Left leg
        draw.rectangle([cx - 22, 133, cx - 5, 240], outline=color, width=2)
        # Right leg
        draw.rectangle([cx + 5, 133, cx + 22, 240], outline=color, width=2)

        return img

    def generate_compass_bar(self, width: int = 1024,
                              height: int = 64) -> Image.Image:
        """Compass bar background with cardinal markers and ticks."""
        img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)

        # Background bar
        draw.rectangle([0, 10, width, height - 10], fill=(20, 25, 20, 140))

        # Tick marks every 10 degrees (36 ticks for 360)
        tick_spacing = width / 36
        for i in range(37):
            x = int(i * tick_spacing)
            deg = i * 10
            if deg % 90 == 0:
                # Cardinal: tall tick + label
                draw.line([(x, 12), (x, height - 12)], fill=HUD_GREEN, width=2)
                labels = {0: "N", 90: "E", 180: "S", 270: "W", 360: "N"}
                label = labels.get(deg, "")
                if label:
                    draw.text((x - 4, height - 28), label, fill=HUD_WHITE)
            elif deg % 30 == 0:
                # Medium tick
                draw.line([(x, 18), (x, height - 18)], fill=HUD_GREEN, width=1)
                draw.text((x - 8, height - 26), str(deg), fill=(150, 180, 150, 180))
            else:
                # Small tick
                draw.line([(x, 24), (x, height - 24)], fill=(100, 130, 100, 120), width=1)

        return img

    def generate_hit_marker(self, size: int = 64) -> Image.Image:
        """Simple crosshair hit marker."""
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        cx, cy = size // 2, size // 2
        gap = 6
        length = 12
        color = (255, 255, 255, 220)
        w = 2
        # Four lines with center gap
        draw.line([(cx - gap - length, cy), (cx - gap, cy)], fill=color, width=w)
        draw.line([(cx + gap, cy), (cx + gap + length, cy)], fill=color, width=w)
        draw.line([(cx, cy - gap - length), (cx, cy - gap)], fill=color, width=w)
        draw.line([(cx, cy + gap), (cx, cy + gap + length)], fill=color, width=w)
        return img

    def generate_squad_icon(self, icon_name: str, size: int = 128) -> Image.Image:
        """Generate tactical command icons."""
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        cx, cy = size // 2, size // 2
        color = HUD_GREEN
        pad = size // 8

        if icon_name == "move_to":
            # Arrow pointing forward
            draw.polygon([(cx, pad), (size - pad, size - pad), (pad, size - pad)], outline=color, width=3)

        elif icon_name == "suppress":
            # Three horizontal lines (bullets)
            for i in range(3):
                y = pad + 15 + i * 25
                draw.line([(pad + 10, y), (size - pad - 10, y)], fill=color, width=3)
                draw.polygon([(size - pad - 10, y - 5), (size - pad, y), (size - pad - 10, y + 5)], fill=color)

        elif icon_name in ("flank_left", "flank_right"):
            # Curved arrow
            flip = -1 if "left" in icon_name else 1
            points = [(cx, size - pad), (cx + flip * 30, cy), (cx, pad)]
            draw.line(points, fill=color, width=3)
            # Arrowhead
            draw.polygon([(cx - 8, pad + 5), (cx, pad - 5), (cx + 8, pad + 5)], fill=color)

        elif icon_name == "hold_position":
            # Raised fist / stop sign
            draw.rectangle([pad + 10, pad + 10, size - pad - 10, size - pad - 10], outline=color, width=3)
            draw.line([(pad + 20, cy), (size - pad - 20, cy)], fill=color, width=3)

        elif icon_name == "regroup":
            # Converging arrows
            draw.line([(pad, pad), (cx, cy)], fill=color, width=2)
            draw.line([(size - pad, pad), (cx, cy)], fill=color, width=2)
            draw.line([(pad, size - pad), (cx, cy)], fill=color, width=2)
            draw.line([(size - pad, size - pad), (cx, cy)], fill=color, width=2)
            draw.ellipse([cx - 8, cy - 8, cx + 8, cy + 8], fill=color)

        elif icon_name == "advance":
            # Double chevron up
            for offset in [0, 20]:
                y = pad + 15 + offset
                draw.line([(pad + 10, y + 20), (cx, y), (size - pad - 10, y + 20)], fill=color, width=3)

        elif icon_name == "cover_me":
            # Shield shape
            draw.arc([pad + 10, pad, size - pad - 10, size - pad], 0, 180, fill=color, width=3)
            draw.line([(pad + 10, cy), (pad + 10, pad + 20)], fill=color, width=3)
            draw.line([(size - pad - 10, cy), (size - pad - 10, pad + 20)], fill=color, width=3)

        elif icon_name == "fall_back":
            # Arrow pointing down/back
            draw.polygon([(cx, size - pad), (pad, pad + 20), (size - pad, pad + 20)], outline=color, width=3)

        elif icon_name == "cease_fire":
            # X mark
            draw.line([(pad + 10, pad + 10), (size - pad - 10, size - pad - 10)], fill=HUD_RED, width=4)
            draw.line([(size - pad - 10, pad + 10), (pad + 10, size - pad - 10)], fill=HUD_RED, width=4)

        elif icon_name == "medical":
            # Red cross
            draw.rectangle([cx - 5, pad + 15, cx + 5, size - pad - 15], fill=HUD_RED)
            draw.rectangle([pad + 15, cy - 5, size - pad - 15, cy + 5], fill=HUD_RED)

        elif icon_name == "resupply":
            # Ammo box
            draw.rectangle([pad + 15, pad + 20, size - pad - 15, size - pad - 20], outline=color, width=3)
            draw.line([(cx, pad + 30), (cx, size - pad - 30)], fill=color, width=2)
            draw.line([(pad + 25, cy), (size - pad - 25, cy)], fill=color, width=2)

        else:
            # Generic circle marker
            draw.ellipse([pad, pad, size - pad, size - pad], outline=color, width=3)

        return img

    def generate_formation_icon(self, formation: str,
                                 size: int = 128) -> Image.Image:
        """Generate formation diagram icons (dot patterns)."""
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        dot_r = 6
        color = HUD_GREEN

        positions = {
            "wedge": [(0.5, 0.2), (0.3, 0.5), (0.7, 0.5), (0.2, 0.8), (0.8, 0.8)],
            "line": [(0.15, 0.5), (0.325, 0.5), (0.5, 0.5), (0.675, 0.5), (0.85, 0.5)],
            "column": [(0.5, 0.1), (0.5, 0.3), (0.5, 0.5), (0.5, 0.7), (0.5, 0.9)],
            "echelon": [(0.2, 0.2), (0.35, 0.35), (0.5, 0.5), (0.65, 0.65), (0.8, 0.8)],
        }

        dots = positions.get(formation, positions["wedge"])
        for i, (fx, fy) in enumerate(dots):
            x, y = int(fx * size), int(fy * size)
            fill = (220, 220, 100, 240) if i == 0 else color  # Leader is highlighted
            draw.ellipse([x - dot_r, y - dot_r, x + dot_r, y + dot_r], fill=fill)

        return img

    def generate_roe_icon(self, roe: str, size: int = 128) -> Image.Image:
        """Generate Rules of Engagement icons."""
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        cx, cy = size // 2, size // 2

        if roe == "hold_fire":
            # Crosshair with X
            draw.ellipse([cx - 30, cy - 30, cx + 30, cy + 30], outline=HUD_RED, width=2)
            draw.line([(cx - 20, cy - 20), (cx + 20, cy + 20)], fill=HUD_RED, width=3)
        elif roe == "return_fire":
            # Crosshair with shield
            draw.ellipse([cx - 25, cy - 25, cx + 25, cy + 25], outline=HUD_YELLOW, width=2)
            draw.line([(cx, cy - 15), (cx, cy + 15)], fill=HUD_YELLOW, width=2)
            draw.line([(cx - 15, cy), (cx + 15, cy)], fill=HUD_YELLOW, width=2)
        elif roe == "fire_at_will":
            # Crosshair active
            draw.ellipse([cx - 25, cy - 25, cx + 25, cy + 25], outline=HUD_GREEN, width=2)
            draw.line([(cx, cy - 20), (cx, cy + 20)], fill=HUD_GREEN, width=2)
            draw.line([(cx - 20, cy), (cx + 20, cy)], fill=HUD_GREEN, width=2)
        elif roe == "weapons_free":
            # Double crosshair
            draw.ellipse([cx - 30, cy - 30, cx + 30, cy + 30], outline=HUD_GREEN, width=2)
            draw.ellipse([cx - 18, cy - 18, cx + 18, cy + 18], outline=HUD_GREEN, width=1)
            draw.line([(cx, cy - 25), (cx, cy + 25)], fill=HUD_GREEN, width=2)
            draw.line([(cx - 25, cy), (cx + 25, cy)], fill=HUD_GREEN, width=2)

        return img


class AIUIGenerator:
    """Uses AI for concept art (menu backgrounds, loading screens)."""

    def __init__(self, config_path: str):
        self._api_key = os.environ.get("STABILITY_API_KEY")

    def is_available(self) -> bool:
        return bool(self._api_key)

    def generate_concept_art(self, prompt: str, width: int = 1920,
                              height: int = 1080) -> Optional[Image.Image]:
        if not self.is_available():
            print("[AIUIGenerator] STABILITY_API_KEY not set")
            return None

        import io
        url = "https://api.stability.ai/v2beta/stable-image/generate/sd3"
        headers = {"Authorization": f"Bearer {self._api_key}", "Accept": "image/*"}
        data = {"prompt": prompt, "aspect_ratio": "16:9", "output_format": "png"}

        try:
            resp = requests.post(url, headers=headers, data=data,
                                 files={"none": ""}, timeout=120)
            resp.raise_for_status()
            return Image.open(io.BytesIO(resp.content))
        except requests.RequestException as e:
            print(f"[AIUIGenerator] Generation failed: {e}")
            return None


class UIGenerator:
    """Main UI generator — routes to code or AI based on asset type."""

    def __init__(self, manifest_path: str, api_config_path: Optional[str] = None):
        with open(manifest_path) as f:
            self.manifest = json.load(f)
        self.code_gen = CodeUIGenerator()
        self.ai_gen = AIUIGenerator(api_config_path or "")

    def generate_all(self, output_dir: Path, skip_ai: bool = False) -> dict[str, Path]:
        """Generate all UI assets."""
        output_dir.mkdir(parents=True, exist_ok=True)
        generated = {}

        # HUD elements (code-generated)
        print("[UI] Generating HUD elements...")
        hud_dir = output_dir / "hud"
        hud_dir.mkdir(parents=True, exist_ok=True)

        img = self.code_gen.generate_damage_indicator()
        p = hud_dir / "T_DamageIndicator.png"
        img.save(p, "PNG")
        generated["damage_indicator"] = p

        img = self.code_gen.generate_suppression_vignette()
        p = hud_dir / "T_SuppressionVignette.png"
        img.save(p, "PNG")
        generated["suppression_vignette"] = p

        img = self.code_gen.generate_hit_marker()
        p = hud_dir / "T_HitMarker.png"
        img.save(p, "PNG")
        generated["hit_marker"] = p

        img = self.code_gen.generate_compass_bar()
        p = hud_dir / "T_CompassBar.png"
        img.save(p, "PNG")
        generated["compass_bar"] = p

        img = self.code_gen.generate_body_diagram()
        p = hud_dir / "T_BodyDiagram.png"
        img.save(p, "PNG")
        generated["body_diagram"] = p

        # Stance icons
        for stance in ["standing", "crouching", "prone"]:
            img = self.code_gen.generate_stance_icon(stance)
            p = hud_dir / f"T_Stance_{stance}.png"
            img.save(p, "PNG")
            generated[f"stance_{stance}"] = p

        # Squad command icons
        print("[UI] Generating squad command icons...")
        icons_dir = output_dir / "icons"
        icons_dir.mkdir(parents=True, exist_ok=True)

        command_icons = [
            "move_to", "suppress", "flank_left", "flank_right",
            "hold_position", "regroup", "advance", "cover_me",
            "fall_back", "cease_fire", "medical", "resupply",
        ]
        for name in command_icons:
            img = self.code_gen.generate_squad_icon(name)
            p = icons_dir / f"T_Cmd_{name}.png"
            img.save(p, "PNG")
            generated[f"cmd_{name}"] = p

        # Formation icons
        for formation in ["wedge", "line", "column", "echelon"]:
            img = self.code_gen.generate_formation_icon(formation)
            p = icons_dir / f"T_Formation_{formation}.png"
            img.save(p, "PNG")
            generated[f"formation_{formation}"] = p

        # ROE icons
        for roe in ["hold_fire", "return_fire", "fire_at_will", "weapons_free"]:
            img = self.code_gen.generate_roe_icon(roe)
            p = icons_dir / f"T_ROE_{roe}.png"
            img.save(p, "PNG")
            generated[f"roe_{roe}"] = p

        # Menu art (AI-generated)
        if not skip_ai and self.ai_gen.is_available():
            print("[UI] Generating menu concept art...")
            menu_dir = output_dir / "menu"
            menu_dir.mkdir(parents=True, exist_ok=True)

            menu_prompts = {
                "main_menu_bg": "Cinematic wide shot of Taoyuan Beach Taiwan at dawn, military fortifications, barbed wire, dark clouds, photorealistic, desaturated war photography, 8K",
                "loading_screen_1": "US Marines squad advancing through beach fortifications at sunrise, silhouettes, photorealistic war photography, cinematic lighting",
                "loading_screen_2": "Urban combat in Taoyuan city Taiwan, destroyed buildings, smoke rising, military vehicles, photorealistic, desaturated",
                "loading_screen_3": "Military drone surveillance view of coastal battlefield, infrared overlay, tactical display, photorealistic",
            }
            for name, prompt in menu_prompts.items():
                img = self.ai_gen.generate_concept_art(prompt)
                if img:
                    p = menu_dir / f"T_{name}.png"
                    img.save(p, "PNG")
                    generated[name] = p
        else:
            print("[UI] Skipping AI menu art (no API key or --code-only)")

        print(f"[UI] Generated {len(generated)} UI assets")
        return generated
