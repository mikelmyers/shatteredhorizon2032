"""Asset data models for the pipeline inventory."""

import sqlite3
import uuid
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Optional


ASSET_TYPES = [
    "concept_art", "3d_mesh", "texture", "character",
    "environment", "vfx", "music", "sfx", "ui",
]

ASSET_STATUSES = [
    "generated", "approved", "quarantined", "integrated", "archived",
]

STYLE_CHECK_RESULTS = ["pass", "warn", "fail", "pending"]

PRIORITIES = ["low", "normal", "high"]


def _now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


@dataclass
class Asset:
    """A tracked asset in the pipeline inventory."""

    id: str = field(default_factory=lambda: str(uuid.uuid4()))
    type: str = ""
    name: Optional[str] = None
    prompt: Optional[str] = None
    tool_used: Optional[str] = None
    tool_tier: Optional[str] = None
    cost_usd: float = 0.0
    file_path: Optional[str] = None
    style_check_result: Optional[str] = None
    style_check_score: Optional[float] = None
    status: str = "generated"
    tags: Optional[str] = None
    created_at: str = field(default_factory=_now_iso)
    updated_at: str = field(default_factory=_now_iso)
    notes: Optional[str] = None

    @classmethod
    def from_row(cls, row: sqlite3.Row) -> "Asset":
        """Construct an Asset from a sqlite3.Row."""
        return cls(**dict(row))

    def to_dict(self) -> dict:
        """Return a plain dictionary representation of the asset."""
        return {
            "id": self.id,
            "type": self.type,
            "name": self.name,
            "prompt": self.prompt,
            "tool_used": self.tool_used,
            "tool_tier": self.tool_tier,
            "cost_usd": self.cost_usd,
            "file_path": self.file_path,
            "style_check_result": self.style_check_result,
            "style_check_score": self.style_check_score,
            "status": self.status,
            "tags": self.tags,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
            "notes": self.notes,
        }


@dataclass
class AssetRequest:
    """Structured asset generation request."""

    id: str = field(default_factory=lambda: str(uuid.uuid4()))
    type: str = ""
    prompt: str = ""
    style_refs: list[str] = field(default_factory=list)
    priority: str = "normal"
    force_paid: bool = False
    tags: list[str] = field(default_factory=list)

    @classmethod
    def from_dict(cls, d: dict) -> "AssetRequest":
        """Construct an AssetRequest from a dictionary.

        Generates a UUID for the id field if one is not provided.
        """
        data = dict(d)
        if not data.get("id"):
            data["id"] = str(uuid.uuid4())
        return cls(
            id=data["id"],
            type=data.get("type", ""),
            prompt=data.get("prompt", ""),
            style_refs=data.get("style_refs", []),
            priority=data.get("priority", "normal"),
            force_paid=data.get("force_paid", False),
            tags=data.get("tags", []),
        )

    def to_dict(self) -> dict:
        """Return a plain dictionary representation of the request."""
        return {
            "id": self.id,
            "type": self.type,
            "prompt": self.prompt,
            "style_refs": self.style_refs,
            "priority": self.priority,
            "force_paid": self.force_paid,
            "tags": self.tags,
        }
