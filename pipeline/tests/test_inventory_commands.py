"""Tests for pipeline.inventory.commands — pure CLI command handlers."""

import os
import tempfile
import unittest

from pipeline.inventory.db import InventoryDB
from pipeline.inventory.models import Asset
from pipeline.inventory.commands import (
    list_assets,
    show_asset,
    approve_asset,
    quarantine_asset,
    export_assets,
    get_stats,
)


class TestInventoryCommands(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
        self.tmp.close()
        self.db = InventoryDB(db_path=self.tmp.name)
        # Seed data
        self.db.insert(Asset(id="a1", type="concept_art", name="FOB", status="generated",
                             tool_tier="free", cost_usd=0, style_check_result="pending"))
        self.db.insert(Asset(id="a2", type="sfx", name="Boom", status="approved",
                             tool_tier="paid", cost_usd=5.0, style_check_result="pass"))

    def tearDown(self):
        self.db.conn.close()
        os.unlink(self.tmp.name)

    def test_list_assets_returns_dicts(self):
        result = list_assets(self.db)
        self.assertEqual(len(result), 2)
        self.assertIsInstance(result[0], dict)
        self.assertIn("id", result[0])

    def test_list_assets_filtered(self):
        result = list_assets(self.db, type="sfx")
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["type"], "sfx")

    def test_show_asset_found(self):
        result = show_asset(self.db, "a1")
        self.assertIsNotNone(result)
        self.assertEqual(result["name"], "FOB")

    def test_show_asset_not_found(self):
        result = show_asset(self.db, "nonexistent")
        self.assertIsNone(result)

    def test_approve_asset(self):
        msg = approve_asset(self.db, "a1")
        self.assertIn("approved", msg)
        self.assertEqual(self.db.get("a1").status, "approved")

    def test_approve_nonexistent(self):
        msg = approve_asset(self.db, "nope")
        self.assertIn("not found", msg)

    def test_quarantine_asset(self):
        msg = quarantine_asset(self.db, "a1", "too bright")
        self.assertIn("quarantined", msg)
        self.assertEqual(self.db.get("a1").status, "quarantined")
        self.assertEqual(self.db.get("a1").notes, "too bright")

    def test_quarantine_nonexistent(self):
        msg = quarantine_asset(self.db, "nope", "reason")
        self.assertIn("not found", msg)

    def test_export_assets_json(self):
        out = export_assets(self.db, "json")
        import json
        data = json.loads(out)
        self.assertEqual(len(data), 2)

    def test_get_stats(self):
        stats = get_stats(self.db)
        self.assertEqual(stats["total"], 2)
        self.assertEqual(stats["total_cost"], 5.0)


if __name__ == "__main__":
    unittest.main()
