#!/usr/bin/env python3
"""
Shattered Horizon 2032 — UI/HUD Art Generation Pipeline

Usage:
    python generate_ui.py --full             # All UI assets
    python generate_ui.py --code-only        # Code-generated only (no API)
    python generate_ui.py --category hud     # Single category
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from modules.ui_generator import UIGenerator


def main():
    parser = argparse.ArgumentParser(description="SH2032 UI Pipeline")
    parser.add_argument("--full", action="store_true", help="Generate all UI assets")
    parser.add_argument("--code-only", action="store_true", help="No AI, code only")
    parser.add_argument("--category", type=str,
                        choices=["hud", "icons", "menu", "settings"])
    parser.add_argument("--output", type=str, default=None)
    args = parser.parse_args()

    pipeline_dir = Path(__file__).parent
    manifest_path = str(pipeline_dir / "config" / "ui_manifest.json")
    output_dir = Path(args.output) if args.output else pipeline_dir / "output"

    generator = UIGenerator(manifest_path)

    print("=" * 60)
    print("SHATTERED HORIZON 2032 — UI PIPELINE")
    print("Primordia Games — HUD & UI Art Generation")
    print("=" * 60)

    skip_ai = args.code_only
    generated = generator.generate_all(output_dir, skip_ai=skip_ai)

    print(f"\n{'=' * 60}")
    print(f"UI PIPELINE COMPLETE — {len(generated)} assets")
    print(f"Output: {output_dir}")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
