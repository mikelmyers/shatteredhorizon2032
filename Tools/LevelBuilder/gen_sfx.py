"""Procedurally synthesize placeholder milsim SFX (CC0 / our own) into Tools/incoming/.
Autonomous (no external assets): weapon fire, footsteps per surface, impacts, supersonic
crack, dry-fire, reload clicks. Crude but real audio so the game isn't silent until
better assets are dropped in. Run: py Tools/LevelBuilder/gen_sfx.py
"""
import os
import struct
import wave

import numpy as np

SR = 44100
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "incoming")
RNG = np.random.default_rng(20320612)


def _ma_lowpass(x, k):
    if k <= 1:
        return x
    return np.convolve(x, np.ones(k) / k, mode="same")


def _write(name, sig):
    sig = sig / (np.max(np.abs(sig)) + 1e-9)
    sig = np.clip(sig * 0.92, -1, 1)
    pcm = (sig * 32767).astype(np.int16)
    path = os.path.join(OUT, name)
    with wave.open(path, "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", s) for s in pcm))
    return path


def _noise(n):
    return RNG.uniform(-1, 1, n)


def gunshot(seed_decay=9.0, body_lp=45, bright=1.0):
    n = int(0.30 * SR)
    t = np.linspace(0, 1, n)
    body = _ma_lowpass(_noise(n), body_lp) * np.exp(-seed_decay * t)        # low boom
    crack = _noise(n) * np.exp(-70 * t) * bright                            # sharp transient
    mech = _ma_lowpass(_noise(n), 6) * np.exp(-40 * t) * 0.25               # action snap
    return 0.8 * body + 0.55 * crack + mech


def footstep(lp, decay, vol):
    n = int(0.16 * SR)
    t = np.linspace(0, 1, n)
    return _ma_lowpass(_noise(n), lp) * np.exp(-decay * t) * vol


def impact(lp):
    n = int(0.12 * SR)
    t = np.linspace(0, 1, n)
    click = _noise(n) * np.exp(-90 * t)
    body = _ma_lowpass(_noise(n), lp) * np.exp(-30 * t) * 0.5
    return 0.7 * click + body


def crack():  # supersonic round near-miss: very short high-freq snap
    n = int(0.05 * SR)
    t = np.linspace(0, 1, n)
    return _noise(n) * np.exp(-120 * t)


def click(dur=0.04, lp=8):  # mechanical click (mag/bolt)
    n = int(dur * SR)
    t = np.linspace(0, 1, n)
    return _ma_lowpass(_noise(n), lp) * np.exp(-110 * t)


def main():
    os.makedirs(OUT, exist_ok=True)
    made = []
    # Weapon fire variants (rifle).
    for i, (d, lp, b) in enumerate([(9, 45, 1.0), (8, 55, 0.9), (10, 38, 1.1)], 1):
        made.append(_write(f"sh_wpn_fire_0{i}.wav", gunshot(d, lp, b)))
    # Footsteps per surface.
    made.append(_write("sh_step_sand_01.wav", footstep(70, 55, 0.5)))
    made.append(_write("sh_step_sand_02.wav", footstep(60, 60, 0.5)))
    made.append(_write("sh_step_concrete_01.wav", footstep(20, 70, 0.8)))
    made.append(_write("sh_step_concrete_02.wav", footstep(18, 75, 0.8)))
    made.append(_write("sh_step_grass_01.wav", footstep(90, 50, 0.4)))
    # Impacts + ballistic.
    made.append(_write("sh_impact_01.wav", impact(30)))
    made.append(_write("sh_impact_02.wav", impact(50)))
    made.append(_write("sh_crack_01.wav", crack()))
    # Mechanical.
    made.append(_write("sh_dryfire_01.wav", click(0.05, 6)))
    made.append(_write("sh_magin_01.wav", click(0.08, 10)))
    made.append(_write("sh_boltrelease_01.wav", click(0.06, 5)))
    print("generated %d SFX into %s" % (len(made), OUT))
    for m in made:
        print("  ", os.path.basename(m))


if __name__ == "__main__":
    main()
