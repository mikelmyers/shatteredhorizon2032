"""
UE5 Audio Exporter — Exports processed audio with UE5 naming and configs.
"""

import csv
import json
from pathlib import Path


class UE5AudioExporter:
    """Exports audio assets with UE5-compatible naming and import configs."""

    UE5_AUDIO_PATHS = {
        "weapons": "/Game/Audio/Weapons",
        "projectiles": "/Game/Audio/Projectiles",
        "footsteps": "/Game/Audio/Footsteps",
        "destruction": "/Game/Audio/Destruction",
        "drones": "/Game/Audio/Drones",
        "comms": "/Game/Audio/EW",
        "ui_misc": "/Game/Audio/UI",
        "voice_friendly": "/Game/Audio/Voice/Friendly",
        "voice_enemy": "/Game/Audio/Voice/Enemy",
        "music": "/Game/Audio/Music",
    }

    def __init__(self, manifest_path: str):
        with open(manifest_path) as f:
            self.manifest = json.load(f)

    def export_sfx(self, audio_files: dict[str, Path], output_dir: Path) -> list[dict]:
        """Organize and export SFX files with proper naming."""
        output_dir.mkdir(parents=True, exist_ok=True)
        exported = []

        for sfx_id, source_path in audio_files.items():
            category = self._categorize(sfx_id)
            ue5_dir = self.UE5_AUDIO_PATHS.get(category, "/Game/Audio/Misc")

            dest_dir = output_dir / category
            dest_dir.mkdir(parents=True, exist_ok=True)
            dest_name = f"SFX_{sfx_id}.wav"
            dest_path = dest_dir / dest_name

            import shutil
            shutil.copy2(source_path, dest_path)

            exported.append({
                "id": sfx_id,
                "category": category,
                "filename": dest_name,
                "local_path": str(dest_path),
                "ue5_path": f"{ue5_dir}/{dest_name}",
            })

        print(f"[UE5AudioExporter] Exported {len(exported)} audio files")
        return exported

    def generate_sound_data_table(self, exported: list[dict],
                                   output_dir: Path) -> Path:
        """Generate CSV DataTable mapping sound IDs to file paths."""
        output_dir.mkdir(parents=True, exist_ok=True)
        csv_path = output_dir / "DT_SoundEffects.csv"

        with open(csv_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["---", "SoundID", "Category", "SoundPath", "Loop", "Volume"])
            for item in exported:
                is_loop = "true" if self._is_looping(item["id"]) else "false"
                writer.writerow([
                    item["id"], item["id"], item["category"],
                    item["ue5_path"], is_loop, "1.0",
                ])

        print(f"[UE5AudioExporter] DataTable: {csv_path.name}")
        return csv_path

    def generate_weapon_sound_table(self, weapon_sounds: dict[str, dict[str, Path]],
                                     output_dir: Path) -> Path:
        """Generate weapon-specific sound mapping table."""
        output_dir.mkdir(parents=True, exist_ok=True)
        csv_path = output_dir / "DT_WeaponSounds.csv"

        with open(csv_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "---", "WeaponID", "FireSound", "FireSoundSuppressed",
                "DryFireSound", "ReloadSound", "FireModeSwitchSound",
                "MalfunctionSound",
            ])
            for weapon_id, sounds in weapon_sounds.items():
                ue5_base = f"{self.UE5_AUDIO_PATHS['weapons']}/{weapon_id}"
                writer.writerow([
                    weapon_id, weapon_id,
                    f"{ue5_base}/SFX_{weapon_id}_fire.wav",
                    f"{ue5_base}/SFX_{weapon_id}_fire_suppressed.wav",
                    f"{ue5_base}/SFX_{weapon_id}_dry_fire.wav",
                    f"{ue5_base}/SFX_{weapon_id}_reload.wav",
                    f"{ue5_base}/SFX_{weapon_id}_fire_mode_switch.wav",
                    f"{ue5_base}/SFX_{weapon_id}_malfunction.wav",
                ])

        print(f"[UE5AudioExporter] Weapon sound table: {csv_path.name}")
        return csv_path

    def generate_footstep_table(self, footstep_sounds: dict[str, dict[str, Path]],
                                 output_dir: Path) -> Path:
        """Generate terrain footstep sound mapping table."""
        output_dir.mkdir(parents=True, exist_ok=True)
        csv_path = output_dir / "DT_FootstepSounds.csv"

        with open(csv_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "---", "TerrainType", "WalkSound", "RunSound",
                "CrawlSound", "LandSound",
            ])
            for terrain_id, sounds in footstep_sounds.items():
                ue5_base = f"{self.UE5_AUDIO_PATHS['footsteps']}/{terrain_id}"
                writer.writerow([
                    terrain_id, terrain_id,
                    f"{ue5_base}/SFX_{terrain_id}_walk.wav",
                    f"{ue5_base}/SFX_{terrain_id}_run.wav",
                    f"{ue5_base}/SFX_{terrain_id}_crawl.wav",
                    f"{ue5_base}/SFX_{terrain_id}_land.wav",
                ])

        print(f"[UE5AudioExporter] Footstep table: {csv_path.name}")
        return csv_path

    def generate_batch_import_script(self, exported: list[dict],
                                      output_dir: Path) -> Path:
        """Generate UE5 Python import script for all audio."""
        output_dir.mkdir(parents=True, exist_ok=True)
        script_path = output_dir / "import_audio_to_ue5.py"

        lines = [
            '"""',
            'UE5 Batch Audio Import — Shattered Horizon 2032',
            'Run in UE5 Editor Python console.',
            '"""',
            '',
            'import unreal',
            '',
            'asset_tools = unreal.AssetToolsHelpers.get_asset_tools()',
            'import_tasks = []',
            '',
        ]

        for item in exported:
            lines.append(f'task = unreal.AssetImportTask()')
            lines.append(f'task.filename = r"{item["local_path"]}"')
            parent_path = "/".join(item["ue5_path"].rsplit("/", 1)[:-1])
            lines.append(f'task.destination_path = "{parent_path}"')
            lines.append(f'task.replace_existing = True')
            lines.append(f'task.automated = True')
            lines.append(f'task.save = True')
            lines.append(f'import_tasks.append(task)')
            lines.append('')

        lines.append('asset_tools.import_asset_tasks(import_tasks)')
        lines.append(f'print(f"Imported {{len(import_tasks)}} audio assets")')

        with open(script_path, "w") as f:
            f.write("\n".join(lines))

        print(f"[UE5AudioExporter] Import script: {script_path.name}")
        return script_path

    def _categorize(self, sfx_id: str) -> str:
        """Determine category from SFX ID."""
        if any(sfx_id.startswith(p) for p in ["supersonic", "ricochet", "impact_", "explosion"]):
            return "projectiles"
        if sfx_id.startswith("destroy_"):
            return "destruction"
        if sfx_id.startswith("drone_"):
            return "drones"
        if sfx_id.startswith("radio_") or sfx_id.startswith("comms_"):
            return "comms"
        if sfx_id in ("menu_click", "compass_tick", "squad_command_select",
                       "heartbeat_stress", "tinnitus", "breathing_exertion"):
            return "ui_misc"
        return "misc"

    def _is_looping(self, sfx_id: str) -> bool:
        """Check if a sound should loop."""
        looping = {
            "radio_static", "radio_crackle", "drone_motor_loop",
            "drone_isr_buzz", "drone_close_warning", "drone_jamming_active",
            "heartbeat_stress", "breathing_exertion",
        }
        return sfx_id in looping
