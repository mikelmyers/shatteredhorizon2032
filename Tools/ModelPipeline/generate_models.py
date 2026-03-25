#!/usr/bin/env python3
"""
Shattered Horizon 2032 — 3D Model Generation Pipeline

Usage:
    python generate_models.py --full
    python generate_models.py --category weapons
    python generate_models.py --model M4A1
"""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from modules.model_generator import ModelGenerator


def main():
    parser = argparse.ArgumentParser(description="SH2032 3D Model Pipeline")
    parser.add_argument("--full", action="store_true")
    parser.add_argument("--category", type=str,
                        choices=["characters", "weapons", "drones"])
    parser.add_argument("--model", type=str, help="Single model ID")
    parser.add_argument("--output", type=str, default=None)
    args = parser.parse_args()

    pipeline_dir = Path(__file__).parent
    manifest_path = str(pipeline_dir / "config" / "model_manifest.json")
    output_dir = Path(args.output) if args.output else pipeline_dir / "output"

    generator = ModelGenerator(manifest_path)

    print("=" * 60)
    print("SHATTERED HORIZON 2032 — 3D MODEL PIPELINE")
    print("Primordia Games — AI 3D Model Generation")
    print("=" * 60)

    if not generator.get_available_provider():
        print("\nNo 3D generation API keys found.")
        print("Set one of: MESHY_API_KEY, TRIPO_API_KEY")
        print("\nThe manifest is ready at config/model_manifest.json")
        print("with detailed prompts for all 23 models.")
        return

    generated = generator.generate_from_manifest(output_dir, args.category)

    print(f"\n{'=' * 60}")
    print(f"MODEL PIPELINE COMPLETE — {len(generated)} models")
    print(f"Output: {output_dir}")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
