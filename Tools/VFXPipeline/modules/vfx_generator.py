"""
VFX Generator — Pure code Niagara system config generation.
No AI APIs needed. Generates parameter configs for UE5 Niagara systems.
"""

import json
from pathlib import Path


class VFXGenerator:
    """Generates Niagara VFX system configurations from manifest data."""

    def __init__(self, manifest_path: str):
        with open(manifest_path) as f:
            self.manifest = json.load(f)

    def generate_muzzle_flash(self, weapon_id: str, caliber_id: str) -> dict:
        """Generate muzzle flash Niagara config for a weapon."""
        params = self.manifest["muzzle_flash"][caliber_id]
        return {
            "system_name": f"NS_MuzzleFlash_{weapon_id}",
            "ue5_path": f"/Game/VFX/Weapons/{weapon_id}/NS_MuzzleFlash_{weapon_id}",
            "type": "muzzle_flash",
            "emitters": [
                {
                    "name": "Flash",
                    "spawn_rate": 0,
                    "spawn_burst": 1,
                    "lifetime": params["flash_duration"],
                    "initial_size": params["flash_size"],
                    "size_over_life": {"start": params["flash_size"], "end": params["flash_size"] * 2},
                    "color": params["flash_color"],
                    "color_over_life": {"start": params["flash_color"], "end": [1, 1, 1, 0]},
                    "material": "/Game/VFX/Materials/M_MuzzleFlash",
                    "render_type": "sprite",
                    "light_intensity": params["flash_size"] * 50,
                    "light_color": params["flash_color"][:3],
                },
                {
                    "name": "Smoke",
                    "spawn_rate": 0,
                    "spawn_burst": int(params["smoke_amount"] * 10),
                    "lifetime": 0.5,
                    "initial_size": params["flash_size"] * 0.5,
                    "size_over_life": {"start": params["flash_size"] * 0.5, "end": params["flash_size"] * 3},
                    "color": [0.3, 0.3, 0.3, params["smoke_amount"]],
                    "color_over_life": {"start": [0.3, 0.3, 0.3, params["smoke_amount"]], "end": [0.5, 0.5, 0.5, 0]},
                    "velocity": {"min": [0, 0, 0.2], "max": [0.1, 0.1, 0.5]},
                    "drag": 2.0,
                    "material": "/Game/VFX/Materials/M_Smoke",
                    "render_type": "sprite",
                },
                {
                    "name": "Sparks",
                    "spawn_rate": 0,
                    "spawn_burst": params["spark_count"],
                    "lifetime": 0.15,
                    "initial_size": 0.005,
                    "color": [1.0, 0.8, 0.3, 1.0],
                    "velocity": {"min": [-3, -3, -1], "max": [3, 3, 3]},
                    "gravity": -9.81,
                    "drag": 1.0,
                    "material": "/Game/VFX/Materials/M_Spark",
                    "render_type": "ribbon",
                },
            ],
        }

    def generate_shell_eject(self, weapon_id: str, caliber_id: str) -> dict:
        """Generate shell casing ejection config."""
        params = self.manifest["shell_eject"].get(caliber_id, self.manifest["shell_eject"]["556x45"])
        return {
            "system_name": f"NS_ShellEject_{weapon_id}",
            "ue5_path": f"/Game/VFX/Weapons/{weapon_id}/NS_ShellEject_{weapon_id}",
            "type": "shell_eject",
            "emitters": [
                {
                    "name": "Shell",
                    "spawn_rate": 0,
                    "spawn_burst": 1,
                    "lifetime": 2.0,
                    "mesh_scale": params["shell_scale"],
                    "velocity": {
                        "direction": [1, 0, 0.3],
                        "speed": params["eject_velocity"],
                        "angle_variance": params["eject_angle"],
                    },
                    "rotation_rate": params["spin_rate"],
                    "gravity": -9.81,
                    "bounce": 0.3,
                    "material": f"/Game/VFX/Materials/M_Shell_{params['material']}",
                    "mesh": "/Game/Meshes/VFX/SM_ShellCasing",
                    "render_type": "mesh",
                    "collision": True,
                },
            ],
        }

    def generate_impact(self, surface_type: str) -> dict:
        """Generate bullet impact VFX config for a surface type."""
        params = self.manifest["impact_effects"][surface_type]
        emitters = [
            {
                "name": "ImpactBurst",
                "spawn_rate": 0,
                "spawn_burst": params["particle_count"],
                "lifetime": 0.3,
                "initial_size": params["particle_size"],
                "color": params["particle_color"],
                "velocity": {"min": [-2, -2, 0], "max": [2, 2, 3]},
                "gravity": -9.81,
                "drag": 3.0,
                "material": f"/Game/VFX/Materials/M_Debris_{params['debris_type']}",
                "render_type": "sprite",
            },
        ]

        if params["dust_amount"] > 0:
            emitters.append({
                "name": "DustCloud",
                "spawn_rate": 0,
                "spawn_burst": int(params["dust_amount"] * 10),
                "lifetime": 1.0,
                "initial_size": 0.05,
                "size_over_life": {"start": 0.05, "end": 0.3},
                "color": [0.6, 0.55, 0.45, params["dust_amount"] * 0.5],
                "color_over_life": {"start": [0.6, 0.55, 0.45, params["dust_amount"] * 0.5], "end": [0.7, 0.65, 0.55, 0]},
                "velocity": {"min": [-0.5, -0.5, 0], "max": [0.5, 0.5, 1]},
                "drag": 5.0,
                "material": "/Game/VFX/Materials/M_Dust",
                "render_type": "sprite",
            })

        if params["spark_count"] > 0:
            emitters.append({
                "name": "Sparks",
                "spawn_rate": 0,
                "spawn_burst": params["spark_count"],
                "lifetime": 0.2,
                "initial_size": 0.003,
                "color": [1.0, 0.9, 0.5, 1.0],
                "velocity": {"min": [-5, -5, 0], "max": [5, 5, 5]},
                "gravity": -9.81,
                "material": "/Game/VFX/Materials/M_Spark",
                "render_type": "ribbon",
            })

        return {
            "system_name": f"NS_Impact_{surface_type}",
            "ue5_path": f"/Game/VFX/Impacts/NS_Impact_{surface_type}",
            "type": "impact",
            "surface_type": surface_type,
            "decal_size": params["decal_size"],
            "decal_material": f"/Game/VFX/Decals/MI_BulletHole_{surface_type}",
            "emitters": emitters,
        }

    def generate_destruction(self, destructible_type: str) -> dict:
        """Generate destruction VFX config."""
        params = self.manifest["destruction_vfx"][destructible_type]
        return {
            "system_name": f"NS_Destruction_{destructible_type}",
            "ue5_path": f"/Game/VFX/Destruction/NS_Destruction_{destructible_type}",
            "type": "destruction",
            "emitters": [
                {
                    "name": "DebrisChunks",
                    "spawn_rate": 0,
                    "spawn_burst": params["debris_count"],
                    "lifetime": 3.0,
                    "initial_size_range": [0.05, 0.3],
                    "velocity": {"min": [-1, -1, 0], "max": [1, 1, 1], "speed": params["fragment_velocity"]},
                    "gravity": -9.81,
                    "bounce": 0.2,
                    "rotation_rate": 5.0,
                    "collision": True,
                    "material": f"/Game/VFX/Materials/M_Debris_{destructible_type}",
                    "render_type": "mesh",
                },
                {
                    "name": "DustCloud",
                    "spawn_rate": 0,
                    "spawn_burst": 20,
                    "lifetime": params["smoke_duration"],
                    "initial_size": params["dust_cloud_size"] * 0.3,
                    "size_over_life": {"start": params["dust_cloud_size"] * 0.3, "end": params["dust_cloud_size"]},
                    "color": [0.6, 0.55, 0.45, 0.6],
                    "color_over_life": {"start": [0.6, 0.55, 0.45, 0.6], "end": [0.7, 0.65, 0.55, 0]},
                    "velocity": {"min": [-1, -1, 0], "max": [1, 1, 2]},
                    "drag": 2.0,
                    "material": "/Game/VFX/Materials/M_DustCloud",
                    "render_type": "sprite",
                },
            ],
            "collapse_delay": params["collapse_delay"],
        }

    def generate_footstep(self, terrain_type: str) -> dict:
        """Generate footstep VFX config for a terrain type."""
        params = self.manifest["footstep_vfx"][terrain_type]
        emitters = []

        if params["dust_size"] > 0:
            emitters.append({
                "name": "Dust",
                "spawn_rate": 0,
                "spawn_burst": params["particle_count"],
                "lifetime": params["lifetime"],
                "initial_size": params["dust_size"],
                "size_over_life": {"start": params["dust_size"], "end": params["dust_size"] * 3},
                "color": params["dust_color"],
                "color_over_life": {"start": params["dust_color"], "end": [params["dust_color"][0], params["dust_color"][1], params["dust_color"][2], 0]},
                "velocity": {"min": [-0.3, -0.3, 0], "max": [0.3, 0.3, 0.5]},
                "drag": 3.0,
                "material": "/Game/VFX/Materials/M_FootDust",
                "render_type": "sprite",
            })

        if params["splash_height"] > 0:
            emitters.append({
                "name": "Splash",
                "spawn_rate": 0,
                "spawn_burst": params["particle_count"],
                "lifetime": params["lifetime"],
                "initial_size": 0.02,
                "color": params["dust_color"],
                "velocity": {"min": [-0.5, -0.5, 0], "max": [0.5, 0.5, params["splash_height"] * 10]},
                "gravity": -9.81,
                "material": "/Game/VFX/Materials/M_WaterSplash",
                "render_type": "sprite",
            })

        return {
            "system_name": f"NS_Footstep_{terrain_type}",
            "ue5_path": f"/Game/VFX/Footsteps/NS_Footstep_{terrain_type}",
            "type": "footstep",
            "terrain_type": terrain_type,
            "emitters": emitters,
        }

    def generate_drone_vfx(self, drone_type: str) -> dict:
        """Generate drone VFX config."""
        params = self.manifest["drone_vfx"][drone_type]
        return {
            "system_name": f"NS_Drone_{drone_type}",
            "ue5_path": f"/Game/VFX/Drones/NS_Drone_{drone_type}",
            "type": "drone",
            "params": params,
        }

    def generate_all(self) -> list[dict]:
        """Generate all VFX configs from manifest."""
        configs = []

        # Muzzle flash + shell eject per weapon
        for caliber_id, caliber_data in self.manifest["muzzle_flash"].items():
            for weapon_id in caliber_data["weapons"]:
                configs.append(self.generate_muzzle_flash(weapon_id, caliber_id))
                if caliber_id in self.manifest["shell_eject"]:
                    configs.append(self.generate_shell_eject(weapon_id, caliber_id))

        # Impacts per surface
        for surface_type in self.manifest["impact_effects"]:
            configs.append(self.generate_impact(surface_type))

        # Destruction per type
        for dest_type in self.manifest["destruction_vfx"]:
            configs.append(self.generate_destruction(dest_type))

        # Footsteps per terrain
        for terrain_type in self.manifest["footstep_vfx"]:
            configs.append(self.generate_footstep(terrain_type))

        # Drone VFX
        for drone_type in self.manifest["drone_vfx"]:
            configs.append(self.generate_drone_vfx(drone_type))

        print(f"[VFXGenerator] Generated {len(configs)} VFX configs")
        return configs
