#!/usr/bin/env python3
"""Shattered Horizon 2032 - Animation Pipeline CLI.

Generates UE5 animation configs, procedural curves, and Mixamo
integration data for all weapons and characters.

Usage examples::

    # Full generation
    python generate_animations.py --full

    # Single weapon animations
    python generate_animations.py --weapon M4A1

    # Only procedural curves (no Mixamo)
    python generate_animations.py --procedural-only

    # Only recoil/bob/sway curves
    python generate_animations.py --curves-only

    # Custom output directory
    python generate_animations.py --full --output ./my_output
"""

from __future__ import annotations

import argparse
import json
import logging
import sys
from pathlib import Path
from typing import List, Optional

# Ensure the package is importable when running from the pipeline root
_PIPELINE_ROOT = Path(__file__).resolve().parent
if str(_PIPELINE_ROOT) not in sys.path:
    sys.path.insert(0, str(_PIPELINE_ROOT))

from modules.procedural_animator import ProceduralAnimator, AnimationCurve
from modules.mixamo_integrator import get_recommended_animations
from modules.ue5_anim_exporter import UE5AnimExporter

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
DEFAULT_MANIFEST = _PIPELINE_ROOT / "config" / "animation_manifest.json"
DEFAULT_OUTPUT = _PIPELINE_ROOT / "output"

# Weapon database path (sibling pipeline)
WEAPON_DATABASE = (
    _PIPELINE_ROOT.parent / "WeaponDataPipeline" / "config" / "weapon_database.json"
)

logger = logging.getLogger("AnimationPipeline")


def setup_logging(verbose: bool = False) -> None:
    """Configure root logger."""
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="[%(levelname)s] %(name)s: %(message)s",
    )


def load_manifest(path: Path) -> dict:
    """Load and validate the animation manifest."""
    if not path.exists():
        logger.error("Manifest file not found: %s", path)
        sys.exit(1)
    with path.open("r", encoding="utf-8") as fh:
        data = json.load(fh)
    wm = len(data.get("weapon_montages", []))
    ca = len(data.get("character_animations", []))
    logger.info("Loaded manifest: %d weapon montages, %d character anims from %s", wm, ca, path)
    return data


def load_weapon_database() -> Optional[dict]:
    """Attempt to load the weapon database for curve generation."""
    if not WEAPON_DATABASE.exists():
        logger.warning(
            "Weapon database not found at %s. "
            "Procedural curves will use default values.",
            WEAPON_DATABASE,
        )
        return None
    with WEAPON_DATABASE.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def get_weapon_ids_from_manifest(manifest: dict) -> List[str]:
    """Extract unique weapon IDs from the manifest."""
    ids = set()
    for m in manifest.get("weapon_montages", []):
        wid = m.get("weapon_id")
        if wid:
            ids.add(wid)
    return sorted(ids)


# ---------------------------------------------------------------------------
# Generation functions
# ---------------------------------------------------------------------------


def generate_procedural_curves(
    output_dir: Path,
    weapon_db: Optional[dict],
    weapon_ids: Optional[List[str]] = None,
) -> None:
    """Generate all procedural animation curves."""
    curves_dir = output_dir / "curves"
    curves_dir.mkdir(parents=True, exist_ok=True)

    animator = ProceduralAnimator()
    weapons = weapon_db.get("weapons", {}) if weapon_db else {}

    # --- Recoil curves ---
    recoil_dir = curves_dir / "recoil"
    recoil_dir.mkdir(parents=True, exist_ok=True)

    target_weapons = weapon_ids or list(weapons.keys())
    count = 0
    for wid in target_weapons:
        wdata = weapons.get(wid)
        if wdata is None:
            logger.warning("No weapon data for '%s', skipping recoil curve.", wid)
            continue
        wdata_copy = dict(wdata)
        wdata_copy["_id"] = wid
        curve = animator.generate_recoil_curve(wdata_copy)
        animator.export_curve_as_json(
            curve, output_path=recoil_dir / f"{curve.name}.json"
        )
        count += 1
    logger.info("Generated %d recoil curves -> %s", count, recoil_dir)

    # --- Weapon bob curves (for different movement speeds) ---
    bob_dir = curves_dir / "weapon_bob"
    bob_dir.mkdir(parents=True, exist_ok=True)

    speed_presets = [
        ("walk", 1.5, 3.5),
        ("run", 4.0, 3.5),
        ("sprint", 6.5, 3.5),
        ("walk_heavy", 1.5, 7.5),
        ("run_heavy", 4.0, 7.5),
    ]
    for label, speed, weight in speed_presets:
        curve = animator.generate_weapon_bob_curve(
            movement_speed=speed, weapon_weight=weight
        )
        animator.export_curve_as_json(
            curve, name=f"WB_{label}", output_path=bob_dir / f"WB_{label}.json"
        )
    logger.info("Generated %d weapon bob curves -> %s", len(speed_presets), bob_dir)

    # --- Weapon sway curves (hip vs ADS) ---
    sway_dir = curves_dir / "weapon_sway"
    sway_dir.mkdir(parents=True, exist_ok=True)

    sway_presets = [
        ("hip_relaxed", 0.3, 0.25),
        ("hip_tense", 0.5, 0.3),
        ("ads_steady", 0.8, 0.2),
        ("ads_sniper", 0.6, 0.2),
        ("ads_held_breath", 0.95, 0.1),
    ]
    for label, stability, breathing in sway_presets:
        curve = animator.generate_weapon_sway_curve(
            ads_stability=stability, breathing_rate=breathing
        )
        animator.export_curve_as_json(
            curve, name=f"WS_{label}", output_path=sway_dir / f"WS_{label}.json"
        )
    logger.info("Generated %d weapon sway curves -> %s", len(sway_presets), sway_dir)

    # --- ADS transition curves ---
    ads_dir = curves_dir / "ads_transition"
    ads_dir.mkdir(parents=True, exist_ok=True)

    ads_times = set()
    for wid in target_weapons:
        wdata = weapons.get(wid)
        if wdata:
            ads_times.add(wdata.get("ads_time_s", 0.25))
    # Also add some standard presets
    ads_times.update({0.15, 0.25, 0.35, 0.50})

    for ads_time in sorted(ads_times):
        if ads_time <= 0.0:
            continue
        curve = animator.generate_ads_transition_curve(ads_time=ads_time)
        animator.export_curve_as_json(
            curve, output_path=ads_dir / f"{curve.name}.json"
        )
    logger.info("Generated %d ADS transition curves -> %s", len(ads_times), ads_dir)


def generate_montages_and_configs(
    manifest: dict,
    output_dir: Path,
    weapon_ids: Optional[List[str]] = None,
) -> None:
    """Generate montage configs, AnimBP config, and blend space."""
    exporter = UE5AnimExporter(manifest)

    # Montage configs
    exporter.export_montage_configs(
        weapon_ids=weapon_ids,
        output_dir=output_dir / "montages",
    )

    # AnimBP state machine
    exporter.generate_anim_blueprint_config(
        character_type="soldier",
        output_path=output_dir / "anim_blueprint_soldier.json",
    )

    # Blend space
    exporter.generate_blend_space_config(
        output_path=output_dir / "blend_space_locomotion.json",
    )

    # Batch import script
    exporter.generate_batch_import_script(
        weapon_ids=weapon_ids,
        output_path=output_dir / "import_animations_to_ue5.py",
    )


def generate_mixamo_reference(output_dir: Path) -> None:
    """Write the recommended Mixamo animation mapping."""
    mapping = get_recommended_animations()
    filepath = output_dir / "mixamo_recommendations.json"
    filepath.parent.mkdir(parents=True, exist_ok=True)
    with filepath.open("w", encoding="utf-8") as fh:
        json.dump(mapping, fh, indent=2, ensure_ascii=False)
    logger.info("Wrote Mixamo recommendations -> %s", filepath)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    """Build the CLI argument parser."""
    parser = argparse.ArgumentParser(
        prog="generate_animations",
        description="Shattered Horizon 2032 - Animation Pipeline",
    )

    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--full",
        action="store_true",
        help="Generate all animation configs, curves, and references.",
    )
    mode.add_argument(
        "--weapon",
        type=str,
        metavar="WEAPON_ID",
        help="Generate animations for a single weapon (e.g. M4A1).",
    )
    mode.add_argument(
        "--procedural-only",
        action="store_true",
        help="Only generate procedural curves (no Mixamo).",
    )
    mode.add_argument(
        "--curves-only",
        action="store_true",
        help="Only generate recoil/bob/sway curves.",
    )

    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        metavar="DIR",
        help="Output directory (default: %(default)s).",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=DEFAULT_MANIFEST,
        metavar="PATH",
        help="Path to animation_manifest.json (default: %(default)s).",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable debug logging.",
    )

    return parser


def main() -> None:
    """CLI entry point."""
    parser = build_parser()
    args = parser.parse_args()

    setup_logging(args.verbose)
    manifest = load_manifest(args.manifest)
    weapon_db = load_weapon_database()
    output_dir: Path = args.output
    output_dir.mkdir(parents=True, exist_ok=True)

    if args.full:
        generate_procedural_curves(output_dir, weapon_db)
        generate_montages_and_configs(manifest, output_dir)
        generate_mixamo_reference(output_dir)

    elif args.weapon:
        wid = args.weapon
        all_ids = get_weapon_ids_from_manifest(manifest)
        if wid not in all_ids:
            logger.error("Unknown weapon ID: %s", wid)
            logger.info("Available weapons: %s", ", ".join(all_ids))
            sys.exit(1)
        generate_procedural_curves(output_dir, weapon_db, weapon_ids=[wid])
        generate_montages_and_configs(manifest, output_dir, weapon_ids=[wid])

    elif args.procedural_only:
        generate_procedural_curves(output_dir, weapon_db)

    elif args.curves_only:
        generate_procedural_curves(output_dir, weapon_db)

    logger.info("Done. Output directory: %s", output_dir)


if __name__ == "__main__":
    main()
