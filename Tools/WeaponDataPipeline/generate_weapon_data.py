#!/usr/bin/env python3
"""Shattered Horizon 2032 - Weapon Data Pipeline CLI.

Generates UE5 DataAsset JSON files, CSV DataTables, and ballistic range
tables for all 11 in-game weapons.

Usage examples::

    # Full generation for all 11 weapons
    python generate_weapon_data.py --full

    # Single weapon
    python generate_weapon_data.py --weapon M4A1

    # Only the 3 missing weapons (new additions)
    python generate_weapon_data.py --missing-only

    # Range tables only (useful for balancing)
    python generate_weapon_data.py --range-tables

    # Custom output directory
    python generate_weapon_data.py --full --output ./my_output
"""

from __future__ import annotations

import argparse
import json
import logging
import sys
from pathlib import Path

# Ensure the package is importable when running from the pipeline root
_PIPELINE_ROOT = Path(__file__).resolve().parent
if str(_PIPELINE_ROOT) not in sys.path:
    sys.path.insert(0, str(_PIPELINE_ROOT))

from modules.ballistics_calculator import BallisticsCalculator
from modules.weapon_data_generator import WeaponDataGenerator
from modules.ue5_weapon_exporter import UE5WeaponExporter

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
DEFAULT_DATABASE = _PIPELINE_ROOT / "config" / "weapon_database.json"
DEFAULT_OUTPUT = _PIPELINE_ROOT / "output"
MISSING_WEAPONS = ["Lapua338", "M2_50BMG", "AK_762x39"]

logger = logging.getLogger("WeaponDataPipeline")


def setup_logging(verbose: bool = False) -> None:
    """Configure root logger."""
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="[%(levelname)s] %(name)s: %(message)s",
    )


def load_database(path: Path) -> dict:
    """Load and validate the weapon database JSON."""
    if not path.exists():
        logger.error("Database file not found: %s", path)
        sys.exit(1)
    with path.open("r", encoding="utf-8") as fh:
        data = json.load(fh)
    weapon_count = len(data.get("weapons", {}))
    logger.info("Loaded weapon database: %d weapons from %s", weapon_count, path)
    return data


def run_full(
    database: dict,
    output_dir: Path,
    weapon_ids: list[str] | None = None,
) -> None:
    """Run the full pipeline: DataAssets + DataTable + range cards + UE5 script."""
    gen = WeaponDataGenerator(database)
    exporter = UE5WeaponExporter(gen)

    label = ", ".join(weapon_ids) if weapon_ids else "ALL"
    logger.info("Generating full output for weapons: %s", label)

    # 1. DataAsset JSON files
    asset_dir = output_dir / "data_assets"
    paths = exporter.export_data_assets(weapon_ids=weapon_ids, output_dir=asset_dir)
    logger.info("  DataAssets written: %d files -> %s", len(paths), asset_dir)

    # 2. Combined CSV DataTable
    csv_path = output_dir / "WeaponDataTable.csv"
    exporter.generate_weapon_data_table(
        weapon_ids=weapon_ids, output_path=csv_path,
    )
    logger.info("  DataTable CSV -> %s", csv_path)

    # 3. Range cards
    rc_dir = output_dir / "range_cards"
    rc_paths = exporter.generate_range_cards(weapon_ids=weapon_ids, output_dir=rc_dir)
    logger.info("  Range cards written: %d files -> %s", len(rc_paths), rc_dir)

    # 4. UE5 import script
    script_path = output_dir / "import_weapons_to_ue5.py"
    exporter.generate_ue5_import_script(
        weapon_ids=weapon_ids, output_path=script_path,
    )
    logger.info("  UE5 import script -> %s", script_path)


def run_range_tables(
    database: dict,
    output_dir: Path,
    weapon_ids: list[str] | None = None,
) -> None:
    """Generate only range tables."""
    gen = WeaponDataGenerator(database)
    exporter = UE5WeaponExporter(gen)

    rc_dir = output_dir / "range_cards"
    rc_paths = exporter.generate_range_cards(weapon_ids=weapon_ids, output_dir=rc_dir)
    logger.info("Range cards written: %d files -> %s", len(rc_paths), rc_dir)


def build_parser() -> argparse.ArgumentParser:
    """Build the CLI argument parser."""
    parser = argparse.ArgumentParser(
        prog="generate_weapon_data",
        description="Shattered Horizon 2032 - Weapon Data Pipeline",
    )

    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--full",
        action="store_true",
        help="Generate all 11 weapon data assets + range tables.",
    )
    mode.add_argument(
        "--weapon",
        type=str,
        metavar="WEAPON_ID",
        help="Generate data for a single weapon (e.g. M4A1).",
    )
    mode.add_argument(
        "--missing-only",
        action="store_true",
        help="Generate data only for the 3 missing weapons: "
        + ", ".join(MISSING_WEAPONS),
    )
    mode.add_argument(
        "--range-tables",
        action="store_true",
        help="Generate range tables only (useful for balancing).",
    )

    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        metavar="DIR",
        help="Output directory (default: %(default)s).",
    )
    parser.add_argument(
        "--database",
        type=Path,
        default=DEFAULT_DATABASE,
        metavar="PATH",
        help="Path to weapon_database.json (default: %(default)s).",
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
    database = load_database(args.database)
    output_dir: Path = args.output
    output_dir.mkdir(parents=True, exist_ok=True)

    if args.full:
        run_full(database, output_dir)

    elif args.weapon:
        wid = args.weapon
        if wid not in database.get("weapons", {}):
            logger.error("Unknown weapon ID: %s", wid)
            logger.info(
                "Available weapons: %s",
                ", ".join(sorted(database["weapons"].keys())),
            )
            sys.exit(1)
        run_full(database, output_dir, weapon_ids=[wid])

    elif args.missing_only:
        run_full(database, output_dir, weapon_ids=MISSING_WEAPONS)

    elif args.range_tables:
        run_range_tables(database, output_dir)

    logger.info("Done. Output directory: %s", output_dir)


if __name__ == "__main__":
    main()
