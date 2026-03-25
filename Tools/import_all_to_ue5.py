#!/usr/bin/env python3
"""
Shattered Horizon 2032 — Master UE5 Import Script Generator
Primordia Games

Scans all pipeline outputs and generates a single unified UE5 Python
import script. Run this after all pipelines have completed.

Usage:
    python import_all_to_ue5.py
    # Then run the generated script in UE5 Editor Python console
"""

import json
from pathlib import Path


def main():
    tools_dir = Path(__file__).parent

    print("=" * 60)
    print("SHATTERED HORIZON 2032 — MASTER IMPORT GENERATOR")
    print("Scanning all pipeline outputs...")
    print("=" * 60)

    # Collect all UE5 import scripts from pipeline outputs
    import_scripts = []
    data_tables = []
    total_assets = 0

    pipelines = [
        ("ArtPipeline", "Terrain Textures"),
        ("AudioPipeline", "Audio (SFX + Music + Voice)"),
        ("UIPipeline", "UI/HUD Art"),
        ("VFXPipeline", "VFX Niagara Configs"),
        ("ModelPipeline", "3D Models"),
        ("WeaponDataPipeline", "Weapon Data Assets"),
        ("AnimationPipeline", "Animation Configs"),
    ]

    for dirname, label in pipelines:
        pipeline_dir = tools_dir / dirname / "output"
        if not pipeline_dir.exists():
            print(f"  [ ] {label} — no output found")
            continue

        # Count assets
        asset_count = sum(1 for _ in pipeline_dir.rglob("*") if _.is_file())
        total_assets += asset_count

        # Find import scripts
        for script in pipeline_dir.rglob("*import*ue5*.py"):
            import_scripts.append(script)
        for script in pipeline_dir.rglob("*batch_import*.py"):
            import_scripts.append(script)
        for script in pipeline_dir.rglob("*create_niagara*.py"):
            import_scripts.append(script)

        # Find data tables
        for csv_file in pipeline_dir.rglob("*.csv"):
            data_tables.append(csv_file)

        print(f"  [x] {label} — {asset_count} files")

    # Generate master import script
    output_path = tools_dir / "ue5_master_import.py"
    lines = [
        '"""',
        'MASTER UE5 IMPORT SCRIPT — Shattered Horizon 2032',
        'Primordia Games — Import All Generated Assets',
        '',
        'Run this in UE5 Editor: Edit > Editor Preferences > Python',
        '"""',
        '',
        'import unreal',
        'import subprocess',
        'import os',
        '',
        f'# Total assets across all pipelines: {total_assets}',
        f'# Import scripts found: {len(import_scripts)}',
        f'# Data tables found: {len(data_tables)}',
        '',
        'print("=" * 60)',
        'print("SHATTERED HORIZON 2032 — MASTER ASSET IMPORT")',
        'print("=" * 60)',
        '',
    ]

    for i, script in enumerate(import_scripts):
        rel_path = script.relative_to(tools_dir)
        lines.append(f'# Import script {i+1}: {rel_path}')
        lines.append(f'print("Running: {rel_path}")')
        lines.append(f'exec(open(r"{script}").read())')
        lines.append('')

    lines.append(f'print("Import complete. {total_assets} total asset files processed.")')

    with open(output_path, "w") as f:
        f.write("\n".join(lines))

    print(f"\n{'=' * 60}")
    print(f"MASTER IMPORT SCRIPT GENERATED")
    print(f"  Total assets: {total_assets}")
    print(f"  Import scripts: {len(import_scripts)}")
    print(f"  Data tables: {len(data_tables)}")
    print(f"  Output: {output_path}")
    print(f"\nNext: Run {output_path.name} in UE5 Editor Python console")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
