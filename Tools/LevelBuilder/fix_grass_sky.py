# Fix "grass in the sky": the MWLandscapeAutoMaterial procedural grass (LGT_MWAM_Grass)
# spawns across the whole landscape incl. the low beach/under-ocean terrain. At the low
# trench camera angle, that distant grass renders against the ocean/sky ~200m out and
# reads as floating grass. Reducing the grass varieties' END cull distance stops the
# distant grass from drawing while keeping the foreground (dune/trench) grass.
#
# Edits the grass-type asset only (no map change); grass is re-spawned from it on load.
# Headless: UnrealEditor-Cmd.exe <proj> -ExecutePythonScript="...fix_grass_sky.py" -nullrhi
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sh_lib as S  # noqa: E402

GRASS_TYPE = "/Game/MWLandscapeAutoMaterial/Procedurals/LGT_MWAM_Grass"
END_CULL = 12000   # cm (~120 m): near grass stays, horizon grass culls
START_FADE = 9000  # begin fading before the hard cull


def main():
    gt = unreal.load_asset(GRASS_TYPE)
    if not gt:
        S.log("MISSING grass type %s" % GRASS_TYPE)
        return
    varieties = gt.get_editor_property("grass_varieties")
    changed = 0
    new_list = []
    for v in varieties:
        try:
            ecd = v.get_editor_property("end_cull_distance")
            scd = v.get_editor_property("start_cull_distance")
            ecd.set_editor_property("default", END_CULL)
            scd.set_editor_property("default", START_FADE)
            v.set_editor_property("end_cull_distance", ecd)
            v.set_editor_property("start_cull_distance", scd)
            changed += 1
        except Exception as e:  # noqa: BLE001
            S.log("variety edit failed: %s" % e)
        new_list.append(v)
    gt.set_editor_property("grass_varieties", new_list)
    unreal.EditorAssetLibrary.save_loaded_asset(gt)
    msg = "grass cull set: %d/%d varieties -> end=%d start=%d" % (
        changed, len(varieties), END_CULL, START_FADE)
    S.log(msg)
    with open(os.path.join(S.OUT, "fix_grass_result.txt"), "w", encoding="utf-8") as f:
        f.write(msg + "\n")


if __name__ == "__main__":
    main()
