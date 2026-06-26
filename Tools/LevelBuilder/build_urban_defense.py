# Additive urban-combat fortification pass (Wave 6 - make the city read as a
# CONTESTED battlefield per LEVEL_REFERENCE.md). Places jersey-barrier chicane
# checkpoints at street intersections, rubble piles off building edges, and
# concrete roadblocks - all on road centerlines / block edges (which the level
# plan guarantees are open), so nothing clips into buildings.
#
# ADDITIVE + IDEMPOTENT under outliner folder "Dressing/UrbanDefense". Uses only
# basic shapes + the CC0 M_SH materials. Run headless (kill all UnrealEditor first):
#   UnrealEditor-Cmd.exe <proj> -ExecutePythonScript="...build_urban_defense.py" -d3d11
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sh_lib as S  # noqa: E402

CUBE = "/Engine/BasicShapes/Cube"
CONCRETE = "/Game/SH/Materials/M_SH_concrete"
ROCK = "/Game/SH/Materials/M_SH_rock"
FOLDER = "Dressing/UrbanDefense"

U = S.PLAN["urban"]
RNG = S.RNG


def jersey_barrier(x, y, yaw, folder, label):
    """A concrete jersey barrier (~3m x 0.85m x 1m)."""
    z = S.trace_z(x, y, None)
    if z is None or z > 2000:   # skip if the trace hit a rooftop, not the street
        return False
    S.spawn_sm(CUBE, x, y, yaw=yaw, z=z, ground=False,
               scale=(3.0, 0.85, 1.0), material=CONCRETE,
               label=label, folder=folder, sink=0.0)
    return True


def checkpoint(sx, ay, idx):
    """A staggered jersey-barrier chicane controlling one intersection approach."""
    f = FOLDER + "/Checkpoints"
    # Chicane across the avenue (barriers run along X, staggered in Y) leaving a gap.
    placed = 0
    base_yaw = RNG.choice([0.0, 90.0])
    for k in range(4):
        # stagger: alternate sides to force a zigzag through the checkpoint
        if base_yaw == 0.0:  # barriers span X, staggered along the street (X), offset in Y
            bx = sx + (k - 1.5) * 360
            by = ay + (260 if k % 2 == 0 else -260)
        else:                # barriers span Y, offset in X
            bx = sx + (260 if k % 2 == 0 else -260)
            by = ay + (k - 1.5) * 360
        if jersey_barrier(bx, by, base_yaw + RNG.uniform(-4, 4), f, f"CP{idx}_{k}"):
            placed += 1
    return placed


def rubble_pile(cx, cy, folder, idx):
    """A pile of broken concrete/rock chunks (war damage)."""
    n = RNG.randint(4, 8)
    placed = 0
    for k in range(n):
        rx = cx + RNG.uniform(-350, 350)
        ry = cy + RNG.uniform(-350, 350)
        z = S.trace_z(rx, ry, None)
        if z is None or z > 2000:
            continue
        s = RNG.uniform(0.35, 1.1)
        mat = CONCRETE if RNG.random() < 0.5 else ROCK
        S.spawn_sm(CUBE, rx, ry,
                   yaw=RNG.uniform(0, 360), z=z + s * 30, ground=False,
                   scale=(s, s * RNG.uniform(0.7, 1.3), s * RNG.uniform(0.6, 1.0)),
                   pitch=RNG.uniform(-25, 25), roll=RNG.uniform(-25, 25),
                   material=mat, label=f"Rubble{idx}_{k}", folder=folder)
        placed += 1
    return placed


def main():
    S.open_map()
    S.clear_folder_actors(FOLDER)

    streets = U["streets"]   # X positions of N-S roads
    avenues = U["avenues"]   # Y positions of E-W roads
    blocks = U["blocks"]

    # --- Checkpoints at a selection of intersections (control key crossings) ---
    cp_total = 0
    inter = [(sx, ay) for sx in streets for ay in avenues]
    RNG.shuffle(inter)
    for i, (sx, ay) in enumerate(inter[:10]):
        cp_total += checkpoint(sx, ay, i)
    S.log(f"urban checkpoints: {cp_total} barriers across 10 intersections")

    # --- Rubble piles just off building-block edges (road side) ---
    rub_total = 0
    RNG.shuffle(blocks)
    for i, b in enumerate(blocks[:24]):
        cx, cy, w, d = b["cx"], b["cy"], b["w"], b["d"]
        # pick a random edge, step out onto the road
        edge = RNG.choice(["n", "s", "e", "w"])
        if edge == "n":
            px, py = cx + RNG.uniform(-w * 0.3, w * 0.3), cy + d * 0.5 + 500
        elif edge == "s":
            px, py = cx + RNG.uniform(-w * 0.3, w * 0.3), cy - d * 0.5 - 500
        elif edge == "e":
            px, py = cx + w * 0.5 + 500, cy + RNG.uniform(-d * 0.3, d * 0.3)
        else:
            px, py = cx - w * 0.5 - 500, cy + RNG.uniform(-d * 0.3, d * 0.3)
        rub_total += rubble_pile(px, py, FOLDER + "/Rubble", i)
    S.log(f"urban rubble: {rub_total} chunks across 24 piles")

    S.save_map()
    with open(os.path.join(S.OUT, "urban_defense_result.txt"), "w", encoding="utf-8") as f:
        f.write("urban defense built: %d checkpoint barriers + %d rubble chunks\n"
                % (cp_total, rub_total))
    S.log("urban defense pass complete, map saved")


if __name__ == "__main__":
    main()
