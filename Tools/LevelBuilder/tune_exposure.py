# REVERTED: an autonomous exposure/grade "fix" made the look worse (hazy/washed
# out) -- the original golden-hour exposure was intentional and reads better.
# Restores the GlobalPP volume to the original values (fix_exposure.py) and
# disables the color-grade overrides this script had added. The final look is a
# human-sign-off call (AAA_LEVEL_SPEC Wave 7), not an autonomous one.
import os
import sys

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
        # REVERT to the original baseline. Two autonomous exposure attempts both
        # DEGRADED the look (one hazy, one washed-out) — exposure is a human-eye
        # tuning task. Restoring the shipped values; the user tunes from here.
        # (Knobs: auto_exposure_max_brightness, auto_exposure_bias on this GlobalPP.)
        s.set_editor_property("override_auto_exposure_min_brightness", True)
        s.set_editor_property("auto_exposure_min_brightness", -2.0)
        s.set_editor_property("override_auto_exposure_max_brightness", True)
        s.set_editor_property("auto_exposure_max_brightness", 16.0)
        s.set_editor_property("override_auto_exposure_bias", True)
        s.set_editor_property("auto_exposure_bias", 0.3)
        s.set_editor_property("override_auto_exposure_speed_up", True)
        s.set_editor_property("auto_exposure_speed_up", 5.0)
        s.set_editor_property("override_auto_exposure_speed_down", True)
        s.set_editor_property("auto_exposure_speed_down", 2.0)
        s.set_editor_property("override_color_contrast", False)
        s.set_editor_property("override_color_saturation", False)
        s.set_editor_property("override_film_toe", False)
        a.set_editor_property("settings", s)
        done += 1
        S.log("exposure reverted to original baseline (max=16, bias=0.3)")
S.save_map()
with open(os.path.join(HERE, "output", "tune_exposure_done.txt"), "w") as f:
    f.write("reverted exposure on %d GlobalPP volume(s) to baseline (max=16 bias=0.3)\n" % done)
