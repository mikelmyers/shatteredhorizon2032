"""Audio style validation -- file format, BPM, key, mood and duration checks."""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Optional

logger = logging.getLogger(__name__)

# Allowed audio file extensions.
_ALLOWED_EXTENSIONS: set[str] = {".wav", ".ogg", ".mp3"}


class AudioChecker:
    """Validate audio assets against style bible rules."""

    def __init__(self, config: dict) -> None:
        self.config: dict = config
        self.bpm_range: list[int] = config.get("music_bpm_range", [60, 140])
        self.approved_keys: list[str] = config.get("approved_keys", [])
        self.approved_moods: list[str] = config.get("approved_moods", [])
        self.max_duration: int = config.get("max_duration_seconds", 300)

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def check(
        self,
        file_path: str,
        metadata: dict | None = None,
    ) -> tuple[float, list[str]]:
        """Validate *file_path* and return ``(score, issues)``.

        ``metadata`` may contain:
        - ``bpm`` (float/int)
        - ``key`` (str)  -- e.g. ``"Cm"``, ``"Dm"``
        - ``mood`` (str)
        - ``duration_seconds`` (float/int)
        """

        meta = metadata or {}
        issues: list[str] = []
        checks_passed: int = 0
        checks_total: int = 0

        # --- File extension ---------------------------------------------------
        checks_total += 1
        ext = Path(file_path).suffix.lower()
        if ext in _ALLOWED_EXTENSIONS:
            checks_passed += 1
        else:
            issues.append(
                f"FAIL: Audio extension '{ext}' not allowed -- expected one of {sorted(_ALLOWED_EXTENSIONS)}"
            )

        # --- BPM --------------------------------------------------------------
        bpm: Optional[float] = meta.get("bpm")
        if bpm is not None:
            checks_total += 1
            if self.bpm_range[0] <= bpm <= self.bpm_range[1]:
                checks_passed += 1
            else:
                issues.append(
                    f"WARN: BPM {bpm} outside approved range {self.bpm_range}"
                )

        # --- Key --------------------------------------------------------------
        key: Optional[str] = meta.get("key")
        if key is not None:
            checks_total += 1
            if key in self.approved_keys:
                checks_passed += 1
            else:
                issues.append(
                    f"WARN: Key '{key}' not in approved keys {self.approved_keys}"
                )

        # --- Mood -------------------------------------------------------------
        mood: Optional[str] = meta.get("mood")
        if mood is not None:
            checks_total += 1
            if mood.lower() in [m.lower() for m in self.approved_moods]:
                checks_passed += 1
            else:
                issues.append(
                    f"WARN: Mood '{mood}' not in approved moods {self.approved_moods}"
                )

        # --- Duration ---------------------------------------------------------
        duration: Optional[float] = meta.get("duration_seconds")
        if duration is not None:
            checks_total += 1
            if duration <= self.max_duration:
                checks_passed += 1
            else:
                issues.append(
                    f"FAIL: Duration {duration:.1f}s exceeds maximum {self.max_duration}s"
                )

        # --- Compute score ----------------------------------------------------
        score = checks_passed / checks_total if checks_total > 0 else 1.0
        score = max(0.0, min(1.0, score))

        return round(score, 4), issues
