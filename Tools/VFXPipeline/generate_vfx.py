#!/usr/bin/env python3
"""
Shattered Horizon 2032 — VFX Config Generation Pipeline
100% code-driven — no API keys needed.

Usage:
    python generate_vfx.py --full
    python generate_vfx.py --category muzzle
    python generate_vfx.py --weapon M4A1
"""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from modules.vfx_generator import VFXGenerator
from modules.ue5_vfx_exporter import UE5VFXExporter


def main():
    parser = argparse.ArgumentParser(description="SH2032 VFX Config Pipeline")
    parser.add_argument("--full", action="store_true", help="Generate all VFX configs")
    parser.add_argument("--category", type=str,
                        choices=["muzzle", "shells", "impacts", "destruction",
                                 "footsteps", "drones"])
    parser.add_argument("--weapon", type=str, help="Single weapon VFX")
    parser.add_argument("--output", type=str, default=None)
    args = parser.parse_args()

    pipeline_dir = Path(__file__).parent
    manifest_path = str(pipeline_dir / "config" / "vfx_manifest.json")
    output_dir = Path(args.output) if args.output else pipeline_dir / "output"

    generator = VFXGenerator(manifest_path)
    exporter = UE5VFXExporter()

    print("=" * 60)
    print("SHATTERED HORIZON 2032 — VFX PIPELINE")
    print("Primordia Games — Niagara Config Generation")
    print("=" * 60)

    if args.full or not (args.category or args.weapon):
        configs = generator.generate_all()
    elif args.weapon:
        configs = []
        manifest = generator.manifest
        for cal_id, cal_data in manifest["muzzle_flash"].items():
            if args.weapon in cal_data["weapons"]:
                configs.append(generator.generate_muzzle_flash(args.weapon, cal_id))
                if cal_id in manifest["shell_eject"]:
                    configs.append(generator.generate_shell_eject(args.weapon, cal_id))
    elif args.category:
        configs = []
        if args.category == "muzzle":
            for cal_id, cal_data in generator.manifest["muzzle_flash"].items():
                for wid in cal_data["weapons"]:
                    configs.append(generator.generate_muzzle_flash(wid, cal_id))
        elif args.category == "shells":
            for cal_id in generator.manifest["shell_eject"]:
                cal_weapons = generator.manifest["muzzle_flash"].get(cal_id, {}).get("weapons", [])
                for wid in cal_weapons:
                    configs.append(generator.generate_shell_eject(wid, cal_id))
        elif args.category == "impacts":
            for st in generator.manifest["impact_effects"]:
                configs.append(generator.generate_impact(st))
        elif args.category == "destruction":
            for dt in generator.manifest["destruction_vfx"]:
                configs.append(generator.generate_destruction(dt))
        elif args.category == "footsteps":
            for tt in generator.manifest["footstep_vfx"]:
                configs.append(generator.generate_footstep(tt))
        elif args.category == "drones":
            for dt in generator.manifest["drone_vfx"]:
                configs.append(generator.generate_drone_vfx(dt))
    else:
        configs = generator.generate_all()

    # Export
    config_dir = output_dir / "niagara_configs"
    exporter.export_configs(configs, config_dir)
    exporter.generate_vfx_data_table(configs, output_dir / "DataTables")
    exporter.generate_ue5_creation_script(configs, config_dir)

    print(f"\n{'=' * 60}")
    print(f"VFX PIPELINE COMPLETE — {len(configs)} systems configured")
    print(f"Output: {output_dir}")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
