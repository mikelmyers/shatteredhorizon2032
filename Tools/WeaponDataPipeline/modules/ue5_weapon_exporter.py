"""Export weapon data to UE5-consumable formats.

Produces per-weapon DataAsset JSON files, a combined CSV DataTable,
range-card CSVs for balancing, and a UE5 Editor Python import script.
"""

from __future__ import annotations

import csv
import io
import json
import logging
from dataclasses import asdict
from pathlib import Path
from typing import Dict, List, Optional

from .ballistics_calculator import BallisticsCalculator, RangeTableEntry
from .weapon_data_generator import WeaponDataGenerator

logger = logging.getLogger(__name__)


class UE5WeaponExporter:
    """Handles all export tasks for the Weapon Data Pipeline."""

    def __init__(self, generator: WeaponDataGenerator) -> None:
        """Initialise with a :class:`WeaponDataGenerator`.

        Args:
            generator: A fully-initialised weapon data generator.
        """
        self._generator = generator
        self._database = generator._database
        self._weapons = generator._weapons

    # ------------------------------------------------------------------
    # Per-weapon DataAsset JSON
    # ------------------------------------------------------------------

    def export_data_assets(
        self,
        weapon_ids: Optional[List[str]] = None,
        output_dir: str | Path = "output/data_assets",
    ) -> List[Path]:
        """Write one JSON DataAsset file per weapon.

        Args:
            weapon_ids: Subset of weapon IDs, or *None* for all.
            output_dir: Directory to write into (created if absent).

        Returns:
            List of paths to the written files.
        """
        out = Path(output_dir)
        out.mkdir(parents=True, exist_ok=True)

        assets = self._generator.generate_all_weapons(weapon_ids)
        paths: List[Path] = []

        for asset in assets:
            wid = asset.get("WeaponID", "unknown")
            filepath = out / f"DA_{wid}.json"
            with filepath.open("w", encoding="utf-8") as fh:
                json.dump(asset, fh, indent=2, ensure_ascii=False)
            paths.append(filepath)
            logger.info("Wrote DataAsset: %s", filepath)

        return paths

    # ------------------------------------------------------------------
    # Combined CSV DataTable
    # ------------------------------------------------------------------

    def generate_weapon_data_table(
        self,
        weapon_ids: Optional[List[str]] = None,
        output_path: Optional[str | Path] = None,
    ) -> str:
        """Generate a CSV suitable for UE5 DataTable import.

        The first column is ``---`` (UE5 row-name convention), followed
        by every field from the ``FSHWeaponData`` struct.

        Args:
            weapon_ids: Subset of weapon IDs, or *None* for all.
            output_path: If provided, writes the CSV to this path.

        Returns:
            The CSV content as a string.
        """
        assets = self._generator.generate_all_weapons(weapon_ids)
        if not assets:
            return ""

        # Build header from the first asset's keys
        fieldnames = ["---"] + [k for k in assets[0].keys()]
        buf = io.StringIO()
        writer = csv.DictWriter(buf, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()

        for asset in assets:
            row: Dict[str, str] = {"---": asset.get("WeaponID", "unknown")}
            for key, value in asset.items():
                if isinstance(value, list):
                    row[key] = "|".join(str(v) for v in value)
                elif isinstance(value, bool):
                    row[key] = "true" if value else "false"
                else:
                    row[key] = str(value)
            writer.writerow(row)

        csv_text = buf.getvalue()

        if output_path is not None:
            p = Path(output_path)
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(csv_text, encoding="utf-8")
            logger.info("Wrote DataTable CSV: %s", p)

        return csv_text

    # ------------------------------------------------------------------
    # Range cards (for balancing / debugging)
    # ------------------------------------------------------------------

    def generate_range_cards(
        self,
        weapon_ids: Optional[List[str]] = None,
        output_dir: str | Path = "output/range_cards",
        max_range: Optional[int] = None,
        step: int = 50,
    ) -> List[Path]:
        """Generate per-weapon CSV range tables.

        Args:
            weapon_ids: Subset of weapon IDs, or *None* for all.
            output_dir: Directory to write into.
            max_range: Override max range (otherwise per-weapon default).
            step: Distance step in metres.

        Returns:
            List of paths to written CSV files.
        """
        out = Path(output_dir)
        out.mkdir(parents=True, exist_ok=True)

        ids = weapon_ids or self._generator.weapon_ids
        paths: List[Path] = []

        for wid in ids:
            wdata = self._weapons.get(wid)
            if wdata is None:
                logger.warning("Weapon '%s' not found, skipping range card.", wid)
                continue

            table = BallisticsCalculator.generate_range_table(
                wdata,
                max_range=max_range,
                step=step,
            )

            filepath = out / f"range_card_{wid}.csv"
            with filepath.open("w", encoding="utf-8", newline="") as fh:
                writer = csv.writer(fh)
                writer.writerow([
                    "distance_m",
                    "velocity_mps",
                    "energy_j",
                    "drop_m",
                    "time_of_flight_s",
                    "remaining_penetration_mm",
                ])
                for entry in table:
                    writer.writerow([
                        entry.distance_m,
                        entry.velocity_mps,
                        entry.energy_j,
                        entry.drop_m,
                        entry.time_of_flight_s,
                        entry.remaining_penetration_mm,
                    ])

            paths.append(filepath)
            logger.info("Wrote range card: %s", filepath)

        return paths

    # ------------------------------------------------------------------
    # UE5 Editor Python import script
    # ------------------------------------------------------------------

    def generate_ue5_import_script(
        self,
        weapon_ids: Optional[List[str]] = None,
        output_path: str | Path = "output/import_weapons_to_ue5.py",
        data_asset_dir: str = "/Game/ShatteredHorizon/Data/Weapons",
    ) -> Path:
        """Generate a Python script for the UE5 Editor scripting plugin.

        The script uses ``unreal.EditorAssetLibrary`` and
        ``unreal.AssetTools`` to create or update DataAsset objects for
        every weapon.

        Args:
            weapon_ids: Subset of weapon IDs, or *None* for all.
            output_path: Where to write the generated script.
            data_asset_dir: UE5 content-browser path for the assets.

        Returns:
            Path to the written script.
        """
        assets = self._generator.generate_all_weapons(weapon_ids)
        p = Path(output_path)
        p.parent.mkdir(parents=True, exist_ok=True)

        lines: List[str] = [
            '"""Auto-generated UE5 Editor script: import Shattered Horizon 2032 weapon data.',
            "",
            "Run via: Edit -> Execute Python Script  (requires Editor Scripting Utilities plugin).",
            '"""',
            "",
            "import unreal",
            "",
            "",
            "WEAPON_DATA = " + json.dumps(assets, indent=2),
            "",
            "",
            "def import_weapons():",
            '    """Create or update weapon DataAsset objects in the Content Browser."""',
            "    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()",
            f'    base_path = "{data_asset_dir}"',
            "",
            "    for weapon in WEAPON_DATA:",
            '        asset_name = f"DA_{weapon[\'WeaponID\']}"',
            '        asset_path = f"{base_path}/{asset_name}"',
            "",
            "        # Check if asset already exists",
            "        existing = unreal.EditorAssetLibrary.does_asset_exist(asset_path)",
            "        if existing:",
            '            asset = unreal.EditorAssetLibrary.load_asset(asset_path)',
            '            unreal.log(f"Updating existing asset: {asset_name}")',
            "        else:",
            "            factory = unreal.DataAssetFactory()",
            "            asset = asset_tools.create_asset(",
            "                asset_name=asset_name,",
            "                package_path=base_path,",
            '                asset_class=unreal.DataAsset,',
            "                factory=factory,",
            "            )",
            '            unreal.log(f"Created new asset: {asset_name}")',
            "",
            "        # Set properties",
            "        for key, value in weapon.items():",
            '            if key == "WeaponID":',
            "                continue",
            '            if key == "FireModes":',
            "                # Fire modes need special enum handling",
            "                continue",
            "            try:",
            "                if isinstance(value, bool):",
            "                    asset.set_editor_property(key, value)",
            "                elif isinstance(value, float):",
            "                    asset.set_editor_property(key, value)",
            "                elif isinstance(value, int):",
            "                    asset.set_editor_property(key, value)",
            "                else:",
            "                    asset.set_editor_property(key, str(value))",
            "            except Exception as e:",
            '                unreal.log_warning(f"Could not set {key}: {e}")',
            "",
            "        unreal.EditorAssetLibrary.save_loaded_asset(asset)",
            "",
            f'    unreal.log("Imported {{len(WEAPON_DATA)}} weapons to {data_asset_dir}")',
            "",
            "",
            'if __name__ == "__main__":',
            "    import_weapons()",
            "",
        ]

        p.write_text("\n".join(lines), encoding="utf-8")
        logger.info("Wrote UE5 import script: %s", p)
        return p
