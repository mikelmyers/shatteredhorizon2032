"""Local tool handlers that generate configuration files rather than assets.

These tools produce JSON descriptors or templates that are consumed by
Unreal Engine 5 or other local tooling.  They never call external APIs
and are always free.
"""

from __future__ import annotations

import json
import logging
from pathlib import Path
from typing import TYPE_CHECKING

from pipeline.orchestrator.base_tool import ToolHandler

if TYPE_CHECKING:
    from pipeline.inventory.models import AssetRequest

logger = logging.getLogger(__name__)


class MetaHumanHandler(ToolHandler):
    """Generate a MetaHuman character descriptor JSON.

    This descriptor can be imported into Unreal Engine's MetaHuman Creator
    as a starting point for character creation.
    """

    tool_id: str = "metahuman"
    tool_name: str = "MetaHuman Creator"

    def is_available(self) -> bool:
        return True  # Local config generation — always available.

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        return 0.0

    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        descriptor = {
            "type": "metahuman_descriptor",
            "version": "1.0",
            "prompt": request.prompt,
            "character": {
                "description": request.prompt,
                "body_type": "average",
                "gender": "neutral",
                "age_range": "adult",
                "style_refs": request.style_refs,
            },
            "export_settings": {
                "target_engine": "UE5",
                "lod_levels": 4,
                "include_animations": True,
                "include_clothing": True,
            },
            "notes": (
                "Import this descriptor into MetaHuman Creator to begin "
                "character customisation. Refine the facial features and "
                "body proportions to match the art direction."
            ),
        }

        out_path = Path(output_dir) / f"{request.id}_metahuman.json"
        out_path.write_text(json.dumps(descriptor, indent=2))

        logger.info("Generated MetaHuman descriptor: %s", out_path)
        return {
            "success": True,
            "file_path": str(out_path),
            "cost": 0.0,
            "metadata": {"tool": self.tool_id, "descriptor_type": "metahuman"},
            "error": "",
        }


class UE5PCGHandler(ToolHandler):
    """Generate an Unreal Engine 5 Procedural Content Generation config.

    Produces a JSON configuration that can be loaded into a UE5 PCG graph
    to procedurally populate an environment.
    """

    tool_id: str = "ue5_pcg"
    tool_name: str = "UE5 PCG (Procedural Content Generation)"

    def is_available(self) -> bool:
        return True

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        return 0.0

    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        config = {
            "type": "ue5_pcg_config",
            "version": "1.0",
            "prompt": request.prompt,
            "environment": {
                "description": request.prompt,
                "biome": "auto",
                "size_meters": {"x": 500, "y": 500},
                "style_refs": request.style_refs,
            },
            "pcg_settings": {
                "seed": 42,
                "density": "medium",
                "scatter_meshes": True,
                "use_landscape_splines": True,
                "foliage_enabled": True,
                "water_bodies": "auto_detect",
            },
            "layers": [
                {"name": "terrain", "enabled": True},
                {"name": "foliage", "enabled": True},
                {"name": "props", "enabled": True},
                {"name": "lighting", "enabled": True},
            ],
            "notes": (
                "Load this config into a UE5 PCG graph. Adjust density "
                "and seed values to iterate on the environment layout."
            ),
        }

        out_path = Path(output_dir) / f"{request.id}_pcg.json"
        out_path.write_text(json.dumps(config, indent=2))

        logger.info("Generated UE5 PCG config: %s", out_path)
        return {
            "success": True,
            "file_path": str(out_path),
            "cost": 0.0,
            "metadata": {"tool": self.tool_id, "config_type": "pcg"},
            "error": "",
        }


class NiagaraHandler(ToolHandler):
    """Generate an Unreal Engine 5 Niagara VFX template config.

    Produces a JSON template describing a particle system that can be
    imported into the Niagara visual effects editor.
    """

    tool_id: str = "niagara"
    tool_name: str = "UE5 Niagara VFX"

    def is_available(self) -> bool:
        return True

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        return 0.0

    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        template = {
            "type": "niagara_template",
            "version": "1.0",
            "prompt": request.prompt,
            "effect": {
                "description": request.prompt,
                "category": "auto",
                "style_refs": request.style_refs,
            },
            "emitter_settings": {
                "spawn_rate": 100,
                "lifetime": {"min": 0.5, "max": 2.0},
                "initial_velocity": {"min": 50, "max": 200},
                "size": {"min": 1.0, "max": 5.0},
                "color_mode": "gradient",
                "simulation_space": "world",
            },
            "modules": [
                {"name": "SpawnRate", "enabled": True},
                {"name": "InitialSize", "enabled": True},
                {"name": "VelocityFromPoint", "enabled": True},
                {"name": "GravityForce", "enabled": True},
                {"name": "ColorOverLife", "enabled": True},
                {"name": "SizeOverLife", "enabled": True},
                {"name": "SpriteRenderer", "enabled": True},
            ],
            "notes": (
                "Import this template into Niagara to create the base "
                "particle system. Tune emitter parameters and add custom "
                "modules to match the desired visual effect."
            ),
        }

        out_path = Path(output_dir) / f"{request.id}_niagara.json"
        out_path.write_text(json.dumps(template, indent=2))

        logger.info("Generated Niagara VFX template: %s", out_path)
        return {
            "success": True,
            "file_path": str(out_path),
            "cost": 0.0,
            "metadata": {"tool": self.tool_id, "template_type": "niagara"},
            "error": "",
        }
