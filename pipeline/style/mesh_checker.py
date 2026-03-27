"""3D mesh / asset validation -- poly counts, texture resolution, naming conventions."""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Optional

logger = logging.getLogger(__name__)

# Allowed mesh file extensions.
_ALLOWED_EXTENSIONS: set[str] = {".fbx", ".obj", ".uasset"}


class MeshChecker:
    """Validate 3D assets against poly budgets, texture resolution limits and naming rules."""

    def __init__(self, config: dict) -> None:
        self.poly_budget: dict[str, int] = config.get("poly_budget", {})
        self.texture_resolution: dict[str, int] = config.get("texture_resolution", {})
        self.naming_conventions: dict[str, str] = config.get("naming_conventions", {})

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
        - ``poly_count`` (int)
        - ``asset_category`` (str) -- key into ``poly_budget``
        - ``texture_width`` / ``texture_height`` (int)
        - ``is_hero`` (bool)
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
                f"FAIL: File extension '{ext}' not allowed -- expected one of {sorted(_ALLOWED_EXTENSIONS)}"
            )

        # --- Naming convention ------------------------------------------------
        checks_total += 1
        name = Path(file_path).stem
        expected_prefix = self.naming_conventions.get("mesh_prefix", "SM_")
        if name.startswith(expected_prefix):
            checks_passed += 1
        else:
            issues.append(
                f"WARN: Mesh name '{name}' should start with '{expected_prefix}'"
            )

        # --- Poly budget ------------------------------------------------------
        poly_count: Optional[int] = meta.get("poly_count")
        asset_category: Optional[str] = meta.get("asset_category")

        if poly_count is not None and asset_category is not None:
            checks_total += 1
            budget = self.poly_budget.get(asset_category)
            if budget is not None:
                if poly_count <= budget:
                    checks_passed += 1
                elif poly_count <= budget * 1.1:
                    issues.append(
                        f"WARN: Poly count {poly_count:,} near budget limit {budget:,} "
                        f"for category '{asset_category}'"
                    )
                    # Half credit for being close.
                    checks_passed += 0.5
                else:
                    issues.append(
                        f"FAIL: Poly count {poly_count:,} exceeds budget {budget:,} "
                        f"for category '{asset_category}'"
                    )
            else:
                # Unknown category -- pass with a note.
                checks_passed += 1
                issues.append(
                    f"WARN: No poly budget defined for category '{asset_category}'"
                )

        # --- Texture resolution -----------------------------------------------
        tex_w: Optional[int] = meta.get("texture_width")
        tex_h: Optional[int] = meta.get("texture_height")

        if tex_w is not None and tex_h is not None:
            checks_total += 1
            tex_min: int = self.texture_resolution.get("min", 512)
            tex_max: int = self.texture_resolution.get("max", 4096)
            hero_min: int = self.texture_resolution.get("hero_min", 2048)
            is_hero: bool = meta.get("is_hero", False)

            resolution_ok = True
            if tex_w < tex_min or tex_h < tex_min:
                issues.append(
                    f"FAIL: Texture resolution {tex_w}x{tex_h} below minimum {tex_min}"
                )
                resolution_ok = False
            if tex_w > tex_max or tex_h > tex_max:
                issues.append(
                    f"FAIL: Texture resolution {tex_w}x{tex_h} exceeds maximum {tex_max}"
                )
                resolution_ok = False
            if is_hero and (tex_w < hero_min or tex_h < hero_min):
                issues.append(
                    f"WARN: Hero asset texture {tex_w}x{tex_h} below hero minimum {hero_min}"
                )
                resolution_ok = False

            # Check power-of-two.
            if (tex_w & (tex_w - 1)) != 0 or (tex_h & (tex_h - 1)) != 0:
                issues.append(
                    f"WARN: Texture {tex_w}x{tex_h} is not power-of-two"
                )

            if resolution_ok:
                checks_passed += 1

        # --- Compute score ----------------------------------------------------
        score = checks_passed / checks_total if checks_total > 0 else 1.0
        score = max(0.0, min(1.0, score))

        return round(score, 4), issues
