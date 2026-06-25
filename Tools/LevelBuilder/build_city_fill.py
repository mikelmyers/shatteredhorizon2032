# Fill EMPTY urban blocks with real buildings (Wave 6 - remove dead space so the
# city reads as a real town, not empty lots). Trace-detects each block center: if
# it hits the ground (no building), spawns a properly-sized building from the
# project's curated intact/damaged sets (the same assets + sizing logic as
# build_urban.build_blocks). Occupied blocks trace to a rooftop and are skipped,
# so this never duplicates existing buildings.
#
# ADDITIVE + IDEMPOTENT under "Dressing/CityFill". Run headless (kill UnrealEditor first):
#   UnrealEditor-Cmd.exe <proj> -ExecutePythonScript="...build_city_fill.py" -d3d11
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sh_lib as S  # noqa: E402

FOLDER = "Dressing/CityFill"
U = S.PLAN["urban"]
RNG = S.RNG


def _sizes(slot_list):
    out = []
    for e in slot_list:
        p = e["path"] if isinstance(e, dict) else e
        s = e.get("size", "") if isinstance(e, dict) else ""
        try:
            dims = tuple(int(v) for v in s.split("x"))
        except ValueError:
            dims = (1000, 1000, 1000)
        if len(dims) < 2:
            dims = (1000, 1000, 1000)
        out.append((p, dims))
    return out


INTACT = _sizes(S.PICKS["urban_buildings"]["whole_buildings_intact"])
DAMAGED = _sizes([e for e in S.PICKS["urban_buildings"]["whole_buildings_damaged"]
                  if "Skyscraper" not in (e["path"] if isinstance(e, dict) else e)])


def highway_band():
    hw = S.PLAN.get("highway", {})
    if "y0" in hw and "y1" in hw:
        return hw["y0"] - 2500, hw["y1"] + 2500
    if "y" in hw:
        return hw["y"] - 4000, hw["y"] + 4000
    return (1e9, 1e9)  # none


def main():
    S.open_map()
    S.clear_folder_actors(FOLDER)
    hw_lo, hw_hi = highway_band()

    filled = 0
    skipped_occupied = 0
    for blk in U["blocks"]:
        cx, cy, w, d = blk["cx"], blk["cy"], blk["w"], blk["d"]
        if hw_lo <= cy <= hw_hi:
            continue
        # Trace at the block center: a hit well above ground => a building already
        # stands here (or a culled-but-present one) => skip to avoid duplication.
        z = S.trace_z(cx, cy, None)
        if z is None:
            continue
        if z > 800:
            skipped_occupied += 1
            continue

        # Empty block -> place a building sized to fit (matches build_urban logic).
        r = RNG.random()
        dmg_chance = 0.55 if blk.get("row", 9) <= 1 else 0.3   # seaward rows more shelled
        pool = DAMAGED if r < dmg_chance else INTACT
        p, dims = RNG.choice(pool)
        yaw = RNG.choice([0, 90, 180, 270])
        bw, bd = (dims[0], dims[1]) if yaw in (0, 180) else (dims[1], dims[0])
        sc = min(1.0, (w * 0.92) / max(bw, 1), (d * 0.92) / max(bd, 1))
        sc = max(sc, 0.55)
        a = S.spawn_sm(p, cx, cy, yaw + RNG.uniform(-2, 2), scale=(sc, sc, sc),
                       center_xy=True, sink=25, z=z, ground=False,
                       label=f"Fill_{int(cx)}_{int(cy)}", folder=FOLDER)
        if a:
            # Cull distant fills to protect dev-box perf.
            mc = a.static_mesh_component
            mc.set_cull_distances(0, 90000)
            filled += 1

    S.save_map()
    msg = "city fill: %d empty blocks filled, %d already occupied (skipped)" % (
        filled, skipped_occupied)
    S.log(msg)
    with open(os.path.join(S.OUT, "city_fill_result.txt"), "w", encoding="utf-8") as f:
        f.write(msg + "\n")


if __name__ == "__main__":
    main()
