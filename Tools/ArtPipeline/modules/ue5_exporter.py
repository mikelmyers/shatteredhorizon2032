"""
UE5 Exporter — Generates Unreal Engine 5 material configs and asset metadata.

Produces:
  - Properly named texture files (T_AssetName_D, T_AssetName_N, etc.)
  - Material parameter JSON configs for batch import
  - UE5 Data Table CSV for terrain material mappings
  - Asset registry entries matching SHTerrainManager's 15 terrain types
"""

import csv
import json
import shutil
from pathlib import Path
from typing import Optional


class UE5Exporter:
    """Exports processed textures into UE5-ready format with material configs."""

    def __init__(self, manifest_path: str):
        with open(manifest_path) as f:
            self.manifest = json.load(f)
        self.naming = self.manifest["ue5_naming_convention"]

    def export_texture_set(self, pbr_maps: dict[str, Path], asset_name: str,
                            output_dir: Path, category: str = "Terrain") -> dict:
        """
        Copy and rename PBR texture set to UE5 naming convention.
        Returns metadata dict for the exported asset.
        """
        export_dir = output_dir / category / asset_name
        export_dir.mkdir(parents=True, exist_ok=True)

        prefix = self.naming["texture_prefix"]
        suffixes = self.naming["suffixes"]
        exported = {}

        for map_type, source_path in pbr_maps.items():
            if map_type in suffixes:
                suffix = suffixes[map_type]
                dest_name = f"{prefix}{asset_name}{suffix}.png"
                dest_path = export_dir / dest_name
                shutil.copy2(source_path, dest_path)
                exported[map_type] = str(dest_path)

        print(f"[UE5Exporter] Exported {len(exported)} maps for {asset_name}")
        return {
            "asset_name": asset_name,
            "category": category,
            "textures": exported,
            "ue5_content_path": f"/Game/Textures/{category}/{asset_name}/",
        }

    def generate_material_config(self, asset_name: str, category: str,
                                  textures: dict, output_path: Path) -> Path:
        """Generate a JSON material configuration for UE5 import."""
        mi_prefix = self.naming["material_instance_prefix"]

        config = {
            "material_instance_name": f"{mi_prefix}{asset_name}",
            "parent_material": f"/Game/Materials/{category}/M_{category}_Master",
            "texture_parameters": {},
            "scalar_parameters": {
                "RoughnessMultiplier": 1.0,
                "NormalStrength": 1.0,
                "HeightScale": 0.05,
                "AOIntensity": 1.0,
                "TilingX": 4.0,
                "TilingY": 4.0,
            },
        }

        param_names = {
            "albedo": "BaseColor",
            "normal": "NormalMap",
            "roughness": "RoughnessMap",
            "ao": "AmbientOcclusion",
            "height": "HeightMap",
        }

        for map_type, tex_path in textures.items():
            if map_type in param_names:
                config["texture_parameters"][param_names[map_type]] = {
                    "texture_path": tex_path,
                    "ue5_param_name": param_names[map_type],
                }

        config_path = output_path / f"{mi_prefix}{asset_name}_config.json"
        with open(config_path, "w") as f:
            json.dump(config, f, indent=2)

        print(f"[UE5Exporter] Material config: {config_path.name}")
        return config_path

    def generate_terrain_data_table(self, terrain_assets: list[dict],
                                     output_path: Path) -> Path:
        """
        Generate a CSV Data Table mapping terrain IDs to material instances.
        This maps directly to SHTerrainManager's terrain type enum.
        """
        csv_path = output_path / "DT_TerrainMaterials.csv"

        with open(csv_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "---", "TerrainID", "MaterialInstance", "FootstepSound",
                "ImpactDecal", "PhysicalMaterial", "ContentPath",
            ])

            for asset in terrain_assets:
                name = asset["asset_name"]
                mi_name = f"{self.naming['material_instance_prefix']}{name}"
                writer.writerow([
                    name,
                    name,
                    mi_name,
                    f"/Game/Audio/Footsteps/SFX_{name}_footstep",
                    f"/Game/VFX/Impacts/MI_{name}_impact",
                    f"/Game/Physics/PM_{name}",
                    asset.get("ue5_content_path", ""),
                ])

        print(f"[UE5Exporter] Terrain data table: {csv_path.name}")
        return csv_path

    def generate_destruction_material_table(self, terrain_id: str,
                                             stage_assets: list[dict],
                                             output_path: Path) -> Path:
        """
        Generate destruction stage material mapping for a terrain type.
        Maps to SHDestructionSystem's 5-stage destruction model.
        """
        csv_path = output_path / f"DT_Destruction_{terrain_id}.csv"

        stages = self.manifest.get("destruction_stages", [])

        with open(csv_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "---", "Stage", "StageName", "MaterialInstance",
                "DebrisParticle", "TransitionDuration",
            ])

            for i, stage in enumerate(stages):
                if i < len(stage_assets):
                    asset = stage_assets[i]
                    mi_name = f"{self.naming['material_instance_prefix']}{terrain_id}_stage{stage['stage']}"
                else:
                    mi_name = f"{self.naming['material_instance_prefix']}{terrain_id}_stage0"

                writer.writerow([
                    f"{terrain_id}_stage{stage['stage']}",
                    stage["stage"],
                    stage["name"],
                    mi_name,
                    f"/Game/VFX/Destruction/NS_{terrain_id}_break_{stage['stage']}",
                    0.3 if stage["stage"] > 0 else 0.0,
                ])

        print(f"[UE5Exporter] Destruction table for {terrain_id}: {csv_path.name}")
        return csv_path

    def generate_batch_import_script(self, all_assets: list[dict],
                                      output_path: Path) -> Path:
        """
        Generate a Python script for UE5 Editor batch import.
        Can be run inside UE5's Python console to auto-import all textures.
        """
        script_path = output_path / "ue5_batch_import.py"

        lines = [
            '"""',
            'UE5 Batch Import Script — Shattered Horizon 2032',
            'Run this inside UE5 Editor Python console to import all generated textures.',
            'Edit > Editor Preferences > Python > Enable Developer Mode',
            '"""',
            '',
            'import unreal',
            '',
            'asset_tools = unreal.AssetToolsHelpers.get_asset_tools()',
            'import_tasks = []',
            '',
        ]

        for asset in all_assets:
            for map_type, tex_path in asset.get("textures", {}).items():
                content_path = asset.get("ue5_content_path", "/Game/Textures/")
                lines.append(f'task = unreal.AssetImportTask()')
                lines.append(f'task.filename = r"{tex_path}"')
                lines.append(f'task.destination_path = "{content_path}"')
                lines.append(f'task.replace_existing = True')
                lines.append(f'task.automated = True')
                lines.append(f'task.save = True')
                lines.append(f'import_tasks.append(task)')
                lines.append('')

        lines.append('asset_tools.import_asset_tasks(import_tasks)')
        lines.append(f'print(f"Imported {{len(import_tasks)}} textures")')

        with open(script_path, "w") as f:
            f.write("\n".join(lines))

        print(f"[UE5Exporter] Batch import script: {script_path.name}")
        return script_path
