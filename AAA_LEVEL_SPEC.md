# Shattered Horizon 2032 — "One Level to AAA" Production Spec

**Goal:** Take the single playable level (M01 Taoyuan Beach) and raise it to a AAA-quality
**vertical slice** across all eight greatness pillars — built entirely by **you + AI**, using
**free / AI-generated assets first**, marketplace only later, no commissions.

**Constraints (locked):**
- **Team:** solo dev + Claude. Every task must be doable via code, headless automation, AI
  generation, or a free download. No "hand-paint a texture" / "hire an animator" tasks.
- **Assets:** maximize free + AI-generated. Marketplace (Fab/Synty/pro audio) is a *future*
  fallback for gaps only. Track licenses; prefer CC0, then CC-BY, avoid non-commercial.
- **Performance:** the project must remain **playable on the dev box (i5-8400 / Intel UHD630)**
  for authoring + validation, while the **AAA visual ceiling targets mid/high PC + console.**
  → The whole project is built **scalable**: a **Low (SM5, baked-lighting) path** that runs on
  the dev box, and a **High (Lumen/Nanite) path** for the real target. (Honest note: true AAA
  *visuals* at good FPS are not achievable on UHD630 — the floor target there is "stable and
  readable," the ceiling is realized on a 3060+/console.)
- **Feel:** realism-forward ("reality is key"), lethal but readable — a blend of
  Arma/Tarkov depth with Sandstorm/Ready-or-Not readability.

**How we know it's done:** each wave has explicit **acceptance criteria** validated with the
**auto-playtest harness** (`bAutoPlaytest` + HUD soak screenshots) and the headless build.

---

## 0. Reading guide

- **§1 Pillars & target bar** — the AAA definition we're aiming at, per pillar.
- **§2 The Waves** — the ordered production plan (Wave 0–9). This is the spine.
- **§3 Asset Sourcing Catalog** — exact free/AI sources per asset type, license, integration path. *(The user's explicit priority.)*
- **§4 Scalability & Performance framework** — Low/High split, how the dev box stays playable.
- **§5 Tooling & automation** — pipeline extensions, validation, QA.
- **§6 Risk register & honest limits.**
- **§7 Definition of "AAA done" for the slice.**

Waves are sequenced by **leverage** (biggest felt gap first) and **dependency** (foundations
before content). Each wave is independently shippable — the game is playable after every wave.

---

## 1. Pillars & the target bar

| # | Pillar | AAA bar (what we're chasing) | SH2032 today |
|---|---|---|---|
| 1 | Game feel / juice | Layered feedback on every action, <100ms input→response | Core juice in (hit markers, fire kick, damage indicators); gaps in VFX/decals |
| 2 | Gunfeel | Heavy readable recoil, distinct guns, animated reloads | Deep sim; **no authored animation**, untuned |
| 3 | Movement | Intentional, physical, responsive | Tuned this session; needs anim + in-hand tuning |
| 4 | **Audio** | The milsim differentiator — soundscape *is* gameplay | **Systems exist, ~no assets** (biggest gap) |
| 5 | AI | Flank, cover, fair reactions, human timing | Engages; **"crack-shot" fairness risk**, no tells |
| 6 | Level design | Sightlines, cover, pacing, choice | One level, daylight, unvalidated by human play |
| 7 | Systemic depth | Ballistics, wounding, suppression | **Crown jewel — already AAA-tier on paper** |
| 8 | Production polish | Cohesive art/audio/anim/UI/music | Pre-alpha presentation |

The plan's job: bring 1–6 and 8 up to where 7 already is.

---

## 2. THE WAVES

> Each wave: **Objective · Why · Tasks · Assets (source) · Acceptance · Validation.**

---

### WAVE 0 — Foundations: scalability, pipeline, validation, perf baseline
*Unblocks everything; guarantees we keep running on the dev box.*

**Objective.** Establish the Low/High scalability split, harden the asset-import pipeline,
expand the validation harness, and capture an honest perf baseline so every later wave is
measured, not guessed.

**Why.** We can't add Megascans/Lumen blindly — the dev box will die. We need a switch:
author/validate on Low, target the look on High. And we need fast, repeatable validation
(we already have the auto-playtest harness — formalize it).

**Tasks.**
1. **Scalability buckets.** Author `DefaultScalability.ini` + a runtime quality selector:
   - **Low (dev box):** SM5, **baked lighting** (GPU Lightmass), Lumen OFF, Nanite OFF/fallback
     meshes, VSM off → CSM low, foliage/draw-distance reduced, 720p + screen-percentage.
   - **High (3060+/console):** Lumen GI+reflections, Nanite, VSM, 1080–1440p.
   - Wire a startup flag + in-game Settings (we have `SHSettingsWidget`) to switch.
2. **Lighting strategy decision (load-bearing):** the Low path uses **baked static lighting**
   for the daylight scene — this is the #1 trick to get a near-AAA look on weak HW. Set the
   directional sun + sky to **Stationary/Static**, build with GPU Lightmass. High path overrides
   to Lumen. (Memory gotcha: editor-spawned lights default Stationary — verify, set explicitly.)
3. **Pipeline hardening.** Fix the known-broken weapon-data importer path; add a generic
   **batch asset importer** (headless Python) that ingests downloaded packs (meshes, textures,
   audio, anims) into `/Game/SH/...` with correct settings (sRGB flags, compression, LOD,
   sound class). One script, data-driven from a manifest JSON.
4. **Validation harness v2.** Promote `bAutoPlaytest` into a small **scenario system**: named
   routines (move-test, gunfeel-test, encounter-test, perf-flythrough) selectable by ini, each
   logging structured metrics (avg FPS, frame-time spikes, shots/hits/kills, deaths). Keep the
   HUD soak screenshots.
5. **Perf baseline.** Capture `stat unit`, `stat gpu`, draw calls, primitive count on the dev
   box now → numbers to beat. Add a `-perfreport` routine that writes a CSV.

**Assets.** None (engine + code + automation only).

**Acceptance.**
- One command launches Low on the dev box and stays **≥ a defined floor FPS** (target: 30 @720p
  Low after baking; record the real number).
- Same project launches High on a target rig with Lumen/Nanite.
- Batch importer ingests a sample free pack end-to-end into the level.
- Auto-playtest writes a metrics CSV.

**Validation.** Headless build green; `-perfreport` CSV on Low; screenshot confirms baked-light scene.

---

### WAVE 1 — Audio (the #1 differentiator)
*A milsim with no soundscape cannot be great. Highest leverage.*

**Objective.** Wire a full milsim soundscape: weapon fire/mechanical, impacts, footsteps,
ambience, suppression, supersonic cracks, reverb, radio comms — so the moment-to-moment is
audio-driven (Insurgency/Tarkov standard).

**Why.** Audio is the genre's primary sense; we already have the *systems*
(`SHFootstepSystem`, `SHAmbientSoundscape`, `SHReverbZoneManager`, supersonic hooks) but
essentially no sounds wired. Wiring even placeholder SFX transforms feel instantly.

**Tasks.**
1. **Audio architecture.** Establish Sound Classes/Mixes (Weapons, Foley, Ambience, VO/Radio,
   UI, Music), attenuation/occlusion settings, and a `SHAudioSystem` master mix (already exists —
   extend). Distance-based low-pass + reverb sends.
2. **Weapon audio.** Per-weapon fire report (distinct character per gun — AK vs M4), tail,
   mechanical (bolt, trigger, dry-fire, mag in/out, bolt release), suppressed variants, fire-mode
   switch. Hook into `SHWeaponBase::PlayFireSound`/reload/`ClearMalfunction` (call sites already
   exist — they just need assets + a few extra hooks).
3. **Footsteps (wire the built system).** Attach `USHFootstepSystem` to `ASHPlayerCharacter`;
   drive `PlayFootstep` from **stride detection in code** (speed+timer) as a fallback until anim
   notifies exist; populate the surface sound DB (sand/concrete/water/grass/metal — the level's
   real surfaces); confirm `MakeNoise` → AI hearing.
4. **Ballistic audio.** Supersonic crack near-miss (hook exists in `SHProjectile`), bullet
   impacts per surface, whizz-by, ricochet tails.
5. **Ambience & reverb.** Layered bed (ocean/wind/distant battle — `SHAmbientSoundscape`);
   reverb zones (open beach vs urban interior vs trench) via `SHReverbZoneManager` + **impulse
   responses** from OpenAIR.
6. **Suppression audio.** Muffled "ear-ringing"/low-pass ducking when suppressed (ties to the
   existing suppression value) — the Sandstorm signature.
7. **Radio/VO.** Squad callouts ("Man down", contact reports — `ESHVoiceLineType` exists) via
   **AI TTS** with radio FX (band-pass + static + comms blips). EW jamming already garbles orders;
   add the audio garble.
8. **Music.** Minimal adaptive stingers (contact/calm/objective) — restrained, milsim-appropriate.

**Assets & sources (free/AI — see §3 for detail).**
- **SFX:** Sonniss GDC Game Audio bundles (free, royalty-free, commercial OK — huge), Freesound
  (filter **CC0**), OpenGameArt, gamesounds.xyz. Gun-specific free packs from Sonniss.
- **AI-generated SFX:** ElevenLabs text-to-SFX (free tier), Meta **AudioGen/AudioCraft** (open,
  local) for impacts/foley/whizz-bys, Stable Audio.
- **Radio VO:** **Piper TTS** or **Coqui XTTS** (open, local, free, commercial-OK) for squad
  callouts; ElevenLabs free tier for hero lines. Apply radio FX in-engine (Submix effects).
- **Reverb IRs:** OpenAIR (open impulse-response library).
- **Music:** Incompetech (Kevin MacLeod, CC-BY), Sonniss music, or AI (Stable Audio / AIVA free).

**Acceptance.**
- Firing, reloading, footsteps (per surface), impacts, supersonic cracks, ambience, and at least
  one radio callout all audible and mixed.
- Suppression audibly ducks/muffles.
- Two weapons sound distinct.

**Validation.** Auto-playtest gunfeel routine; capture audio via OBS/log markers; manual listen.
(Audio can't be screenshotted — validate by the log call-count + a human listen pass.)

---

### WAVE 2 — Animation & viewmodel feel
*Half of gunfeel is animation. Currently bind-pose-ish sample arms.*

**Objective.** Believable first-person arms + weapon animation: idle/sway (have, procedural),
fire, reload (tactical + empty), ADS raise/lower, sprint lower, inspect, fire-mode, jam-clear —
layered on the procedural systems we already own.

**Why.** Pillars 1–2. The sim is deep but the *hands* read as placeholder; AAA gunfeel is
animation + procedural recoil + camera kick together (we have the latter two).

**Tasks.**
1. **Rig & retarget.** Standardize on a single FP arms skeleton (UE Mannequin or the sample
   pack we use); set up **IK Retargeter** so we can pull free humanoid anims onto it.
2. **Core montages per weapon archetype** (rifle/pistol/launcher): fire, reload (tac/empty),
   ADS in/out, equip/holster, sprint pose, jam-clear, fire-select. Wire to the existing
   `FireMontage_FP`/`ReloadMontage_FP`/etc. slots in `USHWeaponDataAsset` (they're already
   referenced — they just need real montages).
3. **Procedural layer integration.** Keep `SHWeaponAnimSystem` (sway/bob/recoil/breathing)
   composing on top; tune additive blend so authored + procedural don't fight.
4. **Hand IK to weapon** (left-hand to foregrip/mag via socket) for credibility.
5. **Enemy/squad anims.** Hit reactions/flinch (montages referenced by `SHHitFeedback` — need
   assets), death (ragdoll exists; add death anims blend), cover entry, suppressed cowering.

**Assets & sources (free/AI).**
- **Mixamo** (free, Adobe): body locomotion, reactions, deaths — retarget to characters.
- **Epic Lyra / FirstPerson template / Game Animation Sample** (all **free** on Fab): high-quality
  FP + TP locomotion and weapon poses — the single best free anim source for UE.
- **Fab free anim packs** (rotating free section).
- **AI / procedural:** **Cascadeur** (free for indies under revenue cap) for physics-based custom
  keyframes (reloads, jam-clears); **UE Control Rig** to author FP poses directly in-engine (no
  external DCC needed — code/engine only); **ML Deformer** for High path.
- Gap (signature reloads) → marketplace later.

**Acceptance.** Each archetype has fire/reload/ADS/sprint that reads clean at 60fps; left-hand
lands on the weapon; enemies visibly flinch on hit.

**Validation.** Auto-playtest gunfeel routine + screenshots at the ADS/fire/reload poses; slow-mo
capture for review.

---

### WAVE 3 — Visual fidelity & lighting (scalable)
*The biggest AAA-look lever, gated by the Low/High split.*

**Objective.** Bring the scene to a believable, atmospheric standard: lighting, materials,
impact/blood/muzzle VFX, tracers, decals, time-of-day — looking AAA on High, staying playable on Low.

**Why.** Pillars 1, 6, 8. Quixel Megascans being **free in Fab** is the single biggest free
AAA-visual unlock available to a solo dev.

**Tasks.**
1. **Lighting.** Bake the daylight scene with **GPU Lightmass** for Low; Lumen for High. Author
   a believable golden-hour/overcast key + sky (the warm sunset we saw is a fine anchor). Add
   exposure/eye-adaptation, fog/atmosphere, light shafts.
2. **Materials/textures.** Replace placeholder surfaces with **Megascans** scanned PBR (sand,
   concrete, rubble, vegetation, water) + **ambientCG/Poly Haven CC0** fillers. Master materials
   with detail-normal + wetness for the beach.
3. **Combat VFX (Niagara).** Muzzle flash + dynamic muzzle light (brief point light), tracers,
   per-surface impact (dirt/sand/concrete/metal/water/flesh), blood spray + decals, bullet-hole
   decals on world hits (wire the hitscan path to spawn them — currently only projectiles do),
   shell-casing physics, explosion/frag VFX. We already have **62 Niagara param JSONs** — assemble
   them; fill gaps from Epic free VFX (Niagara fluids/effects samples).
4. **Decal system + pooling** (perf-aware budget; reuse the pooling work in the project).
5. **Post/FX.** Subtle film grain, chromatic aberration (light), motion blur (optional/togglable),
   sharpening; the suppression vignette/desat already exists — make it cohesive.
6. **Water & ocean** (the beach is the hero) — single-layer water on Low, more on High.

**Assets & sources (free/AI).**
- **Quixel Megascans** (free via Fab for UE) — hero source for surfaces/props/rocks/vegetation.
- **Poly Haven / ambientCG** (CC0) — textures, HDRIs, models.
- **Epic free VFX** (Niagara samples), our 62 param JSONs.
- **AI textures:** SD + **Materialize** (free) for normal/height/roughness from albedo; Dream
  Textures (Blender). AI for decals (blood, scorch, posters).
- **Props/buildings:** CitySample/Megascans + the 14 CitySample buildings already in-level.

**Acceptance.** High path looks atmospheric and cohesive (golden-hour beach → urban); Low path
runs at floor FPS with baked lighting and still reads well; every shot produces a surface-correct
impact + decal; muzzle flash lights the scene briefly.

**Validation.** Side-by-side Low/High screenshots; perf CSV on Low under combat.

---

### WAVE 4 — HUD / UI to production
*From canvas stand-in to a cohesive milsim interface.*

**Objective.** Replace the canvas `SHSimpleHUD` with a production UMG HUD matching the realism
dial — minimal, diegetic-leaning, readable. Keep everything we wired (hit markers, damage
indicators, ammo states, compass).

**Why.** Pillars 1, 8. Minimal UI is core to milsim immersion (Sandstorm) — but it must be
*polished* minimal, not placeholder.

**Tasks.**
1. **UMG HUD (WBP_HUD):** crosshair (dynamic, hide on ADS for ironsights), ammo + mag state,
   compass strip, objective markers, low-health vignette, hit marker, directional damage,
   suppression FX, fire-mode, stamina, weight, kill feed, interaction prompts.
2. **Menus polish:** main/pause/settings (exist as widgets) — restyle, wire scalability toggle.
3. **After-action / mission flow:** `SHAfterActionWidget` exists — populate with real stats.
4. **Icon/art:** use the **28 real PNG icons** we already have; generate the rest.

**Assets & sources (free/AI).**
- **Existing 28 PNG icons** (in the project).
- **game-icons.net** (CC-BY, thousands of milsim-appropriate icons), **Kenney.nl** (CC0 UI kits).
- **Fonts:** Google Fonts (OFL) stencil/mono milsim fonts.
- **AI:** SD for bespoke icons/markers, ChatGPT/Claude for UMG layout iteration.

**Acceptance.** No canvas debug HUD; cohesive style; all gameplay states represented; readable at
720p and 1440p.

**Validation.** HUD soak screenshots across states (firing, hit, damaged, reloading, low ammo).

---

### WAVE 5 — AI fairness, readability & encounter tuning
*Make the enemy great, not cheap.*

**Objective.** Tune the (already capable) AI so it feels human and fair: cover use, flanking,
human reaction timing, distance-appropriate accuracy, suppression response, and **visible tells**
— and design the level's encounters for pacing and choice.

**Why.** Pillar 5. My playtest showed the #1 tactical-AI failure mode (RoN's exact criticism):
a stationary player melted by focus fire at ~177m. We must tune accuracy/reaction and add
readability, then design encounters around player choice.

**Tasks.**
1. **Accuracy/reaction model.** Distance- and stance-scaled accuracy with a human reaction delay
   and ramp (no instant headshots on first sight); suppression degrades enemy aim (consume the
   suppression we apply); difficulty curve via `SHDifficultyConfig`.
2. **Readability tells.** Muzzle flash + tracers from enemies (so you can locate fire), audible
   weapon reports (Wave 1), call-outs before pushing, visible cover-peek animation, a brief
   "spotted" tell. Fairness = the player can *read* the threat.
3. **Cover & flanking.** Verify the cover system picks valid cover vs the player; enable the
   "proactive re-path on breach" nudge noted as optional polish; squad bounding/suppress-and-move.
4. **Encounter design (the level).** Script the beach→urban push as paced beats with
   reinforcement waves (the wave system exists), defensible cover lines, flank routes, and
   breathers — designed for push/retreat/flank/reposition choices (Arma Game-Master ethos).
5. **Telemetry tuning loop.** Use the auto-playtest encounter routine + metrics CSV
   (time-to-death, hit rate by range) to tune until a *moving* player survives and engagements
   feel fair.

**Assets.** Enemy anims (Wave 2), audio tells (Wave 1) — no new external assets.

**Acceptance.** Auto-playtest *moving* routine survives a first contact and can win engagements;
enemies use cover/flank; no instant-laser deaths; you can always tell where fire comes from.

**Validation.** Encounter metrics CSV (TTD up, fair hit-rate curve); screenshots of tracers/cover.

---

### WAVE 6 — Level art pass & composition
*Turn the functional level into a place.*

**Objective.** Set-dress and compose M01 to AAA standards: landmarks, readable cover, sightlines,
verticality, traversal, dead-space removal, environmental storytelling (a believable invaded
Taiwan beach → town).

**Why.** Pillar 6. The level is built and combat-functional but composed for systems, not for the
human eye/path.

**Tasks.** Landmark silhouettes for navigation; cover placement matched to encounter design (Wave 5);
sightline + flank-route shaping; set-dressing (defenses, wreckage, debris, signage, foliage) via
Megascans/CitySample; verticality (rooftops/overpass we have); remove empty stretches; environmental
story beats (aftermath of landing). Decal grime/wear pass.

**Assets & sources.** Megascans (props/debris/vegetation), CitySample (buildings/streets — already
in use), Poly Haven models, AI-gen signage/posters/decals, Kenney CC0 kit-bash props.

**Acceptance.** No dead space; clear landmarks; cover supports the encounters; a screenshot of any
vista reads as a deliberate, dressed scene.

**Validation.** Flythrough screenshots; encounter routine still fair; perf within budget.

---

### WAVE 7 — Game-feel tuning & polish convergence
*The "stop noticing friction" pass — only a human can sign this off.*

**Objective.** Tune every exposed feel constant in-hand until movement, gunfeel, camera, and TTK
are cohesive and satisfying. This is where the blended realism dial gets dialed.

**Why.** Pillars 1–3. Everything we added is exposed as `EditDefaultsOnly`/config but never felt
by a human. AAA feel = hundreds of small tuning decisions converging.

**Tasks.** Tuning passes on: movement (accel/brake/air/step/jump), camera (fire kick, bob, punch,
ADS FOV/transition, sensitivity curve), recoil (pattern + recovery — fix the recovery-vs-player-input
overcorrection here), TTK/damage, weapon weight/handling per gun, sway, audio mix levels, VFX
intensity. Add accessibility (FOV slider, sensitivity, toggle/hold, subtitles, colorblind markers —
`SHAccessibilitySettings` exists).

**Assets.** None.

**Acceptance.** A human play session feels "intentional, responsive, satisfying"; no single system
sticks out as off; the realism blend is decided and consistent.

**Validation.** Human playtest (you); checklist sign-off per pillar; before/after clips.

---

### WAVE 8 — Performance & stability hardening
*Hit the targets on every tier.*

**Objective.** Reach the FPS floor on the dev box (Low) and the FPS target on mid/high/console
(High); eliminate hitches, leaks, errors.

**Why.** Pillar 8 + the locked constraint. ~6 FPS today on the dev box; combat adds cost.

**Tasks.** Profile (`stat unit/gpu`, Unreal Insights); draw-call/triangle reduction (HLODs,
instancing, Nanite on High), shadow/foliage/lighting budgets on Low (baked), texture streaming
pool, async asset loads, physics/particle pooling (reuse existing pooling), AI tick LOD/throttling,
fix the LWC large-coordinate `Ensure` (rebase the level closer to origin or set world bounds),
resolve the on-screen shader warning. Console: memory budget + 30/60 modes.

**Assets.** None.

**Acceptance.** Low ≥ floor FPS at 720p on the dev box under combat; High ≥ 60fps@1080p on a 3060;
no fatals; clean log; no hitches > budget.

**Validation.** `-perfreport` CSV per tier; soak run with no errors.

---

### WAVE 9 — Cohesion & vertical-slice sign-off
*Does the whole thing feel like a AAA game?*

**Objective.** Tie it together — music beats, narrative framing, mission intro/outro, briefing,
pacing — and run the "is it AAA" review against §7.

**Why.** Pillar 8. Cohesion is the final 10% that separates "good systems" from "a game."

**Tasks.** Mission briefing/intro (`SHBriefingSystem`), objective flow + reactive dialogue
(conditional branching is the known feature gap — scope it here), adaptive music beats, win/loss +
after-action, title/menu polish, a 60–90s "vertical slice" capture. Final license-hygiene audit of
all assets used (track in a manifest).

**Assets.** Music (Wave 1 sources), VO (Wave 1), title art (AI).

**Acceptance.** A stranger can launch, be briefed, play a paced, atmospheric, satisfying combat
beat with audio/animation/VFX/AI all cohering, and reach a resolution — and it reads as AAA on High.

**Validation.** Full slice playthrough capture; §7 checklist all green; license manifest complete.

---

## 3. ASSET SOURCING CATALOG  *(the priority — exact free/AI sources, license, integration)*

> **License hygiene rule:** prefer **CC0** (no attribution, commercial OK) → **CC-BY** (attribution
> required; keep `CREDITS.md`) → **Epic/Fab license** (free for UE projects). **Avoid
> non-commercial / "personal use only"** (e.g. parts of BBC SFX) for anything shippable. Every
> imported asset gets a row in `Tools/asset_manifest.json` (source URL + license + where used).

### Audio
| Need | Free source | AI-gen source | License | Integration |
|---|---|---|---|---|
| Weapon fire/mech | **Sonniss GDC bundles**, Freesound (CC0), gamesounds.xyz | ElevenLabs SFX, Meta AudioGen (local) | CC0 / royalty-free | Batch import → Sound Class "Weapons"; hook `PlayFireSound`/reload |
| Footsteps/foley | Sonniss, Freesound CC0 | AudioGen | CC0 | Footstep DB per surface |
| Impacts/whizz/ricochet | Sonniss, Freesound | AudioGen, Stable Audio | CC0 | `SHProjectile` impact + crack hooks |
| Ambience (ocean/wind/battle) | Freesound CC0, Sonniss | Stable Audio | CC0 | `SHAmbientSoundscape` layers |
| Reverb (zones) | **OpenAIR** impulse responses | — | CC-BY/Open | `SHReverbZoneManager` IR reverb submix |
| Radio/squad VO | — | **Piper / Coqui XTTS** (local, free, commercial), ElevenLabs free tier | Open / check tier | `ESHVoiceLineType` lines + radio submix FX |
| Music | Incompetech (CC-BY), Sonniss | Stable Audio, AIVA free | CC-BY | Adaptive music beats |

### Animation
| Need | Free source | AI/procedural | License | Integration |
|---|---|---|---|---|
| FP/TP locomotion, weapon poses | **Epic Game Animation Sample / Lyra / FP template** (Fab) | — | Epic | IK Retarget to our skeleton |
| Body reactions/deaths | **Mixamo** | — | Free | Retarget; hit-react montages |
| Custom reloads/jam-clear | Fab free packs | **Cascadeur** (free indie), **UE Control Rig** (in-engine) | Free | `*_Montage_FP` slots in WeaponData |
| Hand IK to weapon | — | UE IK (FABRIK/Control Rig) | — | Foregrip/mag sockets |

### Visuals — meshes / textures / VFX
| Need | Free source | AI-gen | License | Integration |
|---|---|---|---|---|
| PBR surfaces (sand/concrete/etc.) | **Quixel Megascans (free in Fab)**, ambientCG, Poly Haven | SD + **Materialize** (normal/height/rough) | CC0 / Epic | Batch import → master materials |
| Props/debris/vegetation | Megascans, Poly Haven, Kenney (CC0), CitySample (in-project) | — | CC0 / Epic | Set-dressing |
| Buildings/urban | CitySample (in-project), Megascans | — | Epic | Composition |
| HDRIs/sky | Poly Haven | — | CC0 | Sky/lighting |
| Combat VFX | **our 62 Niagara JSONs**, Epic Niagara samples (Fab) | — | Epic | Assemble in Niagara |
| Decals (blood/holes/grime/signage) | ambientCG decals | SD | CC0 | Decal pool |
| Characters (soldiers) | Mixamo chars, Fab free, **MetaHuman** (free, High path) | AI textures on base meshes | Free/Epic | Retarget; PLA/US texture variants |
| Weapons | **FPS Weapon Bundle (in-project)**, Fab free | — | Epic | Already wired to WeaponData |

### UI
| Need | Free source | AI | License | Integration |
|---|---|---|---|---|
| Icons/markers | **28 PNGs (in-project)**, game-icons.net, Kenney | SD | CC-BY/CC0 | UMG HUD |
| Fonts | Google Fonts (OFL) | — | OFL | HUD/menus |

### AI-generation toolchain (local-first, free)
- **Audio:** Meta AudioCraft/AudioGen (local), Stable Audio, ElevenLabs (free tier), Piper/Coqui TTS (local).
- **Textures/images:** Stable Diffusion (local) + Materialize (PBR maps) + Dream Textures (Blender).
- **Animation:** Cascadeur (free indie), UE Control Rig/ML Deformer (in-engine).
- **Orchestration:** Claude writes the headless import scripts + UMG/material/Niagara setup; you run AI-gen tools, drop outputs into an `incoming/` folder, the batch importer ingests them.

> **Marketplace (future, when budget opens):** Synty (stylized props), pro milsim SFX packs
> (e.g. dedicated firearm libraries), signature animation packs, Fab premium characters/uniforms.
> These fill the *last* quality gaps — not needed to reach the slice.

---

## 4. Scalability & Performance framework

**The split is the backbone.** Author and validate on **Low** (dev box); realize the AAA look on
**High** (target). One project, switchable.

| Setting | Low (i5-8400/UHD630) | High (3060+/console) |
|---|---|---|
| GI/Reflections | **Baked (GPU Lightmass)**, no Lumen | Lumen GI + reflections |
| Geometry | Nanite off / fallback meshes, aggressive LODs/HLODs | Nanite |
| Shadows | Low CSM, baked where possible | Virtual Shadow Maps |
| Foliage/draw dist | Reduced | Full |
| Resolution | 720p + screen-percentage | 1080–1440p |
| Dynamic lights | Few; muzzle flash budget-limited | Full |
| FPS target | **Floor: stable & readable (aim 30)** | **60+ @1080p** |

**Honest limit:** UHD630 cannot render AAA visuals at high FPS — baked lighting + reduced settings
gets it *playable and readable* for authoring/validation. The AAA bar is met on High. Console parity
is planned in Wave 8 (memory budget + 30/60 modes), realized when you have console/strong-PC access.

---

## 5. Tooling & automation (how you + AI actually execute)

- **Headless asset pipeline** (`Tools/`): extend with a **generic batch importer** driven by
  `asset_manifest.json` (path, type, import settings, license). Drop free/AI assets into `incoming/`,
  run one script, they land correctly configured in `/Game/SH/...`.
- **Auto-playtest harness v2** (`bAutoPlaytest` scenarios): move-test, gunfeel-test, encounter-test,
  perf-flythrough → structured metrics CSV + HUD soak screenshots. The core validation loop.
- **Perf reporting:** `-perfreport` routine writes FPS/frame-time/draw-call CSV per tier.
- **Screenshot QA:** soak screenshots (now HUD-inclusive) read back via the file tools.
- **License manifest + `CREDITS.md`:** every asset tracked; CC-BY attributions auto-collated.
- **Build/validate:** `Build.bat` (editor target) green-gates every wave; headless `-game` runs
  validate behavior.
- **Division of labor:** *Claude* — all code, import/automation scripts, UMG/material/Niagara/
  lighting setup, tuning, validation, doc. *You* — run local AI-gen tools, approve asset picks,
  the human feel sign-off (Wave 7), provide a stronger rig/console when available for High validation.

---

## 6. Risk register & honest limits

| Risk | Mitigation |
|---|---|
| **Dev box can't show the AAA look** | Scalability split; validate look on High when a rig is available; Low stays the authoring floor |
| **Free assets feel inconsistent / "asset-flip"** | Curate to a coherent palette; unify with master materials + post; Megascans gives a consistent scanned base |
| **AI-gen audio/anim quality ceiling** | Use AI for *coverage*, curate hard, flag the few pieces needing marketplace later (signature reloads, hero gun audio) |
| **License contamination (non-commercial slips in)** | Manifest + CC0/CC-BY-only rule; audit in Wave 9 |
| **Feel can't be validated headlessly** | Auto-playtest gets us 80%; the final 20% (Wave 7) needs your hands — that's expected |
| **Animation is the hardest solo gap** | Lean on Epic Game Animation Sample + Control Rig (in-engine, no DCC); Cascadeur for custom; marketplace last |
| **Scope creep on "one level"** | The slice is the boundary — no new levels until M01 is AAA |

---

## 7. Definition of "AAA done" for the slice

The slice is AAA when, on the **High** path, a fresh player can:
- [ ] Launch → briefing → play a paced, atmospheric beach→urban combat beat → resolution.
- [ ] **Feel:** movement intentional; guns heavy/readable/animated; camera/recoil/ADS cohesive (Wave 7 sign-off).
- [ ] **Audio:** distinct weapon reports, directional footsteps, supersonic cracks, suppression muffle, radio callouts, ambience, adaptive music.
- [ ] **Visuals:** cohesive baked/Lumen lighting, Megascans-grade surfaces, muzzle/impact/blood/tracer VFX, decals.
- [ ] **AI:** enemies flank/use cover/react with human timing; fights are fair and readable; you can win by playing well.
- [ ] **Level:** landmarks, cover, sightlines, no dead space, environmental story.
- [ ] **UI:** cohesive minimal milsim HUD; no debug canvas.
- [ ] **Systems:** ballistics/wounding/suppression depth on display (already AAA-tier).
- [ ] **Perf:** High ≥60fps@1080p (3060); Low stays playable on the dev box.
- [ ] **Polish:** no fatal errors, no hitches, license-clean.

And it reads, to someone who plays Sandstorm/Tarkov/Arma, as belonging in that company.

---

*This is a living document. Waves are sequenced by leverage + dependency; the game stays playable
after each. Start point: **Wave 0 (foundations)** then **Wave 1 (audio)** — the highest-leverage gap.*
