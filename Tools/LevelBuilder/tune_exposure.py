# Tune the GlobalPP exposure. The scene was over-exposed (washed out), so we expose
# DOWN via a negative Exposure Compensation (bias) -- the clean, direct knob. Adjust
# EXPOSURE_BIAS and re-run to dial it (more negative = darker).
import os
import sys

EXPOSURE_BIAS = -1.5   # was 0.3 (over-bright). negative = darker.

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import unreal  # noqa: E402
import sh_lib as S  # noqa: E402

S.open_map()
done = 0
for a in S.EAS.get_all_level_actors():
    if a.get_actor_label() == "GlobalPP":
        s = a.get_editor_property("settings")
        # Restore original exposure (fix_exposure.py).
        # The scene is OVER-exposed (washed out), so expose DOWN — directly, via a
        # strong negative Exposure Compensation (bias). Keep the adaptation range at
        # the original max=16 (lowering it moved the wrong way before) and add NO
        # color grading (that caused the haze before). One clean knob, the right way.
        s.set_editor_property("override_auto_exposure_min_brightness", True)
        s.set_editor_property("auto_exposure_min_brightness", -2.0)
        s.set_editor_property("override_auto_exposure_max_brightness", True)
        s.set_editor_property("auto_exposure_max_brightness", 16.0)
        s.set_editor_property("override_auto_exposure_bias", True)
        s.set_editor_property("auto_exposure_bias", EXPOSURE_BIAS)   # negative = darker
        s.set_editor_property("override_auto_exposure_speed_up", True)
        s.set_editor_property("auto_exposure_speed_up", 5.0)
        s.set_editor_property("override_auto_exposure_speed_down", True)
        s.set_editor_property("auto_exposure_speed_down", 2.0)
        s.set_editor_property("override_color_contrast", False)
        s.set_editor_property("override_color_saturation", False)
        s.set_editor_property("override_film_toe", False)
        a.set_editor_property("settings", s)
        done += 1
        S.log("exposure bias set to %.1f (expose down)" % EXPOSURE_BIAS)
S.save_map()
with open(os.path.join(HERE, "output", "tune_exposure_done.txt"), "w") as f:
    f.write("exposure bias set on %d GlobalPP volume(s): bias=%.1f max=16\n" % (done, EXPOSURE_BIAS))
