"""Export animation data to UE5-consumable formats.

Produces montage config JSON, AnimBP state machine configs, blend-space
definitions, and a batch import script for the UE5 Editor.
"""

from __future__ import annotations

import json
import logging
from pathlib import Path
from typing import Any, Dict, List, Optional

logger = logging.getLogger(__name__)


class UE5AnimExporter:
    """Handles all UE5 animation export tasks."""

    def __init__(self, manifest: dict) -> None:
        """Initialise with the animation manifest.

        Args:
            manifest: Parsed contents of ``animation_manifest.json``.
        """
        self._manifest = manifest
        self._weapon_montages: List[dict] = manifest.get("weapon_montages", [])
        self._character_anims: List[dict] = manifest.get("character_animations", [])

    @classmethod
    def from_json(cls, path: str | Path) -> "UE5AnimExporter":
        """Load from a manifest JSON file."""
        p = Path(path)
        with p.open("r", encoding="utf-8") as fh:
            data = json.load(fh)
        return cls(data)

    # ------------------------------------------------------------------
    # Montage configs
    # ------------------------------------------------------------------

    def export_montage_configs(
        self,
        weapon_ids: Optional[List[str]] = None,
        output_dir: str | Path = "output/montages",
    ) -> List[Path]:
        """Write JSON montage definition files.

        Each file contains the full UE5 AnimMontage configuration for a
        single weapon montage, including section names, notify triggers,
        and blend settings.

        Args:
            weapon_ids: Filter to these weapon IDs, or *None* for all.
            output_dir: Output directory.

        Returns:
            List of written file paths.
        """
        out = Path(output_dir)
        out.mkdir(parents=True, exist_ok=True)

        montages = self._weapon_montages
        if weapon_ids is not None:
            wid_set = set(weapon_ids)
            montages = [m for m in montages if m.get("weapon_id") in wid_set]

        paths: List[Path] = []
        for m in montages:
            config = self._build_montage_config(m)
            filepath = out / f"{m['id']}.json"
            with filepath.open("w", encoding="utf-8") as fh:
                json.dump(config, fh, indent=2, ensure_ascii=False)
            paths.append(filepath)

        logger.info("Wrote %d montage configs -> %s", len(paths), out)
        return paths

    @staticmethod
    def _build_montage_config(montage: dict) -> dict:
        """Build a UE5 AnimMontage config dict from a manifest entry."""
        mtype = montage.get("montage_type", "fire")
        duration = montage.get("duration_seconds", 1.0)
        weapon_id = montage.get("weapon_id", "unknown")

        # Determine blend in/out based on montage type
        blend_in = 0.05
        blend_out = 0.15
        if mtype == "reload" or mtype == "reload_empty":
            blend_in = 0.1
            blend_out = 0.2
        elif mtype == "malfunction_clear":
            blend_in = 0.08
            blend_out = 0.2

        # Notify events (animation events triggered at specific times)
        notifies: List[Dict[str, Any]] = []
        if mtype == "fire":
            notifies.append({
                "time": 0.0,
                "notify_name": "AN_MuzzleFlash",
                "description": "Trigger muzzle flash VFX and sound",
            })
            notifies.append({
                "time": 0.02,
                "notify_name": "AN_EjectCasing",
                "description": "Spawn ejected casing",
            })
        elif mtype in ("reload", "reload_empty"):
            notifies.append({
                "time": 0.3,
                "notify_name": "AN_MagOut",
                "description": "Magazine removed from weapon",
            })
            mag_in_time = duration * 0.6
            notifies.append({
                "time": round(mag_in_time, 2),
                "notify_name": "AN_MagIn",
                "description": "New magazine inserted",
            })
            if mtype == "reload_empty":
                notifies.append({
                    "time": round(duration * 0.85, 2),
                    "notify_name": "AN_BoltRelease",
                    "description": "Bolt released to chamber round",
                })
            notifies.append({
                "time": round(duration * 0.95, 2),
                "notify_name": "AN_ReloadComplete",
                "description": "Weapon ready to fire",
            })
        elif mtype == "ads_in":
            notifies.append({
                "time": round(duration * 0.5, 2),
                "notify_name": "AN_ADSHalfway",
                "description": "ADS transition at 50%",
            })
            notifies.append({
                "time": round(duration, 2),
                "notify_name": "AN_ADSComplete",
                "description": "Fully aimed down sights",
            })
        elif mtype == "malfunction_clear":
            notifies.append({
                "time": round(duration * 0.3, 2),
                "notify_name": "AN_MalfunctionTap",
                "description": "Tap magazine base",
            })
            notifies.append({
                "time": round(duration * 0.6, 2),
                "notify_name": "AN_MalfunctionRack",
                "description": "Rack charging handle",
            })
            notifies.append({
                "time": round(duration * 0.95, 2),
                "notify_name": "AN_MalfunctionCleared",
                "description": "Weapon cleared and ready",
            })

        # Sections
        sections = [
            {
                "section_name": "Default",
                "start_time": 0.0,
                "end_time": duration,
            }
        ]

        return {
            "montage_id": montage.get("id", ""),
            "weapon_id": weapon_id,
            "montage_type": mtype,
            "ue5_path": montage.get("ue5_path", ""),
            "duration": duration,
            "is_looping": montage.get("is_looping", False),
            "blend_in_time": blend_in,
            "blend_out_time": blend_out,
            "play_rate": 1.0,
            "sections": sections,
            "notifies": notifies,
            "slot_name": "UpperBody" if mtype != "fire" else "FullBody",
        }

    # ------------------------------------------------------------------
    # AnimBP config
    # ------------------------------------------------------------------

    def generate_anim_blueprint_config(
        self,
        character_type: str = "soldier",
        output_path: Optional[str | Path] = None,
    ) -> dict:
        """Generate an AnimBP state-machine configuration.

        Describes the states and transitions for the character animation
        blueprint in a format that can be used as a reference or
        imported via Editor scripting.

        Args:
            character_type: Character type to generate for.
            output_path: Optional path to write JSON.

        Returns:
            State machine config dict.
        """
        # Filter character anims for the given type
        anims = [
            a for a in self._character_anims
            if a.get("character_type") == character_type
        ]

        # Build state machine
        states: List[Dict[str, Any]] = []

        # Locomotion states
        locomotion_states = {
            "Idle": "Stand_Idle",
            "Walking": "Walk_Forward",
            "Running": "Run_Forward",
            "Sprinting": "Sprint",
            "CrouchIdle": "Crouch_Idle",
            "CrouchWalking": "Crouch_Walk",
            "ProneIdle": "Prone_Idle",
            "ProneCrawling": "Prone_Crawl",
            "Jumping": "Jump_Loop",
        }

        for state_name, anim_name in locomotion_states.items():
            anim_entry = next(
                (a for a in anims if a.get("animation_name") == anim_name),
                None,
            )
            states.append({
                "name": state_name,
                "animation": anim_entry.get("ue5_path", "") if anim_entry else "",
                "is_looping": anim_entry.get("is_looping", True) if anim_entry else True,
                "blend_time": anim_entry.get("blend_time", 0.2) if anim_entry else 0.2,
            })

        # Transitions
        transitions = [
            {"from": "Idle", "to": "Walking", "condition": "Speed > 0.5", "blend_time": 0.2},
            {"from": "Walking", "to": "Idle", "condition": "Speed < 0.3", "blend_time": 0.2},
            {"from": "Walking", "to": "Running", "condition": "Speed > 3.0", "blend_time": 0.15},
            {"from": "Running", "to": "Walking", "condition": "Speed < 2.5", "blend_time": 0.15},
            {"from": "Running", "to": "Sprinting", "condition": "bIsSprinting && Speed > 5.0", "blend_time": 0.2},
            {"from": "Sprinting", "to": "Running", "condition": "!bIsSprinting || Speed < 4.5", "blend_time": 0.2},
            {"from": "Idle", "to": "CrouchIdle", "condition": "bIsCrouching", "blend_time": 0.25},
            {"from": "CrouchIdle", "to": "Idle", "condition": "!bIsCrouching", "blend_time": 0.25},
            {"from": "CrouchIdle", "to": "CrouchWalking", "condition": "Speed > 0.3", "blend_time": 0.2},
            {"from": "CrouchWalking", "to": "CrouchIdle", "condition": "Speed < 0.2", "blend_time": 0.2},
            {"from": "CrouchIdle", "to": "ProneIdle", "condition": "bIsProne", "blend_time": 0.3},
            {"from": "ProneIdle", "to": "CrouchIdle", "condition": "!bIsProne", "blend_time": 0.3},
            {"from": "ProneIdle", "to": "ProneCrawling", "condition": "Speed > 0.1", "blend_time": 0.3},
            {"from": "ProneCrawling", "to": "ProneIdle", "condition": "Speed < 0.05", "blend_time": 0.3},
            {"from": "Idle", "to": "Jumping", "condition": "bIsInAir", "blend_time": 0.1},
            {"from": "Walking", "to": "Jumping", "condition": "bIsInAir", "blend_time": 0.1},
            {"from": "Running", "to": "Jumping", "condition": "bIsInAir", "blend_time": 0.1},
            {"from": "Jumping", "to": "Idle", "condition": "!bIsInAir", "blend_time": 0.15},
        ]

        config = {
            "character_type": character_type,
            "state_machine_name": f"SM_{character_type.capitalize()}_Locomotion",
            "default_state": "Idle",
            "states": states,
            "transitions": transitions,
            "blend_parameters": [
                {"name": "Speed", "type": "float", "range": [0.0, 8.0]},
                {"name": "Direction", "type": "float", "range": [-180.0, 180.0]},
                {"name": "bIsCrouching", "type": "bool"},
                {"name": "bIsProne", "type": "bool"},
                {"name": "bIsSprinting", "type": "bool"},
                {"name": "bIsInAir", "type": "bool"},
            ],
        }

        if output_path is not None:
            p = Path(output_path)
            p.parent.mkdir(parents=True, exist_ok=True)
            with p.open("w", encoding="utf-8") as fh:
                json.dump(config, fh, indent=2, ensure_ascii=False)
            logger.info("Wrote AnimBP config -> %s", p)

        return config

    # ------------------------------------------------------------------
    # Blend space
    # ------------------------------------------------------------------

    def generate_blend_space_config(
        self,
        movement_anims: Optional[List[dict]] = None,
        output_path: Optional[str | Path] = None,
    ) -> dict:
        """Generate a 2D blend-space config (speed x direction).

        The blend space maps character speed (vertical axis) and
        movement direction (horizontal axis) to animation samples.

        Args:
            movement_anims: List of animation entries, or *None* to
                use all locomotion anims from the manifest.
            output_path: Optional path to write JSON.

        Returns:
            Blend space config dict.
        """
        if movement_anims is None:
            locomotion_names = {
                "Walk_Forward", "Walk_Backward", "Walk_Left", "Walk_Right",
                "Run_Forward", "Run_Backward", "Sprint",
                "Stand_Idle",
            }
            movement_anims = [
                a for a in self._character_anims
                if a.get("animation_name") in locomotion_names
            ]

        # Map animations to blend-space sample points (direction, speed)
        direction_speed_map = {
            "Stand_Idle":    (0.0, 0.0),
            "Walk_Forward":  (0.0, 1.5),
            "Walk_Backward": (180.0, 1.5),
            "Walk_Left":     (-90.0, 1.5),
            "Walk_Right":    (90.0, 1.5),
            "Run_Forward":   (0.0, 4.0),
            "Run_Backward":  (180.0, 4.0),
            "Sprint":        (0.0, 6.5),
        }

        samples: List[Dict[str, Any]] = []
        for anim in movement_anims:
            name = anim.get("animation_name", "")
            direction, speed = direction_speed_map.get(name, (0.0, 0.0))
            samples.append({
                "animation": anim.get("ue5_path", ""),
                "animation_name": name,
                "sample_value_x": direction,
                "sample_value_y": speed,
                "rate_scale": 1.0,
            })

        config = {
            "blend_space_name": "BS_Locomotion_2D",
            "ue5_path": "/Game/ShatteredHorizon/Animations/Character/BlendSpaces/BS_Locomotion_2D",
            "axis_x": {
                "name": "Direction",
                "minimum": -180.0,
                "maximum": 180.0,
                "grid_divisions": 4,
            },
            "axis_y": {
                "name": "Speed",
                "minimum": 0.0,
                "maximum": 8.0,
                "grid_divisions": 3,
            },
            "interpolation_time": 0.15,
            "target_weight_interpolation_speed": 5.0,
            "samples": samples,
        }

        if output_path is not None:
            p = Path(output_path)
            p.parent.mkdir(parents=True, exist_ok=True)
            with p.open("w", encoding="utf-8") as fh:
                json.dump(config, fh, indent=2, ensure_ascii=False)
            logger.info("Wrote blend space config -> %s", p)

        return config

    # ------------------------------------------------------------------
    # Batch import script
    # ------------------------------------------------------------------

    def generate_batch_import_script(
        self,
        weapon_ids: Optional[List[str]] = None,
        output_path: str | Path = "output/import_animations_to_ue5.py",
    ) -> Path:
        """Generate a UE5 Editor Python script for batch-importing animations.

        The script creates AnimMontage and AnimSequence assets from the
        manifest data.

        Args:
            weapon_ids: Filter montages to these weapons, or *None*.
            output_path: Where to write the script.

        Returns:
            Path to the written script.
        """
        montages = self._weapon_montages
        if weapon_ids is not None:
            wid_set = set(weapon_ids)
            montages = [m for m in montages if m.get("weapon_id") in wid_set]

        p = Path(output_path)
        p.parent.mkdir(parents=True, exist_ok=True)

        montage_data = json.dumps(montages, indent=2)
        char_data = json.dumps(self._character_anims, indent=2)

        script = f'''"""Auto-generated UE5 Editor script: import Shattered Horizon 2032 animations.

Run via: Edit -> Execute Python Script (requires Editor Scripting Utilities plugin).
"""

import unreal


WEAPON_MONTAGES = {montage_data}

CHARACTER_ANIMATIONS = {char_data}


def create_montage_assets():
    """Create AnimMontage assets for weapon animations."""
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    for montage in WEAPON_MONTAGES:
        ue5_path = montage.get("ue5_path", "")
        if not ue5_path:
            continue

        parts = ue5_path.rsplit("/", 1)
        package_path = parts[0] if len(parts) > 1 else "/Game"
        asset_name = parts[-1]

        if unreal.EditorAssetLibrary.does_asset_exist(ue5_path):
            unreal.log(f"Montage already exists: {{asset_name}}")
            continue

        # Create montage placeholder
        factory = unreal.AnimMontageFactory()
        asset = asset_tools.create_asset(
            asset_name=asset_name,
            package_path=package_path,
            asset_class=unreal.AnimMontage,
            factory=factory,
        )

        if asset:
            unreal.EditorAssetLibrary.save_loaded_asset(asset)
            unreal.log(f"Created montage: {{asset_name}}")
        else:
            unreal.log_warning(f"Failed to create montage: {{asset_name}}")


def create_animation_placeholders():
    """Create placeholder AnimSequence assets for character animations."""
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    for anim in CHARACTER_ANIMATIONS:
        ue5_path = anim.get("ue5_path", "")
        if not ue5_path:
            continue

        if unreal.EditorAssetLibrary.does_asset_exist(ue5_path):
            unreal.log(f"Animation already exists: {{ue5_path}}")
            continue

        parts = ue5_path.rsplit("/", 1)
        package_path = parts[0] if len(parts) > 1 else "/Game"
        asset_name = parts[-1]

        unreal.log(f"Placeholder needed: {{asset_name}} at {{package_path}}")
        # Note: AnimSequence assets require source FBX data.
        # These must be imported via FBX import, not created empty.


def import_all():
    """Run the full import pipeline."""
    unreal.log("=== Shattered Horizon 2032 Animation Import ===")
    create_montage_assets()
    create_animation_placeholders()
    unreal.log("=== Animation import complete ===")


if __name__ == "__main__":
    import_all()
'''

        p.write_text(script, encoding="utf-8")
        logger.info("Wrote UE5 batch import script -> %s", p)
        return p
