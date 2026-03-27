"""Tests for pipeline.style.mesh_checker — 3D asset validation."""

import unittest

from pipeline.style.mesh_checker import MeshChecker


class TestMeshChecker(unittest.TestCase):
    def setUp(self):
        self.config = {
            "poly_budget": {
                "hero_character": 80000,
                "environment_prop": 15000,
                "vehicle": 120000,
                "weapon": 25000,
            },
            "texture_resolution": {
                "min": 512,
                "max": 4096,
                "hero_min": 2048,
            },
            "naming_conventions": {
                "mesh_prefix": "SM_",
            },
        }
        self.checker = MeshChecker(self.config)

    # ------------------------------------------------------------------
    # Extension
    # ------------------------------------------------------------------

    def test_fbx_extension_passes(self):
        score, issues = self.checker.check("SM_prop.fbx")
        fail_issues = [i for i in issues if i.startswith("FAIL") and "extension" in i.lower()]
        self.assertEqual(len(fail_issues), 0)

    def test_obj_extension_passes(self):
        score, issues = self.checker.check("SM_prop.obj")
        fail_issues = [i for i in issues if i.startswith("FAIL") and "extension" in i.lower()]
        self.assertEqual(len(fail_issues), 0)

    def test_bad_extension_fails(self):
        score, issues = self.checker.check("SM_prop.stl")
        fail_issues = [i for i in issues if "extension" in i.lower()]
        self.assertGreater(len(fail_issues), 0)

    # ------------------------------------------------------------------
    # Naming
    # ------------------------------------------------------------------

    def test_correct_naming_prefix(self):
        score, issues = self.checker.check("SM_barrel.fbx")
        naming_issues = [i for i in issues if "name" in i.lower() or "prefix" in i.lower()]
        self.assertEqual(len(naming_issues), 0)

    def test_missing_prefix_warns(self):
        score, issues = self.checker.check("barrel.fbx")
        naming_issues = [i for i in issues if "SM_" in i]
        self.assertGreater(len(naming_issues), 0)

    # ------------------------------------------------------------------
    # Poly budget
    # ------------------------------------------------------------------

    def test_within_poly_budget(self):
        score, issues = self.checker.check(
            "SM_barrel.fbx",
            metadata={"poly_count": 10000, "asset_category": "environment_prop"}
        )
        self.assertGreater(score, 0.8)

    def test_near_poly_budget_warns(self):
        # 15000 * 1.1 = 16500. 16000 is within 1.1x.
        score, issues = self.checker.check(
            "SM_barrel.fbx",
            metadata={"poly_count": 16000, "asset_category": "environment_prop"}
        )
        poly_issues = [i for i in issues if "poly" in i.lower()]
        self.assertGreater(len(poly_issues), 0)
        self.assertIn("WARN", poly_issues[0])

    def test_over_poly_budget_fails(self):
        score, issues = self.checker.check(
            "SM_barrel.fbx",
            metadata={"poly_count": 999999, "asset_category": "environment_prop"}
        )
        poly_issues = [i for i in issues if "poly" in i.lower()]
        self.assertGreater(len(poly_issues), 0)
        self.assertIn("FAIL", poly_issues[0])

    def test_unknown_category_passes_with_note(self):
        score, issues = self.checker.check(
            "SM_thing.fbx",
            metadata={"poly_count": 5000, "asset_category": "unknown_cat"}
        )
        # Should pass but warn about unknown category
        cat_issues = [i for i in issues if "category" in i.lower()]
        self.assertGreater(len(cat_issues), 0)

    # ------------------------------------------------------------------
    # Texture resolution
    # ------------------------------------------------------------------

    def test_texture_within_range(self):
        score, issues = self.checker.check(
            "SM_prop.fbx",
            metadata={"texture_width": 2048, "texture_height": 2048}
        )
        tex_issues = [i for i in issues if "texture" in i.lower() and "FAIL" in i]
        self.assertEqual(len(tex_issues), 0)

    def test_texture_below_min_fails(self):
        score, issues = self.checker.check(
            "SM_prop.fbx",
            metadata={"texture_width": 128, "texture_height": 128}
        )
        tex_issues = [i for i in issues if "below minimum" in i.lower()]
        self.assertGreater(len(tex_issues), 0)

    def test_texture_above_max_fails(self):
        score, issues = self.checker.check(
            "SM_prop.fbx",
            metadata={"texture_width": 8192, "texture_height": 8192}
        )
        tex_issues = [i for i in issues if "exceeds maximum" in i.lower()]
        self.assertGreater(len(tex_issues), 0)

    def test_hero_texture_below_hero_min_warns(self):
        score, issues = self.checker.check(
            "SM_hero.fbx",
            metadata={"texture_width": 1024, "texture_height": 1024, "is_hero": True}
        )
        hero_issues = [i for i in issues if "hero" in i.lower()]
        self.assertGreater(len(hero_issues), 0)

    def test_non_power_of_two_warns(self):
        score, issues = self.checker.check(
            "SM_prop.fbx",
            metadata={"texture_width": 1000, "texture_height": 1000}
        )
        pot_issues = [i for i in issues if "power-of-two" in i.lower()]
        self.assertGreater(len(pot_issues), 0)

    # ------------------------------------------------------------------
    # Score computation
    # ------------------------------------------------------------------

    def test_perfect_score(self):
        score, issues = self.checker.check(
            "SM_barrel.fbx",
            metadata={
                "poly_count": 5000, "asset_category": "environment_prop",
                "texture_width": 2048, "texture_height": 2048,
            }
        )
        self.assertEqual(score, 1.0)
        fail_issues = [i for i in issues if "FAIL" in i]
        self.assertEqual(len(fail_issues), 0)

    def test_no_metadata_still_scores(self):
        score, issues = self.checker.check("SM_prop.fbx")
        # Only extension + naming checked
        self.assertGreater(score, 0.0)


if __name__ == "__main__":
    unittest.main()
