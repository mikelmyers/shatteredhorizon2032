"""Procedural animation curve generation for weapon mechanics.

Generates keyframe-based animation curves for recoil, weapon bob,
weapon sway, and ADS transitions. All curves are exportable as JSON
that UE5 can import as CurveFloat / CurveVector assets.
"""

from __future__ import annotations

import json
import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

try:
    from scipy.interpolate import CubicSpline
    HAS_SCIPY = True
except ImportError:
    HAS_SCIPY = False


# ---------------------------------------------------------------------------
# Caliber-based recoil profiles
# ---------------------------------------------------------------------------
_CALIBER_RECOIL_PROFILES: Dict[str, Dict[str, float]] = {
    "9x19mm": {
        "pitch_impulse": 3.5,
        "yaw_impulse": 1.0,
        "roll_impulse": 0.5,
        "recovery_speed": 8.0,
    },
    "5.56x45mm": {
        "pitch_impulse": 5.0,
        "yaw_impulse": 1.5,
        "roll_impulse": 0.8,
        "recovery_speed": 6.0,
    },
    "7.62x39mm": {
        "pitch_impulse": 7.0,
        "yaw_impulse": 2.5,
        "roll_impulse": 1.2,
        "recovery_speed": 5.0,
    },
    "7.62x51mm": {
        "pitch_impulse": 8.0,
        "yaw_impulse": 2.0,
        "roll_impulse": 1.0,
        "recovery_speed": 4.5,
    },
    ".338_lapua": {
        "pitch_impulse": 12.0,
        "yaw_impulse": 3.0,
        "roll_impulse": 1.5,
        "recovery_speed": 3.0,
    },
    "12.7x99mm": {
        "pitch_impulse": 18.0,
        "yaw_impulse": 4.0,
        "roll_impulse": 2.5,
        "recovery_speed": 2.5,
    },
    "12gauge": {
        "pitch_impulse": 10.0,
        "yaw_impulse": 3.5,
        "roll_impulse": 2.0,
        "recovery_speed": 4.0,
    },
    "40mm": {
        "pitch_impulse": 14.0,
        "yaw_impulse": 2.0,
        "roll_impulse": 1.0,
        "recovery_speed": 2.0,
    },
}


@dataclass
class AnimationKeyframe:
    """A single keyframe in an animation curve."""

    time: float
    value: List[float]  # [pitch, yaw, roll] or [x, y, z] depending on context

    def to_dict(self) -> Dict[str, Any]:
        """Serialise to dict."""
        return {"time": round(self.time, 4), "value": [round(v, 6) for v in self.value]}


@dataclass
class AnimationCurve:
    """A named animation curve consisting of keyframes."""

    name: str
    keyframes: List[AnimationKeyframe] = field(default_factory=list)
    is_looping: bool = False
    duration: float = 0.0
    curve_type: str = "vector"  # "vector" or "float"

    def to_dict(self) -> Dict[str, Any]:
        """Serialise to dict."""
        return {
            "name": self.name,
            "curve_type": self.curve_type,
            "is_looping": self.is_looping,
            "duration": round(self.duration, 4),
            "keyframe_count": len(self.keyframes),
            "keyframes": [kf.to_dict() for kf in self.keyframes],
        }


class ProceduralAnimator:
    """Generates procedural animation curves for Shattered Horizon 2032."""

    # ------------------------------------------------------------------
    # Recoil
    # ------------------------------------------------------------------

    @staticmethod
    def generate_recoil_curve(weapon_data: dict) -> AnimationCurve:
        """Generate a recoil animation curve for a weapon.

        The curve models an impulse followed by a spring-damped recovery
        back to the neutral position. Intensity varies by caliber.

        Args:
            weapon_data: Weapon config dict from the database. Must
                contain ``caliber``, ``recoil_vertical``, and
                ``recoil_horizontal``.

        Returns:
            :class:`AnimationCurve` with keyframes for
            ``[pitch, yaw, roll]``.
        """
        caliber = weapon_data.get("caliber", "5.56x45mm")
        profile = _CALIBER_RECOIL_PROFILES.get(
            caliber, _CALIBER_RECOIL_PROFILES["5.56x45mm"]
        )

        rv = weapon_data.get("recoil_vertical", 3.0)
        rh = weapon_data.get("recoil_horizontal", 1.0)

        # Scale profile by weapon-specific recoil values
        pitch_max = profile["pitch_impulse"] * (rv / 3.0)
        yaw_max = profile["yaw_impulse"] * (rh / 1.0)
        roll_max = profile["roll_impulse"]
        recovery = profile["recovery_speed"]

        # Duration: impulse (0 -> peak) + recovery (peak -> settle)
        impulse_time = 0.04  # 40ms to peak recoil
        recovery_time = 1.0 / recovery
        total = impulse_time + recovery_time + 0.05  # small settle buffer

        keyframes: List[AnimationKeyframe] = []
        num_samples = 60
        dt = total / num_samples

        # Random yaw direction per shot (we use a fixed seed for
        # deterministic export; runtime applies random sign)
        yaw_sign = 1.0

        for i in range(num_samples + 1):
            t = i * dt
            if t <= impulse_time:
                # Quick ramp up (ease-out curve)
                frac = t / impulse_time
                smooth = 1.0 - (1.0 - frac) ** 3
                pitch = pitch_max * smooth
                yaw = yaw_max * smooth * yaw_sign * 0.5
                roll = roll_max * smooth * 0.3
            else:
                # Spring-damped decay
                elapsed = t - impulse_time
                decay = math.exp(-recovery * elapsed)
                # Slight oscillation for realism
                osc = math.cos(recovery * elapsed * 2.0 * math.pi * 0.3)
                pitch = pitch_max * decay * max(osc, 0.0)
                yaw = yaw_max * decay * osc * yaw_sign * 0.5
                roll = roll_max * decay * osc * 0.3

            keyframes.append(AnimationKeyframe(time=t, value=[pitch, yaw, roll]))

        weapon_name = weapon_data.get("_id", weapon_data.get("display_name", "unknown"))
        curve = AnimationCurve(
            name=f"RC_{weapon_name}_Recoil",
            keyframes=keyframes,
            is_looping=False,
            duration=total,
            curve_type="vector",
        )
        return curve

    # ------------------------------------------------------------------
    # Weapon Bob
    # ------------------------------------------------------------------

    @staticmethod
    def generate_weapon_bob_curve(
        movement_speed: float = 3.0,
        weapon_weight: float = 3.5,
        duration: float = 2.0,
    ) -> AnimationCurve:
        """Generate a weapon-bob curve driven by movement.

        Uses sine waves whose amplitude scales with movement speed and
        weapon weight.

        Args:
            movement_speed: Character movement speed in m/s (walk ~1.5,
                run ~4.0, sprint ~6.0).
            weapon_weight: Weapon weight in kg.
            duration: Curve duration in seconds (one full cycle).

        Returns:
            :class:`AnimationCurve` with ``[x, y, z]`` keyframes
            representing weapon offset from rest position.
        """
        # Frequency increases with speed; amplitude with speed and weight
        freq = 1.5 + movement_speed * 0.8  # Hz
        amp_x = 0.002 * movement_speed * (weapon_weight / 3.5)
        amp_y = 0.004 * movement_speed * (weapon_weight / 3.5)
        amp_z = 0.001 * movement_speed

        keyframes: List[AnimationKeyframe] = []
        num_samples = 80
        dt = duration / num_samples

        for i in range(num_samples + 1):
            t = i * dt
            phase = 2.0 * math.pi * freq * t
            x = amp_x * math.sin(phase)
            y = amp_y * math.sin(phase * 2.0)  # vertical bobs at double freq
            z = amp_z * math.cos(phase)
            keyframes.append(AnimationKeyframe(time=t, value=[x, y, z]))

        return AnimationCurve(
            name=f"WB_Bob_Speed{movement_speed:.0f}_Weight{weapon_weight:.1f}",
            keyframes=keyframes,
            is_looping=True,
            duration=duration,
            curve_type="vector",
        )

    # ------------------------------------------------------------------
    # Weapon Sway
    # ------------------------------------------------------------------

    @staticmethod
    def generate_weapon_sway_curve(
        ads_stability: float = 0.7,
        breathing_rate: float = 0.25,
        duration: float = 4.0,
    ) -> AnimationCurve:
        """Generate a figure-8 weapon sway pattern.

        When ADS the pattern is tighter (smaller amplitude); hip-fire
        has wider sway.

        Args:
            ads_stability: 0.0 = maximum sway, 1.0 = rock solid.
            breathing_rate: Breathing frequency in Hz.
            duration: Curve duration in seconds.

        Returns:
            :class:`AnimationCurve` with ``[x, y, z]`` sway offsets.
        """
        # Amplitude inversely proportional to stability
        base_amp = 0.008 * (1.0 - ads_stability)
        freq = breathing_rate

        keyframes: List[AnimationKeyframe] = []
        num_samples = 120
        dt = duration / num_samples

        for i in range(num_samples + 1):
            t = i * dt
            phase = 2.0 * math.pi * freq * t
            # Figure-8 pattern (Lissajous with 1:2 ratio)
            x = base_amp * math.sin(phase)
            y = base_amp * 0.6 * math.sin(2.0 * phase)
            # Slight Z sway from breathing
            z = base_amp * 0.3 * math.sin(phase * 0.5)
            keyframes.append(AnimationKeyframe(time=t, value=[x, y, z]))

        stability_pct = int(ads_stability * 100)
        return AnimationCurve(
            name=f"WS_Sway_Stability{stability_pct}",
            keyframes=keyframes,
            is_looping=True,
            duration=duration,
            curve_type="vector",
        )

    # ------------------------------------------------------------------
    # ADS Transition
    # ------------------------------------------------------------------

    @staticmethod
    def generate_ads_transition_curve(
        ads_time: float = 0.25,
    ) -> AnimationCurve:
        """Generate a smooth ease-in-out ADS transition curve.

        Goes from 0.0 (hip) to 1.0 (full ADS) over *ads_time* seconds
        using a cubic hermite (smoothstep) interpolation.

        Args:
            ads_time: Time in seconds for the hip-to-ADS transition.

        Returns:
            :class:`AnimationCurve` with single-float keyframes
            representing the ADS blend weight (0 = hip, 1 = ADS).
        """
        keyframes: List[AnimationKeyframe] = []
        num_samples = 30
        dt = ads_time / num_samples if num_samples > 0 else ads_time

        for i in range(num_samples + 1):
            t = i * dt
            # Smoothstep: 3t^2 - 2t^3
            frac = t / ads_time if ads_time > 0.0 else 1.0
            frac = max(0.0, min(1.0, frac))
            blend = 3.0 * frac ** 2 - 2.0 * frac ** 3
            keyframes.append(AnimationKeyframe(time=t, value=[blend]))

        return AnimationCurve(
            name=f"ADS_Transition_{int(ads_time * 1000)}ms",
            keyframes=keyframes,
            is_looping=False,
            duration=ads_time,
            curve_type="float",
        )

    # ------------------------------------------------------------------
    # Export
    # ------------------------------------------------------------------

    @staticmethod
    def export_curve_as_json(
        curve: AnimationCurve,
        name: Optional[str] = None,
        output_path: Optional[str | Path] = None,
    ) -> Dict[str, Any]:
        """Export an animation curve as JSON.

        The format is designed for direct import into UE5 as a
        ``UCurveFloat`` or ``UCurveVector`` asset via the Editor
        Scripting plugin.

        Args:
            curve: The :class:`AnimationCurve` to export.
            name: Override the curve name in the export.
            output_path: If provided, writes JSON to this file.

        Returns:
            The curve data as a dict.
        """
        data = curve.to_dict()
        if name is not None:
            data["name"] = name

        # Add UE5 metadata
        data["ue5_asset_class"] = (
            "CurveFloat" if curve.curve_type == "float" else "CurveVector"
        )
        data["ue5_interp_mode"] = "RCIM_Cubic"

        if output_path is not None:
            p = Path(output_path)
            p.parent.mkdir(parents=True, exist_ok=True)
            with p.open("w", encoding="utf-8") as fh:
                json.dump(data, fh, indent=2, ensure_ascii=False)

        return data
