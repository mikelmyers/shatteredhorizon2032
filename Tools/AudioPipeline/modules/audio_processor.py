"""
Audio Processor — Post-processing for all generated audio.
Normalization, trimming, looping, filtering, format conversion.
"""

import io
import struct
import wave
from pathlib import Path
from typing import Optional

import numpy as np


class AudioProcessor:
    """Post-processes audio files for game-ready output."""

    def __init__(self, sample_rate: int = 48000, bit_depth: int = 16):
        self.sample_rate = sample_rate
        self.bit_depth = bit_depth

    def load_wav(self, path: Path) -> tuple[np.ndarray, int]:
        """Load a WAV file and return (samples, sample_rate)."""
        with wave.open(str(path), "rb") as wf:
            sr = wf.getframerate()
            n_frames = wf.getnframes()
            n_channels = wf.getnchannels()
            raw = wf.readframes(n_frames)
            sw = wf.getsampwidth()

        if sw == 2:
            samples = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32767.0
        elif sw == 4:
            samples = np.frombuffer(raw, dtype=np.int32).astype(np.float32) / 2147483647.0
        else:
            samples = np.frombuffer(raw, dtype=np.uint8).astype(np.float32) / 128.0 - 1.0

        if n_channels > 1:
            samples = samples.reshape(-1, n_channels).mean(axis=1)

        return samples, sr

    def save_wav(self, audio: np.ndarray, path: Path,
                 sample_rate: Optional[int] = None) -> Path:
        """Save audio array to WAV file."""
        sr = sample_rate or self.sample_rate
        audio = np.clip(audio, -1.0, 1.0)
        pcm = (audio * 32767).astype(np.int16)

        path.parent.mkdir(parents=True, exist_ok=True)
        with wave.open(str(path), "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(sr)
            wf.writeframes(pcm.tobytes())
        return path

    def load_raw_bytes(self, data: bytes) -> tuple[np.ndarray, int]:
        """Load audio from WAV bytes."""
        buf = io.BytesIO(data)
        with wave.open(buf, "rb") as wf:
            sr = wf.getframerate()
            raw = wf.readframes(wf.getnframes())
            sw = wf.getsampwidth()
            nc = wf.getnchannels()

        if sw == 2:
            samples = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32767.0
        else:
            samples = np.frombuffer(raw, dtype=np.uint8).astype(np.float32) / 128.0 - 1.0

        if nc > 1:
            samples = samples.reshape(-1, nc).mean(axis=1)

        return samples, sr

    def normalize(self, audio: np.ndarray, target_peak: float = 0.9) -> np.ndarray:
        """Peak-normalize audio to target level."""
        peak = np.max(np.abs(audio))
        if peak > 0:
            audio = audio * (target_peak / peak)
        return audio

    def trim_silence(self, audio: np.ndarray, threshold_db: float = -40.0) -> np.ndarray:
        """Remove leading and trailing silence."""
        threshold = 10 ** (threshold_db / 20.0)
        above = np.abs(audio) > threshold

        if not np.any(above):
            return audio

        first = np.argmax(above)
        last = len(above) - np.argmax(above[::-1])

        # Keep small padding
        pad = int(0.01 * self.sample_rate)
        first = max(0, first - pad)
        last = min(len(audio), last + pad)

        return audio[first:last]

    def apply_radio_filter(self, audio: np.ndarray) -> np.ndarray:
        """Bandpass filter to simulate radio transmission (300-3000Hz)."""
        from scipy.signal import butter, filtfilt
        b, a = butter(4, [300, 3000], btype="band", fs=self.sample_rate)
        filtered = filtfilt(b, a, audio)
        # Add subtle distortion
        filtered = np.tanh(filtered * 2.0) * 0.7
        return filtered

    def make_loop(self, audio: np.ndarray, crossfade_ms: int = 100) -> np.ndarray:
        """Create seamless loop with crossfade at boundaries."""
        cf_samples = int(self.sample_rate * crossfade_ms / 1000)
        if len(audio) < cf_samples * 3:
            return audio

        fade_out = np.linspace(1, 0, cf_samples)
        fade_in = np.linspace(0, 1, cf_samples)

        # Crossfade tail into head
        result = audio.copy()
        result[:cf_samples] = audio[:cf_samples] * fade_in + audio[-cf_samples:] * fade_out
        result[-cf_samples:] = audio[-cf_samples:] * fade_out + audio[:cf_samples] * fade_in

        return result

    def resample(self, audio: np.ndarray, orig_sr: int,
                 target_sr: Optional[int] = None) -> np.ndarray:
        """Resample audio to target sample rate."""
        target = target_sr or self.sample_rate
        if orig_sr == target:
            return audio
        ratio = target / orig_sr
        n_out = int(len(audio) * ratio)
        indices = np.linspace(0, len(audio) - 1, n_out)
        return np.interp(indices, np.arange(len(audio)), audio)

    def process_sfx(self, audio_bytes: bytes, output_path: Path,
                    loop: bool = False, radio_filter: bool = False) -> Path:
        """Full processing chain for a sound effect."""
        audio, sr = self.load_raw_bytes(audio_bytes)

        # Resample if needed
        audio = self.resample(audio, sr)

        # Trim silence
        audio = self.trim_silence(audio)

        # Apply filters
        if radio_filter:
            audio = self.apply_radio_filter(audio)

        # Make loop if needed
        if loop:
            audio = self.make_loop(audio)

        # Normalize
        audio = self.normalize(audio)

        # Save
        return self.save_wav(audio, output_path)
