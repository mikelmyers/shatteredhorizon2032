"""Tests for pipeline.style.checker — style routing and result determination."""

import json
import os
import tempfile
import unittest

from pipeline.style.checker import StyleChecker, StyleResult


class TestStyleResult(unittest.TestCase):
    def test_default_values(self):
        r = StyleResult()
        self.assertIsNone(r.asset_id)
        self.assertEqual(r.result, "pending")
        self.assertEqual(r.score, 0.0)
        self.assertEqual(r.issues, [])
        self.assertEqual(r.details, {})

    def test_to_dict(self):
        r = StyleResult(asset_id="a1", result="pass", score=0.92,
                        issues=[], details={"clip_score": 0.92})
        d = r.to_dict()
        self.assertEqual(d["asset_id"], "a1")
        self.assertEqual(d["result"], "pass")
        self.assertEqual(d["score"], 0.92)


class TestDetermineResult(unittest.TestCase):
    """Test the _determine_result helper in isolation."""

    def setUp(self):
        self.checker = StyleChecker()

    def test_pass_high_score_no_issues(self):
        self.assertEqual(self.checker._determine_result(0.9, []), "pass")

    def test_pass_exact_threshold(self):
        self.assertEqual(self.checker._determine_result(0.8, []), "pass")

    def test_warn_below_pass_threshold(self):
        self.assertEqual(self.checker._determine_result(0.7, []), "warn")

    def test_warn_with_minor_issues(self):
        self.assertEqual(self.checker._determine_result(0.85, ["minor note"]), "warn")

    def test_fail_low_score(self):
        self.assertEqual(self.checker._determine_result(0.3, []), "fail")

    def test_fail_with_threshold_issue(self):
        self.assertEqual(
            self.checker._determine_result(0.9, ["CLIP similarity 0.60 below threshold 0.75"]),
            "fail"
        )

    def test_fail_with_fail_keyword(self):
        self.assertEqual(
            self.checker._determine_result(0.9, ["FAIL: naming convention violated"]),
            "fail"
        )

    def test_fail_score_zero(self):
        self.assertEqual(self.checker._determine_result(0.0, []), "fail")

    def test_warn_at_boundary(self):
        self.assertEqual(self.checker._determine_result(0.5, []), "warn")


class TestStyleCheckerRouting(unittest.TestCase):
    """Test that check() routes to the correct sub-checker."""

    def setUp(self):
        self.checker = StyleChecker()

    def test_routes_concept_art_to_image(self):
        # Will run image check (palette + CLIP), both degrade gracefully
        result = self.checker.check("concept_art", "/nonexistent/file.png")
        self.assertIsInstance(result, StyleResult)
        self.assertIn(result.result, ("pass", "warn", "fail"))

    def test_routes_texture_to_image(self):
        result = self.checker.check("texture", "/nonexistent/file.png")
        self.assertIsInstance(result, StyleResult)

    def test_routes_3d_mesh(self):
        result = self.checker.check("3d_mesh", "/nonexistent/file.fbx",
                                    metadata={"poly_count": 5000, "category": "environment_prop"})
        self.assertIsInstance(result, StyleResult)

    def test_routes_music(self):
        result = self.checker.check("music", "/nonexistent/track.wav",
                                    metadata={"bpm": 90, "mood": "tense", "key": "Cm"})
        self.assertIsInstance(result, StyleResult)

    def test_routes_sfx(self):
        result = self.checker.check("sfx", "/nonexistent/sfx.wav",
                                    metadata={"mood": "tense"})
        self.assertIsInstance(result, StyleResult)

    def test_unknown_type_returns_warn(self):
        result = self.checker.check("hologram", "/nonexistent/file.xyz")
        self.assertEqual(result.result, "warn")
        self.assertEqual(result.score, 0.5)
        self.assertIn("No style checker defined", result.issues[0])


class TestMeshChecker(unittest.TestCase):
    """Test mesh checking with metadata."""

    def setUp(self):
        self.checker = StyleChecker()

    def test_mesh_within_budget(self):
        result = self.checker.check_mesh(
            "SM_prop_barrel.fbx",
            metadata={"poly_count": 10000, "asset_category": "environment_prop"}
        )
        self.assertIsInstance(result, StyleResult)
        # Should pass or warn — the file doesn't exist but metadata is checked
        self.assertIn(result.result, ("pass", "warn", "fail"))

    def test_mesh_over_budget(self):
        result = self.checker.check_mesh(
            "SM_building.fbx",
            metadata={"poly_count": 999999, "asset_category": "environment_prop"}
        )
        self.assertIsInstance(result, StyleResult)
        # Extremely over budget should fail
        self.assertIn("poly", " ".join(result.issues).lower())

    def test_mesh_bad_naming(self):
        result = self.checker.check_mesh(
            "barrel.obj",  # no SM_ prefix
            metadata={"poly_count": 5000, "category": "environment_prop"}
        )
        self.assertIsInstance(result, StyleResult)

    def test_mesh_no_metadata(self):
        result = self.checker.check_mesh("SM_test.fbx", metadata=None)
        self.assertIsInstance(result, StyleResult)


class TestAudioChecker(unittest.TestCase):
    """Test audio checking with metadata."""

    def setUp(self):
        self.checker = StyleChecker()

    def test_audio_valid_metadata(self):
        result = self.checker.check_audio(
            "ambient_rain.wav",
            metadata={"bpm": 80, "mood": "ambient", "key": "Dm"}
        )
        self.assertIsInstance(result, StyleResult)

    def test_audio_bpm_out_of_range(self):
        result = self.checker.check_audio(
            "track.wav",
            metadata={"bpm": 200, "mood": "tense"}  # 200 > 140 max
        )
        self.assertIsInstance(result, StyleResult)
        self.assertIn("bpm", " ".join(result.issues).lower())

    def test_audio_unapproved_mood(self):
        result = self.checker.check_audio(
            "track.wav",
            metadata={"bpm": 90, "mood": "happy"}  # "happy" not in approved
        )
        self.assertIsInstance(result, StyleResult)

    def test_audio_no_metadata(self):
        result = self.checker.check_audio("test.wav", metadata=None)
        self.assertIsInstance(result, StyleResult)

    def test_audio_bad_extension(self):
        result = self.checker.check_audio("track.aac", metadata={"bpm": 90})
        self.assertIsInstance(result, StyleResult)


class TestCheckAllPending(unittest.TestCase):
    """Test check_all_pending integration with a mock InventoryDB."""

    def setUp(self):
        self.checker = StyleChecker()

    def test_check_all_pending_with_mock_db(self):
        """Use a real InventoryDB with in-memory-like temp file."""

        class MockAsset:
            def __init__(self, id, type, file_path, style_check_result, status):
                self.id = id
                self.type = type
                self.file_path = file_path
                self.style_check_result = style_check_result
                self.status = status

        class MockDB:
            def __init__(self):
                self.updated = {}
                self.quarantined = {}

            def list_assets(self, status=None):
                return [
                    MockAsset("a1", "concept_art", "/fake/img.png", "pending", "generated"),
                    MockAsset("a2", "music", "/fake/track.wav", "pending", "generated"),
                    MockAsset("a3", "sfx", "/fake/sfx.wav", "pass", "generated"),  # already checked
                ]

            def update_style_check(self, asset_id, result, score):
                self.updated[asset_id] = {"result": result, "score": score}

            def quarantine(self, asset_id, reason=""):
                self.quarantined[asset_id] = reason

        db = MockDB()
        results = self.checker.check_all_pending(db)
        # Should only process a1 and a2 (a3 has result != "pending")
        self.assertEqual(len(results), 2)
        self.assertIn("a1", db.updated)
        self.assertIn("a2", db.updated)
        self.assertNotIn("a3", db.updated)


if __name__ == "__main__":
    unittest.main()
