#!/usr/bin/env python3
"""
Shattered Horizon 2032 — SFX Generation Pipeline
Primordia Games — Automated Sound Effect Generation

Usage:
    python generate_sfx.py --full                    # Full pipeline
    python generate_sfx.py --synth-only              # Code synthesis only (no API)
    python generate_sfx.py --category weapons         # Single category
    python generate_sfx.py --weapon M4A1             # Single weapon sounds
"""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from modules.ai_sfx_generator import AISFXGenerator
from modules.audio_processor import AudioProcessor
from modules.ue5_audio_exporter import UE5AudioExporter


def main():
    parser = argparse.ArgumentParser(description="SH2032 SFX Pipeline")
    parser.add_argument("--full", action="store_true", help="Run full pipeline")
    parser.add_argument("--category", type=str,
                        choices=["weapons", "projectiles", "footsteps",
                                 "destruction", "drones", "comms", "ui"],
                        help="Single category")
    parser.add_argument("--weapon", type=str, help="Single weapon ID")
    parser.add_argument("--synth-only", action="store_true",
                        help="Code synthesis only, no API keys needed")
    parser.add_argument("--skip-sourcing", action="store_true")
    parser.add_argument("--output", type=str, default=None)
    args = parser.parse_args()

    pipeline_dir = Path(__file__).parent
    config_dir = pipeline_dir / "config"
    output_dir = Path(args.output) if args.output else pipeline_dir / "output"
    output_dir.mkdir(parents=True, exist_ok=True)

    manifest_path = config_dir / "sfx_manifest.json"
    api_config_path = config_dir / "api_config.json"

    with open(manifest_path) as f:
        manifest = json.load(f)

    generator = AISFXGenerator(str(api_config_path))
    processor = AudioProcessor()
    exporter = UE5AudioExporter(str(manifest_path))

    print("=" * 60)
    print("SHATTERED HORIZON 2032 — SFX PIPELINE")
    print("Primordia Games — Sound Effect Generation")
    print("=" * 60)

    all_generated: dict[str, Path] = {}
    weapon_sounds: dict[str, dict[str, Path]] = {}
    footstep_sounds: dict[str, dict[str, Path]] = {}

    categories_to_run = []
    if args.full or args.synth_only:
        categories_to_run = ["weapons", "projectiles", "footsteps",
                             "destruction", "drones", "comms", "ui"]
    elif args.category:
        categories_to_run = [args.category]
    elif args.weapon:
        categories_to_run = ["weapons"]
    else:
        parser.print_help()
        return

    # --- WEAPONS ---
    if "weapons" in categories_to_run:
        print("\n[Weapons] Generating weapon audio...")
        weapons_cfg = manifest["weapons"]
        target_weapons = None
        if args.weapon:
            target_weapons = [args.weapon]

        for caliber_id, caliber_data in weapons_cfg["calibers"].items():
            for weapon_id in caliber_data["weapons"]:
                if target_weapons and weapon_id not in target_weapons:
                    continue

                weapon_sounds[weapon_id] = {}
                for stype in weapons_cfg["sound_types"]:
                    prompt = stype["description_template"].format(
                        caliber_sound=caliber_data["sound_character"],
                        weapon_name=weapon_id.replace("_", " "),
                    )
                    sfx_id = f"{weapon_id}_{stype['type']}"
                    duration = stype["duration"]

                    audio_bytes = generator.generate(
                        sfx_id, prompt, duration, synth_only=args.synth_only
                    )
                    if audio_bytes:
                        out_path = output_dir / "sfx" / "weapons" / weapon_id / f"SFX_{sfx_id}.wav"
                        processed = processor.process_sfx(audio_bytes, out_path)
                        all_generated[sfx_id] = processed
                        weapon_sounds[weapon_id][stype["type"]] = processed

    # --- PROJECTILES ---
    if "projectiles" in categories_to_run:
        print("\n[Projectiles] Generating projectile audio...")
        for item in manifest["projectiles"]:
            audio_bytes = generator.generate(
                item["id"], item["description"], item["duration"],
                synth_only=args.synth_only,
            )
            if audio_bytes:
                out_path = output_dir / "sfx" / "projectiles" / f"SFX_{item['id']}.wav"
                processed = processor.process_sfx(audio_bytes, out_path)
                all_generated[item["id"]] = processed

    # --- FOOTSTEPS ---
    if "footsteps" in categories_to_run:
        print("\n[Footsteps] Generating footstep audio...")
        for terrain in manifest["footsteps"]["terrain_types"]:
            footstep_sounds[terrain["id"]] = {}
            for move in manifest["footsteps"]["movement_types"]:
                sfx_id = f"{terrain['id']}_{move['type']}"
                prompt = f"{terrain['description']}, {move['modifier']}"
                audio_bytes = generator.generate(
                    sfx_id, prompt, move["duration"],
                    synth_only=args.synth_only,
                )
                if audio_bytes:
                    out_path = output_dir / "sfx" / "footsteps" / terrain["id"] / f"SFX_{sfx_id}.wav"
                    processed = processor.process_sfx(audio_bytes, out_path)
                    all_generated[sfx_id] = processed
                    footstep_sounds[terrain["id"]][move["type"]] = processed

    # --- DESTRUCTION ---
    if "destruction" in categories_to_run:
        print("\n[Destruction] Generating destruction audio...")
        for item in manifest["destruction"]:
            audio_bytes = generator.generate(
                item["id"], item["description"], item["duration"],
                synth_only=args.synth_only,
            )
            if audio_bytes:
                out_path = output_dir / "sfx" / "destruction" / f"SFX_{item['id']}.wav"
                processed = processor.process_sfx(audio_bytes, out_path)
                all_generated[item["id"]] = processed

    # --- DRONES ---
    if "drones" in categories_to_run:
        print("\n[Drones] Generating drone audio...")
        for item in manifest["drones"]:
            audio_bytes = generator.generate(
                item["id"], item["description"], item["duration"],
                synth_only=args.synth_only,
            )
            if audio_bytes:
                is_loop = item.get("loop", False)
                out_path = output_dir / "sfx" / "drones" / f"SFX_{item['id']}.wav"
                processed = processor.process_sfx(audio_bytes, out_path, loop=is_loop)
                all_generated[item["id"]] = processed

    # --- COMMS ---
    if "comms" in categories_to_run:
        print("\n[Comms] Generating comms audio...")
        for item in manifest["comms"]:
            audio_bytes = generator.generate(
                item["id"], item["description"], item["duration"],
                synth_only=args.synth_only,
            )
            if audio_bytes:
                is_loop = item.get("loop", False)
                out_path = output_dir / "sfx" / "comms" / f"SFX_{item['id']}.wav"
                processed = processor.process_sfx(
                    audio_bytes, out_path, loop=is_loop, radio_filter=True
                )
                all_generated[item["id"]] = processed

    # --- UI/MISC ---
    if "ui" in categories_to_run:
        print("\n[UI] Generating UI audio...")
        for item in manifest["ui_misc"]:
            audio_bytes = generator.generate(
                item["id"], item["description"], item["duration"],
                synth_only=True,  # UI sounds always use synth
            )
            if audio_bytes:
                is_loop = item.get("loop", False)
                out_path = output_dir / "sfx" / "ui" / f"SFX_{item['id']}.wav"
                processed = processor.process_sfx(audio_bytes, out_path, loop=is_loop)
                all_generated[item["id"]] = processed

    # --- EXPORT ---
    print("\n[Export] Generating UE5 export artifacts...")
    export_dir = output_dir / "ue5_export"

    exported = exporter.export_sfx(all_generated, export_dir / "Audio")
    exporter.generate_sound_data_table(exported, export_dir / "DataTables")
    if weapon_sounds:
        exporter.generate_weapon_sound_table(weapon_sounds, export_dir / "DataTables")
    if footstep_sounds:
        exporter.generate_footstep_table(footstep_sounds, export_dir / "DataTables")
    exporter.generate_batch_import_script(exported, export_dir)

    print(f"\n{'=' * 60}")
    print(f"SFX PIPELINE COMPLETE")
    print(f"  Total sounds generated: {len(all_generated)}")
    print(f"  Output: {output_dir}")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
