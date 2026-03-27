"""CLI command handlers for inventory management.

Pure functions that accept an InventoryDB instance and return formatted
output strings or dicts. These are called by the CLI layer.
"""

from typing import Optional

from .db import InventoryDB


def list_assets(
    db: InventoryDB,
    type: Optional[str] = None,
    status: Optional[str] = None,
    tag: Optional[str] = None,
) -> list[dict]:
    """Return a filtered list of assets as plain dicts."""
    assets = db.list_assets(type=type, status=status, tag=tag)
    return [a.to_dict() for a in assets]


def show_asset(db: InventoryDB, asset_id: str) -> Optional[dict]:
    """Return a single asset as a dict, or None if not found."""
    asset = db.get(asset_id)
    if asset is None:
        return None
    return asset.to_dict()


def approve_asset(db: InventoryDB, asset_id: str) -> str:
    """Approve an asset. Returns a status message."""
    asset = db.get(asset_id)
    if asset is None:
        return f"Asset {asset_id} not found."
    db.approve(asset_id)
    return f"Asset {asset_id} approved."


def quarantine_asset(db: InventoryDB, asset_id: str, reason: str) -> str:
    """Quarantine an asset with a reason. Returns a status message."""
    asset = db.get(asset_id)
    if asset is None:
        return f"Asset {asset_id} not found."
    db.quarantine(asset_id, reason)
    return f"Asset {asset_id} quarantined: {reason}"


def export_assets(db: InventoryDB, format: str = "json") -> str:
    """Export all assets in the given format (json or csv)."""
    return db.export(format=format)


def get_stats(db: InventoryDB) -> dict:
    """Return aggregate inventory statistics."""
    return db.get_stats()
