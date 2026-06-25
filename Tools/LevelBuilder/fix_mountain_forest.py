# Fix "forest in the sky": HISM_MountainForest is 5,791 pines on the distant
# mountains (~1.4 km from the beach). At that range the trees render against the
# hazy sky and look like floating foliage. Add a cull distance so the mountain
# forest only draws when the player is near it (deep urban / mountain base), not
# from the beach. Foreground gameplay views read clean; the mountains remain a
# (bare, hazed) backdrop instead of a floating-forest band.
#
# Edits the map (saves). Run with NO editor open:
#   UnrealEditor-Cmd.exe <proj> -ExecutePythonScript="...fix_mountain_forest.py" -d3d11
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sh_lib as S  # noqa: E402

START_CULL = 55000   # cm (~550 m): begin fading
END_CULL = 75000     # cm (~750 m): fully culled -> not visible from beach (1.4 km)


def main():
    S.open_map()
    done = 0
    for a in S.EAS.get_all_level_actors():
        if a.get_actor_label() == "HISM_MountainForest":
            for c in a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
                c.set_cull_distances(START_CULL, END_CULL)
                done += 1
    S.save_map()
    msg = "mountain forest cull set on %d component(s): start=%d end=%d" % (
        done, START_CULL, END_CULL)
    S.log(msg)
    with open(os.path.join(S.OUT, "fix_mountain_forest_result.txt"), "w", encoding="utf-8") as f:
        f.write(msg + "\n")


if __name__ == "__main__":
    main()
