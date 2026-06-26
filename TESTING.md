# Shattered Horizon 2032 — Testing & Evals

How we verify the game actually *plays* (not just compiles). Games don't have a single
"accuracy" number like an ML model — instead you assert a **suite of properties across
tiers**, cheapest/fastest first. This doc maps each tier to what exists here.

## One command

```
powershell -File Tools/run_evals.ps1
```

Runs the unit tests + a headless smoke-playtest + a perf check and prints a **GREEN/RED
scorecard** (also written to `Tools/LevelBuilder/output/eval_scorecard.txt`). Exit code 0 =
all passed, 1 = a regression. **Run it after any change** — yours or an AI's — to confirm the
game still plays. This is the game equivalent of a CI eval gate.

## The testing tiers

| Tier | What it proves | How | In the eval suite |
|---|---|---|---|
| **1. Build** | It compiles | `Build.bat` | (prerequisite) |
| **2. Unit tests** | Game *logic* is correct: damage hit-zones, ballistics ordering, AI perception (sight/hearing/NVG/smoke), suppression, the Primordia decision engine, mission structure | 52 UE automation tests (`SH2032.*`), headless `-nullrhi` | `unit_tests` |
| **3. Smoke / integration** | The core loop runs end-to-end: input live, player spawns, weapon equips, mission loads, no crash | Headless `-game` auto-playtest (`bAutoPlaytest`) | `boot_input_live`, `weapon_equipped`, `mission_loaded`, `no_crashes` |
| **4. Behavioral** | Measurable gameplay properties hold: combat actually fires, ambience plays, no audio errors | Parse the playtest log for markers | `combat_runs`, `ambience_playing`, `no_sound_errors` |
| **5. Performance** | Frame rate stays above the floor | Perf sampler CSV (`bPerfReport`) | `perf_floor` |
| **6. Soak / stress** | No leaks/explosions over long runs / combat at scale | longer `bAutoPlaytest` runs | (manual, on demand) |
| **7. Regression / golden** | Deterministic outcomes don't drift; screenshots match | screenshot harness (`shots.py`), seeded sims | (future: golden baselines) |
| **8. Feel / fun** | Is it *satisfying*? | **Human playtest + telemetry** | ❌ not automatable — that's you |

The big mental shift from ML evals: tiers 2–5 are objective pass/fail and live in CI; **tier 8
(fun/feel) is irreducibly human.** Automation gets you to "it works, it's fair, it performs" —
a person decides "it feels good." We have evidence that forcing the subjective parts
autonomously *degrades* quality (an exposure auto-tune made the look worse and was reverted).

## What the eval suite asserts today

- **unit_tests** — 52 pass, 0 fail
- **boot_input_live** — Enhanced Input mapping context added
- **weapon_equipped** — loadout auto-applied on spawn
- **mission_loaded** — M01 + its 4 phases loaded
- **ambience_playing** — ambient soundscape active
- **combat_runs** — player actually fires under AI
- **no_crashes** — zero fatal errors
- **no_sound_errors** — zero sound-load failures
- **perf_floor** — average FPS ≥ floor (dev box floor is ~4–8; this is a *hardware* floor — the
  scalability split delivers real FPS on target hardware, see AAA_LEVEL_SPEC §4)

## Extending it

- **New gameplay system →** add unit tests in `Source/ShatteredHorizon2032Tests/Tests/` (copy an
  existing `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, name it `SH2032.<Area>.<Thing>`).
- **New behavioral property →** add a `LogHas`/`LogCount` assertion in `run_evals.ps1` keyed on a
  log marker (add a `UE_LOG` if needed).
- **Fairness/encounter metrics →** the auto-playtest already logs firing/range/death; assert
  thresholds (e.g. "a moving player survives first contact", "AI hit-rate within a fair band").

## Note on the AI playtest bot

`bAutoPlaytest` drives a scripted player (walk → face nearest enemy → ADS → fire in bursts →
strafe). It exercises every system end-to-end and is great for smoke/regression, but it is **not
a skill benchmark** — it doesn't lead targets or use cover like a human. Use it to prove the loop
works and doesn't regress, not to judge difficulty balance (that's a human playtest).
