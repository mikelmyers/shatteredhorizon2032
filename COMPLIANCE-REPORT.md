# SHATTERED HORIZON 2032 — Doctrine Compliance Report

**Audit Date:** 2026-03-25
**Sources:** SH2032 Warfare Doctrine v2, SH2032 Build Philosophy, Full Source Audit
**Purpose:** Identify what exists, what's missing, what contradicts doctrine, and what must be fixed before Round 2.

---

## EXECUTIVE SUMMARY

The codebase is a serious military simulation with strong fundamentals. Combat systems (ballistics, suppression, damage, cover) are production-quality. AI has solid individual and squad behavior. The Primordia integration architecture is in place but incompletely wired. The critical gaps are in **higher-order AI cognition** (campaign memory, commander-level learning, the six named Primordia subsystems), **player physiology** (combat stress effects), and **three missing calibers** that block sniper and insurgent weapon gameplay.

**Verdict:** ~72% doctrine-compliant. Round 2 is blocked by 7 critical items.

---

## 1. WHAT EXISTS AND IS DOCTRINE-COMPLIANT

### 1.1 Ballistics & Terminal Effects — STRONG
| Doctrine Requirement | Status | Notes |
|---|---|---|
| Full projectile flight physics (drag, gravity, wind) | **COMPLIANT** | Velocity Verlet integration, transonic drag rise, sub-stepping |
| 5.56×45mm NATO (940 m/s, 500m) | **COMPLIANT** | M27 IAR: 940 m/s, 550m; M4A1: 910 m/s, 500m |
| 9×19mm Parabellum (370 m/s, 50m) | **COMPLIANT** | M17 SIG: 375 m/s, 50m |
| Wound location behavior (10 zones) | **COMPLIANT** | 6 hit zones with multipliers, bleedout, limb penalties |
| Armor interaction (IIIA/III/IV) | **COMPLIANT** | Per-zone armor, integrity degradation, spall mechanics |
| Penetration & cover destruction | **COMPLIANT** | Material-specific resistance, velocity-scaled penetration |
| Ricochet system | **COMPLIANT** | Angle-dependent, material-specific, max 2 bounces |
| Fragmentation | **COMPLIANT** | 30 fragments, inner/outer radius, per-fragment damage |

### 1.2 Suppression — STRONG
| Doctrine Requirement | Status | Notes |
|---|---|---|
| Caliber-based suppression impulse | **COMPLIANT** | 7 caliber classes, pistol through autocannon |
| Proximity-based (not hit-based) | **COMPLIANT** | 500cm radius, distance-scaled |
| Cascading effects (aim, movement, ADS) | **COMPLIANT** | 5 levels: None through Pinned |
| Visual/audio feedback | **COMPLIANT** | Desaturation, vignette, tinnitus, camera shake |
| AI respects suppression | **COMPLIANT** | Pinned AI won't advance, morale degrades |

### 1.3 HUD Philosophy — STRONG
| Doctrine Requirement | Status | Notes |
|---|---|---|
| No enemy position markers | **COMPLIANT** | No floating markers, no enemy HUD elements |
| No kill confirmation | **COMPLIANT** | No kill feed, no hit markers by default |
| No health bar | **COMPLIANT** | Injury communicated via limb damage panel with symptoms |
| No minimap | **COMPLIANT** | Compass widget only |
| Ammo tracked per-round | **COMPLIANT** | CurrentMagAmmo + ReserveAmmo, per-round consumption |
| Immersive mode | **COMPLIANT** | Full HUD disable toggle available |

### 1.4 Detection Systems — STRONG
| Doctrine Requirement | Status | Notes |
|---|---|---|
| Physics-based visual detection | **COMPLIANT** | LOS traces, distance curves, posture multipliers |
| Acoustic detection | **COMPLIANT** | 12 sound types, range by type, weather-affected |
| Detection accumulation (not instant) | **COMPLIANT** | 0–1 accumulator, awareness state gates |
| Weather affects detection | **COMPLIANT** | Rain 0.6x hearing, fog reduces sight, etc. |
| Awareness states | **COMPLIANT** | Unaware → Suspicious → Alert → Combat → Searching |

### 1.5 World Persistence & Destruction — STRONG
| Doctrine Requirement | Status | Notes |
|---|---|---|
| Destroyed structures remain | **COMPLIANT** | 5-stage destruction, structural collapse cascading |
| Craters persist | **COMPLIANT** | Generated at explosion sites, provide cover |
| Debris persists | **COMPLIANT** | Max 500 active, physics settle, performance-managed |
| Fire spread | **COMPLIANT** | Vegetation fire spread mechanics implemented |

### 1.6 AI Individual & Squad Behavior — STRONG
| Doctrine Requirement | Status | Notes |
|---|---|---|
| Cover-based tactics | **COMPLIANT** | EQS-based cover search, quality scoring, lean system |
| Fire and maneuver | **COMPLIANT** | Bounding advance, flanking, suppressing behaviors |
| Squad coordination | **COMPLIANT** | Target sharing, formation system, buddy teams |
| Morale system | **COMPLIANT** | 6 states, cascade penalties, surrender mechanics |
| Role-based behavior | **COMPLIANT** | 7 enemy roles, 5 squad roles with distinct loadouts |
| Voice callouts | **COMPLIANT** | Contextual callouts, stress variants, cooldown system |

### 1.7 Electronic Warfare — STRONG
| Doctrine Requirement | Status | Notes |
|---|---|---|
| GPS jamming/spoofing | **COMPLIANT** | GPS denial zones, compass drift computed |
| Comms jamming | **COMPLIANT** | Radio static intensity, message disruption |
| Drone jamming | **COMPLIANT** | Forces autonomous behavior or RTH |
| Escalating EW pressure | **COMPLIANT** | 5-level escalation over mission time |

### 1.8 Drone Systems — STRONG
| Doctrine Requirement | Status | Notes |
|---|---|---|
| ISR drone with detection cone | **COMPLIANT** | 30° half-angle, thermal/daylight/NVG modes |
| Battery management | **COMPLIANT** | 20min flight time, drain multipliers, auto-RTH |
| Signal link degradation | **COMPLIANT** | Range-based quality, autonomous on loss |
| Counter-drone system | **COMPLIANT** | Dedicated system exists |
| Detection relay to AI | **COMPLIANT** | 2s delay, confidence threshold, awareness boost |

### 1.9 Logistics — COMPLIANT
| Doctrine Requirement | Status | Notes |
|---|---|---|
| Finite ammunition per-round | **COMPLIANT** | Magazine + reserve tracking |
| Equipment weight affects movement | **COMPLIANT** | 45kg max carry, stamina penalty above 15kg |
| Weapon malfunction/jam | **COMPLIANT** | Heat-based, dirt-based, per-shot probability |
| Medical treatment required | **COMPLIANT** | Limb treatment, bleeding stops only when treated |

### 1.10 Night & Weather — COMPLIANT
| Doctrine Requirement | Status | Notes |
|---|---|---|
| Weather affects combat | **COMPLIANT** | 6 weather types, visibility/sound/terrain effects |
| Night reduces visibility | **COMPLIANT** | Night: 300cm max without NVG, 5000cm with NVG |
| Rain masks sound | **COMPLIANT** | 0.6x hearing multiplier |

---

## 2. WHAT'S MISSING

### 2.1 CRITICAL — Blocks Round 2

| # | Missing System | Doctrine Source | Impact |
|---|---|---|---|
| **C1** | **.338 Lapua Magnum** (250gr, 915 m/s, 1500m) | Build Philosophy §III.A | No sniper rifle beyond 800m. Blocks long-range gameplay. |
| **C2** | **12.7×99mm .50 BMG** (660gr, 930 m/s, 1800m) | Build Philosophy §III.A | No heavy MG or anti-materiel rifle. |
| **C3** | **7.62×39mm** (123gr, 715 m/s, 400m) | Build Philosophy §III.A | No enemy AK-platform weapons. PLA loadout incomplete. |
| **C4** | **Combat Stress Physiology** — tunnel vision (vignetting), fine motor degradation, auditory exclusion, time distortion, cognitive narrowing, Combat Stress Reaction | Warfare Doctrine §II.B | Doctrine calls this a first-ever simulation feature. Only suppression visual effects exist. No heart-rate model, no stress accumulator independent of suppression. |
| **C5** | **Primordia Named Subsystems** — Thinker, Mnemonic, Echo, Aletheia, Simulon, Astraea are not implemented as discrete subsystems | Build Philosophy §IV | Architecture has PrimordiaClient + DecisionEngine + TacticalAnalyzer. None map 1:1 to the six named subsystems. No subsystem event instrumentation. |
| **C6** | **Campaign-Level AI Learning** — "The enemy learns at the campaign level" | Warfare Doctrine §II.F | TacticalAnalyzer detects tactics per-session but no persistent cross-session learning. Tactic counters are suggested but not executed. |
| **C7** | **Indirect Fire System** — observer-to-fire coordination, time of flight (8–12s), counter-battery, danger close authorization | Warfare Doctrine §II.E | M320 grenade launcher exists (direct fire). No mortar/artillery call-for-fire system, no indirect fire physics (high-angle trajectory), no counter-battery mechanics. |

### 2.2 HIGH PRIORITY — Should ship

| # | Missing System | Doctrine Source | Notes |
|---|---|---|---|
| **H1** | **AI Instrumentation Overlay** — shows which subsystem fired and why | Build Philosophy §VIII | No instrumentation emitting structured events. No debug overlay. |
| **H2** | **AI Replay System** — watch what AI was thinking post-match | Build Philosophy §VIII | Not implemented. |
| **H3** | **Decision Engine Escalation** — reserve commitment, stall detection, morale retreat | Build Philosophy §IV | `TickEscalation()`, `TickMoraleAndRetreat()`, `GenerateLocalFallbackOrders()` are stubbed. |
| **H4** | **Reinforcement Wave Integration** | Build Philosophy §IV | `OnReinforcementWaveSpawned()` is stubbed. New squads can't be assigned to pending orders. |
| **H5** | **Sound Physics — Doppler/Boom Propagation** | Warfare Doctrine §II, Build Philosophy §III.G | Supersonic crack exists but is instant. Doctrine requires time-delayed crack-then-thump encoding shooter distance. |
| **H6** | **Three-Tier Command Structure** — Strategic Commander, Operational Commander, Tactical Unit | Warfare Doctrine §III.C | DecisionEngine acts as single-tier order translator. No explicit strategic vs. operational vs. tactical command layers. |
| **H7** | **Difficulty as Cognitive Depth** — 5 difficulty levels (Recruit through Primordia Unleashed) | Build Philosophy §IV | Framework references difficulty but no implementation that scales AI subsystem engagement per level. |
| **H8** | **Enemy Supply Lines as Targets** | Warfare Doctrine §II.G | Logistics constraint exists for player. No enemy logistics system to interdict. |

### 2.3 MEDIUM PRIORITY — Polish

| # | Missing System | Doctrine Source |
|---|---|---|
| **M1** | Spin drift / gyroscopic stabilization | Build Philosophy §III.A |
| **M2** | Indirect evidence & tracking (footprints, shell casings, disturbed terrain) | Warfare Doctrine §IV |
| **M3** | Terrain trace visibility is defined but rendering not confirmed | Warfare Doctrine §III.B |
| **M4** | AI communication layer — legible enemy coordination visible to player | Build Philosophy §VIII |
| **M5** | Stale intelligence decay on operational map | Warfare Doctrine §II.C |
| **M6** | Self fog-of-war — ammo estimated by weight/feel, not exact count | Warfare Doctrine §II.C |

---

## 3. WHAT CONTRADICTS DOCTRINE

| # | Contradiction | Expected | Actual | Severity |
|---|---|---|---|---|
| **X1** | **M110 SASS muzzle velocity** | 840 m/s (doctrine) | 783 m/s (code) | **HIGH** — 57 m/s error changes effective range and ballistic drop. Fix in `SHWeaponData.cpp`. |
| **X2** | **Ammo counter is exact** | "No magical ammo counter" — player estimates by weight/feel | HUD displays exact `CurrentMag / Capacity / Reserve` | **MEDIUM** — Contradicts Self fog-of-war doctrine. At minimum, reserve count should be approximate. |
| **X3** | **Hit markers available** | "No kill confirmation unless you see the body" | Optional hit markers exist (off by default in milsim mode) | **LOW** — Off by default is acceptable, but option's existence contradicts spirit. Consider removing entirely or limiting to arcade mode. |
| **X4** | **Enemy headshots always lethal** | Doctrine specifies "skull graze → stagger, 3–5s recovery" | Code: `bIsHeadshot && DamageType == Ballistic → always lethal` | **MEDIUM** — No graze mechanic. All headshots are instant kills regardless of angle or caliber. |
| **X5** | **Sight ranges too short for AI** | Doctrine: visual detection 5m–800m+ | AI ClearDay max sight: 15,000cm (150m), instant at 2,000cm (20m) | **HIGH** — Doctrine says engagements at 300–800m. AI can't see past 150m. This fundamentally breaks engagement distance doctrine. |
| **X6** | **Morale state names inconsistent** | Build Philosophy: Suppression degrades decision quality | SHMoraleSystem uses Determined/Steady/Shaken/Breaking/Routed. SHEnemyCharacter uses Confident/Steady/Shaken/Breaking/Broken/Surrendered. | **LOW** — Two parallel morale enums. Should consolidate. |

---

## 4. ROUND 2 BLOCKERS — PRIORITIZED FIX LIST

### Must Fix (Blocks Gameplay)

1. **Fix AI sight range** (X5) — Increase ClearDay MaxRange from 15,000cm to 80,000cm+ (800m). Instant detection at 15,000cm (150m). This is the single most impactful fix. Without it, every firefight is close-quarters regardless of terrain.

2. **Fix M110 muzzle velocity** (X1) — Change from 783 m/s to 840 m/s in `SHWeaponData.cpp`.

3. **Add missing calibers** (C1–C3) — Add .338 Lapua, .50 BMG, 7.62×39mm weapon data entries with corresponding weapons (e.g., M2 Browning, sniper rifle, AK-variant for PLA enemies).

4. **Implement combat stress physiology** (C4) — Create `SHCombatStressSystem` component: heart rate model driving tunnel vision vignette, fine motor degradation (aim sway multiplier), auditory exclusion (audio ducking), time distortion (subtle time dilation), cognitive narrowing (UI responsiveness). Distinct from suppression — stress accumulates from sustained combat exposure, proximity to explosions, witnessing casualties.

5. **Implement indirect fire** (C7) — Call-for-fire system: observer designates target → fire mission request → time-of-flight delay (8–12s) → impact with area saturation. Counter-battery: AI detects firing position and returns fire. Danger close: authorization requirement near friendlies.

6. **Implement head graze mechanic** (X4) — Angle-of-incidence check on headshots. Grazing hits (>60° from surface normal) → stagger + 3–5s recovery instead of instant kill.

7. **Wire Primordia Decision Engine stubs** (H3) — Complete `TickEscalation()` (commit reserves when orders stall >30s), `TickMoraleAndRetreat()` (pull back squads below morale threshold), `GenerateLocalFallbackOrders()` (hold/advance when Primordia disconnected), `OnReinforcementWaveSpawned()` (assign new squads to pending orders).

### Should Fix (Breaks Immersion)

8. **Implement crack-thump sound delay** (H5) — Supersonic crack arrives first, muzzle report arrives `distance / 343` seconds later. Currently both are instantaneous.

9. **Approximate ammo display** (X2) — Replace exact reserve count with estimated ranges (e.g., "Full", "Half", "Low", "Last Mag") or remove reserve count from HUD entirely.

10. **Implement three-tier command hierarchy** (H6) — Strategic Commander (campaign objectives, force allocation), Operational Commander (sector-level orders, multi-squad coordination), Tactical Unit (execution). Currently DecisionEngine is flat.

11. **Add AI instrumentation** (H1) — Each decision cycle emits: subsystem name, input state, output decision, confidence score, timestamp. Performance budget: <2ms/frame.

12. **Add difficulty scaling** (H7) — Wire difficulty enum to AI perception config, decision engine depth, and Primordia subsystem engagement per Build Philosophy §IV specification.

---

## 5. SYSTEMS HEALTH SCORECARD

| System | Completeness | Doctrine Compliance | Round 2 Ready? |
|---|---|---|---|
| Ballistics & Physics | 85% | 90% (missing calibers, M110 velocity) | **NO** — missing calibers |
| Damage & Wounds | 95% | 85% (no head graze) | **YES** with graze fix |
| Suppression | 100% | 100% | **YES** |
| Cover & Destruction | 95% | 95% | **YES** |
| HUD Philosophy | 90% | 80% (exact ammo counter) | **YES** (minor fix) |
| Detection & Perception | 90% | 60% (sight range too short) | **NO** — range fix critical |
| Player Physiology | 40% | 30% (fatigue only, no stress) | **NO** — combat stress missing |
| Individual Enemy AI | 90% | 85% | **YES** |
| Squad AI (Player) | 95% | 90% | **YES** |
| Primordia Integration | 60% | 40% (stubs, no named subsystems) | **NO** — stubs must complete |
| Indirect Fire | 10% | 10% (only M320 direct fire) | **NO** — system missing |
| Electronic Warfare | 90% | 90% | **YES** |
| Drones | 90% | 90% | **YES** |
| Logistics | 75% | 70% (player only, no enemy) | **YES** (acceptable for Round 2) |
| Sound Physics | 70% | 60% (no crack-thump delay) | **YES** (degraded) |
| Weather & Night | 90% | 90% | **YES** |
| Campaign AI Learning | 0% | 0% | **NO** — not started |
| AI Instrumentation | 0% | 0% | No (polish) |

**Overall: 72% doctrine-compliant. 7 critical blockers for Round 2.**

---

*"Every system is authentic when a combat veteran plays it, does not laugh, and does not explain what is wrong."*
*— Current status: veteran would notice AI engaging at 150m max, missing artillery, and the absence of combat stress. Fix these and the foundation is solid.*
