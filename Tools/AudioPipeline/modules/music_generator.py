"""
Music Generator — Pluggable AI music generation for game soundtrack.

Supports multiple backends:
  - Suno API (cloud, text-to-music)
  - Stable Audio API (cloud, text-to-music)

Generates music tracks from text prompts with style, duration, and key
parameters. Each backend is a pluggable provider — enable whichever
you have access to via environment variables.
"""

import io
import json
import os
import time
from abc import ABC, abstractmethod
from pathlib import Path
from typing import Optional

import requests


class MusicProvider(ABC):
    """Base class for AI music generation providers."""

    @abstractmethod
    def generate(self, prompt: str, duration_seconds: int,
                 style: Optional[str] = None) -> Optional[Path]:
        """Generate a music track and return the path to the audio file."""
        pass

    @abstractmethod
    def is_available(self) -> bool:
        pass


class SunoProvider(MusicProvider):
    """Suno API provider for AI music generation."""

    def __init__(self, output_dir: Path):
        self._api_key = os.environ.get("SUNO_API_KEY")
        self.base_url = os.environ.get(
            "SUNO_API_URL", "https://api.suno.ai/v1"
        )
        self.output_dir = output_dir
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def is_available(self) -> bool:
        return bool(self._api_key)

    def generate(self, prompt: str, duration_seconds: int,
                 style: Optional[str] = None) -> Optional[Path]:
        if not self.is_available():
            return None

        headers = {
            "Authorization": f"Bearer {self._api_key}",
            "Content-Type": "application/json",
        }

        payload = {
            "prompt": prompt,
            "duration": duration_seconds,
            "make_instrumental": True,
        }
        if style:
            payload["style"] = style

        try:
            # Submit generation request
            resp = requests.post(
                f"{self.base_url}/generation",
                headers=headers,
                json=payload,
                timeout=30,
            )
            resp.raise_for_status()
            job = resp.json()
            job_id = job.get("id") or job.get("job_id")

            if not job_id:
                print(f"[Suno] No job ID returned: {job}")
                return None

            # Poll for completion
            for _ in range(180):
                time.sleep(5)
                poll_resp = requests.get(
                    f"{self.base_url}/generation/{job_id}",
                    headers=headers,
                    timeout=15,
                )
                poll_resp.raise_for_status()
                status = poll_resp.json()

                if status.get("status") == "completed":
                    audio_url = status.get("audio_url") or status.get("output_url")
                    if not audio_url:
                        print(f"[Suno] Completed but no audio URL: {status}")
                        return None
                    return self._download(audio_url, job_id)

                elif status.get("status") in ("failed", "error"):
                    print(f"[Suno] Generation failed: {status.get('error', 'unknown')}")
                    return None

            print("[Suno] Generation timed out after 15 minutes")
            return None

        except requests.RequestException as e:
            print(f"[Suno] Request failed: {e}")
            return None

    def _download(self, url: str, job_id: str) -> Optional[Path]:
        """Download generated audio to local file."""
        try:
            resp = requests.get(url, timeout=120)
            resp.raise_for_status()
            out_path = self.output_dir / f"suno_{job_id}.wav"
            out_path.write_bytes(resp.content)
            print(f"[Suno] Downloaded: {out_path.name}")
            return out_path
        except requests.RequestException as e:
            print(f"[Suno] Download failed: {e}")
            return None


class StableAudioProvider(MusicProvider):
    """Stability AI Stable Audio API provider."""

    def __init__(self, output_dir: Path):
        self._api_key = os.environ.get("STABILITY_API_KEY")
        self.base_url = os.environ.get(
            "STABLE_AUDIO_API_URL",
            "https://api.stability.ai/v2beta/stable-audio",
        )
        self.output_dir = output_dir
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def is_available(self) -> bool:
        return bool(self._api_key)

    def generate(self, prompt: str, duration_seconds: int,
                 style: Optional[str] = None) -> Optional[Path]:
        if not self.is_available():
            return None

        headers = {
            "Authorization": f"Bearer {self._api_key}",
            "Content-Type": "application/json",
            "Accept": "audio/*",
        }

        full_prompt = prompt
        if style:
            full_prompt = f"{style}. {prompt}"

        payload = {
            "prompt": full_prompt,
            "duration": min(duration_seconds, 180),
            "output_format": "wav",
        }

        try:
            resp = requests.post(
                f"{self.base_url}/generate",
                headers=headers,
                json=payload,
                timeout=300,
            )
            resp.raise_for_status()

            # If the response is audio data directly
            content_type = resp.headers.get("content-type", "")
            if "audio" in content_type or "octet-stream" in content_type:
                return self._save_audio(resp.content, prompt)

            # If the response is JSON with a URL or job ID
            data = resp.json()
            if "audio_url" in data:
                return self._download(data["audio_url"], prompt)

            # Polling-based workflow
            job_id = data.get("id") or data.get("job_id")
            if job_id:
                return self._poll_and_download(job_id, headers, prompt)

            print(f"[StableAudio] Unexpected response format: {data}")
            return None

        except requests.RequestException as e:
            print(f"[StableAudio] Request failed: {e}")
            return None

    def _save_audio(self, audio_bytes: bytes, prompt: str) -> Path:
        """Save raw audio bytes to file."""
        safe_name = prompt[:40].replace(" ", "_").replace("/", "_")
        out_path = self.output_dir / f"stable_{safe_name}.wav"
        out_path.write_bytes(audio_bytes)
        print(f"[StableAudio] Saved: {out_path.name}")
        return out_path

    def _download(self, url: str, prompt: str) -> Optional[Path]:
        """Download audio from URL."""
        try:
            resp = requests.get(url, timeout=120)
            resp.raise_for_status()
            return self._save_audio(resp.content, prompt)
        except requests.RequestException as e:
            print(f"[StableAudio] Download failed: {e}")
            return None

    def _poll_and_download(self, job_id: str, headers: dict,
                           prompt: str) -> Optional[Path]:
        """Poll for job completion and download result."""
        for _ in range(120):
            time.sleep(5)
            try:
                poll_resp = requests.get(
                    f"{self.base_url}/status/{job_id}",
                    headers=headers,
                    timeout=15,
                )
                poll_resp.raise_for_status()
                status = poll_resp.json()

                if status.get("status") == "completed":
                    audio_url = status.get("audio_url") or status.get("output_url")
                    if audio_url:
                        return self._download(audio_url, prompt)
                    print(f"[StableAudio] Completed but no URL: {status}")
                    return None

                elif status.get("status") in ("failed", "error"):
                    print(f"[StableAudio] Failed: {status.get('error', 'unknown')}")
                    return None

            except requests.RequestException as e:
                print(f"[StableAudio] Poll error: {e}")

        print("[StableAudio] Timed out after 10 minutes")
        return None


class MusicGenerator:
    """Manages AI music providers and generates game soundtrack tracks."""

    def __init__(self, output_dir: Path):
        self.output_dir = output_dir
        self.raw_dir = output_dir / "_raw"
        self.raw_dir.mkdir(parents=True, exist_ok=True)

        self.providers: list[MusicProvider] = [
            SunoProvider(self.raw_dir),
            StableAudioProvider(self.raw_dir),
        ]

    def get_available_provider(self) -> Optional[MusicProvider]:
        """Return the first available provider."""
        for provider in self.providers:
            if provider.is_available():
                name = provider.__class__.__name__
                print(f"[MusicGenerator] Using provider: {name}")
                return provider
        print("[MusicGenerator] No music providers available.")
        print("  Set SUNO_API_KEY or STABILITY_API_KEY to enable generation.")
        return None

    def generate_track(self, prompt: str, duration_seconds: int,
                       style: Optional[str] = None) -> Optional[Path]:
        """Generate a single music track using the first available provider."""
        provider = self.get_available_provider()
        if not provider:
            return None
        return provider.generate(prompt, duration_seconds, style)

    def _build_prompt(self, track: dict) -> str:
        """Build a full generation prompt from a track manifest entry."""
        parts = [track["description"]]

        if track.get("bpm"):
            parts.append(f"{track['bpm']} BPM")
        if track.get("key"):
            parts.append(f"Key of {track['key']}")

        # Add game-context qualifiers
        category = track.get("category", "")
        if category == "ambient":
            parts.append("Atmospheric, cinematic, immersive game soundtrack")
        elif category == "combat":
            parts.append("Intense, dynamic, modern military game combat music")
        elif category == "stingers":
            parts.append("Short cinematic musical stinger for video game")
        elif category == "menu":
            parts.append("Polished cinematic menu music for AAA video game")

        return ". ".join(parts)

    def _style_for_category(self, category: str) -> str:
        """Return a style tag for the provider based on track category."""
        styles = {
            "ambient": "ambient, atmospheric, cinematic, dark, electronic",
            "combat": "action, intense, orchestral, hybrid, electronic, drums",
            "stingers": "cinematic, orchestral, short, dramatic",
            "menu": "cinematic, orchestral, emotional, theme, hybrid",
        }
        return styles.get(category, "cinematic, game soundtrack")

    def generate_all(self, manifest: dict) -> dict[str, Optional[Path]]:
        """Generate all tracks in a manifest. Returns {track_id: Path|None}."""
        results: dict[str, Optional[Path]] = {}
        tracks = manifest.get("tracks", [])
        total = len(tracks)

        print(f"\n[MusicGenerator] Generating {total} tracks...\n")

        for i, track in enumerate(tracks, 1):
            track_id = track["id"]
            category = track.get("category", "unknown")
            duration = track.get("duration_seconds", 60)

            print(f"[{i}/{total}] {track_id} ({category}, {duration}s)")

            prompt = self._build_prompt(track)
            style = self._style_for_category(category)

            path = self.generate_track(prompt, duration, style)
            results[track_id] = path

            if path:
                print(f"  -> {path}")
            else:
                print(f"  -> FAILED (no provider or generation error)")

        succeeded = sum(1 for p in results.values() if p is not None)
        print(f"\n[MusicGenerator] Done: {succeeded}/{total} tracks generated.\n")
        return results
