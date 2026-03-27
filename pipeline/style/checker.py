"""Main style check orchestrator -- routes assets to the appropriate sub-checker."""

from __future__ import annotations

import json
import logging
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

logger = logging.getLogger(__name__)

STYLE_DIR = Path(__file__).parent
PIPELINE_DIR = STYLE_DIR.parent
DEFAULT_CONFIG_PATH = STYLE_DIR / "bible" / "style_config.json"


@dataclass
class StyleResult:
    """Result of a style consistency check."""

    asset_id: Optional[str] = None
    result: str = "pending"  # pass / warn / fail
    score: float = 0.0
    issues: list[str] = field(default_factory=list)
    details: dict = field(default_factory=dict)

    def to_dict(self) -> dict:
        return {
            "asset_id": self.asset_id,
            "result": self.result,
            "score": self.score,
            "issues": self.issues,
            "details": self.details,
        }


class StyleChecker:
    """Unified style checker that delegates to specialized sub-checkers."""

    # Asset types routed to image checking.
    _IMAGE_TYPES: set[str] = {"concept_art", "texture", "ui", "environment"}

    # Asset types routed to mesh checking.
    _MESH_TYPES: set[str] = {"3d_mesh", "mesh", "character", "vehicle", "weapon", "foliage"}

    # Asset types routed to audio checking.
    _AUDIO_TYPES: set[str] = {"music", "sfx", "audio"}

    def __init__(self, config_path: str | Path | None = None) -> None:
        config_file = Path(config_path) if config_path else DEFAULT_CONFIG_PATH
        with open(config_file, "r") as fh:
            self.config: dict = json.load(fh)

        # Lazily instantiated sub-checkers.
        self._clip_scorer: Optional[object] = None
        self._palette_checker: Optional[object] = None
        self._mesh_checker: Optional[object] = None
        self._audio_checker: Optional[object] = None

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def check(
        self,
        asset_type: str,
        file_path: str,
        metadata: dict | None = None,
    ) -> StyleResult:
        """Route to the correct sub-checker based on *asset_type*."""

        asset_type_lower = asset_type.lower()

        if asset_type_lower in self._IMAGE_TYPES:
            return self.check_image(file_path)
        elif asset_type_lower in self._MESH_TYPES:
            return self.check_mesh(file_path, metadata)
        elif asset_type_lower in self._AUDIO_TYPES:
            return self.check_audio(file_path, metadata)
        else:
            return StyleResult(
                result="warn",
                score=0.5,
                issues=[f"No style checker defined for asset type '{asset_type}'"],
            )

    def check_image(self, file_path: str) -> StyleResult:
        """Run palette check and optional CLIP scoring on an image asset."""

        issues: list[str] = []
        details: dict = {}
        scores: list[float] = []

        # --- Palette check ---------------------------------------------------
        from .palette_checker import PaletteChecker

        if self._palette_checker is None:
            self._palette_checker = PaletteChecker(self.config.get("palette", {}))

        palette_checker: PaletteChecker = self._palette_checker  # type: ignore[assignment]
        palette_score, palette_issues = palette_checker.check(file_path)
        issues.extend(palette_issues)
        scores.append(palette_score)
        details["palette_score"] = palette_score

        # --- CLIP scoring -----------------------------------------------------
        from .clip_scorer import CLIPScorer

        if self._clip_scorer is None:
            ref_dir = STYLE_DIR / "bible" / "references"
            self._clip_scorer = CLIPScorer(reference_dir=ref_dir)

        clip_scorer: CLIPScorer = self._clip_scorer  # type: ignore[assignment]
        clip_score = clip_scorer.score_image(file_path)
        scores.append(clip_score)
        details["clip_score"] = clip_score

        if not clip_scorer.available:
            issues.append("CLIP model unavailable -- returned default score 0.5")
        else:
            threshold = self.config.get("clip_threshold", 0.75)
            if clip_score < threshold:
                issues.append(
                    f"CLIP similarity {clip_score:.3f} below threshold {threshold}"
                )

        combined_score = sum(scores) / len(scores) if scores else 0.0
        result_str = self._determine_result(combined_score, issues)

        return StyleResult(
            result=result_str,
            score=round(combined_score, 4),
            issues=issues,
            details=details,
        )

    def check_mesh(self, file_path: str, metadata: dict | None = None) -> StyleResult:
        """Delegate to :class:`MeshChecker`."""

        from .mesh_checker import MeshChecker

        if self._mesh_checker is None:
            self._mesh_checker = MeshChecker(self.config)

        mesh_checker: MeshChecker = self._mesh_checker  # type: ignore[assignment]
        score, issues = mesh_checker.check(file_path, metadata)
        result_str = self._determine_result(score, issues)

        return StyleResult(
            result=result_str,
            score=round(score, 4),
            issues=issues,
            details={"mesh_score": score},
        )

    def check_audio(self, file_path: str, metadata: dict | None = None) -> StyleResult:
        """Delegate to :class:`AudioChecker`."""

        from .audio_checker import AudioChecker

        if self._audio_checker is None:
            self._audio_checker = AudioChecker(self.config.get("audio", {}))

        audio_checker: AudioChecker = self._audio_checker  # type: ignore[assignment]
        score, issues = audio_checker.check(file_path, metadata)
        result_str = self._determine_result(score, issues)

        return StyleResult(
            result=result_str,
            score=round(score, 4),
            issues=issues,
            details={"audio_score": score},
        )

    def check_all_pending(self, inventory_db: object) -> list[StyleResult]:
        """Check all assets whose ``style_check_result`` is ``'pending'``.

        *inventory_db* must expose ``list_assets`` and ``update_style_check``
        methods compatible with :class:`pipeline.inventory.db.InventoryDB`.
        """

        db = inventory_db  # type: ignore[assignment]
        pending_assets = db.list_assets(status="generated")  # type: ignore[attr-defined]

        results: list[StyleResult] = []
        for asset in pending_assets:
            if getattr(asset, "style_check_result", None) != "pending":
                continue

            file_path: str = getattr(asset, "file_path", "") or ""
            asset_type: str = getattr(asset, "type", "")
            asset_id: str = getattr(asset, "id", "")

            style_result = self.check(asset_type, file_path)
            style_result.asset_id = asset_id

            # Persist the result back to the database.
            try:
                db.update_style_check(asset_id, style_result.result, style_result.score)  # type: ignore[attr-defined]
                if style_result.result == "fail":
                    db.quarantine(  # type: ignore[attr-defined]
                        asset_id,
                        reason="; ".join(style_result.issues),
                    )
            except Exception as exc:
                logger.warning("Failed to persist style result for %s: %s", asset_id, exc)

            results.append(style_result)

        return results

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _determine_result(self, score: float, issues: list[str]) -> str:
        """Return ``'pass'``, ``'warn'``, or ``'fail'`` based on score and issues.

        Thresholds:
        - score >= 0.8 and no FAIL-level issues  -> ``'pass'``
        - score >= 0.5 or only WARN-level issues  -> ``'warn'``
        - otherwise                                -> ``'fail'``
        """

        has_fail = any("FAIL" in issue.upper() or "below threshold" in issue.lower() for issue in issues)

        if has_fail or score < 0.5:
            return "fail"
        if issues or score < 0.8:
            return "warn"
        return "pass"
