"""
Voice Processor — Post-processing for AI-generated voice lines.
Radio filter, stress effects, normalization.
"""

import io
import wave
from pathlib import Path
from typing import Optional

import numpy as np
from scipy.signal import butter, filtfilt, resample


class VoiceProcessor:
    """Post-processes voice lines for in-game use."""

    def __init__(self, sample_rate: int = 48000):
        self.sr = sample_rate

    def load_wav(self, path: Path) -> tuple[np.ndarray, int]:
        """Load audio from WAV file."""
        with wave.open(str(path), "rb") as wf:
            sr = wf.getframerate()
            raw = wf.readframes(wf.getnframes())
            sw = wf.getsampwidth()
            nc = wf.getnchannels()

        if sw == 2:
            audio = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32767.0
        else:
            audio = np.frombuffer(raw, dtype=np.uint8).astype(np.float32) / 128.0 - 1.0

        if nc > 1:
            audio = audio.reshape(-1, nc).mean(axis=1)

        return audio, sr

    def save_wav(self, audio: np.ndarray, path: Path) -> Path:
        """Save audio to WAV file."""
        audio = np.clip(audio, -1.0, 1.0)
        pcm = (audio * 32767).astype(np.int16)
        path.parent.mkdir(parents=True, exist_ok=True)
        with wave.open(str(path), "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(self.sr)
            wf.writeframes(pcm.tobytes())
        return path

    def apply_radio_filter(self, audio: np.ndarray,
                            sr: Optional[int] = None) -> np.ndarray:
        """Bandpass 300-3000Hz to simulate radio transmission."""
        fs = sr or self.sr
        b, a = butter(4, [300, 3000], btype="band", fs=fs)
        filtered = filtfilt(b, a, audio)
        # Light saturation for radio character
        filtered = np.tanh(filtered * 1.5) * 0.8
        return filtered

    def apply_stress_effect(self, audio: np.ndarray, stress_level: str,
                             sr: Optional[int] = None) -> np.ndarray:
        """Modify audio based on stress level."""
        fs = sr or self.sr

        if stress_level == "calm":
            # Slightly slower, lower pitch
            target_len = int(len(audio) / 0.95)
            audio = np.interp(
                np.linspace(0, len(audio) - 1, target_len),
                np.arange(len(audio)), audio
            )
        elif stress_level == "stressed":
            # Faster, slight pitch up
            target_len = int(len(audio) / 1.08)
            audio = np.interp(
                np.linspace(0, len(audio) - 1, target_len),
                np.arange(len(audio)), audio
            )
            # Add slight breathiness (high-freq noise)
            noise = np.random.randn(len(audio)) * 0.03
            b, a = butter(4, 2000, btype="high", fs=fs)
            noise = filtfilt(b, a, noise)
            audio = audio + noise

        return np.clip(audio, -1.0, 1.0)

    def normalize(self, audio: np.ndarray, target_peak: float = 0.9) -> np.ndarray:
        """Peak normalize."""
        peak = np.max(np.abs(audio))
        if peak > 0:
            audio = audio * (target_peak / peak)
        return audio

    def trim_silence(self, audio: np.ndarray,
                      threshold_db: float = -35.0) -> np.ndarray:
        """Remove leading/trailing silence."""
        threshold = 10 ** (threshold_db / 20.0)
        above = np.abs(audio) > threshold
        if not np.any(above):
            return audio
        first = max(0, np.argmax(above) - int(0.02 * self.sr))
        last = min(len(audio), len(audio) - np.argmax(above[::-1]) + int(0.05 * self.sr))
        return audio[first:last]

    def process_voice_line(self, input_path: Path, output_path: Path,
                            stress_level: str = "normal",
                            radio_filter: bool = False) -> Path:
        """Full processing chain for a voice line."""
        audio, sr = self.load_wav(input_path)

        # Apply stress effect
        audio = self.apply_stress_effect(audio, stress_level, sr)

        # Optional radio filter
        if radio_filter:
            audio = self.apply_radio_filter(audio, sr)

        # Trim and normalize
        audio = self.trim_silence(audio)
        audio = self.normalize(audio)

        return self.save_wav(audio, output_path)
