"""
UE5 VFX Exporter — Exports Niagara configs and generates UE5 creation scripts.
"""

import csv
import json
from pathlib import Path


class UE5VFXExporter:
    """Exports VFX configs as JSON and generates UE5 Niagara creation scripts."""

    def export_configs(self, configs: list[dict], output_dir: Path) -> list[Path]:
        """Write all VFX configs as individual JSON files."""
        output_dir.mkdir(parents=True, exist_ok=True)
        paths = []
        for cfg in configs:
            name = cfg["system_name"]
            path = output_dir / f"{name}.json"
            with open(path, "w") as f:
                json.dump(cfg, f, indent=2)
            paths.append(path)
        print(f"[UE5VFXExporter] Exported {len(paths)} VFX config files")
        return paths

    def generate_vfx_data_table(self, configs: list[dict],
                                 output_dir: Path) -> Path:
        """Generate CSV mapping VFX IDs to Niagara system paths."""
        output_dir.mkdir(parents=True, exist_ok=True)
        csv_path = output_dir / "DT_VFXSystems.csv"

        with open(csv_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["---", "SystemName", "Type", "UE5Path"])
            for cfg in configs:
                writer.writerow([
                    cfg["system_name"],
                    cfg["system_name"],
                    cfg.get("type", "unknown"),
                    cfg.get("ue5_path", ""),
                ])

        print(f"[UE5VFXExporter] DataTable: {csv_path.name}")
        return csv_path

    def generate_ue5_creation_script(self, configs: list[dict],
                                      output_dir: Path) -> Path:
        """Generate UE5 Python script that creates Niagara systems from configs."""
        output_dir.mkdir(parents=True, exist_ok=True)
        script_path = output_dir / "create_niagara_systems.py"

        lines = [
            '"""',
            'UE5 Niagara System Creator — Shattered Horizon 2032',
            'Creates Niagara systems from JSON configs.',
            'Run in UE5 Editor Python console.',
            '"""',
            '',
            'import unreal',
            'import json',
            'import os',
            '',
            'config_dir = os.path.dirname(os.path.abspath(__file__))',
            '',
            'def create_niagara_system(config):',
            '    """Create a Niagara system from a config dict."""',
            '    system_name = config["system_name"]',
            '    ue5_path = config.get("ue5_path", f"/Game/VFX/{system_name}")',
            '    ',
            '    # Create the Niagara system asset',
            '    factory = unreal.NiagaraSystemFactoryNew()',
            '    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()',
            '    ',
            '    parent_path = "/".join(ue5_path.rsplit("/", 1)[:-1])',
            '    asset_name = ue5_path.rsplit("/", 1)[-1]',
            '    ',
            '    system = asset_tools.create_asset(',
            '        asset_name=asset_name,',
            '        package_path=parent_path,',
            '        asset_class=unreal.NiagaraSystem,',
            '        factory=factory,',
            '    )',
            '    ',
            '    if system:',
            '        print(f"Created: {ue5_path}")',
            '    else:',
            '        print(f"Failed: {ue5_path}")',
            '    ',
            '    return system',
            '',
            '# Load all configs from this directory',
            'created = 0',
            'for filename in os.listdir(config_dir):',
            '    if filename.startswith("NS_") and filename.endswith(".json"):',
            '        filepath = os.path.join(config_dir, filename)',
            '        with open(filepath) as f:',
            '            config = json.load(f)',
            '        result = create_niagara_system(config)',
            '        if result:',
            '            created += 1',
            '',
            'print(f"Created {created} Niagara systems")',
        ]

        with open(script_path, "w") as f:
            f.write("\n".join(lines))

        print(f"[UE5VFXExporter] Creation script: {script_path.name}")
        return script_path
