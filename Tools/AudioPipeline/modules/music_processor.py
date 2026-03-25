"""
Music Processor — Post-processing for generated music tracks.

Handles loudness normalization, loop-point creation, fades, trimming,
and final WAV export. Uses soundfile for I/O and numpy for DSP.
"""

import struct
from pathlib import Path
from typing import Optional

import numpy as np
import soundfile as sf


def _read_audio(path: Path) -> tuple[np.ndarray, int]:
    """Read an audio file and return (samples, sample_rate).
    Samples are float64, shape (N,) for mono or (N, channels) for multi-channel.
    """
    data, sr = sf.read(str(path), dtype="float64", always_2d=False)
    return data, sr


def _write_audio(path: Path, data: np.ndarray, sample_rate: int,
                 subtype: str = "PCM_16") -> Path:
    """Write audio data to a WAV file."""
    path.parent.mkdir(parents=True, exist_ok=True)
    sf.write(str(path), data, sample_rate, subtype=subtype)
    return path


def _to_mono(data: np.ndarray) -> np.ndarray:
    """Convert to mono by averaging channels if multi-channel."""
    if data.ndim == 1:
        return data
    return np.mean(data, axis=1)


def _measure_lufs(data: np.ndarray, sample_rate: int) -> float:
    """Measure integrated loudness in LUFS (simplified ITU-R BS.1770).

    This is a simplified implementation using RMS with K-weighting
    approximation. For production use, consider pyloudnorm.
    """
    mono = _to_mono(data)

    # K-weighting approximation: pre-emphasis high-shelf filter
    # Simple approximation using first-order high-shelf
    # For accurate LUFS, use pyloudnorm library
    from scipy.signal import lfilter

    try:
        # Stage 1: High-shelf boost (+4 dB above 1.5 kHz approximation)
        # Using a simple first-order filter
        fc = 1500.0 / sample_rate
        b_shelf = np.array([1.0 + fc, -(1.0 - fc)]) * 0.5
        a_shelf = np.array([1.0, -(1.0 - 2 * fc)]) * 0.5
        # Normalize
        a_shelf = a_shelf / a_shelf[0]
        b_shelf = b_shelf / b_shelf[0]
        weighted = lfilter(b_shelf, a_shelf, mono)
    except ImportError:
        # Fallback: plain RMS without K-weighting
        weighted = mono

    # Gated measurement: 400ms blocks with -70 LUFS gate
    block_size = int(sample_rate * 0.4)
    if block_size == 0:
        block_size = len(weighted)

    num_blocks = len(weighted) // block_size
    if num_blocks == 0:
        rms = np.sqrt(np.mean(weighted ** 2))
        return 20.0 * np.log10(max(rms, 1e-10)) - 0.691

    block_powers = []
    for i in range(num_blocks):
        block = weighted[i * block_size:(i + 1) * block_size]
        power = np.mean(block ** 2)
        lufs_block = 10.0 * np.log10(max(power, 1e-10)) - 0.691
        if lufs_block > -70.0:  # Absolute gate
            block_powers.append(power)

    if not block_powers:
        rms = np.sqrt(np.mean(weighted ** 2))
        return 20.0 * np.log10(max(rms, 1e-10)) - 0.691

    mean_power = np.mean(block_powers)
    return 10.0 * np.log10(max(mean_power, 1e-10)) - 0.691


def normalize_loudness(track_path: Path, target_lufs: float = -14.0,
                       output_path: Optional[Path] = None) -> Path:
    """Normalize track loudness to target LUFS.

    Args:
        track_path: Input audio file path.
        target_lufs: Target loudness in LUFS (default -14.0 for streaming,
                     -23.0 for broadcast EBU R128).
        output_path: Output path. If None, overwrites input.

    Returns:
        Path to the normalized file.
    """
    data, sr = _read_audio(track_path)
    current_lufs = _measure_lufs(data, sr)

    gain_db = target_lufs - current_lufs
    gain_linear = 10.0 ** (gain_db / 20.0)

    print(f"[Loudness] {track_path.name}: {current_lufs:.1f} LUFS "
          f"-> {target_lufs:.1f} LUFS (gain: {gain_db:+.1f} dB)")

    normalized = data * gain_linear

    # Soft-clip to prevent overs
    peak = np.max(np.abs(normalized))
    if peak > 0.99:
        print(f"[Loudness] Peak {peak:.3f} exceeds limit, applying limiter")
        normalized = np.tanh(normalized * (1.0 / peak) * 0.99) * 0.99

    out = output_path or track_path
    return _write_audio(out, normalized, sr)


def create_loop_points(track_path: Path, crossfade_ms: int = 3000,
                       output_path: Optional[Path] = None) -> Path:
    """Create seamless loop points by crossfading the tail into the head.

    Args:
        track_path: Input audio file path.
        crossfade_ms: Crossfade duration in milliseconds.
        output_path: Output path. If None, overwrites input.

    Returns:
        Path to the looped file.
    """
    data, sr = _read_audio(track_path)
    crossfade_samples = int(sr * crossfade_ms / 1000.0)

    if crossfade_samples >= len(data) // 2:
        print(f"[Loop] Crossfade ({crossfade_ms}ms) too long for track, "
              f"reducing to half length")
        crossfade_samples = len(data) // 4

    if crossfade_samples < 1:
        print(f"[Loop] Track too short for crossfade, skipping")
        out = output_path or track_path
        return _write_audio(out, data, sr)

    # Extract head and tail regions
    tail_start = len(data) - crossfade_samples
    tail_region = data[tail_start:].copy()
    head_region = data[:crossfade_samples].copy()

    # Create equal-power crossfade curves
    t = np.linspace(0, 1, crossfade_samples)
    fade_out = np.sqrt(1 - t)  # Tail fades out
    fade_in = np.sqrt(t)       # Head fades in

    # Apply fades (handle mono and stereo)
    if data.ndim == 2:
        fade_out = fade_out[:, np.newaxis]
        fade_in = fade_in[:, np.newaxis]

    crossfaded = tail_region * fade_out + head_region * fade_in

    # Rebuild: body (after head, before tail) + crossfaded region
    body = data[crossfade_samples:tail_start]
    looped = np.concatenate([crossfaded, body])

    print(f"[Loop] {track_path.name}: {crossfade_ms}ms crossfade, "
          f"{len(data)} -> {len(looped)} samples")

    out = output_path or track_path
    return _write_audio(out, looped, sr)


def create_stems(track_path: Path, stem_count: int = 4,
                 output_dir: Optional[Path] = None) -> list[Path]:
    """Placeholder for future stem separation (e.g., via Demucs).

    Currently copies the full track as a single stem.
    When implemented, will separate into drums, bass, vocals, other.

    Args:
        track_path: Input audio file path.
        stem_count: Number of stems to separate into (2 or 4).
        output_dir: Directory for stem output files.

    Returns:
        List of paths to stem files.
    """
    data, sr = _read_audio(track_path)
    stem_dir = output_dir or track_path.parent / f"{track_path.stem}_stems"
    stem_dir.mkdir(parents=True, exist_ok=True)

    stem_names = ["drums", "bass", "other", "vocals"][:stem_count]
    paths = []

    print(f"[Stems] {track_path.name}: placeholder — copying full mix "
          f"as {stem_count} identical stems")
    print(f"[Stems] TODO: integrate Demucs or similar source separation model")

    for name in stem_names:
        stem_path = stem_dir / f"{track_path.stem}_{name}.wav"
        _write_audio(stem_path, data, sr)
        paths.append(stem_path)

    return paths


def trim_to_duration(track_path: Path, target_seconds: float,
                     output_path: Optional[Path] = None) -> Path:
    """Trim or pad a track to an exact duration.

    If the track is longer, it is truncated. If shorter, silence is appended.

    Args:
        track_path: Input audio file path.
        target_seconds: Desired duration in seconds.
        output_path: Output path. If None, overwrites input.

    Returns:
        Path to the trimmed/padded file.
    """
    data, sr = _read_audio(track_path)
    target_samples = int(sr * target_seconds)
    current_samples = len(data)

    if current_samples > target_samples:
        data = data[:target_samples]
        print(f"[Trim] {track_path.name}: trimmed "
              f"{current_samples / sr:.1f}s -> {target_seconds:.1f}s")
    elif current_samples < target_samples:
        pad_samples = target_samples - current_samples
        if data.ndim == 2:
            padding = np.zeros((pad_samples, data.shape[1]), dtype=data.dtype)
        else:
            padding = np.zeros(pad_samples, dtype=data.dtype)
        data = np.concatenate([data, padding])
        print(f"[Trim] {track_path.name}: padded "
              f"{current_samples / sr:.1f}s -> {target_seconds:.1f}s")
    else:
        print(f"[Trim] {track_path.name}: already {target_seconds:.1f}s")

    out = output_path or track_path
    return _write_audio(out, data, sr)


def fade_in_out(track_path: Path, fade_in_ms: int = 500,
                fade_out_ms: int = 1000,
                output_path: Optional[Path] = None) -> Path:
    """Apply fade-in and fade-out to a track.

    Args:
        track_path: Input audio file path.
        fade_in_ms: Fade-in duration in milliseconds.
        fade_out_ms: Fade-out duration in milliseconds.
        output_path: Output path. If None, overwrites input.

    Returns:
        Path to the faded file.
    """
    data, sr = _read_audio(track_path)

    fade_in_samples = int(sr * fade_in_ms / 1000.0)
    fade_out_samples = int(sr * fade_out_ms / 1000.0)

    fade_in_samples = min(fade_in_samples, len(data))
    fade_out_samples = min(fade_out_samples, len(data))

    # Build fade curves
    if fade_in_samples > 0:
        fade_in_curve = np.linspace(0.0, 1.0, fade_in_samples)
        if data.ndim == 2:
            fade_in_curve = fade_in_curve[:, np.newaxis]
        data[:fade_in_samples] *= fade_in_curve

    if fade_out_samples > 0:
        fade_out_curve = np.linspace(1.0, 0.0, fade_out_samples)
        if data.ndim == 2:
            fade_out_curve = fade_out_curve[:, np.newaxis]
        data[-fade_out_samples:] *= fade_out_curve

    print(f"[Fade] {track_path.name}: in={fade_in_ms}ms, out={fade_out_ms}ms")

    out = output_path or track_path
    return _write_audio(out, data, sr)


def export_wav(track_path: Path, output_path: Path,
               sample_rate: int = 48000, bit_depth: int = 16) -> Path:
    """Export a track as a WAV file with specific sample rate and bit depth.

    Resamples if necessary and converts to the target bit depth.

    Args:
        track_path: Input audio file path.
        output_path: Output WAV file path.
        sample_rate: Target sample rate in Hz (default 48000).
        bit_depth: Target bit depth (16 or 24, default 16).

    Returns:
        Path to the exported file.
    """
    data, sr = _read_audio(track_path)

    # Resample if needed
    if sr != sample_rate:
        try:
            from scipy.signal import resample
            num_samples = int(len(data) * sample_rate / sr)
            if data.ndim == 2:
                resampled = np.zeros((num_samples, data.shape[1]))
                for ch in range(data.shape[1]):
                    resampled[:, ch] = resample(data[:, ch], num_samples)
                data = resampled
            else:
                data = resample(data, num_samples)
            print(f"[Export] Resampled {sr} -> {sample_rate} Hz")
        except ImportError:
            print(f"[Export] WARNING: scipy not available, skipping resample "
                  f"({sr} Hz != {sample_rate} Hz)")

    subtype_map = {
        16: "PCM_16",
        24: "PCM_24",
        32: "FLOAT",
    }
    subtype = subtype_map.get(bit_depth, "PCM_16")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    sf.write(str(output_path), data, sample_rate, subtype=subtype)

    file_size = output_path.stat().st_size / (1024 * 1024)
    duration = len(data) / sample_rate
    print(f"[Export] {output_path.name}: {duration:.1f}s, "
          f"{sample_rate}Hz, {bit_depth}bit, {file_size:.1f}MB")

    return output_path
