"""Generate UE5-compatible weapon DataAsset configurations.

Reads raw ballistic data from ``weapon_database.json`` and produces
structured JSON that maps 1-to-1 with the ``FSHWeaponData`` UE5 struct
defined in ``SHWeaponData.h``.
"""

from __future__ import annotations

import copy
import json
import logging
from pathlib import Path
from typing import Any, Dict, List, Optional

from .ballistics_calculator import BallisticsCalculator

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Base damage tuning
# ---------------------------------------------------------------------------
# Base damage is derived from muzzle energy so that heavier / faster rounds
# naturally deal more damage.  The constant is tuned so that a 5.56 round
# (~1800 J) yields roughly 34 base HP damage.
_BASE_DAMAGE_PER_JOULE: float = 34.0 / 1800.0

# Suppression impulse scales with caliber energy.  A 5.56 round at ~1800 J
# produces ~120 suppression units.
_SUPPRESSION_PER_JOULE: float = 120.0 / 1800.0

# Fire-mode enum mapping for UE5
_FIRE_MODE_MAP: Dict[str, str] = {
    "semi": "EFireMode::Semi",
    "auto": "EFireMode::FullAuto",
    "burst3": "EFireMode::Burst3",
    "single": "EFireMode::Single",
    "pump": "EFireMode::Pump",
}


def _compute_base_damage(weapon: dict) -> float:
    """Derive base damage from muzzle energy."""
    energy = BallisticsCalculator.calculate_energy(
        weapon["bullet_mass_gr"],
        weapon["muzzle_velocity_mps"],
    )
    return round(energy * _BASE_DAMAGE_PER_JOULE, 1)


def _compute_suppression(weapon: dict) -> float:
    """Derive suppression impulse from muzzle energy."""
    energy = BallisticsCalculator.calculate_energy(
        weapon["bullet_mass_gr"],
        weapon["muzzle_velocity_mps"],
    )
    return round(energy * _SUPPRESSION_PER_JOULE, 1)


class WeaponDataGenerator:
    """Generates UE5 ``FSHWeaponData`` JSON for each weapon."""

    def __init__(self, database: dict) -> None:
        """Initialise with the full weapon database dict.

        Args:
            database: Parsed contents of ``weapon_database.json``.
        """
        self._database = database
        self._weapons: Dict[str, dict] = database.get("weapons", {})

    @property
    def weapon_ids(self) -> List[str]:
        """Return sorted list of weapon identifiers."""
        return sorted(self._weapons.keys())

    # ------------------------------------------------------------------
    # Single weapon
    # ------------------------------------------------------------------

    @staticmethod
    def generate_weapon_data_asset(weapon_config: dict) -> dict:
        """Generate a single UE5 DataAsset JSON for *weapon_config*.

        The returned dict matches the ``FSHWeaponData`` struct field
        names exactly so it can be deserialised in Unreal without any
        manual mapping.

        Args:
            weapon_config: A single weapon entry from the database.

        Returns:
            Dict ready to serialise as a UE5 DataAsset JSON file.
        """
        wc = weapon_config
        fire_modes_ue5 = [
            _FIRE_MODE_MAP.get(m, f"EFireMode::{m.capitalize()}")
            for m in wc.get("fire_modes", [])
        ]

        base_damage = _compute_base_damage(wc)
        suppression = _compute_suppression(wc)

        # Head / limb multipliers from damage_model
        dm = wc.get("damage_model", {})
        headshot_mult = dm.get("head", 2.5)
        limb_mult = round(
            (
                dm.get("upper_arm", 0.7)
                + dm.get("lower_arm", 0.6)
                + dm.get("upper_leg", 0.75)
                + dm.get("lower_leg", 0.65)
            )
            / 4.0,
            3,
        )

        asset: Dict[str, Any] = {
            "WeaponName": wc.get("display_name", "Unknown"),
            "WeaponID": wc.get("_id", "unknown"),
            "Caliber": wc.get("caliber", ""),
            "BulletMassGrains": wc.get("bullet_mass_gr", 0),
            "MuzzleVelocity": wc.get("muzzle_velocity_mps", 0.0),
            "BallisticCoefficient": wc.get("bc_g1", 0.0),
            "MaxEffectiveRange": wc.get("effective_range_m", 0.0),
            "RateOfFireCyclic": wc.get("rpm_cyclic", 0),
            "RateOfFireSemi": wc.get("rpm_semi", 0),
            "MagazineCapacity": wc.get("mag_capacity", 0),
            "ReloadTime": wc.get("reload_time_s", 0.0),
            "BaseDamage": base_damage,
            "HeadshotMultiplier": headshot_mult,
            "LimbDamageMultiplier": limb_mult,
            "ArmorPenetrationMM": wc.get("penetration_mm_steel", 0.0),
            "bIsExplosive": wc.get("is_explosive", False),
            "BlastRadius": wc.get("blast_radius_m", 0.0),
            "FragmentCount": wc.get("frag_count", 0),
            "SuppressionImpulse": suppression,
            "RecoilVertical": wc.get("recoil_vertical", 0.0),
            "RecoilHorizontal": wc.get("recoil_horizontal", 0.0),
            "ADSTime": wc.get("ads_time_s", 0.0),
            "FireModes": fire_modes_ue5,
        }
        return asset

    # ------------------------------------------------------------------
    # Batch
    # ------------------------------------------------------------------

    def generate_all_weapons(
        self,
        weapon_ids: Optional[List[str]] = None,
    ) -> List[dict]:
        """Generate DataAsset dicts for every (or selected) weapon(s).

        Args:
            weapon_ids: Optional subset of weapon IDs.  When *None*, all
                weapons in the database are processed.

        Returns:
            List of DataAsset dicts.
        """
        ids = weapon_ids or self.weapon_ids
        results: List[dict] = []

        for wid in ids:
            wc = self._weapons.get(wid)
            if wc is None:
                logger.warning("Weapon '%s' not found in database, skipping.", wid)
                continue
            # Inject the ID so the asset can reference it
            config = copy.deepcopy(wc)
            config["_id"] = wid
            asset = self.generate_weapon_data_asset(config)
            results.append(asset)
            logger.info("Generated DataAsset for %s", wid)

        return results

    # ------------------------------------------------------------------
    # Loading helper
    # ------------------------------------------------------------------

    @classmethod
    def from_json(cls, path: str | Path) -> "WeaponDataGenerator":
        """Convenience constructor: load from a JSON file path.

        Args:
            path: Path to ``weapon_database.json``.

        Returns:
            Initialised :class:`WeaponDataGenerator`.
        """
        path = Path(path)
        with path.open("r", encoding="utf-8") as fh:
            data = json.load(fh)
        return cls(data)
