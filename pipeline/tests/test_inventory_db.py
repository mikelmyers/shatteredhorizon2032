"""Tests for pipeline.inventory.db — SQLite inventory backend."""

import json
import os
import tempfile
import unittest

from pipeline.inventory.db import InventoryDB
from pipeline.inventory.models import Asset


class TestInventoryDB(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
        self.tmp.close()
        self.db = InventoryDB(db_path=self.tmp.name)

    def tearDown(self):
        self.db.conn.close()
        os.unlink(self.tmp.name)

    # ------------------------------------------------------------------
    # Insert / Get
    # ------------------------------------------------------------------

    def test_insert_and_get(self):
        asset = Asset(
            id="a1", type="concept_art", name="FOB", prompt="test prompt",
            tool_used="stable_diffusion", tool_tier="free", cost_usd=0.0,
            status="generated", tags="exterior,military",
        )
        self.db.insert(asset)
        fetched = self.db.get("a1")
        self.assertIsNotNone(fetched)
        self.assertEqual(fetched.id, "a1")
        self.assertEqual(fetched.type, "concept_art")
        self.assertEqual(fetched.name, "FOB")
        self.assertEqual(fetched.tags, "exterior,military")

    def test_get_nonexistent(self):
        self.assertIsNone(self.db.get("does-not-exist"))

    def test_insert_replaces_on_conflict(self):
        a1 = Asset(id="a1", type="concept_art", name="v1")
        self.db.insert(a1)
        a1_v2 = Asset(id="a1", type="concept_art", name="v2")
        self.db.insert(a1_v2)
        fetched = self.db.get("a1")
        self.assertEqual(fetched.name, "v2")

    # ------------------------------------------------------------------
    # Update
    # ------------------------------------------------------------------

    def test_update_fields(self):
        self.db.insert(Asset(id="a1", type="sfx", status="generated"))
        self.db.update("a1", status="approved", notes="looks good")
        fetched = self.db.get("a1")
        self.assertEqual(fetched.status, "approved")
        self.assertEqual(fetched.notes, "looks good")
        self.assertIsNotNone(fetched.updated_at)

    def test_update_no_kwargs_is_noop(self):
        self.db.insert(Asset(id="a1", type="sfx"))
        self.db.update("a1")  # should not raise

    # ------------------------------------------------------------------
    # list_assets with filters
    # ------------------------------------------------------------------

    def test_list_assets_all(self):
        for i in range(5):
            self.db.insert(Asset(id=f"a{i}", type="sfx", status="generated"))
        result = self.db.list_assets()
        self.assertEqual(len(result), 5)

    def test_list_assets_by_type(self):
        self.db.insert(Asset(id="a1", type="concept_art"))
        self.db.insert(Asset(id="a2", type="sfx"))
        self.db.insert(Asset(id="a3", type="concept_art"))
        result = self.db.list_assets(type="concept_art")
        self.assertEqual(len(result), 2)

    def test_list_assets_by_status(self):
        self.db.insert(Asset(id="a1", type="sfx", status="generated"))
        self.db.insert(Asset(id="a2", type="sfx", status="approved"))
        result = self.db.list_assets(status="approved")
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].id, "a2")

    def test_list_assets_by_tag(self):
        self.db.insert(Asset(id="a1", type="sfx", tags="weapon,hero"))
        self.db.insert(Asset(id="a2", type="sfx", tags="ambient"))
        result = self.db.list_assets(tag="weapon")
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].id, "a1")

    def test_list_assets_limit_offset(self):
        for i in range(10):
            self.db.insert(Asset(id=f"a{i:02d}", type="sfx"))
        result = self.db.list_assets(limit=3, offset=0)
        self.assertEqual(len(result), 3)

    # ------------------------------------------------------------------
    # Status helpers
    # ------------------------------------------------------------------

    def test_approve(self):
        self.db.insert(Asset(id="a1", type="sfx", status="generated"))
        self.db.approve("a1")
        self.assertEqual(self.db.get("a1").status, "approved")

    def test_quarantine(self):
        self.db.insert(Asset(id="a1", type="sfx", status="generated"))
        self.db.quarantine("a1", "palette violation")
        fetched = self.db.get("a1")
        self.assertEqual(fetched.status, "quarantined")
        self.assertEqual(fetched.style_check_result, "fail")
        self.assertEqual(fetched.notes, "palette violation")

    def test_update_style_check(self):
        self.db.insert(Asset(id="a1", type="sfx"))
        self.db.update_style_check("a1", "pass", 0.92)
        fetched = self.db.get("a1")
        self.assertEqual(fetched.style_check_result, "pass")
        self.assertAlmostEqual(fetched.style_check_score, 0.92)

    # ------------------------------------------------------------------
    # Stats
    # ------------------------------------------------------------------

    def test_get_stats_empty(self):
        stats = self.db.get_stats()
        self.assertEqual(stats["total"], 0)
        self.assertEqual(stats["total_cost"], 0)
        self.assertEqual(stats["style_pass_rate"], 0.0)

    def test_get_stats_populated(self):
        self.db.insert(Asset(id="a1", type="concept_art", status="approved",
                             tool_tier="free", cost_usd=0, style_check_result="pass"))
        self.db.insert(Asset(id="a2", type="sfx", status="generated",
                             tool_tier="paid", cost_usd=5.0, style_check_result="fail"))
        self.db.insert(Asset(id="a3", type="sfx", status="approved",
                             tool_tier="free", cost_usd=0, style_check_result="pass"))
        stats = self.db.get_stats()
        self.assertEqual(stats["total"], 3)
        self.assertEqual(stats["by_type"]["concept_art"], 1)
        self.assertEqual(stats["by_type"]["sfx"], 2)
        self.assertEqual(stats["by_status"]["approved"], 2)
        self.assertEqual(stats["by_tier"]["free"], 2)
        self.assertEqual(stats["by_tier"]["paid"], 1)
        self.assertAlmostEqual(stats["style_pass_rate"], 66.66, places=1)
        self.assertEqual(stats["total_cost"], 5.0)

    # ------------------------------------------------------------------
    # Export
    # ------------------------------------------------------------------

    def test_export_json(self):
        self.db.insert(Asset(id="a1", type="sfx", name="boom"))
        out = self.db.export("json")
        data = json.loads(out)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["id"], "a1")

    def test_export_csv(self):
        self.db.insert(Asset(id="a1", type="sfx", name="boom"))
        out = self.db.export("csv")
        self.assertIn("a1", out)
        self.assertIn("sfx", out)
        # CSV should have a header
        lines = out.strip().split("\n")
        self.assertGreaterEqual(len(lines), 2)

    def test_export_unknown_format_raises(self):
        with self.assertRaises(ValueError):
            self.db.export("xml")

    def test_export_empty_csv(self):
        out = self.db.export("csv")
        self.assertEqual(out, "")

    # ------------------------------------------------------------------
    # Search
    # ------------------------------------------------------------------

    def test_search_by_name(self):
        self.db.insert(Asset(id="a1", type="sfx", name="M4 Carbine Fire"))
        self.db.insert(Asset(id="a2", type="sfx", name="Explosion Large"))
        result = self.db.search("carbine")
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].id, "a1")

    def test_search_by_prompt(self):
        self.db.insert(Asset(id="a1", type="concept_art", prompt="FOB at dusk"))
        result = self.db.search("dusk")
        self.assertEqual(len(result), 1)

    def test_search_by_tags(self):
        self.db.insert(Asset(id="a1", type="sfx", tags="weapon,hero"))
        result = self.db.search("hero")
        self.assertEqual(len(result), 1)

    def test_search_no_results(self):
        self.db.insert(Asset(id="a1", type="sfx", name="test"))
        result = self.db.search("nonexistent")
        self.assertEqual(len(result), 0)


if __name__ == "__main__":
    unittest.main()
