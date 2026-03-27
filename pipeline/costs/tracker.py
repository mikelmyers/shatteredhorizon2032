import json
import sqlite3
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional


COSTS_DIR = Path(__file__).resolve().parent


class CostTracker:

    def __init__(self, db_path: str | Path, tools_config_path: str | Path) -> None:
        self.db_path = Path(db_path)
        self.tools_config_path = Path(tools_config_path)
        self.config: dict = {}
        self.tools_by_id: dict[str, dict] = {}
        self.total_monthly_budget: float = 0.0
        self.alert_threshold_pct: float = 0.0
        self.reload_config()
        self.conn: sqlite3.Connection = sqlite3.connect(str(self.db_path))
        self.conn.row_factory = sqlite3.Row
        self._init_db()

    def _init_db(self) -> None:
        self.conn.execute(
            """
            CREATE TABLE IF NOT EXISTS ledger (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                tool_id TEXT NOT NULL,
                cost REAL NOT NULL,
                month TEXT NOT NULL,
                timestamp TEXT NOT NULL,
                asset_id TEXT,
                notes TEXT
            )
            """
        )
        self.conn.commit()

    def reload_config(self) -> None:
        with open(self.tools_config_path, "r") as f:
            self.config = json.load(f)
        self.tools_by_id = {t["id"]: t for t in self.config["tools"]}
        self.total_monthly_budget = self.config.get("total_monthly_budget", 0.0)
        self.alert_threshold_pct = self.config.get("alert_threshold_pct", 80)

    def _load_config(self) -> dict:
        return self.config

    def _current_month(self) -> str:
        return datetime.now(timezone.utc).strftime("%Y-%m")

    def log_cost(self, tool_id: str, cost: float, asset_id: str = "", notes: str = "") -> None:
        now = datetime.now(timezone.utc)
        month = now.strftime("%Y-%m")
        timestamp = now.isoformat()
        self.conn.execute(
            "INSERT INTO ledger (tool_id, cost, month, timestamp, asset_id, notes) VALUES (?, ?, ?, ?, ?, ?)",
            (tool_id, cost, month, timestamp, asset_id, notes),
        )
        self.conn.commit()

    def get_tool_spend(self, tool_id: str, month: Optional[str] = None) -> float:
        month = month or self._current_month()
        row = self.conn.execute(
            "SELECT COALESCE(SUM(cost), 0) AS total FROM ledger WHERE tool_id = ? AND month = ?",
            (tool_id, month),
        ).fetchone()
        return float(row["total"])

    def get_total_spend(self, month: Optional[str] = None) -> float:
        month = month or self._current_month()
        row = self.conn.execute(
            "SELECT COALESCE(SUM(cost), 0) AS total FROM ledger WHERE month = ?",
            (month,),
        ).fetchone()
        return float(row["total"])

    def get_tool_config(self, tool_id: str) -> Optional[dict]:
        return self.tools_by_id.get(tool_id)

    def check_budget(self, tool_id: str, estimated_cost: float = 0.0) -> dict:
        tool = self.tools_by_id.get(tool_id)
        if tool is None:
            return {
                "allowed": False,
                "reason": f"Unknown tool: {tool_id}",
                "tool_spend": 0.0,
                "tool_cap": 0.0,
                "total_spend": 0.0,
                "total_budget": self.total_monthly_budget,
                "warning": None,
            }

        tier = tool.get("tier", "")
        tool_cap = tool.get("monthly_cap", 0.0)
        month = self._current_month()
        tool_spend = self.get_tool_spend(tool_id, month)
        total_spend = self.get_total_spend(month)

        result: dict = {
            "allowed": True,
            "reason": "OK",
            "tool_spend": tool_spend,
            "tool_cap": tool_cap,
            "total_spend": total_spend,
            "total_budget": self.total_monthly_budget,
            "warning": None,
        }

        if tier == "free":
            return result

        if tool_cap > 0 and tool_spend + estimated_cost > tool_cap:
            result["allowed"] = False
            result["reason"] = (
                f"{tool['name']} monthly cap exceeded: "
                f"${tool_spend + estimated_cost:.2f} > ${tool_cap:.2f}"
            )
            return result

        if total_spend + estimated_cost > self.total_monthly_budget:
            result["allowed"] = False
            result["reason"] = (
                f"Total monthly budget exceeded: "
                f"${total_spend + estimated_cost:.2f} > ${self.total_monthly_budget:.2f}"
            )
            return result

        if tool_cap > 0 and tool_spend / tool_cap >= self.alert_threshold_pct / 100:
            result["warning"] = (
                f"{tool['name']} at {tool_spend / tool_cap * 100:.0f}% of monthly cap "
                f"(${tool_spend:.2f} / ${tool_cap:.2f})"
            )

        return result

    def get_summary(self, month: Optional[str] = None) -> list[dict]:
        month = month or self._current_month()
        rows: list[dict] = []
        for tool in self.config["tools"]:
            tid = tool["id"]
            spent = self.get_tool_spend(tid, month)
            cap = tool.get("monthly_cap", 0.0)
            tier = tool.get("tier", "")

            if cap > 0:
                pct_used = spent / cap * 100
            else:
                pct_used = 0.0

            if tier == "free":
                status = "ok"
            elif cap > 0 and spent >= cap:
                status = "blocked"
            elif cap > 0 and pct_used >= self.alert_threshold_pct:
                status = "warning"
            else:
                status = "ok"

            rows.append({
                "tool_name": tool["name"],
                "tier": tier,
                "spent": spent,
                "cap": cap,
                "pct_used": round(pct_used, 1),
                "status": status,
            })
        return rows

    def get_by_tool(self, month: Optional[str] = None) -> list[dict]:
        month = month or self._current_month()
        cursor = self.conn.execute(
            "SELECT tool_id, SUM(cost) AS spent, COUNT(*) AS calls "
            "FROM ledger WHERE month = ? GROUP BY tool_id ORDER BY spent DESC",
            (month,),
        )
        results: list[dict] = []
        for row in cursor.fetchall():
            tool = self.tools_by_id.get(row["tool_id"], {})
            results.append({
                "tool_id": row["tool_id"],
                "tool_name": tool.get("name", row["tool_id"]),
                "tier": tool.get("tier", "unknown"),
                "spent": float(row["spent"]),
                "calls": int(row["calls"]),
            })
        return results

    def get_forecast(self) -> dict:
        month = self._current_month()
        now = datetime.now(timezone.utc)
        day_of_month = now.day

        year = now.year
        m = now.month
        if m == 12:
            days_in_month = (datetime(year + 1, 1, 1) - datetime(year, 12, 1)).days
        else:
            days_in_month = (datetime(year, m + 1, 1) - datetime(year, m, 1)).days

        total_spend = self.get_total_spend(month)
        if day_of_month > 0:
            daily_rate = total_spend / day_of_month
        else:
            daily_rate = 0.0

        projected = daily_rate * days_in_month

        return {
            "month": month,
            "days_elapsed": day_of_month,
            "days_in_month": days_in_month,
            "current_spend": total_spend,
            "daily_rate": round(daily_rate, 2),
            "projected_total": round(projected, 2),
            "monthly_budget": self.total_monthly_budget,
            "on_track": projected <= self.total_monthly_budget,
        }

    def set_cap(self, tool_id: str, amount: float) -> None:
        with open(self.tools_config_path, "r") as f:
            config = json.load(f)

        found = False
        for tool in config["tools"]:
            if tool["id"] == tool_id:
                tool["monthly_cap"] = amount
                found = True
                break

        if not found:
            raise ValueError(f"Unknown tool_id: {tool_id}")

        with open(self.tools_config_path, "w") as f:
            json.dump(config, f, indent=2)
            f.write("\n")

        self.reload_config()


def get_tracker(base_path: Optional[str | Path] = None) -> CostTracker:
    if base_path is not None:
        base = Path(base_path)
    else:
        base = COSTS_DIR
    db_path = base / "ledger.db"
    tools_config_path = base / "tools.json"
    return CostTracker(db_path=db_path, tools_config_path=tools_config_path)
