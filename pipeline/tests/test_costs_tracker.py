"""Tests for pipeline.costs.tracker — budget enforcement and spend tracking."""

import json
import os
import shutil
import tempfile
import unittest

from pipeline.costs.tracker import CostTracker


class TestCostTracker(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        # Write a minimal tools.json
        self.tools_config = {
            "tools": [
                {"id": "stable_diffusion", "name": "Stable Diffusion (local)", "tier": "free", "monthly_cap": 0, "cost_per_call": 0},
                {"id": "leonardo", "name": "Leonardo.ai", "tier": "freemium", "monthly_cap": 24.00, "cost_per_call": 0},
                {"id": "midjourney", "name": "Midjourney", "tier": "paid", "monthly_cap": 30.00, "cost_per_call": 0},
                {"id": "suno", "name": "Suno", "tier": "freemium", "monthly_cap": 10.00, "cost_per_call": 0},
            ],
            "total_monthly_budget": 100.00,
            "alert_threshold_pct": 80,
        }
        self.config_path = os.path.join(self.tmpdir, "tools.json")
        with open(self.config_path, "w") as f:
            json.dump(self.tools_config, f)

        self.db_path = os.path.join(self.tmpdir, "ledger.db")
        self.tracker = CostTracker(db_path=self.db_path, tools_config_path=self.config_path)

    def tearDown(self):
        self.tracker.conn.close()
        shutil.rmtree(self.tmpdir)

    # ------------------------------------------------------------------
    # Config loading
    # ------------------------------------------------------------------

    def test_config_loaded(self):
        self.assertEqual(self.tracker.total_monthly_budget, 100.00)
        self.assertEqual(self.tracker.alert_threshold_pct, 80)
        self.assertIn("stable_diffusion", self.tracker.tools_by_id)
        self.assertIn("midjourney", self.tracker.tools_by_id)

    def test_get_tool_config(self):
        cfg = self.tracker.get_tool_config("leonardo")
        self.assertEqual(cfg["name"], "Leonardo.ai")
        self.assertEqual(cfg["monthly_cap"], 24.00)

    def test_get_tool_config_unknown(self):
        self.assertIsNone(self.tracker.get_tool_config("nonexistent"))

    # ------------------------------------------------------------------
    # Logging and querying spend
    # ------------------------------------------------------------------

    def test_log_cost_and_query(self):
        self.tracker.log_cost("leonardo", 0.12, asset_id="a1")
        self.tracker.log_cost("leonardo", 0.12, asset_id="a2")
        spend = self.tracker.get_tool_spend("leonardo")
        self.assertAlmostEqual(spend, 0.24)

    def test_get_total_spend(self):
        self.tracker.log_cost("leonardo", 5.00)
        self.tracker.log_cost("midjourney", 3.00)
        total = self.tracker.get_total_spend()
        self.assertAlmostEqual(total, 8.00)

    def test_spend_zero_initially(self):
        self.assertAlmostEqual(self.tracker.get_tool_spend("leonardo"), 0.0)
        self.assertAlmostEqual(self.tracker.get_total_spend(), 0.0)

    # ------------------------------------------------------------------
    # Budget enforcement — check_budget()
    # ------------------------------------------------------------------

    def test_free_tool_always_allowed(self):
        result = self.tracker.check_budget("stable_diffusion", 0.0)
        self.assertTrue(result["allowed"])
        self.assertEqual(result["reason"], "OK")

    def test_paid_tool_within_budget(self):
        result = self.tracker.check_budget("midjourney", 0.10)
        self.assertTrue(result["allowed"])

    def test_tool_cap_exceeded(self):
        # Log enough to exceed leonardo's $24 cap
        self.tracker.log_cost("leonardo", 23.50)
        result = self.tracker.check_budget("leonardo", 1.00)
        self.assertFalse(result["allowed"])
        self.assertIn("cap exceeded", result["reason"])

    def test_tool_cap_exact_boundary(self):
        # Spend exactly the cap — next call should be blocked
        self.tracker.log_cost("suno", 10.00)
        result = self.tracker.check_budget("suno", 0.01)
        self.assertFalse(result["allowed"])

    def test_total_budget_exceeded(self):
        # Spend close to total budget using a tool with high cap
        # Use leonardo ($24 cap) — log $23 to leonardo and $76.50 to midjourney
        # so total = $99.50. Next $1 to leonardo => total $100.50 > $100 budget
        self.tracker.log_cost("midjourney", 29.00)  # under midjourney's $30 cap
        self.tracker.log_cost("leonardo", 23.00)    # under leonardo's $24 cap
        self.tracker.log_cost("suno", 9.50)         # under suno's $10 cap
        # Total now: $61.50. Need more — log to stable_diffusion (free, no cap tracking issue)
        # Actually we need to push total past $100. Log more to tools that have headroom.
        self.tracker.log_cost("midjourney", 0.50)   # midjourney at $29.50
        # Total = $62. Need to get to $99+. Use a fresh approach: set leonardo cap higher.
        self.tracker.set_cap("leonardo", 100.00)
        self.tracker.log_cost("leonardo", 37.50)    # leonardo at $60.50
        # Total now = $99.50. Next $1 to leonardo => total $100.50 > $100 budget
        result = self.tracker.check_budget("leonardo", 1.00)
        self.assertFalse(result["allowed"])
        self.assertIn("Total monthly budget exceeded", result["reason"])

    def test_warning_at_threshold(self):
        # Leonardo cap is $24, 80% = $19.20
        self.tracker.log_cost("leonardo", 20.00)
        result = self.tracker.check_budget("leonardo", 0.10)
        self.assertTrue(result["allowed"])
        self.assertIsNotNone(result["warning"])
        self.assertIn("Leonardo.ai", result["warning"])

    def test_no_warning_below_threshold(self):
        self.tracker.log_cost("leonardo", 5.00)
        result = self.tracker.check_budget("leonardo", 0.10)
        self.assertTrue(result["allowed"])
        self.assertIsNone(result["warning"])

    def test_unknown_tool_blocked(self):
        result = self.tracker.check_budget("nonexistent_tool", 1.00)
        self.assertFalse(result["allowed"])
        self.assertIn("Unknown tool", result["reason"])

    # ------------------------------------------------------------------
    # Summary
    # ------------------------------------------------------------------

    def test_get_summary_empty(self):
        summary = self.tracker.get_summary()
        self.assertEqual(len(summary), 4)  # all 4 tools
        for row in summary:
            self.assertEqual(row["spent"], 0.0)
            self.assertEqual(row["status"], "ok")

    def test_get_summary_with_spend(self):
        self.tracker.log_cost("suno", 10.00)  # exactly at cap
        summary = self.tracker.get_summary()
        suno_row = [r for r in summary if r["tool_name"] == "Suno"][0]
        self.assertEqual(suno_row["status"], "blocked")
        self.assertAlmostEqual(suno_row["spent"], 10.00)
        self.assertAlmostEqual(suno_row["pct_used"], 100.0)

    def test_get_summary_warning_status(self):
        self.tracker.log_cost("midjourney", 25.00)  # 83% of $30
        summary = self.tracker.get_summary()
        mj_row = [r for r in summary if r["tool_name"] == "Midjourney"][0]
        self.assertEqual(mj_row["status"], "warning")

    # ------------------------------------------------------------------
    # By-tool breakdown
    # ------------------------------------------------------------------

    def test_get_by_tool(self):
        self.tracker.log_cost("leonardo", 1.00, asset_id="a1")
        self.tracker.log_cost("leonardo", 2.00, asset_id="a2")
        self.tracker.log_cost("midjourney", 3.00, asset_id="a3")
        by_tool = self.tracker.get_by_tool()
        self.assertEqual(len(by_tool), 2)
        # Sorted by spend desc
        self.assertEqual(by_tool[0]["tool_id"], "midjourney")
        self.assertEqual(by_tool[0]["calls"], 1)
        self.assertEqual(by_tool[1]["tool_id"], "leonardo")
        self.assertEqual(by_tool[1]["calls"], 2)
        self.assertAlmostEqual(by_tool[1]["spent"], 3.00)

    def test_get_by_tool_empty(self):
        by_tool = self.tracker.get_by_tool()
        self.assertEqual(len(by_tool), 0)

    # ------------------------------------------------------------------
    # Forecast
    # ------------------------------------------------------------------

    def test_get_forecast_no_spend(self):
        forecast = self.tracker.get_forecast()
        self.assertEqual(forecast["current_spend"], 0.0)
        self.assertEqual(forecast["projected_total"], 0.0)
        self.assertTrue(forecast["on_track"])
        self.assertIn("month", forecast)
        self.assertIn("daily_rate", forecast)

    def test_get_forecast_with_spend(self):
        self.tracker.log_cost("midjourney", 50.00)
        forecast = self.tracker.get_forecast()
        self.assertAlmostEqual(forecast["current_spend"], 50.00)
        self.assertGreater(forecast["projected_total"], 0)

    # ------------------------------------------------------------------
    # set_cap
    # ------------------------------------------------------------------

    def test_set_cap(self):
        self.tracker.set_cap("leonardo", 50.00)
        cfg = self.tracker.get_tool_config("leonardo")
        self.assertEqual(cfg["monthly_cap"], 50.00)
        # Verify it persisted to disk
        with open(self.config_path) as f:
            on_disk = json.load(f)
        leo = [t for t in on_disk["tools"] if t["id"] == "leonardo"][0]
        self.assertEqual(leo["monthly_cap"], 50.00)

    def test_set_cap_unknown_tool_raises(self):
        with self.assertRaises(ValueError):
            self.tracker.set_cap("nonexistent", 100.00)

    def test_set_cap_updates_budget_checks(self):
        # Leonardo had $24 cap. Lower it to $5.
        self.tracker.set_cap("leonardo", 5.00)
        self.tracker.log_cost("leonardo", 4.50)
        result = self.tracker.check_budget("leonardo", 1.00)
        self.assertFalse(result["allowed"])


if __name__ == "__main__":
    unittest.main()
