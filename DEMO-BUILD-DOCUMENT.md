# SHATTERED HORIZON 2032 — MISSION 1 BUILD DOCUMENT

**Document Version:** 1.1
**Date:** 2026-03-26
**Target:** Ship-quality Mission 1 — the game's first level AND the investment vehicle to raise $25–30M
**Scope:** Mission 1 — Operation Breakwater (Taoyuan Beach) — FULL MISSION, not a demo slice
**Mission Runtime:** 45–75 minutes of playable content (varies by difficulty and playstyle)

---

## EXECUTIVE SUMMARY

Shattered Horizon 2032 is a doctrine-authentic military tactical FPS built on Unreal Engine 5.4. The codebase contains **165 C++ source files** implementing production-grade combat simulation, squad AI, an adversarial AI decision engine (Primordia), and a fully scripted first mission. Asset generation pipelines have produced **1,500+ assets** across 7 pipelines (textures, audio, animation, models, VFX, weapons, UI).

### What's Done

The **systems layer is essentially complete.** Every critical gameplay system identified in the compliance report as "missing" or "blocked" has since been implemented:

| Previously Flagged Blocker | Actual Status |
|---|---|
| AI sight range capped at 150m | **FIXED** — 800m clear day (`SHAIPerceptionConfig.cpp:11`) |
| .338 Lapua Magnum missing | **IMPLEMENTED** — `SniperLapua`, 915 m/s (`SHWeaponData.cpp:572`) |
| .50 BMG missing | **IMPLEMENTED** — `M2_Browning`, 930 m/s (`SHWeaponData.cpp:640`) |
| 7.62×39mm missing | **IMPLEMENTED** — `Type56`, 715 m/s (`SHWeaponData.cpp:779`) |
| QBZ-95 (5.8×42mm) missing | **IMPLEMENTED** — 930 m/s (`SHWeaponData.cpp:710`) |
| M110 muzzle velocity wrong (783 m/s) | **FIXED** — 840 m/s (`SHWeaponData.cpp:306`) |
| Head graze mechanic missing | **IMPLEMENTED** — >60° angle check, stagger + recovery (`SHDamageSystem.cpp:136`) |
| Combat stress system missing | **IMPLEMENTED** — Full heart rate model (`SHCombatStressSystem.h/cpp`) |
| Indirect fire system missing | **IMPLEMENTED** — Call-for-fire + indirect fire physics (`SHIndirectFireSystem.h/cpp`, `SHCallForFireSystem.h/cpp`) |
| Primordia Decision Engine stubs empty | **IMPLEMENTED** — `TickEscalation()`, `TickMoraleAndRetreat()`, `GenerateLocalFallbackOrders()`, `OnReinforcementWaveSpawned()` all contain real logic (`SHPrimordiaDecisionEngine.cpp`) |
| Primordia subsystems not wired | **ALL 6 IMPLEMENTED** — Aletheia, Echo, Simulon, Astraea, Mnemonic, TacticalAnalyzer |
| AI debug overlay missing | **IMPLEMENTED** — `SHPrimordiaDebugOverlay.h/cpp` |
| AI replay system missing | **IMPLEMENTED** — `SHPrimordiaReplayTimeline.h/cpp` |
| No main menu / UI screens | **IMPLEMENTED** — Main menu, pause, loading, settings, loadout, AAR, squad command, compass widgets all exist in C++ |

### What Remains

The gap is no longer **systems** — it's **experience assembly.** The code works. The assets are generated. What's missing is:

1. **A playable level** — no `.umap` exists. Taoyuan Beach is scripted in code but has no 3D world.
2. **Asset deployment** — 1,500+ assets sitting in pipeline output directories, not imported into UE5.
3. **Blueprint wiring** — C++ classes need to be connected to Actors, Widgets, and gameplay Blueprints in-editor.
4. **Feel & polish** — Camera, movement, weapon feedback, hit reactions need tuning for visceral impact.
5. **Audio integration** — Sound assets generated but not connected to gameplay events.
6. **Cinematic bookends** — Briefing, transitions between phases, and end screen need visual presentation.
7. **Performance validation** — Zero profiling done. 114+ AI entities per mission untested at scale.
8. **One compilation bug** — `SHPrimordiaDecisionEngine.cpp:91` calls `GetAvailableSquadCount(false)` but declaration missing from header.

### Build Philosophy

This is not a demo. This is not a vertical slice. This is **Mission 1 of Shattered Horizon 2032** — the full first level of a shipped game. It must stand on its own as a complete, replayable, ship-quality experience. It also happens to be what we show investors, which means it must be so good that someone writes a $25–30M check after playing it.

**Full scale.** The GDD specifies 64 km² minimum (8 km × 8 km). Mission 1 delivers that. The player experiences the vastness of a real battlefield — long engagement distances, the terror of crossing open ground, the claustrophobia of urban collapse. No invisible walls at 2 km. No scaled-down compromise.

**Full duration.** A real playthrough of Operation Breakwater runs 45–75 minutes depending on difficulty and playstyle. The 4 phases (Pre-Invasion, Beach Assault, Urban Fallback, Collapse) play out at real operational tempo. Time pressure comes from the mission, not from artificial constraints.

**Full replayability.** Because the AI thinks instead of following scripts, no two playthroughs are the same. Different difficulties change AI cognitive depth, not enemy health. Different loadout choices change tactical options. Different squad order decisions change outcomes. Mission 1 ships as a level people replay to see what the AI does differently.

The competitive moat is the AI. Every other military shooter scripts enemy behavior. Ours thinks. Mission 1 must make that visible — enemies that flank, suppress, communicate, adapt, and break. That's what $25–30M buys: the only military game where the enemy is trying to win.

---

## UPDATED SYSTEMS AUDIT

### Complete Weapons Arsenal (11 Platforms)

| Weapon | Caliber | Velocity | Role | Status |
|---|---|---|---|---|
| M27 IAR | 5.56×45mm | 940 m/s | Squad automatic rifle | **COMPLETE** |
| M4A1 | 5.56×45mm | 910 m/s | Standard carbine | **COMPLETE** |
| M249 SAW | 5.56×45mm | 915 m/s | Light machine gun | **COMPLETE** |
| M110 SASS | 7.62×51mm | 840 m/s | Designated marksman | **COMPLETE** |
| M17 SIG | 9×19mm | 375 m/s | Sidearm | **COMPLETE** |
| M320 GL | 40mm | 76 m/s | Grenade launcher | **COMPLETE** |
| Mossberg 590 | 12ga | 400 m/s | Shotgun | **COMPLETE** |
| Sniper (Lapua) | .338 Lapua | 915 m/s | Long-range sniper | **COMPLETE** |
| M2 Browning | .50 BMG | 930 m/s | Heavy machine gun | **COMPLETE** |
| QBZ-95 | 5.8×42mm | 930 m/s | PLA standard rifle | **COMPLETE** |
| Type 56 | 7.62×39mm | 715 m/s | PLA assault rifle | **COMPLETE** |

### Complete AI Subsystems

| Primordia Module | File | Function | Status |
|---|---|---|---|
| **Aletheia** (Validator) | `SHPrimordiaAletheia.cpp` | Decision validation, kill-zone checks, pattern detection | **COMPLETE** |
| **Echo** (Comms) | `SHPrimordiaEcho.cpp` | Range-based messaging, hand signals, ring buffer history | **COMPLETE** |
| **Simulon** (Threat Model) | `SHPrimordiaSimulon.cpp` | Target prediction, velocity extrapolation, confidence decay | **COMPLETE** |
| **Astraea** (Cognitive State) | `SHPrimordiaAstraea.cpp` | Stress-to-quality mapping, fatigue, recovery modifiers | **COMPLETE** |
| **Mnemonic** (Learning) | `SHPrimordiaMnemonic.cpp` | Player pattern analysis, counter-strategy generation, persistence | **COMPLETE** |
| **Decision Engine** | `SHPrimordiaDecisionEngine.cpp` | Order decomposition, force allocation, escalation, morale retreat | **COMPLETE** (1 bug) |
| **Tactical Analyzer** | `SHPrimordiaTacticalAnalyzer.cpp` | Battlefield JSON, sector analysis, terrain ray-casting | **COMPLETE** |
| **Debug Overlay** | `SHPrimordiaDebugOverlay.cpp` | Real-time decision inspection (<0.1ms overhead) | **COMPLETE** |
| **Replay Timeline** | `SHPrimordiaReplayTimeline.cpp` | Event recording, JSON export, movement paths | **COMPLETE** |
| **Network Client** | `SHPrimordiaClient.cpp` | WebSocket + HTTP polling, auth, heartbeat, auto-reconnect | **COMPLETE** |

### Complete Combat Systems

| System | Files | Status |
|---|---|---|
| Ballistics (drag, gravity, wind, penetration, ricochet) | `SHBallisticsSystem` | **COMPLETE** |
| Damage (6 hit zones, armor, bleedout, head graze) | `SHDamageSystem` | **COMPLETE** |
| Suppression (5 levels, caliber-based, proximity) | `SHSuppressionSystem` | **COMPLETE** |
| Cover (EQS search, quality scoring, lean, destruction) | `SHCoverSystem` | **COMPLETE** |
| Morale (6 states, surrender, cascade) | `SHMoraleSystem` | **COMPLETE** |
| Combat Stress (heart rate, tunnel vision, auditory exclusion) | `SHCombatStressSystem` | **COMPLETE** |
| Fatigue (exertion, wound compound, recovery) | `SHFatigueSystem` | **COMPLETE** |
| Death (ragdoll, animation blending) | `SHDeathSystem` | **COMPLETE** |
| Hit Feedback (directional indicators, impact effects) | `SHHitFeedback` | **COMPLETE** |
| Indirect Fire (mortar/artillery, TOF, CEP) | `SHIndirectFireSystem` | **COMPLETE** |
| Call for Fire (observer designation, authorization) | `SHCallForFireSystem` | **COMPLETE** |
| Optics (scope system) | `SHOpticsSystem` | **COMPLETE** |

### Complete UI Screens

| Screen | File | Status |
|---|---|---|
| Military HUD (suppression vignette, injury, squad, compass, radio) | `SHHUD` | **COMPLETE** |
| Main Menu | `SHMainMenuWidget` | **COMPLETE** |
| Pause Menu | `SHPauseMenuWidget` | **COMPLETE** |
| Loading Screen | `SHLoadingScreenWidget` | **COMPLETE** |
| Loadout Customization | `SHLoadoutWidget` | **COMPLETE** |
| Settings Panel | `SHSettingsWidget` | **COMPLETE** |
| Squad Command (radial) | `SHSquadCommandWidget` | **COMPLETE** |
| Compass (bearing/distance) | `SHCompassWidget` | **COMPLETE** |
| After-Action Report | `SHAfterActionWidget` | **COMPLETE** |

### Complete Supporting Systems

| System | Status |
|---|---|
| Weather (6 types, affects combat) | **COMPLETE** |
| Day/Night cycle | **COMPLETE** |
| Terrain (procedural placement, world streaming) | **COMPLETE** |
| Destruction (5-stage, structural collapse, craters, debris) | **COMPLETE** |
| Electronic Warfare (GPS/comms/drone jamming, 5-level escalation) | **COMPLETE** |
| Drones (ISR thermal/daylight/NVG, FPV strike, counter-drone, battery) | **COMPLETE** |
| Vehicles (base, seat system, mounted weapons) | **COMPLETE** |
| Squad AI (formations, buddy teams, bounding advance, orders) | **COMPLETE** |
| Dialogue System | **COMPLETE** |
| Briefing System | **COMPLETE** |
| Squad Personality | **COMPLETE** |
| Save/Load | **COMPLETE** |
| Replay Recording | **COMPLETE** |
| Progression/Loadout | **COMPLETE** |
| Accessibility Settings | **COMPLETE** |
| Difficulty Settings | **COMPLETE** |
| Camera System | **COMPLETE** |
| Co-op Game Mode | **COMPLETE** |
| Mod Manager | **COMPLETE** |
| Spectator Controller | **COMPLETE** |

### Mission 1 — Operation Breakwater

| Element | Status |
|---|---|
| Squad roster (4 members, full personality profiles, backstories) | **COMPLETE** |
| Briefing (intel, force estimates, EW capabilities, civilian status) | **COMPLETE** |
| Phase 1: Pre-Invasion (0430, 210s of tension, scripted dialogue) | **COMPLETE** |
| Phase 2: Beach Assault (4 reinforcement waves, 114+ enemies, vehicles) | **COMPLETE** |
| Phase 3: Urban Fallback | **COMPLETE** |
| Phase 4: Collapse | **COMPLETE** |
| Enemy PLA squads (6-man, role-based: Rifleman/MG/RPG/Officer) | **COMPLETE** |
| Narrative dialogue with timestamps | **COMPLETE** |

### Known Bugs

| # | Bug | Location | Severity | Fix Effort |
|---|---|---|---|---|
| B1 | `GetAvailableSquadCount(false)` called but not declared in header — **will not compile** | `SHPrimordiaDecisionEngine.cpp:91` / `.h` | **BLOCKER** | 5 min |

---

## WORKSTREAM OVERVIEW

The remaining work falls into **9 parallel workstreams**. Each workstream is independently actionable. Dependencies between workstreams are noted explicitly.

| ID | Workstream | What It Delivers | Blocks Demo? |
|---|---|---|---|
| **A** | Level & Environment | The playable 3D world — Taoyuan Beach | **YES** |
| **B** | Asset Deployment | All generated assets imported and placed in UE5 | **YES** |
| **C** | Integration & Wiring | C++ systems connected to Blueprints, Actors, and gameplay | **YES** |
| **D** | Feel & Polish | Camera, movement, weapon feedback, hit reactions, screen effects | **YES** |
| **E** | Audio Integration | Sound assets connected to gameplay events, spatial audio tuned | **YES** |
| **F** | UI/UX Flow | Complete player journey from main menu to after-action report | **YES** |
| **G** | Cinematics & Narrative | Briefing presentation, phase transitions, voice performance | HIGH IMPACT |
| **H** | Performance & Optimization | Stable 60fps with 114+ AI, destruction, VFX | **YES** |
| **I** | QA & Testing | Automated tests, playtesting, bug bash | **YES** |

---

## WORKSTREAM A — LEVEL & ENVIRONMENT

**Priority:** CRITICAL — Nothing else matters without a world to play in.
**Dependency:** None (can start immediately)
**Feeds into:** B (asset placement), C (actor wiring), E (audio zones), H (performance)

### A.1 — Taoyuan Beach World Design

The full battlefield is **8 km × 8 km (64 km²)** per the GDD. This is not negotiable — the engagement distances, the operational tempo, and the sense of scale that separates this from every other shooter all depend on a real-scale battlefield. The player must feel the vastness. Movement between engagements takes real time. Most of the map is empty space — that emptiness is the point.

The map is divided into **high-density combat zones** connected by **traversal terrain**. World Partition streams content — the player never sees pop-in at tactical distances, but the engine only renders what's needed.

#### A.1.1 — Map Layout Requirements

| Zone | Size | Purpose | Phase | Density |
|---|---|---|---|---|
| **Taiwan Strait (Ocean)** | 8km × 3km | PLA fleet staging. Landing craft approach visible from 3km+. Naval bombardment origin. | Phase 1–2 visual | LOW — water plane, distant vessel LODs |
| **Beach & Landing Zone** | 4km × 1.5km | Full-width beach assault front. Multiple landing sectors. Open kill zone. Craters form dynamically. Tidal flats, sea walls, wire obstacles. | Phase 2 (Beach Assault) | HIGH — destruction, AI, vehicles |
| **Defensive Position (Trench Line)** | 3km × 200m | Player's sector is ~400m wide. Full trench system with bunkers, firing slits, mortar pits, command post. Adjacent sectors have friendly units (visible, audible, not player-controlled). | Phase 1 (Pre-Invasion), Phase 2 | HIGH — fortifications, squad |
| **Coastal Infrastructure** | 3km × 500m | Coastal highway, checkpoint, fuel depot, radar installation (destroyed in Phase 1 bombardment), seawall. Transitional combat zone. | Phase 2→3 | MEDIUM — vehicles, debris, infrastructure |
| **Agricultural Buffer** | 2km × 1km | Rice paddies, irrigation channels, farm buildings, tree lines. Open terrain with sparse concealment. Sniper and MG engagement distances (500–800m). | Phase 2→3 | LOW — vegetation, sparse structures |
| **Taoyuan City Outskirts** | 3km × 2km | 2–5 story residential and commercial buildings, narrow streets, market district, temple, school (civilian shelter), parking structures. Dense urban CQB. | Phase 3 (Urban Fallback) | VERY HIGH — buildings, interiors, civilians |
| **Taoyuan Airport (Background)** | 2km × 2km | Visible on skyline. Burning from Phase 1 bombardment. Smoke column is persistent landmark. PLA air operations visible (distant aircraft, AA tracers). Not player-accessible in Mission 1. | Visual reference | MINIMAL — LOD, VFX, skybox |
| **Collapse Zone & Extraction** | 1km × 1km | Highway interchange, overpass, industrial buildings. Last-stand defensible position. Extraction route south. | Phase 4 (Collapse) | HIGH — final combat |
| **Mountain Backdrop (East)** | Visual only | Forested hills rising behind the city. Establishes geography. | All phases | MINIMAL — HLOD terrain |

**Total Playable Area:** 64 km² (8 km × 8 km)
**High-Density Combat Zones:** ~15 km² (where 90% of gameplay occurs)
**Traversal/Visual Terrain:** ~49 km² (streamed at lower LOD, provides scale and horizon)

#### A.1.2 — Terrain & Landscape Tasks

| # | Task | Detail | Tool |
|---|---|---|---|
| A-01 | Create base heightmap | 8129×8129 resolution (1m per pixel for 8km × 8km), real Taoyuan coastal geography reference, beach-to-urban-to-mountain elevation gradient | World Machine / Gaea |
| A-02 | Import heightmap into UE5 Landscape | World Partition enabled, 256m streaming cells, landscape components sized for streaming | UE5 Editor |
| A-03 | Sculpt beach topography | 4km beach front, tidal flats, gentle slope to defensive ridge, drainage channels, sea walls | UE5 Landscape |
| A-04 | Sculpt urban terrain | Flat with road grid, sidewalk elevation, building footprints, drainage, Taoyuan city reference | UE5 Landscape |
| A-04b | Sculpt agricultural buffer | Rice paddy grid, irrigation channels, farm road network, tree line breaks | UE5 Landscape |
| A-04c | Sculpt mountain backdrop | Eastern hills, forested slopes, visual-only terrain with HLOD | World Machine → UE5 |
| A-05 | Apply landscape material layers | Sand, wet sand, mud, concrete, asphalt, grass, dirt (from Art Pipeline PBR textures) | UE5 Material |
| A-06 | Paint landscape weights | Blend zones between terrain types, road edges, crater-ready areas | UE5 Landscape |
| A-07 | Configure World Partition | 256m streaming cells, proximity-based loading, 3km active radius around player, low-LOD beyond | UE5 World Partition |
| A-08 | Set up HLOD for full 64 km² | LOD0 (combat zones, <500m), LOD1 (traversal, <2km), LOD2 (visual, <5km), LOD3 (horizon, >5km). Airport and mountain as HLOD only. | UE5 HLOD |
| A-09 | Ocean/water plane | Nanite water, wave simulation, landing craft interaction | UE5 Water Plugin |
| A-10 | Skybox & atmosphere | Pre-dawn (0430) to morning (0800) progression, volumetric clouds, haze | UE5 Sky Atmosphere |

#### A.1.3 — Structural & Environmental Assets

| # | Task | Detail | Quantity |
|---|---|---|---|
| A-11 | Defensive bunker (modular) | Sandbag walls, firing slits, overhead cover, command post, mortar pits, ammo storage — destructible | 8–10 variants |
| A-12 | Trench system | Full 3km defensive line: fighting trenches, communication trenches, mortar pits, command bunkers, wire obstacles, mine markers | ~3km linear (player sector ~400m, adjacent sectors visible) |
| A-13 | Beach obstacles | Czech hedgehogs, concrete tetrapods, wire obstacles, anti-vehicle ditches, sea wall sections | 100–150 pieces across 4km beach |
| A-14 | Landing craft (PLA) | Type 726 LCAC, Type 072A LST — beached, approaching, and destroyed variants. Interior for boarding/unloading sequences. | 6–8 models |
| A-15 | Urban buildings (modular) | 2–5 story residential, commercial, industrial shells with playable interiors, destructible walls, climbable roofs, balconies, stairwells | 40–60 buildings (modular kit, ~12 base types with variants) |
| A-16 | Market / commercial district | Street-level shops, signage (Traditional Chinese), market stalls, restaurant, convenience store, temple, school (civilian shelter) | 15–20 structures |
| A-17 | Civilian vehicles | Scooters, sedans, trucks, buses, taxis — abandoned, some burning, some used as barricades | 25–30 variants |
| A-18 | Military vehicles (PLA) | ZBD-05 amphibious IFV, Type 05 AAV, Type 08 wheeled IFV, logistics trucks — active, destroyed, burning variants | 5–6 base models with state variants |
| A-18b | Military vehicles (USMC/ROC) | HMMWV, JLTV, LAV-25 — pre-positioned at defensive line, some destroyed in bombardment | 3–4 base models |
| A-19 | Road infrastructure | Street lights, power lines (destroyable, droop when cut), traffic signals, road markings, drains, highway signs, overpass sections | Full kit |
| A-20 | Vegetation | Coastal grasses, palm trees, tropical shrubs, urban landscaping, rice paddies (agricultural buffer), tree lines, bamboo groves | Procedural placement via `SHProceduralPlacement` + manual hero placement |
| A-21 | Destruction pre-sets | Pre-damaged building variants for Phase 3/4 (progressive destruction), plus pre-bombardment damaged variants for coastal infrastructure | Per building, per destruction stage |
| A-21b | Agricultural structures | Farm buildings, storage sheds, irrigation pump houses, raised grain stores — sparse cover in open terrain | 6–8 variants |
| A-21c | Highway / overpass infrastructure | Multi-lane highway, on/off ramps, overpass bridge (Phase 4 collapse zone), jersey barriers, highway signage | Kit for ~2km highway segment |

#### A.1.4 — Lighting & Atmosphere

| # | Task | Detail |
|---|---|---|
| A-22 | Time-of-day blueprint | Drives sun position, color temperature, shadow direction across 0430–0800+ (full mission may extend to midday depending on player pace). Real Taoyuan lat/long for accurate sun angles. |
| A-23 | Pre-dawn lighting (Phase 1) | Deep blue-black, stars visible, horizon glow starting. Flashlights and NVG territory. |
| A-24 | Dawn lighting (Phase 2) | Golden hour from behind PLA forces — player shoots into the light. Deliberate. |
| A-25 | Morning lighting (Phase 3–4) | Harsh daylight, smoke diffusion, fire glow in shadows. |
| A-26 | Volumetric fog/smoke | Artillery smoke, burning vehicles, building fires — all affect visibility and AI detection. |
| A-27 | Lumen GI configuration | Global illumination for interior spaces, fire bounce lighting. |
| A-28 | Nanite mesh configuration | Enable Nanite on all static meshes >1k triangles. |

#### A.1.5 — Environment VFX

| # | Task | Detail |
|---|---|---|
| A-29 | Ocean spray & mist | Niagara system — surf on beach, spray from explosions near water |
| A-30 | Sand/dust kick-up | Bullet impacts on sand, footstep dust, vehicle movement dust |
| A-31 | Fire & smoke systems | Burning vehicles, building fires, with light emission and AI visibility impact |
| A-32 | Artillery crater VFX | Dynamic crater formation — mesh deformation + particle burst + debris |
| A-33 | Weather particles | Light rain starting Phase 3 (per mission script), affects all systems |

---

## WORKSTREAM B — ASSET DEPLOYMENT

**Priority:** CRITICAL — Parallel with Workstream A.
**Dependency:** Asset pipelines already built and tested. Assets generated but not imported.
**Feeds into:** A (placement), C (wiring), D (feel), E (audio)

### B.1 — Pipeline Execution & Import

All 7 asset pipelines have generation scripts and manifests. The master import script exists (`Tools/import_all_to_ue5.py`). The work is: run the pipelines, validate output, import into the UE5 project, and organize in the Content Browser.

#### B.1.1 — Texture Assets (Art Pipeline)

| # | Task | Asset Count | Detail |
|---|---|---|---|
| B-01 | Generate all PBR terrain textures | 375 textures | 15 terrain types × 5 destruction stages × 5 maps (albedo, normal, roughness, AO, height) |
| B-02 | Validate texture quality | 375 | Resolution check (2048×2048 min), seamless tiling, PBR value ranges |
| B-03 | Import into UE5 Content/Textures | 375 | Correct compression settings, texture groups, streaming mips |
| B-04 | Create Material Instances | ~30 | Landscape materials, structural materials, vehicle materials |
| B-05 | Create Landscape Material | 1 | Multi-layer landscape material with auto-slope, wetness, destruction blending |

#### B.1.2 — Audio Assets (Audio Pipeline)

| # | Task | Asset Count | Detail |
|---|---|---|---|
| B-06 | Generate weapon SFX | ~50 | Per-weapon: fire, fire-distant, reload, empty-click, malfunction-clear, suppressed variants |
| B-07 | Generate impact SFX | ~40 | Per-material: bullet impact on sand/concrete/metal/wood/flesh/water/glass/dirt |
| B-08 | Generate footstep SFX | ~30 | Per-surface × stance: sand, concrete, wood, metal, water, grass × walk/run/sprint |
| B-09 | Generate explosion SFX | ~15 | Grenade, mortar, artillery (near/mid/far), vehicle destruction, building collapse |
| B-10 | Generate ambient SFX | ~20 | Ocean, wind, distant battle, radio chatter, birds (pre-invasion), insects |
| B-11 | Generate environment SFX | ~19 | Fire crackle, debris settle, glass break, door breach, vehicle engine |
| B-12 | Generate music/soundtrack | 25 tracks | Phase-specific tension scoring: silence → dread → chaos → desperation → silence |
| B-13 | Generate voice lines | 260+ lines | Squad dialogue (English), PLA callouts (Mandarin), radio traffic |
| B-14 | Import all audio into UE5 | 400+ | MetaSound sources, attenuation settings, concurrency groups |

#### B.1.3 — Animation Assets (Animation Pipeline)

| # | Task | Asset Count | Detail |
|---|---|---|---|
| B-15 | Generate character animations | 55+ montages | Locomotion (8-dir), weapon handling (per weapon), combat actions, injury states |
| B-16 | Generate facial animations | ~10 | Squad member dialogue delivery, stress expressions, death |
| B-17 | Import into UE5 Animation system | 65+ | Retarget to character skeletons, create AnimBP, blend spaces |
| B-18 | Create Animation Blueprints | 3 | Player, friendly squad, enemy (with per-role variants) |
| B-19 | Set up blend spaces | ~8 | Locomotion (speed × direction), aim offset, lean, injury compensation |

#### B.1.4 — 3D Model Assets (Model Pipeline)

| # | Task | Asset Count | Detail |
|---|---|---|---|
| B-20 | Generate/source character models | 6+ | Player arms (first-person), squad members (third-person), PLA enemy variants |
| B-21 | Generate/source weapon models | 11 | First-person and third-person variants for all 11 weapons |
| B-22 | Generate/source prop models | ~50 | Crates, barriers, equipment, medical supplies, ammo boxes, radios |
| B-23 | Import all models into UE5 | 67+ | Nanite-enabled, LODs generated, collision configured |
| B-24 | Create skeletal meshes | 6+ | Character skeletons rigged for animation system |

#### B.1.5 — VFX Assets (VFX Pipeline)

| # | Task | Asset Count | Detail |
|---|---|---|---|
| B-25 | Generate Niagara VFX systems | 62 | Muzzle flash, bullet tracers, impact sparks, blood, explosions, smoke, fire, water splash, debris, suppression screen effects |
| B-26 | Import and configure in UE5 | 62 | Niagara emitters, GPU simulation where possible, performance budgets |
| B-27 | Create VFX parameter collections | 5 | Weather-driven VFX modifiers, time-of-day driven, destruction-stage driven |

#### B.1.6 — Weapon Data Assets

| # | Task | Asset Count | Detail |
|---|---|---|---|
| B-28 | Generate USHWeaponDataAsset instances | 11 | One DataAsset per weapon using factory defaults from `SHWeaponData.cpp` |
| B-29 | Validate ballistic data | 11 | Cross-reference muzzle velocity, BC, effective range against doctrine sources |
| B-30 | Create weapon Blueprints | 11 | BP_Weapon_M27, BP_Weapon_M4A1, etc. — mesh + data + VFX + audio wired |

#### B.1.7 — UI Art Assets (UI Pipeline)

| # | Task | Asset Count | Detail |
|---|---|---|---|
| B-31 | Generate HUD art elements | 28 | Compass rose, injury silhouette, squad icons, ammo indicator, radio overlay, suppression vignette, stance indicator |
| B-32 | Import into UE5 as Slate/UMG assets | 28 | Correct anchoring, DPI scaling, atlas configuration |
| B-33 | Create Widget Blueprints | 9 | WBP for each UI screen, binding to C++ widget classes |

### B.2 — Content Organization

| # | Task | Detail |
|---|---|---|
| B-34 | Establish Content directory structure | `/Content/ShatteredHorizon/` with subdirs: Characters, Weapons, Environment, Audio, VFX, UI, Materials, Maps, Blueprints |
| B-35 | Asset naming convention enforcement | Prefix convention: `T_` (textures), `SM_` (static mesh), `SK_` (skeletal), `M_` (material), `MI_` (material instance), `NS_` (Niagara), `WBP_` (widget), `ABP_` (anim BP), `BP_` (blueprint) |
| B-36 | Asset audit & orphan cleanup | Remove unused imports, verify all references resolve, no broken asset links |

---

## WORKSTREAM C — INTEGRATION & WIRING

**Priority:** CRITICAL — Systems exist in C++ but must be connected to the game world.
**Dependency:** A (level exists), B (assets imported)
**Feeds into:** D (feel), E (audio), F (UI), H (performance)

This is the workstream that turns 165 C++ files into a playable game. Every system needs to be wired to Blueprints, Actors, and gameplay events in-editor.

### C.1 — Bug Fixes (Day 1)

| # | Task | Detail | File |
|---|---|---|---|
| C-01 | Fix compilation blocker | Add `int32 GetAvailableSquadCount(bool bIncludeReserves) const;` to `SHPrimordiaDecisionEngine.h` | `AI/Primordia/SHPrimordiaDecisionEngine.h` |
| C-02 | Full project compile | Clean build, resolve any additional linker/include errors | All |
| C-03 | Fix morale enum inconsistency | Consolidate `SHMoraleSystem` (Determined/Steady/Shaken/Breaking/Routed) with `SHEnemyCharacter` (Confident/Steady/Shaken/Breaking/Broken/Surrendered) into single authoritative enum | `Combat/SHMoraleSystem.h`, `AI/SHEnemyCharacter.h` |

### C.2 — Player Character Wiring

| # | Task | Detail |
|---|---|---|
| C-04 | Create BP_PlayerCharacter | Blueprint subclass of `SHPlayerCharacter`, configure mesh, camera, capsule |
| C-05 | Wire Enhanced Input | Input mapping context for movement, weapon, squad commands, drone, lean, stance |
| C-06 | Wire first-person arms | Attach FP arm mesh to camera, weapon socket configuration |
| C-07 | Wire weapon system | Equip/unequip flow, weapon swap, reload animation triggers, ADS transition |
| C-08 | Wire combat stress to post-process | Heart rate → vignette intensity, desaturation, DOF changes via post-process dynamic material |
| C-09 | Wire suppression to post-process | Suppression level → screen shake, vignette, audio ducking, aim penalty |
| C-10 | Wire fatigue to movement | Stamina → sprint speed, sway amplitude, reload speed modifier |
| C-11 | Wire injury system to gameplay | Per-limb damage → movement penalty, aim penalty, bleedout timer, death |
| C-12 | Wire optics system | ADS → FOV change, scope overlay render target, scope sway |
| C-13 | Wire drone controls | ISR drone deploy/recall, camera feed UI, FPV strike drone flight controls |

### C.3 — Enemy AI Wiring

| # | Task | Detail |
|---|---|---|
| C-14 | Create BP_EnemyCharacter (per role) | 7 variants: Rifleman, MG Gunner, RPG, Grenadier, Marksman, Officer, Medic |
| C-15 | Wire Behavior Tree | Connect `SHEnemyAIController` behavior tree to cover, fire, maneuver, morale nodes |
| C-16 | Wire perception to AI controller | `SHAIPerceptionConfig` DataAsset → AI Perception Component sight/hearing channels |
| C-17 | Wire Primordia Decision Engine | Mission script → Primordia directives → tactical orders → squad assignments → individual behavior |
| C-18 | Wire morale to behavior | Morale state changes → behavior tree transitions (hold/retreat/surrender/rout) |
| C-19 | Wire voice callouts | AI state changes → callout triggers → audio playback with Mandarin dialogue |
| C-20 | Wire death system | Health → 0 triggers `SHDeathSystem` → animation → ragdoll blend → physics |
| C-21 | Wire AI suppression response | Incoming fire proximity → `SHSuppressionSystem` → suppression level → behavior tree gate |
| C-22 | Wire squad communication (Echo) | Proximity-based message passing, hand signals in visual range, radio beyond |
| C-23 | Wire threat prediction (Simulon) | Target tracking → velocity extrapolation → aim prediction for AI shooting |
| C-24 | Wire decision validation (Aletheia) | Pre-execution validation of tactical orders, rejection logging |
| C-25 | Wire cognitive state (Astraea) | Stress → decision quality degradation, fatigue → alertness decay |
| C-26 | Wire behavioral learning (Mnemonic) | Player action recording → pattern analysis → counter-strategy generation |

### C.4 — Friendly Squad Wiring

| # | Task | Detail |
|---|---|---|
| C-27 | Create BP_SquadMember (per role) | Vasquez (SAW), Kim (Grenadier), Chen (Marksman), Williams (Corpsman) |
| C-28 | Wire squad AI controller | `SHSquadAIController` → follow, cover, engage, heal, revive behaviors |
| C-29 | Wire squad orders | Radial command menu → order types → squad member acknowledgment + execution |
| C-30 | Wire squad dialogue | Personality-driven dialogue triggers → contextual voice line selection → playback |
| C-31 | Wire squad personality | `SHSquadPersonality` traits → combat behavior modifiers (aggressive/cautious/calm) |
| C-32 | Wire squad medical | Williams (Corpsman) → triage priority → approach downed member → heal animation → health restore |

### C.5 — Weapon System Wiring

| # | Task | Detail |
|---|---|---|
| C-33 | Create weapon Blueprints (11) | BP_Weapon per weapon type, mesh + DataAsset + VFX + audio references |
| C-34 | Wire ballistics system | Fire event → `SHBallisticsSystem` projectile spawn → physics simulation → impact |
| C-35 | Wire penetration system | Impact event → material check → penetration calculation → exit wound / ricochet |
| C-36 | Wire fragmentation | Grenade/explosive detonation → `SHBallisticsSystem` fragment generation → per-fragment damage |
| C-37 | Wire weapon heat/jam | Continuous fire → heat accumulation → malfunction chance → jam animation → clear |
| C-38 | Wire tracer system | Tracer interval → visible projectile with trail VFX (every 4th round for MG) |
| C-39 | Wire mounted weapons | Vehicle seat → `SHMountedWeapon` → traverse limits, overheat, ammo belt |

### C.6 — Mission Script Wiring

| # | Task | Detail |
|---|---|---|
| C-40 | Wire mission definition to game mode | `SHMission_TaoyuanBeach::CreateMissionDefinition()` → `SHGameMode` → phase management |
| C-41 | Wire phase transitions | Phase 1→2→3→4 triggers: timer, event, objective completion |
| C-42 | Wire objective system | `SHObjectiveSystem` → HUD objective display → completion detection → next objective |
| C-43 | Wire enemy wave spawning | Phase 2 wave timers → spawn points on beach → `MakePLASquad()` → squad assignment |
| C-44 | Wire reinforcement waves to Primordia | New squads → `OnReinforcementWaveSpawned()` → tactical order assignment |
| C-45 | Wire scripted events | Dialogue triggers, artillery barrages, air strikes, EW jamming escalation per mission timeline |
| C-46 | Wire EW escalation | Time-based EW levels → GPS drift, comms jamming, drone jamming per `SHElectronicWarfare` |
| C-47 | Wire indirect fire events | Mission script artillery calls → `SHIndirectFireSystem` → TOF delay → impact area |
| C-48 | Wire weather progression | Clear (Phase 1) → haze (Phase 2) → overcast (Phase 3) → rain (Phase 4) |

### C.7 — World Systems Wiring

| # | Task | Detail |
|---|---|---|
| C-49 | Wire destruction system | Damage events → `SHDestructionSystem` → 5-stage destruction → structural collapse cascade |
| C-50 | Wire cover system to environment | Destructible cover → cover quality reduction → AI cover search re-evaluation |
| C-51 | Wire weather to gameplay | `SHWeatherSystem` → detection modifiers, movement penalties, VFX, audio changes |
| C-52 | Wire terrain manager | Crater generation from explosions, debris persistence, track marks from vehicles |
| C-53 | Wire procedural placement | `SHProceduralPlacement` for environmental detail: debris, shell casings, scattered equipment |

---

## WORKSTREAM D — FEEL & POLISH

**Priority:** CRITICAL — This is the difference between "tech demo" and "$25M investment."
**Dependency:** C (systems wired)
**Feeds into:** G (cinematics), I (QA)

Feel is the emotional layer. It's what makes an investor lean forward in their chair. Every system is implemented — this workstream makes them *felt*.

### D.1 — Camera & Movement Feel

| # | Task | Detail |
|---|---|---|
| D-01 | Camera shake system | Procedural shake for: weapon fire (per-caliber), nearby explosions (distance-scaled), footstep bob, vehicle riding |
| D-02 | Camera recoil | Visual recoil separate from mechanical recoil — camera punches up, recovers smoothly |
| D-03 | Camera injury effects | Hit taken → directional flinch, screen blood splatter (per-region), red vignette pulse |
| D-04 | Sprint camera | FOV increase on sprint, slight forward lean, heavy bob pattern |
| D-05 | Lean camera | Smooth lean transition, head exposure matches camera offset for fair AI detection |
| D-06 | Death camera | Slow-motion fall, camera separates from head, world goes muffled — not instant black |
| D-07 | Movement weight | Acceleration/deceleration curves feel heavy and grounded — not floaty, not snappy |
| D-08 | Stance transitions | Stand→crouch→prone with appropriate speed, no instant teleporting between stances |
| D-09 | Vault/mantle | Context-sensitive vault over low cover, mantle onto ledges — military, not parkour |
| D-10 | Head bob calibration | Subtle, stance-dependent, suppressible in settings for accessibility |

### D.2 — Weapon Feel

| # | Task | Detail |
|---|---|---|
| D-11 | Muzzle flash tuning | Per-weapon flash intensity, duration, light emission radius. Night vs. day variants. |
| D-12 | Recoil animation polish | Bolt carrier animation, magazine physics, ejection port brass ejection |
| D-13 | ADS transition polish | Smooth zoom, scope alignment, brief blur-to-focus, parallax on non-scoped weapons |
| D-14 | Weapon sway tuning | Breath cycle, fatigue-amplified, reduced in prone/bipod, zeroed when freshly ADS |
| D-15 | Reload animation polish | Tactical vs. empty reload distinction, magazine handling, charging handle |
| D-16 | Dry fire feedback | Audible click, bolt lock animation, force reload prompt |
| D-17 | Weapon switch polish | Holster current → draw new, appropriate timing per weapon size |
| D-18 | Bipod deploy | SAW and sniper bipod placement on surfaces, accuracy bonus, restricted traverse |

### D.3 — Impact & Hit Feedback

| # | Task | Detail |
|---|---|---|
| D-19 | Bullet impact VFX per material | Sand puff, concrete chip + dust, metal spark, wood splinter, water splash, glass shatter, flesh mist |
| D-20 | Bullet crack/whizz | Supersonic crack for near-miss rounds, subsonic whizz for slower projectiles — **directional** |
| D-21 | Hit confirmation (subtle) | No hitmarker. Instead: target flinch animation, blood particle, audio thud at distance. Player earns the confirm. |
| D-22 | Kill confirmation (subtle) | Target ragdoll, weapon drop sound, squad member callout ("Target down"). No UI element. |
| D-23 | Suppression screen effects | Progressive: slight desaturation → vignette → camera shake → tunnel vision → flinch animations |
| D-24 | Explosion feel | Screen shake + audio compression + debris particles + radial force on loose objects + temporary deafness |
| D-25 | Vehicle destruction | Multi-stage: mobility kill → fire → cook-off (ammo) → hull breach. Each stage visible and audible. |

### D.4 — Environmental Feel

| # | Task | Detail |
|---|---|---|
| D-26 | Dust and atmosphere | Persistent dust haze in combat zones, smoke drift from fires, artillery haze |
| D-27 | Dynamic debris | Explosions scatter physical debris that settles, provides micro-cover, crunches underfoot |
| D-28 | Fire propagation visual | Flames spread between objects, smoke rises, embers drift — affects AI visibility |
| D-29 | Water interaction | Footstep splashes, bullet impacts, blood dispersion in puddles |
| D-30 | Wind effects | Vegetation sway, flag/cloth movement, paper/debris blown, affects ballistics |

### D.5 — Stress & Immersion Feel

| # | Task | Detail |
|---|---|---|
| D-31 | Tunnel vision implementation | Heart rate > 160bpm → progressive vignette narrowing, per `SHCombatStressSystem` |
| D-32 | Auditory exclusion | Heart rate > 170bpm → selective audio dropout, own heartbeat becomes audible |
| D-33 | Fine motor degradation | High stress → increased weapon sway, slower reload, fumbled magazine changes |
| D-34 | Time distortion | Peak stress moments → subtle time dilation (0.85x), sound pitch shift |
| D-35 | Recovery feel | Safe behind cover → heart rate decreasing → vision widening → hearing returning. Relief is gameplay. |
| D-36 | Squad stress transmission | Hearing squad members panting, cursing, praying under fire. Their stress is audible. |

---

## WORKSTREAM E — AUDIO INTEGRATION

**Priority:** CRITICAL — Audio is 50% of immersion. A muted demo is a failed demo.
**Dependency:** B (audio assets imported), C (gameplay events wired)
**Feeds into:** D (feel), G (cinematics)

### E.1 — Weapon Audio

| # | Task | Detail |
|---|---|---|
| E-01 | Per-weapon fire sound | Each of 11 weapons gets unique fire sound. MetaSound source with distance attenuation, outdoor/indoor variants. |
| E-02 | Crack-thump sound propagation | **Supersonic crack arrives first** at listener position. Muzzle report arrives `distance / 343` seconds later. This is THE audio signature of a real firefight. Must be correct. |
| E-03 | Suppressed weapon variants | Reduced report volume, shifted frequency, shorter attenuation distance |
| E-04 | Reload sounds | Magazine release, insert, charging handle, bolt catch — per weapon, per reload type |
| E-05 | Dry fire / malfunction | Click, bolt jam, clearing action sounds |
| E-06 | Brass casing sounds | Shell casing hitting ground — different sound per surface (concrete, sand, metal, water) |
| E-07 | Bullet snap/whizz | Near-miss audio at listener — doppler-correct, directional, distance-scaled intensity |
| E-08 | Ricochet audio | Bullet deflection — distinct "zing" with random pitch variation |
| E-09 | Mounted weapon audio | M2 Browning heavy report, sustained fire loop with barrel heat progression |

### E.2 — Impact & Explosion Audio

| # | Task | Detail |
|---|---|---|
| E-10 | Bullet impact per material | Distinct sound: sand (thud), concrete (crack + chip), metal (ping), wood (thunk), water (splash), glass (shatter), flesh (wet impact) |
| E-11 | Grenade explosion | Close/mid/far variants with appropriate low-frequency roll-off at distance |
| E-12 | Artillery/mortar impact | Massive bass impact, debris rain, secondary detonations. **Close hit = temporary deafness (auditory exclusion).** |
| E-13 | Building destruction | Structural collapse — progressive: crack, groan, cascade, crash, dust settle |
| E-14 | Vehicle destruction | Engine fire, ammo cook-off pops, fuel explosion, hull groaning |

### E.3 — Environmental Audio

| # | Task | Detail |
|---|---|---|
| E-15 | Ocean ambience | Persistent wave sounds, surf, tide — scaled by distance to waterline |
| E-16 | Wind ambience | Dynamic wind audio tied to `SHWeatherSystem`, gusting, steady, whistling through structures |
| E-17 | Pre-invasion silence | Phase 1 signature: **near-total silence**. Distant insects. A dog barking far away. Then nothing. The silence IS the tension. |
| E-18 | Distant battle ambience | Phase 2+: Sounds of fighting beyond the player's engagement — artillery thuds, distant gunfire, aircraft. War is bigger than you. |
| E-19 | Fire ambience | Crackling, roaring, wood popping — tied to fire VFX sources, spatial audio |
| E-20 | Rain ambience | Phase 3–4: Rain on different surfaces, increasing intensity, masking other sounds per doctrine |
| E-21 | Indoor vs outdoor transition | Reverb zone changes when entering/exiting buildings — `SHReverbZoneManager` drives MetaSound environment |

### E.4 — Voice & Communication Audio

| # | Task | Detail |
|---|---|---|
| E-22 | Squad voice lines | 260+ contextual lines: contact reports, orders acknowledged, injury callouts, personality-specific combat dialogue. Mixed into spatial audio at squad member positions. |
| E-23 | PLA voice lines (Mandarin) | Enemy callouts: contact, reload, grenade, advance, retreat, wounded, surrender. Player doesn't understand them — that's the point. Fog of war is linguistic too. |
| E-24 | Radio filter | Squad comms through radio get bandpass filter, static, compression — feels like real radio |
| E-25 | Radio degradation under EW | EW jamming levels → progressive radio quality degradation: clean → static → garbled → dead |
| E-26 | Stress voice modulation | Heart rate affects squad voice delivery: calm → strained → shouting → panicked. Not just different lines — actual audio processing. |

### E.5 — Music & Score

| # | Task | Detail |
|---|---|---|
| E-27 | Adaptive music system | Music intensity tied to combat state, not scripted. Detects: engagement, suppression, casualties, phase transitions. |
| E-28 | Phase 1 score: Dread | Minimal. Subsonic hum. Single sustained note. Heart of the tension is silence — music punctuates, doesn't fill. |
| E-29 | Phase 2 score: Chaos | Percussive, dissonant. Enters only at Wave 2+ when overwhelm sets in. Drops out during lulls to let gunfire be the soundtrack. |
| E-30 | Phase 3 score: Desperation | Strings. Emotional. Squad members falling back. The score acknowledges loss without melodrama. |
| E-31 | Phase 4 score: Silence returns | Music fades as the line breaks. The last sounds are footsteps, breathing, and distant explosions. Then nothing. |

### E.6 — Spatial Audio Configuration

| # | Task | Detail |
|---|---|---|
| E-32 | HRTF configuration | Head-related transfer function for accurate directional audio — critical for locating unseen shooters |
| E-33 | Occlusion system | Buildings, terrain, vehicles occlude sound sources realistically |
| E-34 | Reverb zone placement | Per-building interior reverb, open field, trench (narrow reverb), urban canyon (echo) |
| E-35 | Sound propagation paths | Sound travels around corners, through windows, reflects off buildings — not just line-of-sight |
| E-36 | Concurrency management | Performance budget: max simultaneous voices, priority system (weapon fire > ambience > music) |

---

## WORKSTREAM F — UI/UX FLOW

**Priority:** CRITICAL — The investor experience starts at the main menu.
**Dependency:** B (UI art assets), C (widget wiring)
**Feeds into:** G (cinematics), I (QA)

### F.1 — Main Menu

| # | Task | Detail |
|---|---|---|
| F-01 | Main menu background | Animated scene: slow dolly across Taoyuan Beach defensive position at dawn. Atmospheric, moody, sets tone. |
| F-02 | Menu layout | OPERATION BREAKWATER title. Options: BEGIN MISSION, LOADOUT, SETTINGS, QUIT. Minimal. Military aesthetic. No clutter. |
| F-03 | Menu audio | Low ambient — ocean, distant wind. Subtle subsonic hum. |
| F-04 | Loadout screen | Pre-mission weapon and equipment selection. Primary, secondary, grenades, medical, special equipment. Weight displayed. |
| F-05 | Settings screen | Graphics, audio, controls, accessibility, difficulty. Functional, not a showcase. |
| F-06 | Difficulty selection | Integrated into mission start. 5 tiers with clear descriptions of what changes (AI cognitive depth, not enemy health). |

### F.2 — Mission Flow

| # | Task | Detail |
|---|---|---|
| F-07 | Loading screen | Mission briefing content during load: map overview, force disposition, squad roster, intel. Not a progress bar — it's worldbuilding. |
| F-08 | Briefing presentation | `SHBriefingSystem` content rendered visually: satellite imagery, force arrows on map, squad photos, mission objectives. Voiced by commanding officer. |
| F-09 | Phase transition UI | Subtle screen text: "PHASE 2 — BEACH ASSAULT — 0500" with timestamp. Fades in/out. No pause in gameplay. |
| F-10 | Objective updates | New objectives appear as radio transmissions + HUD text: "DEFEND SECTOR 7" / "PROTECT EVACUATION ROUTE". Acknowledged by squad leader. |
| F-11 | Pause menu | Dim overlay. RESUME, RESTART PHASE, SETTINGS, QUIT TO MENU. Confirmation on quit. |
| F-12 | Death screen | No "YOU DIED". Screen goes dark. Squad radio: "VIPER-1 IS DOWN!" Heartbeat fades. Option to restart from last checkpoint. |

### F.3 — HUD Implementation

| # | Task | Detail |
|---|---|---|
| F-13 | Compass widget | Top-center. Bearing, cardinal directions, squad member bearing markers, last-known enemy bearing markers (decaying). |
| F-14 | Injury silhouette | Bottom-left. Body outline with per-region color coding: green → yellow → red → black. Bleed indicators pulse. |
| F-15 | Ammo indicator | Bottom-right. Current magazine (approximate: "FULL" / "HALF" / "LOW" / "LAST MAG"). Fire mode indicator. **No exact round count** — per doctrine. |
| F-16 | Squad status panel | Left edge. 4 squad members: name, status icon (OK/wounded/down/KIA), relative distance, bearing. |
| F-17 | Radio message log | Right edge. Scrolling radio traffic. Fades after 10s. Color-coded: blue (squad), red (enemy detected), yellow (warning), white (command). |
| F-18 | Suppression vignette | Full-screen overlay. Intensity driven by `SHSuppressionSystem` level: slight darkening → heavy tunnel vision. |
| F-19 | Combat stress overlay | Vignette + desaturation + subtle pulse driven by `SHCombatStressSystem` heart rate model. |
| F-20 | Stance indicator | Small icon: standing/crouching/prone. |
| F-21 | Weapon info | Current weapon name, fire mode. Minimal. |
| F-22 | Squad command radial | Hold key → radial menu: MOVE TO, HOLD, ENGAGE, SUPPRESS, REGROUP, MEDICAL. Context-sensitive per target. |
| F-23 | Drone feed overlay | ISR drone camera feed — picture-in-picture or full-screen toggle. Thermal/daylight mode switch. Battery indicator. Signal quality bar degrading under EW. |
| F-24 | Immersive mode toggle | Single key removes ALL HUD elements. For screenshots, immersion, and the bravest players. |

### F.4 — After-Action Report

| # | Task | Detail |
|---|---|---|
| F-25 | AAR screen | Post-mission statistics presented as military debrief document. |
| F-26 | Statistics displayed | Time survived, shots fired, hits, accuracy, enemies neutralized, squad casualties, phases completed, distance traveled. |
| F-27 | AI decision replay teaser | "THE ENEMY'S PERSPECTIVE" button — shows AI Primordia decision timeline for one key moment. This is the investor hook: see the AI thinking. |
| F-28 | Squad fate summary | Per-squad-member outcome: survived, wounded, KIA — with their portrait and one-line status. Emotional weight. |
| F-29 | Demo end screen | "OPERATION BREAKWATER — MISSION 1 OF 12" / "SHATTERED HORIZON 2032" / "COMING [YEAR]" / Wishlist/follow CTA. |

---

## WORKSTREAM G — CINEMATICS & NARRATIVE PRESENTATION

**Priority:** HIGH IMPACT — The narrative hook is what separates this from every other military shooter.
**Dependency:** B (voice assets), C (dialogue wired), E (audio integrated)
**Feeds into:** F (UI flow), I (QA)

### G.1 — Voice Performance

| # | Task | Detail |
|---|---|---|
| G-01 | Cast & record squad voice actors | 4 actors for Vasquez, Kim, Chen, Williams. Must feel authentic — not action-movie voice, real Marines under fire. |
| G-02 | Cast & record commanding officer | Briefing narration, radio commands during mission. Calm authority cracking under pressure as phases progress. |
| G-03 | Cast & record PLA voice actors | Mandarin-speaking actors for enemy callouts. Authentic PLA military terminology. |
| G-04 | Record radio traffic | Background radio chatter for other units fighting across the island. Creates sense of larger battle. |
| G-05 | Record civilian audio | Distant screaming, vehicle horns, evacuation PA system (Mandarin/English) — Phase 3 urban environment. |
| G-06 | Voice direction standards | Emotional arc per phase: calm/tense → panicked/overwhelmed → grim/resigned → broken/surviving. No heroics. |

### G.2 — Key Narrative Moments

These are the moments investors will remember. Each must land perfectly.

| # | Moment | Phase | Detail |
|---|---|---|---|
| G-07 | **"Sofia, daddy loves you"** | Phase 1 end | Vasquez whispers into a personal radio. Then the bombardment starts. No music. Just the words and then thunder. |
| G-08 | **The silence before** | Phase 1 | 30 seconds of near-total silence after EW jamming kills comms. The squad is alone. The player hears their own breathing. |
| G-09 | **First visual contact** | Phase 1→2 | Dawn light reveals the invasion fleet. No dialogue for 5 seconds. Just the visual. Then Kim: "Oh my God." |
| G-10 | **Auditory exclusion moment** | Phase 2 | First close artillery impact — all sound drops to muffled heartbeat. Vision narrows. The player experiences what combat stress feels like. |
| G-11 | **"We are not getting support"** | Phase 2 | Radio call for air support denied. The squad realizes they're alone. This is the emotional turn. |
| G-12 | **Squad member wounded** | Phase 2/3 | First serious squad casualty. Williams (Corpsman) rushes to treat under fire. Player must provide cover. Personal stakes now. |
| G-13 | **Civilian encounter** | Phase 3 | Moving through urban area, squad encounters civilians sheltering. Chen (Taiwanese-American) translates. ROE complexity — can't fire freely. |
| G-14 | **The line breaks** | Phase 4 | Order to retreat. Defensive position overrun. The player watches positions they held get swarmed. Everything they fought for is gone. |
| G-15 | **Final moment** | Phase 4 | Squad reaches extraction. Head count. Silence. Demo ends not with victory but with survival and the question: *what happens next?* |

### G.3 — Cinematic Camera & Presentation

| # | Task | Detail |
|---|---|---|
| G-16 | Briefing sequence | Pre-mission: satellite imagery dissolves into map, force disposition arrows animate, squad roster appears with photographs and short bios. Voiced by CO. |
| G-17 | Phase transition cameras | Brief (2–3 second) environmental shot between phases: time-lapse of sun moving, smoke rising, destruction spreading. Non-interactive but immersive. |
| G-18 | In-engine cutscene system | Using Sequencer for key moments that need precise camera control. Keep them SHORT — 5–10 seconds max. The game is the experience. |
| G-19 | End sequence | Final shot: camera pulls up and back from the squad, revealing the scale of the beach, the fires, the fleet. Single title card. Silence. |

### G.4 — Narrative Polish

| # | Task | Detail |
|---|---|---|
| G-20 | Dialogue pacing calibration | Every scripted dialogue moment hand-timed for dramatic effect. Silence is as important as words. |
| G-21 | Squad personality expression | Each member's personality trait (Veteran/Calm/Aggressive/Cautious) expressed through combat dialogue, movement style, reaction timing. |
| G-22 | Environmental storytelling | Visual narrative in the world: abandoned children's toys, a half-eaten meal in a building, a dog running through streets. War destroys normalcy. |
| G-23 | Intel documents (optional) | Collectible intel in urban area: PLA documents, civilian letters, military orders. Worldbuilding for players who explore. |

---

## WORKSTREAM H — PERFORMANCE & OPTIMIZATION

**Priority:** CRITICAL — A demo that stutters below 60fps at key moments destroys investor confidence instantly.
**Dependency:** All other workstreams must be substantially complete.
**Feeds into:** I (QA)

### H.1 — Target Specifications

| Metric | Target | Hard Minimum |
|---|---|---|
| Frame rate | 60 fps stable | Never below 45 fps |
| Frame time | 16.67ms | Never above 22ms |
| GPU memory | < 8 GB VRAM | < 10 GB |
| System RAM | < 16 GB | < 24 GB |
| Load time (NVMe) | < 15 seconds | < 30 seconds |
| Simultaneous AI entities | 200+ (Wave 4 peak across full beach) | 80+ |
| Active physics bodies | 500+ (debris, ragdolls) | 200+ |
| Active Niagara particles | 10,000+ | 5,000+ |
| Draw calls per frame | < 2,000 | < 3,500 |

**Target hardware for investor demo:** RTX 4080 or equivalent. Must also be playable on RTX 3070.

### H.2 — AI Performance

| # | Task | Detail |
|---|---|---|
| H-01 | Profile AI tick cost | Measure per-agent tick time across all Primordia subsystems. Budget: < 0.5ms per agent per frame. |
| H-02 | AI LOD system | Distant AI (>500m) use simplified behavior trees — no Primordia subsystem ticking, basic state machine only. |
| H-03 | Stagger AI ticks | Not all 114 AI tick every frame. Stagger: nearby AI every frame, mid-range every 2 frames, far every 4 frames. |
| H-04 | EQS query budgeting | Cover search queries are expensive. Budget: max 5 EQS queries per frame, queued and distributed. |
| H-05 | Perception tick optimization | Sight traces are expensive. LOD perception: nearby = per-frame traces, distant = periodic, very distant = event-only. |
| H-06 | MassEntity migration | Evaluate moving distant/idle AI to MassEntity (enabled in project) for entity-component batch processing. |

### H.3 — Rendering Performance

| # | Task | Detail |
|---|---|---|
| H-07 | Nanite mesh audit | Verify all static meshes use Nanite. Remove unnecessary LOD chains on Nanite meshes. |
| H-08 | Lumen optimization | Configure Lumen for performance: screen traces primary, hardware RT secondary. Reduce Lumen quality at distance. |
| H-09 | Virtual Shadow Maps tuning | Shadow resolution, page pool size, caching — balance quality vs. performance. |
| H-10 | Niagara GPU simulation | Move all particle systems to GPU simulation where possible. CPU particles only for gameplay-critical effects. |
| H-11 | Material complexity audit | Identify expensive materials (many texture samples, complex math). Simplify distant materials. |
| H-12 | Texture streaming configuration | Streaming pool size, priority by distance, pre-loading for mission-critical areas. |
| H-13 | Draw call batching | Instance static meshes (debris, beach obstacles, vegetation). Merge actors where possible. |
| H-14 | Occlusion culling verification | Verify precomputed occlusion + dynamic occlusion working for urban buildings. |

### H.4 — Physics Performance

| # | Task | Detail |
|---|---|---|
| H-15 | Chaos physics budget | Max active simulating bodies, sleep threshold tuning, destruction physics pooling. |
| H-16 | Ragdoll optimization | Max simultaneous ragdolls (budget: 20). Oldest ragdolls simplify to static after 10 seconds. |
| H-17 | Debris cleanup | `SHDestructionSystem` max 500 debris. FIFO cleanup beyond budget. Prioritize visible debris. |
| H-18 | Projectile pooling | Recycle projectile actors. Pool size: 200. Avoid GC spikes from rapid fire. |

### H.5 — Audio Performance

| # | Task | Detail |
|---|---|---|
| H-19 | Voice concurrency limits | Max simultaneous audio voices: 64. Priority: weapon fire > explosions > voice > ambient > music. |
| H-20 | Audio distance culling | Sounds beyond audible range don't process. Aggressive culling for small sounds (footsteps, brass casings). |
| H-21 | MetaSound graph optimization | Profile expensive MetaSound graphs. Simplify distant sound processing chains. |

### H.6 — Memory & Loading

| # | Task | Detail |
|---|---|---|
| H-22 | Memory profiling | Full memory audit: texture memory, mesh memory, audio memory, AI memory, physics memory. |
| H-23 | World Partition streaming | Verify seamless streaming with no hitches during phase transitions. Pre-load urban area during Phase 2. |
| H-24 | Asset pre-loading | Critical assets (weapons, squad models, key VFX) loaded during briefing/loading screen. Zero pop-in during gameplay. |
| H-25 | Garbage collection management | Force GC during non-combat moments (phase transitions). Avoid GC during combat. |

---

## WORKSTREAM I — QA & TESTING

**Priority:** CRITICAL — The demo will be played by investors who are not gamers. It must not crash.
**Dependency:** All workstreams substantially complete.

### I.1 — Automated Testing

| # | Task | Detail |
|---|---|---|
| I-01 | Unit tests — Ballistics | Verify all 11 weapons: muzzle velocity, damage falloff, penetration values match doctrine data. |
| I-02 | Unit tests — Damage system | Verify all 6 hit zones, armor interaction, bleedout, head graze mechanic. |
| I-03 | Unit tests — Suppression | Verify all 5 suppression levels trigger correctly for each caliber class. |
| I-04 | Unit tests — AI perception | Verify sight ranges per visibility condition, hearing ranges per sound type. |
| I-05 | Unit tests — Morale | Verify all morale state transitions and surrender conditions. |
| I-06 | Unit tests — Weapon data | Automated comparison of weapon DataAsset values against doctrine source spreadsheet. |
| I-07 | Integration test — Mission phases | Automated playthrough: verify phase transitions fire, objectives activate, waves spawn. |
| I-08 | Integration test — AI pipeline | Verify full chain: Primordia directive → DecisionEngine → tactical order → squad assignment → individual behavior. |
| I-09 | Integration test — Save/Load | Save mid-mission, reload, verify state consistency. |
| I-10 | Performance regression tests | Automated benchmark: spawn 114 AI, run 60 seconds, assert >45fps on target hardware. |

### I.2 — Manual Playtesting Protocols

| # | Task | Detail |
|---|---|---|
| I-11 | Full playthrough — Normal difficulty | Complete Phase 1–4 without cheats. 10 full playthroughs minimum. |
| I-12 | Full playthrough — Hardest difficulty | Verify AI cognitive depth scaling makes a real difference. |
| I-13 | Edge case: all squad dies | Verify solo play is possible, dialogue adapts, game doesn't softlock. |
| I-14 | Edge case: player dies Phase 1 | Pre-invasion death (falling, friendly fire). Restart works. |
| I-15 | Edge case: speed run | Player rushes through phases. Verify mission script handles early triggers. |
| I-16 | Edge case: passive player | Player does nothing. AI and squad still function, mission progresses. |
| I-17 | Edge case: explore boundaries | Player walks to map edges. Soft boundaries (mines? squad radio warning?) not invisible walls. |
| I-18 | Weapon test: all 11 weapons | Fire every weapon, verify audio, VFX, damage, reload, malfunction. |
| I-19 | AI behavior observation | 10 minutes just watching AI through optics. Do they look intelligent? Do they flank? Communicate? Break? |
| I-20 | Audio immersion pass | Eyes closed. Does the battle sound real? Can you locate shooters by sound? Does suppression feel overwhelming? |

### I.3 — Investor-Specific QA

| # | Task | Detail |
|---|---|---|
| I-21 | First-time player test | Someone who has never seen the game plays the demo cold. Can they navigate menus, understand controls, survive 5 minutes? |
| I-22 | Controller test | Full demo playable on Xbox controller. Not everyone at an investor meeting has a mouse. |
| I-23 | 4K resolution test | Demo must look stunning at 4K. UI scaling, texture resolution, aliasing — all verified. |
| I-24 | Ultrawide test | 21:9 aspect ratio. No broken UI, no stretched elements. |
| I-25 | Demo stability marathon | 2-hour continuous play. No memory leaks, no crashes, no progressive frame drops. |
| I-26 | Graceful failure | If anything does go wrong, the game recovers — no unhandled exceptions, no desktop crash. Worst case: respawn at checkpoint. |
| I-27 | Demo build lockdown | Final demo build tagged, sealed, tested 10x. No last-minute changes. The build that ships to investors is the build that was tested. |

---

## THE MISSION EXPERIENCE — PHASE BY PHASE

This is what the player (and the investor) experiences. Every phase is designed. Timings are approximate — the AI and player agency create natural variation. A full playthrough runs 45–75 minutes.

### Pre-Game (2–3 minutes)

| Beat | What Happens | Systems Active |
|---|---|---|
| Menu | Main menu. Dawn over Taoyuan Beach. Ocean ambient. Title appears. | UI, Audio |
| Start | Player selects BEGIN MISSION. Difficulty selection. Loadout customization. | UI, Loadout System |
| Briefing | Loading screen becomes mission briefing: satellite imagery, force disposition, squad roster, intel summary. CO voiceover. Player reads the situation they're about to enter. | Briefing System, Audio |
| Deploy | Load complete. Fade to black. | — |

### Phase 1 — Pre-Invasion: "The Silence" (~10 minutes)

The entire purpose of Phase 1 is psychological. The player has time to explore the trench system, check their equipment, talk to squad members, look out at the ocean. The silence is the game. When the bombardment comes, it's earned.

| Beat | What Happens | The Player Feels | Systems Showcased |
|---|---|---|---|
| Arrival | Fade in. 0430. Near-total darkness. Player is in trench, looking through firing slit at the black ocean. Free to move along the defensive line. | Unease, curiosity | Lighting, Atmosphere, World Scale |
| Small talk | Squad member offers gum. Nervous small talk. Player can explore — adjacent positions have other Marines (not player squad) doing the same nervous waiting. | Connection to characters | Dialogue, Squad Personality |
| Silence | Ocean. Wind. A dog barks far away. Then stops. **Long stretches of nothing.** The player fills the silence with their own dread. | Dread building | Environmental Audio |
| FLASH traffic | Radio crackle: "Satellite confirms PLA amphibious fleet bearing 270. ETA 30 minutes." | Stakes crystallize | Radio System |
| EW escalation | GPS warning flashes on HUD, then dies. Compass drifts. Radio starts cutting out. Comms with adjacent units go silent. | Isolation | Electronic Warfare |
| Horizon | Distant flashes on the horizon. Like lightning, but too regular. Too bright. Naval gunfire. | Visual spectacle | Lighting, VFX, Draw Distance |
| Personal moment | Vasquez whispers into a personal radio: "Sofia, daddy loves you." No one comments. | Emotional gut punch | Dialogue, Character |
| Bombardment | Preparatory artillery begins hitting the defensive line. Not the player's position — yet. Adjacent positions take hits. Ground shakes. Dust cascades. Screaming on radio. | Visceral terror | Indirect Fire, Camera Shake, Audio, Destruction |
| Dawn | Dawn light begins. The ocean resolves. The player can see landing craft. Not dozens — **hundreds.** Stretching across the entire 4km beach front. The scale is incomprehensible. | "Oh my God" moment | Lighting, Draw Distance, World Scale |
| Transition | Kim: "Oh my God." Vasquez: "Weapons free. Make every round count." Phase 2 begins. | — | — |

### Phase 2 — Beach Assault: "The Overwhelm" (~25 minutes)

The longest phase. The player defends their sector (~400m) of a 4km beach. They can see other sectors being attacked simultaneously — distant gunfire, smoke, explosions — but can only affect their own fight. The AI brings wave after wave with increasing sophistication. This phase ends when the position becomes untenable.

| Beat | What Happens | The Player Feels | Systems Showcased |
|---|---|---|---|
| First contact | Wave 1: 24 PLA infantry land on player's sector. Hovercraft beach. Troops disembark under fire. Player opens fire at 400–600m. | Empowerment (brief) | Ballistics, Weapon Feel, Long-range Engagement |
| Return fire | PLA establishes fire base on beach. First suppression hits player position. Screen darkens, sound distorts. | Fear | Suppression System |
| AI tactics | AI squads use doctrine: one element suppresses from cover (craters, beach obstacles) while another bounds forward. Not scripted — Primordia deciding in real time. | "The AI is smart" | Primordia Decision Engine, Squad Coordination |
| Authenticity | Player kills enemy soldier at 300m — no hitmarker. Must look through optics to confirm the body. Other enemies keep moving. War doesn't pause for kills. | Authenticity | HUD Philosophy, Optics System |
| Stress onset | Nearby artillery impact. AUDITORY EXCLUSION: all sound drops to muffled heartbeat and ringing. Vision narrows. Player's hands shake (weapon sway increases). First taste of combat stress. | Physiological immersion | Combat Stress System |
| Escalation | Wave 2: 18 more infantry + 2 ZBD-05 amphibious vehicles. The player's weapons can't penetrate the vehicles. Must call for support or use M320/RPG. | Desperation, tactical thinking | Vehicle System, Indirect Fire |
| Isolation | Call for air support — denied. "All air assets committed to northern sectors." Call for reinforcement — denied. "No reserves available." The squad is alone. | Isolation, dread | Dialogue, Narrative, Comms |
| Flanking | PLA element attempts to flank player position via drainage channel. AI Aletheia validated the maneuver. Player must reposition or direct squad to counter. | Tactical depth | AI Validation, Squad Orders, Cover System |
| Casualties | Chen gets hit — leg wound, can still fight but reduced mobility. Williams rushes to treat under fire. Player must provide covering fire. **Personal stakes now.** | Personal investment | Squad AI, Medical System, Damage System |
| Drone warfare | Player deploys ISR drone for overwatch. Sees PLA staging for next wave. Drone signal degrades under EW. FPV strike drone available for one high-value target. | Modern warfare | Drone System, EW, ISR |
| Overwhelming force | Wave 3: 30 infantry + vehicle. Adjacent sector falls — player sees friendly position overrun 800m to the left. Surviving Marines retreat past player's position. Chaos. | Overwhelm, chaos | MassEntity, AI Scale, Environmental Storytelling |
| Morale dynamics | One PLA squad breaks under concentrated MG fire — they retreat to cover on the beach. Another squad, led by an aggressive officer, pushes through the gap. | "AI has morale!" | Morale System, Role-based AI |
| Attrition | Player weapon overheats or jams (if fired continuously). Must clear under fire. Ammo running low — HUD shows "LOW" not exact count. Must scavenge or conserve. | Tension, resource management | Weapon Heat/Jam, Logistics |
| Crisis | Wave 4: 42 infantry + 4 vehicles + PLA air strike on defensive positions. The line is untenable. Mortar fire walks onto player position — must displace or die. | Overwhelming dread | Full system stress test, Indirect Fire, Destruction |
| Retreat order | Battalion radio (barely audible through EW): "All Viper elements, fall back to Phase Line Bravo. I say again, fall back." | Loss, failure | Narrative, Mission Script |
| Fighting withdrawal | Player must leapfrog back through coastal infrastructure and agricultural buffer. Rear guard action — sprint, cover, fire, sprint. PLA pursuing. 1–2km of running combat. | Intensity, exhaustion | AI Pursuit, Fatigue System, Open Terrain Combat |
| Transition | Squad reaches urban outskirts. Enters building. Brief respite. Phase 3 begins. | Relief (temporary) | — |

### Phase 3 — Urban Fallback: "The Cost" (~15 minutes)

The terrain changes everything. 800m engagements become 30m engagements. The player moves through a city that was someone's home. Civilians are still here. The AI adapts to CQB — different tactics, different lethality, different moral weight.

| Beat | What Happens | The Player Feels | Systems Showcased |
|---|---|---|---|
| Urban entry | Commercial district. 2–3 story buildings, narrow streets, civilian vehicles. Signage in Traditional Chinese. A café with chairs still on the sidewalk. | Claustrophobia, guilt | Environment, Lighting, Urban Design |
| CQB shift | Close-quarters engagement. Corners, doorways, stairwells. Shotgun/sidearm territory. Enemy is inside buildings. Sound of boots above you. | Tactical shift | Weapon Variety, Cover, Sound Propagation |
| Civilians | Civilians sheltering in a school. Chen (Taiwanese-American) translates — they're terrified, won't leave. ROE: cannot fire freely, must identify targets. Moral complexity. | Moral weight | Narrative, Environmental Storytelling, ROE |
| Sound warfare | PLA squad breaches adjacent building. Player hears them stacking on a door through the wall. Boot scrapes, Mandarin whispers. Audio tells the story before visual. | Audio immersion | Sound Propagation, Occlusion, HRTF |
| Destruction | RPG hits exterior wall. Structural collapse — not clean, but cascading. Dust, debris, secondary cracks. Usable cover changes. AI re-evaluates routes. | Spectacle, unpredictability | Destruction System, VFX, AI Cover Re-evaluation |
| AI adaptation | Mnemonic subsystem has recorded player's tendency (camping, flanking, sniping). PLA adapts — if player camps, they envelope. If player is aggressive, they set ambushes. | "The AI learns" | Mnemonic (Learning), Tactical Analyzer |
| Weather shift | Rain starts. Visibility drops to 150m. Sound changes — rain masks footsteps. Detection ranges shrink. Everything becomes closer, more dangerous. | Atmosphere degradation | Weather System, Detection Modifiers |
| Attrition deepens | Another squad member hit. Williams overwhelmed — treating two casualties. Hard choices about who to stabilize, who to carry, who to leave. | Weight, consequence | Squad AI, Medical, Narrative |
| Sniper threat | PLA marksman in elevated position pins squad. Must locate by muzzle flash or crack-thump timing. Counter-sniper engagement at 200–400m through urban canyon. | Tactical puzzle | Ballistics, Sound Propagation, Optics |
| Collapse order | Radio (barely functional): "All positions falling back. Rally at Highway 4 interchange." Phase 4 begins. | Finality | — |

### Phase 4 — Collapse: "The End" (~10 minutes)

The mission is lost. The beach is lost. The airport is lost. The question is: who gets out. Phase 4 is a fighting retreat to an extraction point under overwhelming pressure. It's the emotional climax.

| Beat | What Happens | The Player Feels | Systems Showcased |
|---|---|---|---|
| Rally | Highway interchange. Overpass provides overhead cover. What's left of the platoon gathers — player's squad plus 8–10 other survivors. Improvised position. | Resignation, solidarity | Cover, Squad Orders, Environment |
| 360° threat | PLA approaching from multiple directions. North (beach pursuit), west (flanking force), east (urban clearance force). The AI coordinates a three-axis assault. | Hopelessness | AI Command Hierarchy, Three-axis Tactics |
| Combined arms | PLA vehicles, dismounted infantry, and mortar fire all coordinated. This is the full Primordia system at maximum: decision engine running escalation, Aletheia validating, Astraea modeling cognitive state, Simulon predicting player positions. | "This AI is real" | Full Primordia Stack |
| Last transmission | Command radio: "Extraction vehicle en route to RP Echo. ETA 3 minutes. Hold." | Last hope | Mission Script |
| Final stand | 3 minutes of the most intense combat in the mission. Everything firing. Destruction cascading. Squad members calling targets, reloading, treating wounded. Player's stress system at max — tunnel vision, auditory exclusion, shaking hands. | Peak intensity, peak immersion | All systems at maximum load |
| Extraction | Armored vehicle arrives. Squad boards under fire. Player provides covering fire from vehicle. Vehicle moves. | Relief, exhaustion | Vehicle System, Mounted Weapons |
| Pullback | Vehicle drives south. Camera stays first-person — player looks back. Taoyuan Beach is burning. The fleet is still coming. Smoke columns from the airport. It's not over. It's just beginning. | Awe + grief + scale | Environment, Lighting, Draw Distance |
| Silence | Engine noise fades. Squad is silent. Head count. Names spoken aloud — who's here, who's not. | Emotional devastation | Dialogue, Squad Personality |
| Black | Fade to black. | — | — |

### Post-Mission (2–3 minutes)

| Beat | What Happens | Purpose |
|---|---|---|
| After-Action Report | Military debrief format: time survived, shots fired, accuracy, enemies neutralized, squad casualties, phases completed, objectives met/failed. | Player sees their performance |
| Squad fate | Per-member outcome: survived, wounded, KIA — with portrait and status. Names matter. | Emotional weight |
| **"The Enemy's Perspective"** | Interactive Primordia AI replay of one key engagement. Shows the AI's decision tree — what it saw, what it considered, why it flanked left instead of right, when it decided to commit reserves. The player sees the intelligence behind the enemy. | **THE investor hook** — no other game can show this |
| Mission context | "OPERATION BREAKWATER — MISSION 1 OF 12" / "The beach fell. The airport fell. But Viper squad survived. The war for Taiwan has begun." | Sets up the full game |
| End screen | "SHATTERED HORIZON 2032" / "COMING [YEAR]" / Wishlist/follow CTA | Investment CTA |

---

## QUALITY GATES

No workstream is "done" until it passes its quality gate. Gates are sequential — earlier gates must pass before later gates are tested.

### Gate 1: IT COMPILES
- [ ] Full clean build with zero errors, zero warnings (treat warnings as errors)
- [ ] All 165 source files compile
- [ ] B1 bug fixed (DecisionEngine header)
- [ ] Editor launches, project loads

### Gate 2: IT RUNS
- [ ] Player spawns in Taoyuan Beach map (full 64 km² world loads, World Partition streaming functional)
- [ ] Player can move, look, shoot
- [ ] At least one weapon fully functional (fire, reload, ADS)
- [ ] At least one enemy AI spawns and exhibits basic behavior (move, shoot, take cover)
- [ ] HUD displays (compass, ammo, squad status)
- [ ] Stable 30+ fps on target hardware

### Gate 3: IT PLAYS
- [ ] All 4 mission phases trigger and transition
- [ ] All 4 enemy waves spawn with correct composition
- [ ] Squad AI follows player, takes cover, engages enemies
- [ ] All 11 weapons functional with correct ballistics
- [ ] Suppression, morale, and combat stress systems visibly affecting gameplay
- [ ] Death/respawn or checkpoint system working
- [ ] Stable 45+ fps

### Gate 4: IT FEELS
- [ ] Camera shake, recoil, hit feedback feel visceral
- [ ] Audio: crack-thump propagation correct
- [ ] Audio: auditory exclusion triggers under combat stress
- [ ] Suppression screen effects scale correctly across 5 levels
- [ ] Weapon feel: each weapon has distinct character
- [ ] Movement feels weighted and grounded
- [ ] Destruction looks and sounds devastating

### Gate 5: IT IMMERSES
- [ ] Phase 1 silence creates genuine tension (playtest feedback)
- [ ] Dawn lighting is beautiful and emotionally effective
- [ ] Squad dialogue feels natural, not robotic
- [ ] AI behavior is visibly intelligent (observers can identify flanking, suppression, morale breaks)
- [ ] Music score enhances without dominating
- [ ] Environmental storytelling details present (civilian objects, destruction narrative)
- [ ] Weather progression affects mood and gameplay

### Gate 6: IT SELLS
- [ ] Complete playthrough from main menu to end screen — zero crashes, all 4 phases, 45+ minutes
- [ ] First-time player can navigate without external instruction
- [ ] AAR screen with "Enemy's Perspective" AI replay functions
- [ ] 4K resolution, stable 60fps, no visual artifacts
- [ ] Controller support functional
- [ ] Replay value confirmed — second playthrough feels different (AI makes different decisions)
- [ ] 10 consecutive full playthroughs with zero critical issues
- [ ] Mission build locked, tagged, and sealed

---

## TASK SUMMARY

| Workstream | Task Count | Priority |
|---|---|---|
| A — Level & Environment | 33 | CRITICAL |
| B — Asset Deployment | 36 | CRITICAL |
| C — Integration & Wiring | 53 | CRITICAL |
| D — Feel & Polish | 36 | CRITICAL |
| E — Audio Integration | 36 | CRITICAL |
| F — UI/UX Flow | 29 | CRITICAL |
| G — Cinematics & Narrative | 23 | HIGH IMPACT |
| H — Performance & Optimization | 25 | CRITICAL |
| I — QA & Testing | 27 | CRITICAL |
| **TOTAL** | **298** | — |

### Parallel Execution Map

```
Week 1-3:   [A: 64km² level blockout + heightmap]  [B: Pipeline execution & import]  [C-01/02/03: Bug fixes & compile]
Week 2-5:   [A: High-density zone detailing]        [B: Asset organization]           [C: Player & AI wiring]
Week 4-7:   [A: Urban district build]               [B: Blueprint creation]           [C: Mission script wiring]       [D: Camera & movement]
Week 5-8:   [A: Lighting, VFX, weather]             [E: Audio integration]            [D: Weapon & impact feel]        [F: UI implementation]
Week 6-9:   [G: Voice recording & narrative]                                          [D: Stress & immersion]          [F: HUD & flow]
Week 8-11:  [H: Performance profiling & 64km² streaming optimization]                 [G: Cinematic polish]
Week 9-13:  [I: Automated tests] [I: Playtesting full 4-phase mission]                [H: AI LOD + MassEntity at scale]
Week 12-14: [I: Full QA, investor-specific testing] [Quality gates 1-6]               [Mission build lockdown]
```

### Critical Path

The **longest sequential chain** that determines minimum calendar time:

1. 64 km² level blockout + heightmap + World Partition (A-01 through A-10) — **must exist before anything else can be placed**
2. High-density zone construction: beach, trench line, urban district (A-11 through A-21c) — **the bulk of environment art**
3. Asset import (B-01 through B-36) — **parallel with level, but placement requires both**
4. C++ → Blueprint wiring (C-04 through C-53) — **requires level + assets**
5. Feel pass (D-01 through D-36) — **requires wired systems**
6. Audio integration (E-01 through E-36) — **requires wired systems + assets**
7. Performance optimization for 64 km² + 200 AI (H-01 through H-25) — **requires everything assembled, expect iteration**
8. Full 4-phase QA (I-01 through I-27) — **requires optimized build**

**Critical path length: 12–14 weeks with a focused team.** The 64 km² world and urban district construction add ~4 weeks over a smaller slice. Worth every day — the scale is part of the pitch.

---

## WHAT MAKES MISSION 1 RAISE $25–30M

The investment thesis is three-fold. Mission 1 isn't a demo or a proof of concept — it's the ship-quality first level of a 12-mission game. Investors aren't being asked to imagine what the game could be. They're playing what it is.

### 1. The AI Is Real
Every other military shooter scripts enemy behavior. Enemies in Shattered Horizon 2032 **think**. They suppress, flank, communicate, adapt, and break. The Primordia system is a genuine competitive moat — 10 dedicated subsystems, 20 source files, behavioral learning, decision validation, cognitive state modeling. The "Enemy's Perspective" replay at the end of the demo makes this visible. Investors see the AI's decision tree, its tactical reasoning, its morale collapse. No other game can show this.

### 2. The Authenticity Is Unprecedented
11 weapons with doctrine-accurate ballistic coefficients. 6 hit zones with graze mechanics. Crack-thump sound propagation. Combat stress physiology (heart rate model, tunnel vision, auditory exclusion). Electronic warfare. No hitmarkers, no kill feed, no health bar. A combat veteran can play this and not laugh. That's not a niche — that's a quality standard the mass market respects even if they play on easier difficulties.

### 3. The Emotion Is Earned
This isn't a power fantasy. It's a loss. Taoyuan Beach falls. The question is who survives. The squad has names, families, fears. Vasquez's daughter Sofia. Chen's family in Taipei. The demo makes you care, then puts that care under fire. When it ends, you're not thinking about game mechanics — you're thinking about what happens next. That's what makes someone write a check.

---

*"Every system is authentic when a combat veteran plays it, does not laugh, and does not explain what is wrong."*

*Mission 1 is that standard, rendered playable, in 45–75 minutes that redefine what a military game can be. Eleven more missions follow. This is the beginning.*

---

**Document ends. 298+ tasks across 9 workstreams. 6 quality gates. One mission that launches a franchise.**
