"""Mixamo animation sourcing integration.

Provides an interface for searching and downloading animations from
Adobe Mixamo. Requires an Adobe account for authentication -- the
client provides the programmatic interface but the user must supply
valid credentials or session tokens.

Note: Mixamo's API is undocumented and subject to change. This module
is based on known endpoints as of 2026.
"""

from __future__ import annotations

import json
import logging
from pathlib import Path
from typing import Any, Dict, List, Optional

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Mixamo endpoints (unofficial)
# ---------------------------------------------------------------------------
_MIXAMO_BASE = "https://www.mixamo.com/api/v1"
_SEARCH_ENDPOINT = f"{_MIXAMO_BASE}/products"
_DOWNLOAD_ENDPOINT = f"{_MIXAMO_BASE}/animations"
_CHARACTER_ENDPOINT = f"{_MIXAMO_BASE}/characters"


# ---------------------------------------------------------------------------
# Recommended animation mapping
# ---------------------------------------------------------------------------

def get_recommended_animations() -> Dict[str, str]:
    """Return a mapping from game animation names to Mixamo searches.

    This provides a curated set of Mixamo animation names that best
    match the Shattered Horizon 2032 animation requirements. The values
    are search terms / exact Mixamo animation names.

    Returns:
        Dict mapping game animation identifier to Mixamo animation name.
    """
    return {
        # Locomotion
        "walking": "Walking",
        "walking_backward": "Walking Backward",
        "walking_strafe_left": "Left Strafe Walking",
        "walking_strafe_right": "Right Strafe Walking",
        "running": "Running",
        "running_backward": "Running Backward",
        "sprinting": "Sprinting Forward",
        "crouch_idle": "Crouch Idle",
        "crouch_walk": "Crouch Walk",
        "prone_idle": "Prone Idle",
        "prone_crawl": "Prone Crawl",

        # Stance transitions
        "stand_to_crouch": "Stand To Crouch",
        "crouch_to_stand": "Crouch To Stand",
        "crouch_to_prone": "Crouch To Prone",
        "prone_to_crouch": "Prone To Crouch",

        # Actions
        "jump_start": "Jump",
        "jump_land": "Hard Landing",
        "vault_low": "Climbing",
        "vault_high": "Wall Climb",
        "grenade_throw": "Throwing",
        "melee_strike": "Rifle Strike",

        # Death / injury
        "death_headshot": "Dying",
        "death_torso": "Falling Back Death",
        "death_explosive": "Flying Back Death",
        "injury_limp": "Injured Walk",
        "injury_arm_hit": "Hit Reaction",
        "injury_stagger": "Stagger",

        # Idle
        "stand_idle": "Idle",
        "stand_idle_rifle": "Rifle Idle",
    }


class MixamoClient:
    """Client for the Adobe Mixamo animation service.

    Authentication must be provided as a bearer token obtained from a
    logged-in Mixamo browser session (``Authorization: Bearer <token>``).

    Example::

        client = MixamoClient(bearer_token="eyJ...")
        results = client.search_animations("walking")
        for anim in results:
            print(anim["id"], anim["description"])
        client.download_animation(results[0]["id"], character_id="...",
                                  output_dir="./downloads")
    """

    def __init__(
        self,
        bearer_token: Optional[str] = None,
        timeout: int = 30,
    ) -> None:
        """Initialise the Mixamo client.

        Args:
            bearer_token: Adobe/Mixamo bearer token. If *None*, all
                network calls will raise :class:`RuntimeError`.
            timeout: HTTP request timeout in seconds.
        """
        if not HAS_REQUESTS:
            raise ImportError(
                "The 'requests' package is required for MixamoClient. "
                "Install it with: pip install requests"
            )

        self._token = bearer_token
        self._timeout = timeout
        self._session = requests.Session()
        if bearer_token:
            self._session.headers.update({
                "Authorization": f"Bearer {bearer_token}",
                "Accept": "application/json",
                "X-Api-Key": "mixamo2",
            })

    def _require_auth(self) -> None:
        """Raise if no bearer token is set."""
        if not self._token:
            raise RuntimeError(
                "MixamoClient requires a bearer_token for API access. "
                "Log in to mixamo.com, open DevTools, and copy the "
                "Authorization header value."
            )

    # ------------------------------------------------------------------
    # Search
    # ------------------------------------------------------------------

    def search_animations(
        self,
        query: str,
        page: int = 1,
        limit: int = 24,
    ) -> List[Dict[str, Any]]:
        """Search Mixamo for animations matching *query*.

        Args:
            query: Search string (e.g. ``"walking"``).
            page: Result page number (1-indexed).
            limit: Maximum results per page.

        Returns:
            List of animation dicts with keys: ``id``, ``description``,
            ``motion_id``, ``preview_url``.
        """
        self._require_auth()

        params = {
            "query": query,
            "page": page,
            "limit": limit,
            "type": "Motion",
        }

        try:
            resp = self._session.get(
                _SEARCH_ENDPOINT,
                params=params,
                timeout=self._timeout,
            )
            resp.raise_for_status()
            data = resp.json()
        except requests.RequestException as exc:
            logger.error("Mixamo search failed for '%s': %s", query, exc)
            return []

        results: List[Dict[str, Any]] = []
        for item in data.get("results", []):
            results.append({
                "id": item.get("id", ""),
                "description": item.get("description", ""),
                "motion_id": item.get("motion_id", ""),
                "preview_url": item.get("preview", ""),
            })

        logger.info(
            "Mixamo search '%s': %d results (page %d)",
            query, len(results), page,
        )
        return results

    # ------------------------------------------------------------------
    # Download
    # ------------------------------------------------------------------

    def download_animation(
        self,
        anim_id: str,
        character_id: str,
        format: str = "fbx",
        output_dir: str | Path = "downloads",
    ) -> Path:
        """Download an animation from Mixamo.

        Initiates an export job on Mixamo's servers, polls for
        completion, then downloads the resulting file.

        Args:
            anim_id: Mixamo animation / product ID.
            character_id: Mixamo character ID to apply the animation to.
            format: Export format (``"fbx"`` or ``"dae"``).
            output_dir: Local directory for the downloaded file.

        Returns:
            Path to the downloaded FBX/DAE file.
        """
        self._require_auth()

        out = Path(output_dir)
        out.mkdir(parents=True, exist_ok=True)

        # 1. Request export
        export_payload = {
            "character_id": character_id,
            "product_name": anim_id,
            "preferences": {
                "format": format.upper(),
                "skin": "false",
                "fps": "30",
                "reducekf": "0",
            },
            "type": "Motion",
        }

        try:
            resp = self._session.post(
                f"{_DOWNLOAD_ENDPOINT}/export",
                json=export_payload,
                timeout=self._timeout,
            )
            resp.raise_for_status()
            export_data = resp.json()
        except requests.RequestException as exc:
            raise RuntimeError(f"Mixamo export request failed: {exc}") from exc

        # 2. Poll for completion
        monitor_url = export_data.get("monitor_url") or export_data.get("uuid", "")
        if not monitor_url:
            raise RuntimeError("No monitor URL returned from Mixamo export.")

        import time
        download_url: Optional[str] = None
        for _ in range(60):  # up to 60 polls (2 minutes)
            try:
                poll = self._session.get(
                    f"{_DOWNLOAD_ENDPOINT}/monitor/{monitor_url}",
                    timeout=self._timeout,
                )
                poll.raise_for_status()
                status = poll.json()
            except requests.RequestException:
                time.sleep(2)
                continue

            if status.get("status") == "completed":
                download_url = status.get("result", {}).get("url")
                break
            elif status.get("status") == "failed":
                raise RuntimeError(
                    f"Mixamo export failed: {status.get('message', 'unknown')}"
                )
            time.sleep(2)

        if not download_url:
            raise RuntimeError("Mixamo export timed out.")

        # 3. Download file
        ext = format.lower()
        filepath = out / f"{anim_id}.{ext}"
        try:
            dl = self._session.get(download_url, stream=True, timeout=120)
            dl.raise_for_status()
            with filepath.open("wb") as fh:
                for chunk in dl.iter_content(chunk_size=8192):
                    fh.write(chunk)
        except requests.RequestException as exc:
            raise RuntimeError(f"Download failed: {exc}") from exc

        logger.info("Downloaded animation -> %s", filepath)
        return filepath
