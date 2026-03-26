# SHATTERED HORIZON 2032 — REMAINING WORK PLAN

**Date:** 2026-03-26
**Status:** All C++ game systems complete. Zero TODOs. Zero stubs. 175+ source files.
**What remains:** Asset generation, UE5 editor assembly, and polish.

---

## PHASE 1: IMMEDIATE — ZERO-COST PIPELINE RUNS (30 minutes)

These pipelines require NO API keys. Run them now.

```bash
# VFX Configs — 65 Niagara parameter sets
cd Tools/VFXPipeline
python generate_vfx.py --full
# ~1 min. Output: 65 JSON configs for muzzle flash, shell eject, impacts, destruction, footsteps, drones.

# Weapon Data — 11 doctrine-accurate DataAssets
cd Tools/WeaponDataPipeline
python generate_weapon_data.py --full
# ~1 min. Output: 11 weapon DataAssets + range cards + DataTable CSV.

# Animations — procedural recoil/sway/bob curves
cd Tools/AnimationPipeline
python generate_animations.py --full
# ~2 min. Output: 55 montage configs + 24 character anim specs + Mixamo recommendations.

# UI Icons — code-generated geometric assets (compass, stance, commands)
cd Tools/UIPipeline
python generate_ui.py --code-only
# ~2 min. Output: 40+ HUD/tactical icons. No API needed.
```

**Total time: ~6 minutes. Total cost: $0.**
**Output: ~170 assets ready for UE5 import.**

---

## PHASE 2: API-POWERED GENERATION (~$63-153, 2-3 hours)

Run these in parallel. Each pipeline is independent.

### Required API Keys

| Provider | Used By | Estimated Cost | Sign-up |
|---|---|---|---|
| ElevenLabs | SFX + Voice Lines | $25-50 | elevenlabs.io |
| Stability AI | Terrain Textures + UI Backgrounds | $8-23 | stability.ai |
| Suno | Music/Soundtrack | $10-30 | suno.com |
| Meshy | 3D Models | $20-50 | meshy.ai |
| Unsplash | Reference Photos (textures) | Free | unsplash.com |
| Pexels | Reference Photos (textures) | Free | pexels.com |

### Execution (parallel where possible)

```bash
# Terminal 1: Terrain Textures (15-45 min)
cd Tools/ArtPipeline
export UNSPLASH_ACCESS_KEY="..." PEXELS_API_KEY="..." STABILITY_API_KEY="..."
python generate.py --full
# Output: 375 PBR texture sets (albedo, normal, roughness, AO, height)

# Terminal 2: Sound Effects (30 min)
cd Tools/AudioPipeline
export ELEVENLABS_API_KEY="..."
python generate_sfx.py --full
# Output: 180 SFX files (weapons, impacts, footsteps, explosions, drones, comms)

# Terminal 3: Music (45 min)
cd Tools/AudioPipeline
export SUNO_API_KEY="..."
python generate_music.py --full
# Output: 25 soundtrack tracks across 4 categories (ambient, combat, stingers, menu)

# Terminal 4: Voice Lines (30 min)
cd Tools/AudioPipeline
python generate_voices.py --full
# Output: 260+ voice lines (English squad + Mandarin PLA callouts)

# Terminal 5: 3D Models (45-60 min — slowest, polling-based)
cd Tools/ModelPipeline
export MESHY_API_KEY="..."
python generate_models.py --full
# Output: 23 GLB meshes (characters, weapons, drones)

# Terminal 6: UI Backgrounds (5 min)
cd Tools/UIPipeline
export STABILITY_API_KEY="..."
python generate_ui.py --full
# Output: 40+ UI assets including AI-generated menu backgrounds
```

**Total wall time: ~60 minutes (parallel). Total cost: ~$63-153.**
**Output: ~1,500+ assets across all categories.**

---

## PHASE 3: MASTER IMPORT TO UE5 (1-2 hours)

After all pipelines complete:

```bash
# Generate the unified import script
cd Tools
python import_all_to_ue5.py
# Creates: ue5_master_import.py
```

Then in UE5 Editor → Python Console:
```python
exec(open('/path/to/Tools/output/ue5_master_import.py').read())
```

This imports:
- 375 textures → `/Game/Textures/Terrain/`
- 180 SFX → `/Game/Audio/SFX/`
- 25 music tracks → `/Game/Audio/Music/`
- 260 voice lines → `/Game/Audio/Voice/`
- 40+ UI assets → `/Game/UI/`
- 65 VFX configs → creates Niagara systems at `/Game/VFX/`
- 11 weapon DataAssets → `/Game/Data/Weapons/`
- 23 3D models → `/Game/Meshes/`
- 55 animation configs → `/Game/Animations/`
- 5 DataTable CSVs (SFX, Weapons, Footsteps, Voice, VFX)

**Manual verification needed after import:**
- [ ] Spot-check 10% of textures for quality (seamless tiling, PBR value ranges)
- [ ] Listen to 5-10 weapon SFX for realism
- [ ] Preview 3-4 music tracks for emotional fit
- [ ] Verify voice line clarity and radio filter quality
- [ ] Check 3D model polygon counts and topology

---

## PHASE 4: BLUEPRINT CREATION (2-3 days)

Create Blueprint subclasses for every C++ class. This connects code to the editor.

### Player & Core (Day 1)

| Blueprint | Parent C++ Class | What to Configure |
|---|---|---|
| `BP_PlayerCharacter` | `ASHPlayerCharacter` | Assign FP arms mesh, camera, capsule size, component references (CameraSystem, HitFeedback, DeathSystem, etc.) |
| `BP_PlayerController` | `ASHPlayerController` | Input mapping context, interaction trace distance |
| `BP_GameMode` | `ASHGameMode` | Set EnemyCharacterClass, ArmorVehicleClass, AmphibiousVehicleClass, Primordia tick interval |
| `BP_GameState` | `ASHGameState` | Default environmental conditions |
| `BP_HUD` | `ASHHUD` | Widget class references for all 9 UI screens |

### Weapons (Day 1)

| Blueprint | Setup |
|---|---|
| `BP_Weapon_M27_IAR` | WeaponData → DA_M27_IAR, mesh, muzzle socket, VFX, audio |
| `BP_Weapon_M4A1` | Same pattern, per weapon |
| `BP_Weapon_M249_SAW` | ... |
| `BP_Weapon_M110_SASS` | ... |
| `BP_Weapon_M17_SIG` | ... |
| `BP_Weapon_M320_GL` | ... |
| `BP_Weapon_Mossberg590` | ... |
| `BP_Weapon_SniperLapua` | ... |
| `BP_Weapon_M2_Browning` | ... |
| `BP_Weapon_QBZ95` | ... |
| `BP_Weapon_Type56` | ... |

For each: assign WeaponDataAsset, skeletal mesh, muzzle/eject socket names, fire/reload sounds, muzzle flash Niagara, shell casing Niagara, projectile class.

### Enemies (Day 2)

| Blueprint | Parent | Configuration |
|---|---|---|
| `BP_Enemy_Rifleman` | `ASHEnemyCharacter` | Role=Rifleman, mesh, weapon=QBZ95/Type56, perception config, behavior tree |
| `BP_Enemy_MG` | `ASHEnemyCharacter` | Role=MachineGunner, heavier mesh, weapon=Type56(MG variant) |
| `BP_Enemy_RPG` | `ASHEnemyCharacter` | Role=RPG, RPG launcher mesh |
| `BP_Enemy_Officer` | `ASHEnemyCharacter` | Role=Officer, officer uniform mesh, higher morale thresholds |
| `BP_Enemy_Marksman` | `ASHEnemyCharacter` | Role=Marksman, scoped rifle, extended sight range |
| `BP_Enemy_Medic` | `ASHEnemyCharacter` | Role=Medic, medical kit mesh |
| `BP_Enemy_Engineer` | `ASHEnemyCharacter` | Role=Engineer |

### Friendly Squad (Day 2)

| Blueprint | Parent | Configuration |
|---|---|---|
| `BP_Squad_Vasquez` | `ASHSquadMember` | SAW Gunner, M249, personality traits, callsign Viper-2 |
| `BP_Squad_Kim` | `ASHSquadMember` | Grenadier, M4A1+M320, personality, Viper-3 |
| `BP_Squad_Chen` | `ASHSquadMember` | Marksman, M110, personality, Viper-4 |
| `BP_Squad_Williams` | `ASHSquadMember` | Corpsman, M4A1, medical kit, personality, Viper-5 |

### Vehicles, Drones, Throwables (Day 3)

| Blueprint | Parent | Configuration |
|---|---|---|
| `BP_Vehicle_ZBD05` | `ASHVehicleBase` | Amphibious IFV, seat config, mounted weapon, armor zones |
| `BP_Vehicle_Type08` | `ASHVehicleBase` | Wheeled IFV |
| `BP_Drone_ISR` | `ASHISRDrone` | Camera config, battery, patrol patterns |
| `BP_Drone_FPV` | `ASHFPVStrikeDrone` | Strike config, explosive payload |
| `BP_Throwable_M67` | `ASHThrowableProjectile` | Frag grenade mesh, VFX, audio, M67 data |
| `BP_Throwable_M18Smoke` | `ASHThrowableProjectile` | Smoke mesh, smoke VFX |
| `BP_Throwable_M84Flash` | `ASHThrowableProjectile` | Flashbang mesh, flash VFX |
| `BP_Door_Standard` | `ASHBreachableEntry` | Standard door mesh, sounds |
| `BP_Door_Reinforced` | `ASHBreachableEntry` | bStartsLocked=true, higher health |
| `BP_Door_Barricaded` | `ASHBreachableEntry` | bStartsBarricaded=true |

### Widget Blueprints (Day 3)

| Widget | Parent C++ Class | Layout |
|---|---|---|
| `WBP_HUD` | `USHHUD` | Compass top-center, injury bottom-left, ammo bottom-right, squad left, radio right |
| `WBP_MainMenu` | `USHMainMenuWidget` | Title, buttons, animated background |
| `WBP_Pause` | `USHPauseMenuWidget` | Resume, Restart, Settings, Quit |
| `WBP_Loading` | `USHLoadingScreenWidget` | Briefing content during load |
| `WBP_Settings` | `USHSettingsWidget` | Tabbed: Video, Audio, Controls, Gameplay, Accessibility |
| `WBP_Loadout` | `USHLoadoutWidget` | Primary, secondary, attachments, equipment, weight display |
| `WBP_SquadCommand` | `USHSquadCommandWidget` | Radial menu: Move, Hold, Engage, Suppress, Regroup, Medical |
| `WBP_AAR` | `USHAfterActionWidget` | Statistics, squad fate, "Enemy's Perspective" button |
| `WBP_DeathRecap` | — | Killer info, weapon, distance, headshot, direction indicator |

---

## PHASE 5: LEVEL CREATION — TAOYUAN BEACH (1-2 weeks)

This is the largest single task. The build document (DEMO-BUILD-DOCUMENT.md) has the full breakdown at tasks A-01 through A-33.

### Week 1: Terrain + Beach + Defensive Line

1. **Heightmap** (A-01/02): 8129×8129 at 1m/pixel from World Machine or Gaea. Import into UE5 Landscape with World Partition (256m cells).

2. **Beach sculpting** (A-03): 4km beach front, tidal flats, drainage channels, sea walls. Apply sand/wet sand/mud landscape materials from generated textures.

3. **Defensive line** (A-11/12): Place modular bunkers and trench system along 3km front. Player's sector ~400m, adjacent sectors with NPC Marines.

4. **Beach obstacles** (A-13): Czech hedgehogs, tetrapods, wire — 100-150 pieces across the beach using procedural placement.

5. **Ocean** (A-09): Water plugin plane, wave simulation, landing craft interaction.

6. **Skybox + atmosphere** (A-10): Pre-dawn to morning progression, volumetric clouds, haze.

### Week 2: Urban District + Infrastructure + Lighting

7. **Urban buildings** (A-15/16): 40-60 modular buildings from ~12 base types. Interiors playable. Traditional Chinese signage. Market district, temple, school.

8. **Coastal road + agricultural buffer** (A-04b/19): Highway, checkpoint, rice paddies, farm buildings.

9. **Collapse zone** (A-21c): Highway interchange, overpass for Phase 4 last stand.

10. **Lighting passes** (A-22 through A-27): Time-of-day blueprint driving 0430-0800+ sun progression. Pre-dawn deep blue, dawn golden hour behind PLA forces, harsh morning light. Lumen GI for interiors.

11. **Environment VFX** (A-29 through A-33): Ocean spray, sand kick-up, fire/smoke systems, artillery craters, rain particles.

12. **Landing craft placement**: 6-8 models along beach, beached and approaching variants.

---

## PHASE 6: WIRING THE LEVEL (3-5 days)

Connect the level geometry to the game systems.

### Spawn Infrastructure
- [ ] Place spawn point actors tagged per wave zone (`BeachZone_Alpha`, `BeachZone_Bravo`, etc.)
- [ ] Configure `BP_GameMode` with enemy/vehicle class references pointing to the Blueprints from Phase 4
- [ ] Set up Primordia Decision Engine instance on the GameMode
- [ ] Place player start at trench position

### Mission Script Connection
- [ ] Wire `SHMission_TaoyuanBeach::CreateMissionDefinition()` to the GameMode
- [ ] Place trigger volumes for phase transitions
- [ ] Set up objective markers at key positions
- [ ] Configure EW jamming zone actors per mission timeline

### Environmental Systems
- [ ] Place `SHReverbZoneManager` volumes in buildings (indoor reverb)
- [ ] Place weather transition triggers
- [ ] Configure `SHWeatherSystem` atmospheric conditions for Taoyuan (25°N, sea level, ~25°C, humid)
- [ ] Feed atmospheric conditions to ballistics system

### Audio Placement
- [ ] Place ambient sound sources (ocean, wind, distant battle)
- [ ] Configure MetaSound sources for weapon audio with crack-thump integration
- [ ] Set up music system triggers per phase
- [ ] Place radio chatter audio sources at adjacent positions

### Cover + Destruction
- [ ] Register all destructible actors with `SHDestructionSystem`
- [ ] Add `USHDestructionReplication` component to all destructible actors
- [ ] Place `SHBreachableEntry` actors on all doors
- [ ] Configure building structural integrity graphs (which supports connect to which)

### AI Navigation
- [ ] Build navigation mesh for full map
- [ ] Place EQS test contexts for cover queries
- [ ] Configure AI perception volumes
- [ ] Set up patrol routes for pre-invasion enemy

---

## PHASE 7: 3D MODEL FINISHING (3-5 days, parallel with Phase 6)

The AI-generated models from Meshy need manual work:

### Characters
- [ ] Import GLB to UE5, retarget to UE5 Mannequin skeleton
- [ ] Create 3 LODs per character (LOD0: full, LOD1: 50%, LOD2: 25%)
- [ ] Assign generated PBR materials
- [ ] Set up physics assets for ragdoll (per `SHDeathSystem`)
- [ ] Test animation compatibility with blend spaces

### Weapons
- [ ] Import 11 weapon GLBs
- [ ] Assign to skeletal mesh sockets on character arms
- [ ] Set up Muzzle and ShellEject socket positions
- [ ] Create LODs
- [ ] Test ADS alignment

### Vehicles + Drones
- [ ] Import vehicle meshes, set up Chaos vehicle config
- [ ] Configure wheel/track collision, seat positions
- [ ] Import drone meshes, configure camera attachment

---

## PHASE 8: ANIMATION INTEGRATION (3-5 days, parallel with Phase 7)

### From Mixamo (Free)
- [ ] Download base locomotion set (walk, run, sprint, crouch, prone — 8-directional)
- [ ] Download combat set (fire, reload, melee, grenade throw, death variants)
- [ ] Retarget all to project skeleton

### Procedural Integration
- [ ] Apply generated recoil curves to weapon AnimMontages
- [ ] Apply weapon bob curves to locomotion blend space
- [ ] Apply weapon sway curves to ADS state
- [ ] Create AnimBlueprint state machines (Player, Squad, Enemy)
- [ ] Set up blend spaces (speed × direction, aim offset, lean)

### Polish
- [ ] Tune animation transition times
- [ ] Add IK for weapon holding, foot placement
- [ ] Test animation-to-ragdoll blend (death system)

---

## PHASE 9: POLISH + TESTING (1 week)

### Audio Mix
- [ ] Balance weapon volumes across all 11 weapons
- [ ] Tune crack-thump delay feel (does distance estimation feel right?)
- [ ] Balance music levels against combat audio
- [ ] Test auditory exclusion at peak stress
- [ ] Verify footstep sounds per surface match visual material

### Visual Polish
- [ ] Camera shake tuning (per weapon, explosions, footstep bob)
- [ ] Suppression vignette intensity curve
- [ ] Combat stress tunnel vision feel
- [ ] Muzzle flash per-weapon tuning (night vs day)
- [ ] Destruction VFX per stage (dust, sparks, debris)

### Testing
- [ ] Run test suite in editor: `Automation RunTests SH2032`
- [ ] 10 full manual playthroughs at Normal difficulty
- [ ] 3 playthroughs at hardest difficulty
- [ ] Edge case testing (passive player, speed run, all-squad-dies)
- [ ] Performance profiling: 200+ AI at 60fps
- [ ] Memory profiling: no leaks over 2-hour session
- [ ] 4K resolution, controller support, ultrawide verification

### Quality Gates (from DEMO-BUILD-DOCUMENT.md)
- [ ] Gate 1: IT COMPILES — clean build, zero warnings
- [ ] Gate 2: IT RUNS — player spawns, moves, shoots in Taoyuan Beach
- [ ] Gate 3: IT PLAYS — all 4 phases, all waves, squad AI functional
- [ ] Gate 4: IT FEELS — camera, audio, suppression, stress feel visceral
- [ ] Gate 5: IT IMMERSES — silence, dawn, dialogue, AI intelligence visible
- [ ] Gate 6: IT SELLS — zero crashes, 60fps, controller, 4K, 10 consecutive playthroughs

---

## TIMELINE ESTIMATE

| Phase | Duration | Parallelizable | Dependency |
|---|---|---|---|
| 1: Zero-cost pipelines | 30 min | — | None |
| 2: API asset generation | 1-2 hours | Yes (all parallel) | API keys |
| 3: UE5 master import | 1-2 hours | — | Phase 1+2 |
| 4: Blueprint creation | 2-3 days | Partially | Phase 3 |
| 5: Level creation | 1-2 weeks | — | Phase 3 (textures needed) |
| 6: Level wiring | 3-5 days | — | Phase 4+5 |
| 7: Model finishing | 3-5 days | Yes (parallel with 6) | Phase 3 (models needed) |
| 8: Animation integration | 3-5 days | Yes (parallel with 7) | Phase 3 + Mixamo |
| 9: Polish + testing | 1 week | — | Everything |

**Critical path: ~4-5 weeks** from pipeline execution to locked build.
**With aggressive parallelization: ~3-4 weeks.**

---

## COST SUMMARY

| Category | Cost |
|---|---|
| Asset pipeline APIs | $63-153 |
| Mixamo animations | $0 (free tier) |
| UE5 license | $0 (revenue share) |
| Voice actors (if replacing AI) | $2,000-10,000 (optional, AI voices work for investor demo) |
| **Total minimum** | **$63-153** |

---

*The code is done. The pipelines are built. The plan is written. What's left is execution.*
