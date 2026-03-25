"""
AI SFX Generator — Generates sound effects via AI APIs or code synthesis.

Providers:
  - ElevenLabs Sound Effects API (cloud)
  - Code Synthesizer (local, no API needed) for mechanical/electronic sounds
"""

import io
import json
import os
import struct
import wave
from abc import ABC, abstractmethod
from pathlib import Path
from typing import Optional

import numpy as np
import requests


class SFXProvider(ABC):
    @abstractmethod
    def generate(self, prompt: str, duration: float) -> Optional[bytes]:
        pass

    @abstractmethod
    def is_available(self) -> bool:
        pass


class ElevenLabsSFXProvider(SFXProvider):
    """ElevenLabs Sound Effects API."""

    def __init__(self, config: dict):
        self.config = config
        self._api_key = os.environ.get(config.get("env_key", "ELEVENLABS_API_KEY"))
        self.base_url = config.get("base_url", "https://api.elevenlabs.io/v1")

    def is_available(self) -> bool:
        return bool(self._api_key) and self.config.get("enabled", False)

    def generate(self, prompt: str, duration: float) -> Optional[bytes]:
        if not self.is_available():
            return None

        url = f"{self.base_url}/sound-generation"
        headers = {"xi-api-key": self._api_key}
        payload = {
            "text": prompt,
            "duration_seconds": min(duration, 22.0),
        }

        try:
            resp = requests.post(url, headers=headers, json=payload, timeout=120)
            resp.raise_for_status()
            return resp.content
        except requests.RequestException as e:
            print(f"[ElevenLabs SFX] Generation failed: {e}")
            return None


class CodeSynthesizer:
    """Generates sounds programmatically — no API needed."""

    def __init__(self, sample_rate: int = 48000):
        self.sr = sample_rate

    def _to_wav_bytes(self, audio: np.ndarray) -> bytes:
        """Convert float audio array to WAV bytes."""
        audio = np.clip(audio, -1.0, 1.0)
        pcm = (audio * 32767).astype(np.int16)
        buf = io.BytesIO()
        with wave.open(buf, "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(self.sr)
            wf.writeframes(pcm.tobytes())
        return buf.getvalue()

    def generate_click(self, duration: float = 0.05, freq: float = 2000.0) -> bytes:
        """Short mechanical click for UI, fire mode switch."""
        n = int(self.sr * duration)
        t = np.linspace(0, duration, n, endpoint=False)
        env = np.exp(-t * 80)
        signal = np.sin(2 * np.pi * freq * t) * env * 0.8
        # Add a sharp transient
        transient = np.zeros(n)
        transient[:int(0.002 * self.sr)] = 0.9
        signal += transient * np.exp(-t * 200)
        return self._to_wav_bytes(signal)

    def generate_static(self, duration: float = 3.0) -> bytes:
        """Radio static noise."""
        n = int(self.sr * duration)
        noise = np.random.randn(n) * 0.3
        # Bandpass to radio frequency range (300-3000 Hz)
        from scipy.signal import butter, filtfilt
        b, a = butter(4, [300, 3000], btype="band", fs=self.sr)
        filtered = filtfilt(b, a, noise)
        # Add crackle bursts
        for _ in range(int(duration * 8)):
            pos = np.random.randint(0, n - 500)
            burst_len = np.random.randint(100, 500)
            filtered[pos:pos + burst_len] += np.random.randn(burst_len) * 0.5
        return self._to_wav_bytes(np.clip(filtered, -1, 1))

    def generate_beep(self, freq: float = 1000.0, duration: float = 0.3,
                      pattern: str = "single") -> bytes:
        """Electronic beep for detection, alerts."""
        if pattern == "single":
            n = int(self.sr * duration)
            t = np.linspace(0, duration, n, endpoint=False)
            env = np.ones(n)
            fade = int(0.01 * self.sr)
            env[:fade] = np.linspace(0, 1, fade)
            env[-fade:] = np.linspace(1, 0, fade)
            signal = np.sin(2 * np.pi * freq * t) * env * 0.6
        elif pattern == "double":
            s1 = self._beep_segment(freq, duration * 0.3)
            gap = np.zeros(int(self.sr * duration * 0.1))
            s2 = self._beep_segment(freq, duration * 0.3)
            signal = np.concatenate([s1, gap, s2])
        elif pattern == "urgent":
            segments = []
            for _ in range(4):
                segments.append(self._beep_segment(freq, duration * 0.15))
                segments.append(np.zeros(int(self.sr * duration * 0.05)))
            signal = np.concatenate(segments)
        else:
            return self.generate_beep(freq, duration, "single")
        return self._to_wav_bytes(signal)

    def _beep_segment(self, freq: float, dur: float) -> np.ndarray:
        n = int(self.sr * dur)
        t = np.linspace(0, dur, n, endpoint=False)
        env = np.ones(n)
        fade = min(int(0.005 * self.sr), n // 4)
        if fade > 0:
            env[:fade] = np.linspace(0, 1, fade)
            env[-fade:] = np.linspace(1, 0, fade)
        return np.sin(2 * np.pi * freq * t) * env * 0.6

    def generate_tone(self, freq: float = 4000.0, duration: float = 5.0) -> bytes:
        """Sustained tone for tinnitus effect."""
        n = int(self.sr * duration)
        t = np.linspace(0, duration, n, endpoint=False)
        # Tinnitus: high freq tone that fades out
        env = np.exp(-t * 0.3)  # Slow fade
        signal = np.sin(2 * np.pi * freq * t) * env * 0.4
        # Add slight frequency wobble
        wobble = np.sin(2 * np.pi * 0.5 * t) * 200
        signal += np.sin(2 * np.pi * (freq + wobble) * t) * env * 0.2
        return self._to_wav_bytes(np.clip(signal, -1, 1))

    def generate_noise_burst(self, duration: float = 0.5,
                              filter_type: str = "lowpass") -> bytes:
        """Filtered noise burst for impacts, explosions."""
        from scipy.signal import butter, filtfilt
        n = int(self.sr * duration)
        noise = np.random.randn(n)
        # Sharp attack, exponential decay
        env = np.exp(-np.linspace(0, 8, n))
        env[:int(0.005 * self.sr)] = np.linspace(0, 1, int(0.005 * self.sr))

        if filter_type == "lowpass":
            b, a = butter(4, 800, btype="low", fs=self.sr)
        elif filter_type == "highpass":
            b, a = butter(4, 2000, btype="high", fs=self.sr)
        elif filter_type == "bandpass":
            b, a = butter(4, [200, 4000], btype="band", fs=self.sr)
        else:
            b, a = butter(4, 1500, btype="low", fs=self.sr)

        filtered = filtfilt(b, a, noise) * env * 0.8
        return self._to_wav_bytes(np.clip(filtered, -1, 1))

    def generate_heartbeat(self, bpm: float = 100, duration: float = 5.0) -> bytes:
        """Heartbeat sound for combat stress."""
        n = int(self.sr * duration)
        signal = np.zeros(n)
        beat_interval = 60.0 / bpm
        samples_per_beat = int(self.sr * beat_interval)

        # Each heartbeat: two thuds (lub-dub)
        beat_len = int(0.08 * self.sr)
        t_beat = np.linspace(0, 0.08, beat_len, endpoint=False)
        lub = np.sin(2 * np.pi * 60 * t_beat) * np.exp(-t_beat * 50)
        dub_len = int(0.06 * self.sr)
        t_dub = np.linspace(0, 0.06, dub_len, endpoint=False)
        dub = np.sin(2 * np.pi * 80 * t_dub) * np.exp(-t_dub * 60) * 0.7

        pos = 0
        while pos + samples_per_beat < n:
            end_lub = min(pos + beat_len, n)
            signal[pos:end_lub] += lub[:end_lub - pos]
            dub_start = pos + int(0.12 * self.sr)
            end_dub = min(dub_start + dub_len, n)
            if dub_start < n:
                signal[dub_start:end_dub] += dub[:end_dub - dub_start]
            pos += samples_per_beat

        return self._to_wav_bytes(np.clip(signal, -1, 1))

    def generate_for_id(self, sfx_id: str, duration: float) -> Optional[bytes]:
        """Route generation based on SFX ID."""
        synth_map = {
            "menu_click": lambda: self.generate_click(0.05, 2000),
            "compass_tick": lambda: self.generate_click(0.03, 3000),
            "squad_command_select": lambda: self.generate_beep(800, 0.3, "double"),
            "heartbeat_stress": lambda: self.generate_heartbeat(110, duration),
            "tinnitus": lambda: self.generate_tone(4200, duration),
            "radio_static": lambda: self.generate_static(duration),
            "radio_crackle": lambda: self.generate_static(duration),
            "comms_restored": lambda: self.generate_beep(1200, 0.5, "double"),
            "comms_jammed": lambda: self.generate_beep(400, 1.0, "urgent"),
            "drone_detected_beep": lambda: self.generate_beep(1500, 0.3, "single"),
            "drone_close_warning": lambda: self.generate_beep(2000, 1.0, "urgent"),
            "drone_jamming_active": lambda: self.generate_static(duration),
            "fire_mode_switch": lambda: self.generate_click(0.1, 1500),
            "dry_fire": lambda: self.generate_click(0.15, 1000),
        }
        fn = synth_map.get(sfx_id)
        if fn:
            return fn()
        return None


class AISFXGenerator:
    """Manages AI providers and code synthesis for SFX generation."""

    def __init__(self, config_path: str):
        with open(config_path) as f:
            config = json.load(f)

        self.providers: list[SFXProvider] = []
        sfx_cfg = config.get("sfx_generation", {})

        if "elevenlabs" in sfx_cfg:
            self.providers.append(ElevenLabsSFXProvider(sfx_cfg["elevenlabs"]))

        self.synth = CodeSynthesizer()

    def get_available_provider(self) -> Optional[SFXProvider]:
        for p in self.providers:
            if p.is_available():
                return p
        return None

    def generate(self, sfx_id: str, prompt: str, duration: float,
                 synth_only: bool = False) -> Optional[bytes]:
        """Generate a sound effect, trying synth first, then AI."""
        # Try code synthesis first for known types
        synth_result = self.synth.generate_for_id(sfx_id, duration)
        if synth_result:
            print(f"[AISFXGenerator] Synthesized: {sfx_id}")
            return synth_result

        if synth_only:
            # Fall back to noise burst for unknown types
            print(f"[AISFXGenerator] Synth fallback (noise burst): {sfx_id}")
            return self.synth.generate_noise_burst(duration, "bandpass")

        # Try AI provider
        provider = self.get_available_provider()
        if provider:
            print(f"[AISFXGenerator] AI generating: {sfx_id}")
            return provider.generate(prompt, duration)

        # Final fallback: noise burst
        print(f"[AISFXGenerator] No provider, noise fallback: {sfx_id}")
        return self.synth.generate_noise_burst(duration, "bandpass")
