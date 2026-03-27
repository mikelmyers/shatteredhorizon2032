"""Tests for pipeline.inventory.models — Asset and AssetRequest dataclasses."""

import unittest
import uuid

from pipeline.inventory.models import (
    ASSET_TYPES,
    ASSET_STATUSES,
    PRIORITIES,
    STYLE_CHECK_RESULTS,
    Asset,
    AssetRequest,
)


class TestAsset(unittest.TestCase):
    def test_default_fields(self):
        a = Asset()
        self.assertTrue(a.id)  # UUID generated
        self.assertEqual(a.type, "")
        self.assertEqual(a.status, "generated")
        self.assertEqual(a.cost_usd, 0.0)
        self.assertIsNotNone(a.created_at)
        self.assertIsNotNone(a.updated_at)

    def test_to_dict_round_trip(self):
        a = Asset(id="test-1", type="concept_art", name="FOB Dusk", cost_usd=0.12)
        d = a.to_dict()
        self.assertEqual(d["id"], "test-1")
        self.assertEqual(d["type"], "concept_art")
        self.assertEqual(d["name"], "FOB Dusk")
        self.assertEqual(d["cost_usd"], 0.12)
        self.assertIn("created_at", d)

    def test_to_dict_has_all_fields(self):
        a = Asset()
        d = a.to_dict()
        expected_keys = {
            "id", "type", "name", "prompt", "tool_used", "tool_tier",
            "cost_usd", "file_path", "style_check_result", "style_check_score",
            "status", "tags", "created_at", "updated_at", "notes",
        }
        self.assertEqual(set(d.keys()), expected_keys)

    def test_unique_ids(self):
        a1 = Asset()
        a2 = Asset()
        self.assertNotEqual(a1.id, a2.id)


class TestAssetRequest(unittest.TestCase):
    def test_from_dict_minimal(self):
        req = AssetRequest.from_dict({"type": "concept_art", "prompt": "test"})
        self.assertTrue(req.id)
        self.assertEqual(req.type, "concept_art")
        self.assertEqual(req.prompt, "test")
        self.assertFalse(req.force_paid)
        self.assertEqual(req.priority, "normal")
        self.assertEqual(req.tags, [])

    def test_from_dict_full(self):
        req = AssetRequest.from_dict({
            "id": "req-1",
            "type": "3d_mesh",
            "prompt": "M4 carbine",
            "style_refs": ["abc"],
            "priority": "high",
            "force_paid": True,
            "tags": ["weapon", "hero"],
        })
        self.assertEqual(req.id, "req-1")
        self.assertTrue(req.force_paid)
        self.assertEqual(req.priority, "high")
        self.assertEqual(req.tags, ["weapon", "hero"])

    def test_to_dict(self):
        req = AssetRequest(type="sfx", prompt="explosion")
        d = req.to_dict()
        self.assertEqual(d["type"], "sfx")
        self.assertEqual(d["prompt"], "explosion")
        self.assertIn("id", d)

    def test_from_dict_generates_uuid_if_empty(self):
        req = AssetRequest.from_dict({"type": "ui", "id": ""})
        self.assertTrue(req.id)
        # Should be a valid UUID
        uuid.UUID(req.id)


class TestConstants(unittest.TestCase):
    def test_asset_types_coverage(self):
        expected = {"concept_art", "3d_mesh", "texture", "character",
                    "environment", "vfx", "music", "sfx", "ui"}
        self.assertEqual(set(ASSET_TYPES), expected)

    def test_statuses(self):
        self.assertIn("generated", ASSET_STATUSES)
        self.assertIn("approved", ASSET_STATUSES)
        self.assertIn("quarantined", ASSET_STATUSES)

    def test_priorities(self):
        self.assertEqual(PRIORITIES, ["low", "normal", "high"])

    def test_style_results(self):
        self.assertIn("pass", STYLE_CHECK_RESULTS)
        self.assertIn("pending", STYLE_CHECK_RESULTS)


if __name__ == "__main__":
    unittest.main()
