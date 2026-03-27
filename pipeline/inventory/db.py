"""SQLite interface for the asset inventory."""

import csv
import io
import json
import sqlite3
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

from .models import Asset

PIPELINE_DIR = Path(__file__).resolve().parent.parent
DEFAULT_DB_PATH = PIPELINE_DIR / "pipeline.db"

CREATE_TABLE_SQL = """\
CREATE TABLE IF NOT EXISTS assets (
    id TEXT PRIMARY KEY,
    type TEXT NOT NULL,
    name TEXT,
    prompt TEXT,
    tool_used TEXT,
    tool_tier TEXT,
    cost_usd REAL DEFAULT 0,
    file_path TEXT,
    style_check_result TEXT,
    style_check_score REAL,
    status TEXT DEFAULT 'generated',
    tags TEXT,
    created_at TEXT,
    updated_at TEXT,
    notes TEXT
)"""


def _now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


class InventoryDB:
    """Asset inventory backed by SQLite."""

    def __init__(self, db_path: str | Path = DEFAULT_DB_PATH) -> None:
        self.db_path = Path(db_path)
        self.conn = sqlite3.connect(str(self.db_path))
        self.conn.row_factory = sqlite3.Row
        self.conn.execute("PRAGMA journal_mode=WAL")
        self._init_db()

    def _init_db(self) -> None:
        """Create the assets table if it does not already exist."""
        self.conn.execute(CREATE_TABLE_SQL)
        self.conn.commit()

    # ------------------------------------------------------------------
    # CRUD
    # ------------------------------------------------------------------

    def insert(self, asset: Asset) -> str:
        """Insert an asset into the database and return its id."""
        d = asset.to_dict()
        cols = ", ".join(d.keys())
        placeholders = ", ".join(["?"] * len(d))
        self.conn.execute(
            f"INSERT OR REPLACE INTO assets ({cols}) VALUES ({placeholders})",
            list(d.values()),
        )
        self.conn.commit()
        return asset.id

    def get(self, asset_id: str) -> Optional[Asset]:
        """Fetch a single asset by id, or None if not found."""
        row = self.conn.execute(
            "SELECT * FROM assets WHERE id = ?", (asset_id,)
        ).fetchone()
        if row is None:
            return None
        return Asset.from_row(row)

    def update(self, asset_id: str, **kwargs: object) -> None:
        """Update arbitrary fields on an asset. Automatically sets updated_at."""
        if not kwargs:
            return
        kwargs["updated_at"] = _now_iso()
        set_clause = ", ".join(f"{k} = ?" for k in kwargs)
        values = list(kwargs.values()) + [asset_id]
        self.conn.execute(
            f"UPDATE assets SET {set_clause} WHERE id = ?", values
        )
        self.conn.commit()

    def list_assets(
        self,
        type: Optional[str] = None,
        status: Optional[str] = None,
        tag: Optional[str] = None,
        limit: int = 100,
        offset: int = 0,
    ) -> list[Asset]:
        """Return a filtered list of assets."""
        query = "SELECT * FROM assets WHERE 1=1"
        params: list[object] = []
        if type is not None:
            query += " AND type = ?"
            params.append(type)
        if status is not None:
            query += " AND status = ?"
            params.append(status)
        if tag is not None:
            query += " AND tags LIKE ?"
            params.append(f"%{tag}%")
        query += " ORDER BY created_at DESC LIMIT ? OFFSET ?"
        params.extend([limit, offset])
        rows = self.conn.execute(query, params).fetchall()
        return [Asset.from_row(r) for r in rows]

    # ------------------------------------------------------------------
    # Status helpers
    # ------------------------------------------------------------------

    def approve(self, asset_id: str) -> None:
        """Mark an asset as approved."""
        self.update(asset_id, status="approved")

    def quarantine(self, asset_id: str, reason: str) -> None:
        """Quarantine an asset with a reason."""
        self.update(
            asset_id,
            status="quarantined",
            style_check_result="fail",
            notes=reason,
        )

    def update_style_check(self, asset_id: str, result: str, score: float) -> None:
        """Update the style-check result and score for an asset."""
        self.update(
            asset_id,
            style_check_result=result,
            style_check_score=score,
        )

    # ------------------------------------------------------------------
    # Reporting
    # ------------------------------------------------------------------

    def get_stats(self) -> dict:
        """Return aggregate statistics about the inventory."""
        total = self.conn.execute(
            "SELECT COUNT(*) AS c FROM assets"
        ).fetchone()["c"]

        by_type = {
            row["type"]: row["c"]
            for row in self.conn.execute(
                "SELECT type, COUNT(*) AS c FROM assets GROUP BY type"
            ).fetchall()
        }

        by_status = {
            row["status"]: row["c"]
            for row in self.conn.execute(
                "SELECT status, COUNT(*) AS c FROM assets GROUP BY status"
            ).fetchall()
        }

        by_tier = {
            row["tool_tier"]: row["c"]
            for row in self.conn.execute(
                "SELECT tool_tier, COUNT(*) AS c FROM assets "
                "WHERE tool_tier IS NOT NULL GROUP BY tool_tier"
            ).fetchall()
        }

        style_pass = self.conn.execute(
            "SELECT COUNT(*) AS c FROM assets WHERE style_check_result = 'pass'"
        ).fetchone()["c"]
        style_checked = self.conn.execute(
            "SELECT COUNT(*) AS c FROM assets "
            "WHERE style_check_result IN ('pass', 'warn', 'fail')"
        ).fetchone()["c"]
        style_pass_rate = (
            (style_pass / style_checked * 100) if style_checked > 0 else 0.0
        )

        total_cost = self.conn.execute(
            "SELECT COALESCE(SUM(cost_usd), 0) AS c FROM assets"
        ).fetchone()["c"]

        return {
            "total": total,
            "by_type": by_type,
            "by_status": by_status,
            "by_tier": by_tier,
            "style_pass_rate": style_pass_rate,
            "total_cost": total_cost,
        }

    def export(self, format: str = "json") -> str:
        """Export all assets as a JSON or CSV string."""
        rows = self.conn.execute(
            "SELECT * FROM assets ORDER BY created_at DESC"
        ).fetchall()
        assets = [dict(r) for r in rows]

        if format == "json":
            return json.dumps(assets, indent=2)
        elif format == "csv":
            if not assets:
                return ""
            output = io.StringIO()
            writer = csv.DictWriter(output, fieldnames=list(assets[0].keys()))
            writer.writeheader()
            writer.writerows(assets)
            return output.getvalue()
        else:
            raise ValueError(f"Unknown export format: {format}")

    def search(self, query: str) -> list[Asset]:
        """Search assets by name, prompt, or tags using LIKE."""
        like = f"%{query}%"
        rows = self.conn.execute(
            "SELECT * FROM assets "
            "WHERE name LIKE ? OR prompt LIKE ? OR tags LIKE ? "
            "ORDER BY created_at DESC",
            (like, like, like),
        ).fetchall()
        return [Asset.from_row(r) for r in rows]


def get_inventory(base_path: Optional[str | Path] = None) -> InventoryDB:
    """Convenience constructor for InventoryDB.

    If *base_path* is provided it is used as the database file path.
    Otherwise the default ``pipeline/pipeline.db`` is used.
    """
    if base_path is not None:
        return InventoryDB(db_path=Path(base_path))
    return InventoryDB()
