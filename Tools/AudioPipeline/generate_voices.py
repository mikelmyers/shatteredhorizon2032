#!/usr/bin/env python3
"""
Shattered Horizon 2032 — Voice Line Generation Pipeline

Usage:
    python generate_voices.py --full                 # All voice lines
    python generate_voices.py --role team_leader     # Single role
    python generate_voices.py --faction friendly      # One faction
    python generate_voices.py --scripts-only          # Just print scripts
"""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from modules.voice_generator import VoiceGenerator


def print_scripts(manifest: dict):
    """Print all dialogue scripts without generating audio."""
    print("\n=== FRIENDLY SQUAD VOICE LINES ===\n")
    squad = manifest["friendly_squad"]
    for context, lines_by_role in squad["contexts"].items():
        print(f"\n--- {context.upper()} ---")
        for role, lines in lines_by_role.items():
            for line in lines:
                print(f"  [{role:20s}] {line}")

    print("\n\n=== ENEMY PLA VOICE LINES (MANDARIN) ===\n")
    enemy = manifest["enemy_pla"]
    for context, ctx_data in enemy["contexts"].items():
        print(f"\n--- {context.upper()} ---")
        print(f"  Primary:     {ctx_data['script_zh']}")
        print(f"  Pinyin:      {ctx_data['pinyin']}")
        print(f"  Translation: {ctx_data['translation']}")
        for v in ctx_data.get("variants", []):
            print(f"  Variant:     {v}")

    # Count
    friendly_count = sum(
        len(lines) * len(squad["roles"]) * 3
        for lines_by_role in squad["contexts"].values()
        for lines in lines_by_role.values()
    ) // len(squad["roles"])
    enemy_count = len(enemy["contexts"]) * len(enemy["speakers"])
    print(f"\n--- TOTALS ---")
    print(f"Friendly lines (with stress variants): ~{friendly_count}")
    print(f"Enemy lines: {enemy_count}")
    print(f"Total: ~{friendly_count + enemy_count}")


def main():
    parser = argparse.ArgumentParser(description="SH2032 Voice Line Pipeline")
    parser.add_argument("--full", action="store_true")
    parser.add_argument("--role", type=str, help="Single role")
    parser.add_argument("--faction", type=str, choices=["friendly", "enemy"])
    parser.add_argument("--scripts-only", action="store_true",
                        help="Print dialogue scripts, no audio generation")
    parser.add_argument("--output", type=str, default=None)
    args = parser.parse_args()

    pipeline_dir = Path(__file__).parent
    manifest_path = pipeline_dir / "config" / "voice_manifest.json"
    api_config_path = pipeline_dir / "config" / "api_config.json"
    output_dir = Path(args.output) if args.output else pipeline_dir / "output" / "voices"

    with open(manifest_path) as f:
        manifest = json.load(f)

    if args.scripts_only:
        print_scripts(manifest)
        return

    print("=" * 60)
    print("SHATTERED HORIZON 2032 — VOICE LINE PIPELINE")
    print("Primordia Games — AI Voice Generation")
    print("=" * 60)

    generator = VoiceGenerator(str(api_config_path))

    if not generator.get_available_provider():
        print("\nNo voice generation API keys found.")
        print("Set one of: ELEVENLABS_API_KEY, AZURE_SPEECH_KEY")
        print("\nUse --scripts-only to view all dialogue scripts.")
        print(f"\nVoice manifest ready at: {manifest_path}")
        print(f"Contains {sum(len(v) for ctx in manifest['friendly_squad']['contexts'].values() for v in ctx.values())} friendly lines")
        print(f"and {len(manifest['enemy_pla']['contexts'])} enemy contexts with variants")
        return

    generated = {}

    if args.full or args.faction in (None, "friendly"):
        print("\n[Friendly] Generating squad voice lines...")
        friendly = generator.generate_all_friendly(manifest, output_dir)
        generated.update(friendly)

    if args.full or args.faction == "enemy":
        print("\n[Enemy] Generating PLA voice lines...")
        enemy = generator.generate_all_enemy(manifest, output_dir)
        generated.update(enemy)

    print(f"\n{'=' * 60}")
    print(f"VOICE PIPELINE COMPLETE — {len(generated)} lines generated")
    print(f"Output: {output_dir}")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
