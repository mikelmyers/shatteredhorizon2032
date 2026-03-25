"""Ballistic calculations using the G1 drag model.

Provides realistic projectile physics for Shattered Horizon 2032 weapon
simulation, including drag, drop, time of flight, energy, and penetration.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import List, Optional

import numpy as np


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
GRAVITY: float = 9.80665  # m/s^2
STANDARD_AIR_DENSITY: float = 1.225  # kg/m^3 at sea level, 15 C
GRAINS_TO_KG: float = 0.0000647989  # 1 grain in kilograms

# G1 standard projectile reference values
G1_REFERENCE_DIAMETER_M: float = 0.00762  # 7.62 mm reference diameter
G1_REFERENCE_AREA_M2: float = math.pi * (G1_REFERENCE_DIAMETER_M / 2.0) ** 2
G1_REFERENCE_MASS_KG: float = 0.00972  # ~150 gr reference projectile


@dataclass
class RangeTableEntry:
    """A single row in a ballistic range table."""

    distance_m: float
    velocity_mps: float
    energy_j: float
    drop_m: float
    time_of_flight_s: float
    remaining_penetration_mm: float = 0.0


class BallisticsCalculator:
    """G1 drag-model ballistic calculator.

    All methods are either static/class-level utilities or operate on
    weapon data dicts loaded from ``weapon_database.json``.
    """

    # -------------------------------------------------------------------
    # G1 drag coefficient lookup (Mach -> Cd)
    # Piecewise fit to the standard G1 drag curve.
    # -------------------------------------------------------------------
    _G1_MACH_CD: List[tuple[float, float]] = [
        (0.00, 0.2629),
        (0.50, 0.2558),
        (0.70, 0.2690),
        (0.80, 0.2860),
        (0.85, 0.3030),
        (0.90, 0.3280),
        (0.925, 0.3530),
        (0.95, 0.3900),
        (0.975, 0.4400),
        (1.00, 0.4920),
        (1.025, 0.5370),
        (1.05, 0.5560),
        (1.075, 0.5680),
        (1.10, 0.5740),
        (1.15, 0.5760),
        (1.20, 0.5680),
        (1.30, 0.5410),
        (1.50, 0.4960),
        (1.75, 0.4560),
        (2.00, 0.4240),
        (2.50, 0.3800),
        (3.00, 0.3520),
    ]

    # ------------------------------------------------------------------
    # Core drag helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _speed_of_sound(temperature_c: float = 15.0) -> float:
        """Speed of sound in m/s for a given temperature in Celsius."""
        return 331.3 * math.sqrt(1.0 + temperature_c / 273.15)

    @classmethod
    def _g1_cd(cls, mach: float) -> float:
        """Interpolate the G1 drag coefficient for a given Mach number."""
        table = cls._G1_MACH_CD
        if mach <= table[0][0]:
            return table[0][1]
        if mach >= table[-1][0]:
            return table[-1][1]
        for i in range(len(table) - 1):
            m0, cd0 = table[i]
            m1, cd1 = table[i + 1]
            if m0 <= mach <= m1:
                t = (mach - m0) / (m1 - m0)
                return cd0 + t * (cd1 - cd0)
        return table[-1][1]  # pragma: no cover

    # ------------------------------------------------------------------
    # Public calculation API
    # ------------------------------------------------------------------

    @classmethod
    def calculate_drag(
        cls,
        velocity_mps: float,
        bc_g1: float,
        air_density: float = STANDARD_AIR_DENSITY,
        temperature_c: float = 15.0,
    ) -> float:
        """Return deceleration (m/s^2) due to aerodynamic drag.

        Uses the G1 drag model:
            F_drag = 0.5 * rho * V^2 * Cd * A_ref
            a_drag = F_drag / m_ref  (scaled by BC)

        The ballistic coefficient *BC* relates the test projectile to the
        G1 standard: ``BC = (sectional_density) / (form_factor)``.  A
        higher BC means less drag.

        Args:
            velocity_mps: Current projectile velocity in m/s.
            bc_g1: G1 ballistic coefficient of the projectile.
            air_density: Air density in kg/m^3.
            temperature_c: Ambient temperature in Celsius.

        Returns:
            Deceleration magnitude in m/s^2 (always >= 0).
        """
        if velocity_mps <= 0.0 or bc_g1 <= 0.0:
            return 0.0

        speed_of_sound = cls._speed_of_sound(temperature_c)
        mach = velocity_mps / speed_of_sound
        cd = cls._g1_cd(mach)

        # Drag on the G1 reference projectile at this velocity
        drag_force_ref = (
            0.5 * air_density * velocity_mps ** 2 * cd * G1_REFERENCE_AREA_M2
        )
        decel_ref = drag_force_ref / G1_REFERENCE_MASS_KG

        # Scale by BC: lower BC -> more drag relative to reference
        # BC = SD / i  =>  decel_actual = decel_ref * (SD_ref / BC)
        sd_ref = G1_REFERENCE_MASS_KG / G1_REFERENCE_AREA_M2
        decel_actual = decel_ref * (sd_ref / (bc_g1 * 1000.0))
        # The factor of 1000 converts the BC (in lb/in^2 convention
        # normalised to the reference) to consistent SI scaling.

        # Density correction relative to standard atmosphere
        density_ratio = air_density / STANDARD_AIR_DENSITY
        decel_actual *= density_ratio

        return max(decel_actual, 0.0)

    @classmethod
    def calculate_drop(
        cls,
        distance_m: float,
        muzzle_velocity_mps: float,
        bc_g1: float,
        air_density: float = STANDARD_AIR_DENSITY,
    ) -> float:
        """Calculate bullet drop in metres at a given distance.

        Performs a numerical integration (Euler method, 0.5 m steps) of
        the projectile trajectory under gravity and G1 drag.

        Args:
            distance_m: Downrange distance in metres.
            muzzle_velocity_mps: Muzzle velocity in m/s.
            bc_g1: G1 ballistic coefficient.
            air_density: Air density in kg/m^3.

        Returns:
            Bullet drop in metres (negative value = below bore axis).
        """
        if distance_m <= 0.0:
            return 0.0

        dt = 0.001  # time step in seconds
        x = 0.0
        y = 0.0
        vx = muzzle_velocity_mps
        vy = 0.0

        while x < distance_m:
            speed = math.sqrt(vx ** 2 + vy ** 2)
            if speed < 1.0:
                break
            drag_decel = cls.calculate_drag(speed, bc_g1, air_density)
            # Drag acts opposite to velocity vector
            ax = -drag_decel * (vx / speed)
            ay = -GRAVITY - drag_decel * (vy / speed)

            vx += ax * dt
            vy += ay * dt
            x += vx * dt
            y += vy * dt

            if x >= distance_m:
                break

        return y

    @classmethod
    def calculate_time_of_flight(
        cls,
        distance_m: float,
        muzzle_velocity_mps: float,
        bc_g1: float,
        air_density: float = STANDARD_AIR_DENSITY,
    ) -> float:
        """Calculate time of flight in seconds to a given distance.

        Args:
            distance_m: Downrange distance in metres.
            muzzle_velocity_mps: Muzzle velocity in m/s.
            bc_g1: G1 ballistic coefficient.
            air_density: Air density in kg/m^3.

        Returns:
            Time of flight in seconds.
        """
        if distance_m <= 0.0:
            return 0.0

        dt = 0.001
        x = 0.0
        t = 0.0
        vx = muzzle_velocity_mps
        vy = 0.0

        while x < distance_m:
            speed = math.sqrt(vx ** 2 + vy ** 2)
            if speed < 1.0:
                break
            drag_decel = cls.calculate_drag(speed, bc_g1, air_density)
            ax = -drag_decel * (vx / speed)
            ay = -GRAVITY - drag_decel * (vy / speed)
            vx += ax * dt
            vy += ay * dt
            x += vx * dt
            t += dt

            if x >= distance_m:
                break

        return t

    @staticmethod
    def calculate_energy(mass_gr: float, velocity_mps: float) -> float:
        """Calculate kinetic energy in joules.

        Args:
            mass_gr: Bullet mass in grains.
            velocity_mps: Velocity in m/s.

        Returns:
            Kinetic energy in joules.
        """
        mass_kg = mass_gr * GRAINS_TO_KG
        return 0.5 * mass_kg * velocity_mps ** 2

    @staticmethod
    def calculate_penetration(
        energy_j: float,
        material_hardness: float = 1.0,
    ) -> float:
        """Estimate penetration depth in mm of mild steel equivalent.

        Uses a simplified energy-based penetration model:
            penetration_mm = k * energy / hardness
        where *k* is tuned so that 5.56 NATO at muzzle (~1800 J)
        penetrates roughly 6 mm of mild steel.

        Args:
            energy_j: Kinetic energy of the projectile in joules.
            material_hardness: Relative hardness (1.0 = mild steel).

        Returns:
            Penetration depth in mm.
        """
        if energy_j <= 0.0 or material_hardness <= 0.0:
            return 0.0
        k = 6.0 / 1800.0  # tuning constant
        return k * energy_j / material_hardness

    @classmethod
    def _velocity_at_distance(
        cls,
        distance_m: float,
        muzzle_velocity_mps: float,
        bc_g1: float,
        air_density: float = STANDARD_AIR_DENSITY,
    ) -> float:
        """Return remaining velocity at a given distance."""
        if distance_m <= 0.0:
            return muzzle_velocity_mps

        dt = 0.001
        x = 0.0
        vx = muzzle_velocity_mps
        vy = 0.0

        while x < distance_m:
            speed = math.sqrt(vx ** 2 + vy ** 2)
            if speed < 1.0:
                return 0.0
            drag_decel = cls.calculate_drag(speed, bc_g1, air_density)
            ax = -drag_decel * (vx / speed)
            ay = -GRAVITY - drag_decel * (vy / speed)
            vx += ax * dt
            vy += ay * dt
            x += vx * dt

            if x >= distance_m:
                break

        return math.sqrt(vx ** 2 + vy ** 2)

    @classmethod
    def generate_range_table(
        cls,
        weapon_data: dict,
        max_range: Optional[int] = None,
        step: int = 50,
    ) -> List[RangeTableEntry]:
        """Generate a complete range table for a weapon.

        Args:
            weapon_data: Weapon configuration dict from the database.
            max_range: Maximum distance in metres (defaults to
                effective_range_m from the weapon data).
            step: Distance increment in metres.

        Returns:
            List of :class:`RangeTableEntry` objects from 0 to
            *max_range* in *step* increments.
        """
        mv = weapon_data["muzzle_velocity_mps"]
        bc = weapon_data["bc_g1"]
        mass_gr = weapon_data["bullet_mass_gr"]
        pen_base = weapon_data.get("penetration_mm_steel", 0.0)

        if max_range is None:
            max_range = int(weapon_data.get("effective_range_m", 500))

        table: List[RangeTableEntry] = []
        for dist in range(0, max_range + 1, step):
            vel = cls._velocity_at_distance(dist, mv, bc)
            energy = cls.calculate_energy(mass_gr, vel)
            drop = cls.calculate_drop(dist, mv, bc) if dist > 0 else 0.0
            tof = cls.calculate_time_of_flight(dist, mv, bc) if dist > 0 else 0.0

            # Scale penetration proportionally to remaining energy
            muzzle_energy = cls.calculate_energy(mass_gr, mv)
            if muzzle_energy > 0:
                pen = pen_base * (energy / muzzle_energy)
            else:
                pen = 0.0

            table.append(
                RangeTableEntry(
                    distance_m=float(dist),
                    velocity_mps=round(vel, 1),
                    energy_j=round(energy, 1),
                    drop_m=round(drop, 4),
                    time_of_flight_s=round(tof, 4),
                    remaining_penetration_mm=round(pen, 2),
                )
            )

        return table
