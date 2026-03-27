"""Color palette validation against the style bible."""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Optional

logger = logging.getLogger(__name__)

# Graceful degradation for optional dependencies.
try:
    from PIL import Image  # type: ignore[import-untyped]

    _PIL_AVAILABLE = True
except ImportError:
    _PIL_AVAILABLE = False

try:
    import numpy as np  # type: ignore[import-untyped]

    _NUMPY_AVAILABLE = True
except ImportError:
    _NUMPY_AVAILABLE = False


class PaletteChecker:
    """Validate image colour palettes against style bible rules.

    Operates in degraded mode if Pillow or numpy are not installed --
    ``available`` will be ``False`` and :meth:`check` returns a neutral
    score of ``0.5``.
    """

    def __init__(self, config: dict) -> None:
        self.config: dict = config
        self.available: bool = _PIL_AVAILABLE and _NUMPY_AVAILABLE

        # Unpack config for convenience.
        self.saturation_max: float = config.get("saturation_max", 0.45)
        self.hue_ranges: dict[str, list[int]] = config.get("hue_ranges", {})
        self.forbidden_hue_ranges: dict[str, list[int]] = config.get("forbidden_hue_ranges", {})

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def check(self, image_path: str) -> tuple[float, list[str]]:
        """Analyse the image at *image_path* and return ``(score, issues)``.

        ``score`` is in the range ``[0, 1]`` (higher is better).
        ``issues`` is a list of human-readable strings describing any
        problems found.

        Scoring breakdown (weights):
        - 0.4 -- saturation compliance
        - 0.4 -- dominant-hue compliance
        - 0.2 -- absence of forbidden hues
        """

        if not self.available:
            return 0.5, ["Pillow/numpy not installed -- palette check skipped"]

        path = Path(image_path)
        if not path.exists():
            return 0.5, [f"File not found: {image_path}"]

        try:
            img = Image.open(image_path).convert("RGB")
        except Exception as exc:
            return 0.5, [f"Could not open image: {exc}"]

        hsv_img = img.convert("HSV")
        hsv_array = np.array(hsv_img)

        issues: list[str] = []

        # --- Saturation check -------------------------------------------------
        saturation_score = self._check_saturation(hsv_array, issues)

        # --- Dominant hue compliance ------------------------------------------
        hue_compliance = self._check_hue_compliance(hsv_array, issues)

        # --- Forbidden hue absence --------------------------------------------
        forbidden_absence = self._check_forbidden_hues(hsv_array, issues)

        # --- Combined score ---------------------------------------------------
        score = 0.4 * saturation_score + 0.4 * hue_compliance + 0.2 * forbidden_absence
        score = max(0.0, min(1.0, score))

        return round(score, 4), issues

    # ------------------------------------------------------------------
    # Private helpers
    # ------------------------------------------------------------------

    def _check_saturation(self, hsv_array: np.ndarray, issues: list[str]) -> float:
        """Return a 0-1 score for saturation compliance."""

        # PIL HSV encodes saturation in [0, 255].
        avg_saturation: float = float(hsv_array[:, :, 1].mean()) / 255.0

        if avg_saturation <= self.saturation_max:
            return 1.0

        overshoot = avg_saturation - self.saturation_max
        issues.append(
            f"FAIL: Average saturation {avg_saturation:.3f} exceeds max {self.saturation_max}"
        )
        # Linearly degrade from 1 -> 0 over a 0.3 overshoot range.
        return max(0.0, 1.0 - overshoot / 0.3)

    def _check_hue_compliance(self, hsv_array: np.ndarray, issues: list[str]) -> float:
        """Return the fraction of saturated pixels that fall within approved hue ranges."""

        # PIL HSV: H in [0, 255] maps to [0, 360].
        hue_channel = hsv_array[:, :, 0].astype(np.float64) * 360.0 / 255.0
        sat_channel = hsv_array[:, :, 1].astype(np.float64) / 255.0

        total_pixels = hue_channel.size

        # Low-saturation pixels (grays / blacks / whites) are always acceptable.
        low_sat_mask = sat_channel < 0.15
        approved_count = int(low_sat_mask.sum())

        for _hue_name, (hue_min, hue_max) in self.hue_ranges.items():
            in_range = (hue_channel >= hue_min) & (hue_channel <= hue_max)
            approved_count += int((in_range & ~low_sat_mask).sum())

        approved_pct = approved_count / total_pixels * 100.0 if total_pixels else 100.0

        if approved_pct < 60.0:
            issues.append(
                f"WARN: Only {approved_pct:.1f}% of pixels in approved hue ranges (target >60%)"
            )

        return min(1.0, approved_pct / 100.0)

    def _check_forbidden_hues(self, hsv_array: np.ndarray, issues: list[str]) -> float:
        """Return 1.0 when no forbidden hues are present, degrading toward 0."""

        hue_channel = hsv_array[:, :, 0].astype(np.float64) * 360.0 / 255.0
        sat_channel = hsv_array[:, :, 1].astype(np.float64) / 255.0
        total_pixels = hue_channel.size

        # Only consider saturated pixels so desaturated grays are not flagged.
        high_sat_mask = sat_channel > 0.3

        worst_pct: float = 0.0

        for hue_name, (hue_min, hue_max) in self.forbidden_hue_ranges.items():
            in_range = (hue_channel >= hue_min) & (hue_channel <= hue_max)
            forbidden_pixels = int((in_range & high_sat_mask).sum())
            forbidden_pct = forbidden_pixels / total_pixels * 100.0 if total_pixels else 0.0
            worst_pct = max(worst_pct, forbidden_pct)

            if forbidden_pct > 5.0:
                issues.append(
                    f"FAIL: {forbidden_pct:.1f}% of pixels in forbidden '{hue_name}' hue range"
                )
            elif forbidden_pct > 2.0:
                issues.append(
                    f"WARN: {forbidden_pct:.1f}% of pixels near forbidden '{hue_name}' range"
                )

        # Score: 1.0 at 0%, 0.0 at >= 10%.
        return max(0.0, 1.0 - worst_pct / 10.0)
