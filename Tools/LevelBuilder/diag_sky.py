# Diagnose "stuff in the sky": report actors whose geometry sits high above the
# terrain, count foliage actors + their instance Z-range, and report building Z so
# we can tell PLACEMENT bugs (actor actually in the sky) from RENDER bugs (actor on
# the ground but Nanite-in-SM5 draws it wrong). Headless, read-only (no save):
#   UnrealEditor-Cmd.exe <proj> -ExecutePythonScript="...diag_sky.py" -nullrhi
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sh_lib as S  # noqa: E402

RESULT = os.path.join(S.OUT, "diag_sky_result.txt")
# Terrain tops out ~1100cm urban, mountains higher; tall buildings ~3000-6000cm.
# Anything whose bounds-top is far above that is "in the sky".
SKY_Z = 20000.0  # 200 m


def main():
    S.open_map()
    EAS = S.EAS
    lines = []

    high = []
    foliage = []
    building_zs = []
    for a in EAS.get_all_level_actors():
        cls = a.get_class().get_name()
        label = a.get_actor_label()
        loc = a.get_actor_location()
        try:
            origin, extent = a.get_actor_bounds(False)
            top = origin.z + extent.z
            bot = origin.z - extent.z
        except Exception:  # noqa: BLE001
            top = loc.z
            bot = loc.z

        if "Foliage" in cls:
            foliage.append((label, cls))
        if any(k in label for k in ("Fill_", "Bldg", "Building", "AccuCit")) or "Bldg" in cls:
            building_zs.append((label, round(loc.z), round(bot), round(top)))
        if top > SKY_Z or loc.z > SKY_Z:
            high.append((round(top), label, cls, round(loc.z)))

    high.sort(reverse=True)
    lines.append("=== ACTORS WITH GEOMETRY ABOVE %dm (top_z, label, class, loc_z) ===" % int(SKY_Z / 100))
    for t in high[:40]:
        lines.append("  top=%d  %s  [%s]  loc_z=%d" % (t[0], t[1], t[2], t[3]))
    lines.append("  (total high actors: %d)" % len(high))

    lines.append("")
    lines.append("=== FOLIAGE ACTORS (InstancedFoliage etc.) ===")
    for f in foliage[:20]:
        lines.append("  %s  [%s]" % (f[0], f[1]))
    lines.append("  (total foliage actors: %d)" % len(foliage))

    # Inspect foliage instance Z-ranges (where the grass/trees actually are).
    lines.append("")
    lines.append("=== FOLIAGE/HISM INSTANCE Z-RANGES ===")
    for a in EAS.get_all_level_actors():
        comps = a.get_components_by_class(unreal.InstancedStaticMeshComponent)
        for c in comps:
            try:
                n = c.get_instance_count()
            except Exception:  # noqa: BLE001
                n = 0
            if n <= 0:
                continue
            zmin, zmax = 1e9, -1e9
            step = max(1, n // 200)
            for i in range(0, n, step):
                try:
                    t = c.get_instance_transform(i, True)
                    z = t.translation.z
                    zmin = min(zmin, z)
                    zmax = max(zmax, z)
                except Exception:  # noqa: BLE001
                    pass
            mesh = c.static_mesh.get_name() if c.static_mesh else "?"
            lines.append("  %s  mesh=%s  n=%d  z=[%d..%d]" % (
                a.get_actor_label(), mesh, n, round(zmin), round(zmax)))

    lines.append("")
    lines.append("=== BUILDING Z (sample; ground ~1100cm) ===")
    for b in building_zs[:15]:
        lines.append("  %s  loc_z=%d  bot=%d  top=%d" % (b[0], b[1], b[2], b[3]))
    lines.append("  (total buildings: %d)" % len(building_zs))

    text = "\n".join(lines) + "\n"
    with open(RESULT, "w", encoding="utf-8") as f:
        f.write(text)
    S.log("[diag_sky] wrote report:\n" + text)


if __name__ == "__main__":
    main()
