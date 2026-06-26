# Additive shoreline fortification pass (Wave 6 — make the beach read as a
# contested Taiwan landing, per LEVEL_REFERENCE.md). Spawns a concrete sea wall
# along the dune base + anti-landing obstacle fields (tetrapod-style sea-defenses
# and dragon's-teeth blocks) at the waterline, all with the CC0 concrete material.
#
# ADDITIVE + IDEMPOTENT: only spawns actors under the "Dressing/SeaDefense" outliner
# folder (cleared and rebuilt each run). Never modifies the landscape or existing
# meshes, so it can't break the playable level. Run headless:
#   UnrealEditor-Cmd.exe <proj> -ExecutePythonScript="...build_sea_defense.py"
import math
import os
import sys

import unreal

# Ensure this script's directory is importable when run via -ExecutePythonScript.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sh_lib as S  # noqa: E402

CUBE = "/Engine/BasicShapes/Cube"
CYL = "/Engine/BasicShapes/Cylinder"
CONCRETE = "/Game/SH/Materials/M_SH_concrete"
ROCK = "/Game/SH/Materials/M_SH_rock"
FOLDER = "Dressing/SeaDefense"

P = S.PLAN
RNG = S.RNG
EAS = S.EAS


def _spawn_mesh_xform(mesh_path, location, rotator, scale, material, label, folder):
    """Spawn a static mesh actor with a full rotator (spawn_sm is yaw-only)."""
    mesh = S.asset(mesh_path)
    if not mesh:
        return None
    a = EAS.spawn_actor_from_class(unreal.StaticMeshActor, location, rotator)
    a.set_mobility(unreal.ComponentMobility.MOVABLE)
    mc = a.static_mesh_component
    mc.set_static_mesh(mesh)
    if material:
        mc.set_material(0, S.asset(material))
    a.set_actor_scale3d(unreal.Vector(*scale))
    a.set_actor_location(location, False, False)
    a.set_mobility(unreal.ComponentMobility.STATIC)
    a.set_actor_label(label)
    a.set_folder_path(folder)
    return a


def build_sea_wall():
    """Low broken concrete revetment along the dune base (cover + authentic)."""
    dune = P["zones"]["dunes_y"]
    wall_y = dune[0] - 600.0  # just seaward of the dune base
    x0, x1 = P["playable_x"][0] + 4000, P["playable_x"][1] - 4000
    seg_len_m = 22.0
    step = seg_len_m * 100 + RNG.uniform(200, 700)  # gaps -> firing positions
    x = x0
    i = 0
    n = 0
    while x < x1:
        # occasional gap (breach / damaged section)
        if RNG.random() < 0.18:
            x += step
            i += 1
            continue
        z = S.trace_z(x, wall_y, 0.0)
        h = RNG.uniform(2.6, 3.4)
        S.spawn_sm(CUBE, x, wall_y + RNG.uniform(-120, 120),
                   yaw=RNG.uniform(-3, 3), z=z, ground=False,
                   scale=(seg_len_m, 2.2, h), material=CONCRETE,
                   label=f"SeaWall_{i}", folder=FOLDER + "/Wall")
        # sandbag-height step in front of some segments (firing step)
        n += 1
        x += step
        i += 1
    S.log(f"sea wall: {n} segments")
    return n


def build_tetrapod(cx, cy, cz, size, folder):
    """One 4-legged concrete tetrapod from cylinders (the iconic Taiwan sea defense)."""
    dirs = [(1, 1, 1), (1, -1, -1), (-1, 1, -1), (-1, -1, 1)]
    leg_len = size * 0.9
    leg_rad = size * 0.32
    inv = 1.0 / math.sqrt(3.0)  # tetrahedral dirs are (+/-1,+/-1,+/-1), |.|=sqrt(3)
    for j, d in enumerate(dirs):
        v = unreal.Vector(d[0] * inv, d[1] * inv, d[2] * inv)
        # The basic Cylinder is oriented along +Z; align its +Z to the leg direction.
        rotr = unreal.MathLibrary.make_rot_from_z(v)
        # offset the leg so its inner end sits at the tetrapod center
        off = v * (leg_len * 50.0)  # cylinder native height 100uu; half-len offset
        loc = unreal.Vector(cx + off.x, cy + off.y, cz + off.z + size * 30.0)
        _spawn_mesh_xform(
            CYL, loc, rotr,
            (leg_rad, leg_rad, leg_len), CONCRETE,
            f"Tetra_{int(cx)}_{int(cy)}_{j}", folder)


def build_tetrapod_field():
    """Piles of tetrapods + dragon's-teeth blocks along the surf/wet-sand line."""
    surf = P["zones"]["surf_y"]
    wet = P["zones"]["wet_sand_y"]
    y_lo, y_hi = surf[1] - 1500, wet[1]  # waterline band
    x0, x1 = P["playable_x"][0] + 3000, P["playable_x"][1] - 3000
    clusters = 16
    tetra_total = 0
    for c in range(clusters):
        cx = RNG.uniform(x0, x1)
        cy = RNG.uniform(y_lo, y_hi)
        count = RNG.randint(3, 6)
        for k in range(count):
            tx = cx + RNG.uniform(-700, 700)
            ty = cy + RNG.uniform(-500, 500)
            tz = S.trace_z(tx, ty, 0.0)
            size = RNG.uniform(2.2, 3.4)
            build_tetrapod(tx, ty, tz, size, FOLDER + "/Tetrapods")
            tetra_total += 1
    S.log(f"tetrapods: {tetra_total} units across {clusters} clusters")

    # Dragon's-teeth blocks scattered between the tetrapods and the wall.
    teeth = 0
    dry = P["zones"]["dry_sand_y"]
    for _ in range(40):
        tx = RNG.uniform(x0, x1)
        ty = RNG.uniform(wet[0], dry[1])
        tz = S.trace_z(tx, ty, 0.0)
        s = RNG.uniform(1.1, 1.8)
        _spawn_mesh_xform(
            CUBE, unreal.Vector(tx, ty, tz + s * 40),
            S.rot(RNG.uniform(-8, 8), RNG.uniform(0, 360), RNG.uniform(-8, 8)),
            (s, s, s * 1.3), CONCRETE, f"Teeth_{teeth}", FOLDER + "/Teeth")
        teeth += 1
    S.log(f"dragon's teeth: {teeth} blocks")


def main():
    S.open_map()
    S.clear_folder_actors(FOLDER)
    nwall = build_sea_wall()
    build_tetrapod_field()
    S.save_map()
    res = os.path.join(S.OUT, "sea_defense_result.txt")
    with open(res, "w", encoding="utf-8") as f:
        f.write("sea defense built: wall=%d segments + tetrapod field + teeth\n" % nwall)
    S.log("sea defense pass complete, map saved")


if __name__ == "__main__":
    main()
