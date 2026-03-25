"""
UIPipeline — UI/HUD art generation pipeline for Shattered Horizon 2032.

Generates all HUD elements, tactical icons, menu backgrounds, and settings
icons. Uses code-based generation (PIL/Pillow) for clean geometric HUD
elements and AI generation (Stability AI) for concept art backgrounds.
"""

from .ui_generator import UIGenerator, CodeGenerator, StabilityAIProvider
from .ui_processor import apply_hud_color_grade, add_glow, make_transparent_bg, resize_and_export

__all__ = [
    "UIGenerator",
    "CodeGenerator",
    "StabilityAIProvider",
    "apply_hud_color_grade",
    "add_glow",
    "make_transparent_bg",
    "resize_and_export",
]
