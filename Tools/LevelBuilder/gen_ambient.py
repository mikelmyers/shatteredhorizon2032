"""Synthesize loopable battlefield ambience beds (CC0 / ours) into Tools/incoming/.
Autonomous, no external assets. Wind bed + distant-battle bed, seamlessly looped via
a tail->head crossfade. Run: py Tools/LevelBuilder/gen_ambient.py
"""
import os
import struct
import wave

import numpy as np

SR = 44100
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "incoming")
RNG = np.random.default_rng(7732)


def _ma(x, k):
    return np.convolve(x, np.ones(k) / k, mode="same") if k > 1 else x


def _loopable(sig, xf_sec=0.6):
    """Crossfade the tail into the head so the clip loops seamlessly."""
    xf = int(xf_sec * SR)
    if xf * 2 >= len(sig):
        return sig
    head = sig[:xf].copy()
    tail = sig[-xf:].copy()
    ramp = np.linspace(0, 1, xf)
    sig[:xf] = head * ramp + tail * (1 - ramp)
    return sig[:-xf]


def _norm(sig, peak=0.85):
    return np.clip(sig / (np.max(np.abs(sig)) + 1e-9) * peak, -1, 1)


def _write(name, sig):
    pcm = (_norm(sig) * 32767).astype(np.int16)
    path = os.path.join(OUT, name)
    with wave.open(path, "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", s) for s in pcm))
    return path


def wind(dur=8.0):
    n = int(dur * SR)
    base = _ma(RNG.uniform(-1, 1, n), 800)            # low whoosh
    # slow amplitude swells
    t = np.linspace(0, dur, n)
    lfo = 0.5 + 0.5 * (0.6 * np.sin(2 * np.pi * 0.13 * t) + 0.4 * np.sin(2 * np.pi * 0.31 * t))
    hiss = _ma(RNG.uniform(-1, 1, n), 40) * 0.15      # faint high hiss
    return _loopable((base * lfo + hiss))


def distant_battle(dur=12.0):
    n = int(dur * SR)
    t = np.linspace(0, dur, n)
    rumble = _ma(RNG.uniform(-1, 1, n), 1500) * (0.4 + 0.3 * np.sin(2 * np.pi * 0.07 * t))  # low war rumble
    sig = rumble.copy()
    # sparse distant booms (artillery) + faint small-arms crackle
    for _ in range(int(dur * 0.8)):
        pos = RNG.integers(0, n - SR)
        ln = int(RNG.uniform(0.25, 0.6) * SR)
        env = np.exp(-np.linspace(0, 7, ln))
        boom = _ma(RNG.uniform(-1, 1, ln), 600) * env * RNG.uniform(0.4, 0.9)
        sig[pos:pos + ln] += boom
    for _ in range(int(dur * 6)):
        pos = RNG.integers(0, n - 4000)
        ln = int(RNG.uniform(0.02, 0.05) * SR)
        env = np.exp(-np.linspace(0, 30, ln))
        crack = RNG.uniform(-1, 1, ln) * env * RNG.uniform(0.05, 0.15)
        sig[pos:pos + ln] += crack
    return _loopable(sig)


def main():
    os.makedirs(OUT, exist_ok=True)
    made = [_write("sh_amb_wind.wav", wind()), _write("sh_amb_battle.wav", distant_battle())]
    print("generated %d ambient beds:" % len(made))
    for m in made:
        print("  ", os.path.basename(m))


if __name__ == "__main__":
    main()
