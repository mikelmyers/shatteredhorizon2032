# SHATTERED HORIZON 2032 — Game Design Document: Warfare Systems

**Primordia Games — GDD v1.0**

---

## 1. Design Philosophy

The warfare systems in Shattered Horizon 2032 simulate the actual nature of armed conflict, not its aesthetics. Every system enforces one principle: **the world exists whether the player is watching or not**. The player is a participant in an ongoing conflict, not the protagonist. The enemy exists to accomplish its own mission.

The quality bar: *"Every system is authentic when a combat veteran plays it, does not laugh, and does not explain what is wrong."*

---

## 2. Battlefield Scale & Architecture

### 2.1 World Scale

| Parameter | Specification |
|---|---|
| Minimum map size | 64 km² (8 km × 8 km) |
| Heightmap resolution | 1 m |
| Engagement distances | 5 m – 1,500 m |
| Draw distance | 3,000 m minimum |
| Pop-in at tactical distances | Zero tolerance |

The battlefield is mostly empty space. Movement between engagements takes real time. Engagements happen at distances where enemies are shapes, not visible character models (300 m – 800 m typical).

### 2.2 World State Persistence

- Cleared positions stay cleared
- Destroyed structures remain destroyed
- Bodies remain on the battlefield
- Terrain disturbance (tracks, craters, debris) persists
- Fire spreads realistically through flammable materials
- Enemy reinforcement is physical — units travel from origin points, not spawned

---

## 3. Information Warfare

### 3.1 The Information Vacuum

Real warfare operates in what Clausewitz called *"a peculiar twilight that gives things exaggerated dimensions and unnatural appearance."* The game enforces a true information vacuum:

- **No kill confirmation** unless the player visually observes the body
- **No enemy position data** unless earned through active reconnaissance
- **No automatic friendly position tracking** — friendly locations are unknown unless communicated
- **Stale intelligence** — all information degrades over time
- **Degraded radio communications** — comms are unreliable, can be jammed or intercepted

### 3.2 Fog of War — Four Doctrinal Levels

| Level | Description |
|---|---|
| **Tactical** | No enemy markers. All detection is physics-based (visual, thermal, acoustic, electronic). |
| **Operational** | Map intelligence decays over time. Stale data may be wrong. |
| **Friendly Force** | AI squads operate independently. They do not wait for or orbit the player. |
| **Self** | No magical ammo counter. No health bar. Injury is assessed by symptoms. Ammunition is estimated by weight and feel. |

### 3.3 Electronic Warfare & Signature Management

- Radio emissions can be detected and geolocated
- GPS can be jammed or spoofed
- Thermal, acoustic, and visual signatures are independently detectable
- Drone surveillance is assumed and must be countered
- All electronic emissions create a detectable footprint

---

## 4. Combat Stress & Player Physiology

Combat stress produces measurable physiological effects. Heart rate escalates from ~72 bpm at rest to 160+ bpm under fire. The game models:

| Effect | Implementation |
|---|---|
| **Tunnel vision** | Progressive vignetting under stress |
| **Fine motor degradation** | Increased aim sway, slower reload timing |
| **Auditory exclusion** | Selective audio dropout at peak stress |
| **Time distortion** | Subtle time scaling during high-intensity moments |
| **Cognitive narrowing** | Reduced interface usability under stress |
| **Combat Stress Reaction** | Rare player state — temporary combat ineffectiveness from sustained exposure |

### 4.1 Fatigue & Exhaustion

- Sustained exertion degrades performance progressively
- Wounds accumulate and compound degradation
- Recovery requires time and safety — no instant healing

---

## 5. Ballistics & Physics

### 5.1 Projectile Flight Model

Full physics simulation for every round fired:

- Muzzle velocity and drag coefficient
- Ballistic coefficient and gravity drop
- Wind drift
- Spin stabilization (gyroscopic drift)
- Transonic instability at range

### 5.2 Caliber Reference Data

| Caliber | Weight | Muzzle Velocity | Effective Range |
|---|---|---|---|
| 5.56×45mm NATO | 62 gr | 940 m/s | ~500 m |
| 7.62×51mm NATO | 147 gr | 840 m/s | ~800 m |
| 7.62×39mm | 123 gr | 715 m/s | ~400 m |
| 9×19mm Parabellum | 115 gr | 370 m/s | ~50 m |
| .338 Lapua Magnum | 250 gr | 915 m/s | ~1,500 m |
| 12.7×99mm (.50 BMG) | 660 gr | 930 m/s | ~1,800 m |

### 5.3 Terminal Ballistics

- **5.56mm**: Fragments above 750 m/s — catastrophic soft tissue damage at close range, reduced effect at distance
- **7.62mm**: Maintains path with deeper penetration, more consistent wound channel
- **Pistol calibers**: Rely on shot placement, limited tissue disruption

### 5.4 Wound Location Behavior

| Location | Effect |
|---|---|
| Upper chest / heart | Any rifle → rapid collapse |
| Lung | Any rifle → stumble, slow collapse (3–8 sec) |
| Spine (upper) | Any rifle → instant motor loss below impact |
| Spine (lower) | Any rifle → leg loss, torso functional |
| Head (brain) | Any caliber → instantaneous ragdoll, no animation blend |
| Head (skull graze) | Any caliber → stagger, 3–5 sec recovery |
| Femoral artery | Any caliber → exsanguination in 2–3 minutes |
| Abdomen | Any rifle → severe pain, hunched movement |
| Arm (bone hit) | Any rifle → weapon drop, one-handed engagement only |
| Leg (bone hit) | Any rifle → immediate fall, crawl only |

### 5.5 Armor Interaction

| Armor Level | Protection |
|---|---|
| Level IIIA | Stops pistol and shotgun rounds |
| Level III | Stops rifle rounds at distance |
| Level IV | Stops armor-piercing rounds |

Blunt trauma behind stopped rounds causes stagger and temporary combat degradation. Armor does not make the wearer invulnerable — energy transfer is modeled.

### 5.6 Penetration & Cover Destruction

| Material | Behavior |
|---|---|
| Drywall (12 mm) | Full pass-through, all calibers |
| Plywood (19 mm) | Full pass-through, all calibers |
| Soft wood (100 mm) | 5.56 stops at ~50 mm; 7.62 penetrates |
| Cinder block (200 mm) | Stops both rifle calibers |
| Sandbags (300 mm) | Stops both rifle calibers |
| Vehicle door (1.2 mm steel) | Penetrated by rifle (degraded) |
| Engine block (150 mm cast iron) | Stops all small arms |
| Glass (6 mm) | Angle-dependent deflection or pass-through |

Cover is destructible. Sustained fire degrades cover integrity. Players and AI must reassess cover viability during engagements.

---

## 6. Indirect Fire Systems

### 6.1 Specification

- **High-angle trajectory** with full physics simulation
- **Area effect saturation** — not point-target weapons
- **Observer-to-fire coordination** required (call-for-fire procedure)
- **Time of flight**: 8–12 seconds depending on range and ordnance
- **Sound signatures**: Outgoing and incoming rounds are audible with directional cues
- **Counter-battery**: Enemy can detect firing positions and return fire
- **Danger close**: Authorization required for fire missions near friendly positions

### 6.2 Explosion & Blast Physics

| Zone | Effect |
|---|---|
| 0–2 m | Lethal to unarmored personnel |
| 2–5 m | Severe injury, knockdown |
| 5–15 m | Fragmentation risk |
| 15 m+ | Concussion effect, AI morale degradation |

- Overpressure wave is the primary kill mechanism, not fragmentation alone
- Explosions near structures generate rubble and change cover geometry
- Blast within 10 m degrades AI decision quality for 3–5 seconds

---

## 7. Sound Design & Audio Physics

- **Supersonic crack**: Sonic boom arrives before muzzle report — time delay encodes shooter distance
- **Subsonic rounds**: Eliminate the crack signature
- **Suppressors**: Reduce 30–35 dB but remain audible at 50 m
- **Directional audio**: Enables acoustic triangulation of shooter position
- **Environment-accurate reverb**: Indoor vs. outdoor propagation modeled separately
- Sound is a primary detection vector — both for and against the player

---

## 8. AI Warfare Doctrine

### 8.1 Three-Tier Command Structure

| Tier | Role | Scope |
|---|---|---|
| **Strategic Commander** (Thinker) | Campaign-level decisions, resource allocation | Theater-wide |
| **Operational Commander** | Sector-level planning, force coordination | Sector |
| **Tactical Unit** | Individual engagement execution | Local |

Orders flow downward but **execution is autonomous**. Tactical units make their own decisions within the commander's intent. If communications are severed, units continue operating on last known orders.

### 8.2 AI Behavioral Capabilities

- **Dynamic movement & cover selection** based on live threat vectors
- **Squad coordination** with explicit roles (point, flanking, suppression, overwatch)
- **Session memory** — AI tracks player behavior and recognizes patterns mid-session
- **Threat assessment** — rational prioritization of targets and threats
- **Retreat logic** — intelligent withdrawal, reorganization, and counterattack
- **Morale system** — suppression degrades decision quality; sustained pressure causes breaks
- **Unpredictability** — variance in decision cycles prevents player exploitation

### 8.3 The Living Battlefield

The enemy has a mission independent of the player:

- Enemy units operate on their own timeline
- They accomplish objectives whether the player engages or not
- Internal communication drives coordination
- Commanders make operational decisions based on their own intelligence
- The enemy **learns at the campaign level** — tactics that work once may be countered later

### 8.4 Primordia AI Integration

| Subsystem | Function |
|---|---|
| **Thinker** | Commander & squad leader AI, operational decisions |
| **Mnemonic** | Session memory, player pattern recognition |
| **Echo** | AI communication, squad callouts and coordination |
| **Aletheia** | Decision validation, prevents obviously wrong AI actions |
| **Simulon** | Threat modeling, predictive player movement |
| **Astraea** | Morale and cognitive state, suppression tracking |

Performance budget: < 2 ms per frame overhead for all AI subsystems combined.

### 8.5 Difficulty as Cognitive Depth

| Difficulty | AI Configuration |
|---|---|
| **Recruit** | Thinker & Mnemonic disabled; reactive only |
| **Regular** | Thinker limited; short-term memory; basic coordination |
| **Hardened** | Thinker active; full session memory; basic commander AI |
| **Veteran** | Full stack; predictive movement; full commander AI |
| **Primordia Unleashed** | All subsystems at maximum cognitive depth |

Difficulty scales AI intelligence, not aim accuracy or health pools.

---

## 9. Detection Systems

All detection is physics-based. No gamified "awareness meters."

| Detection Type | Range | Notes |
|---|---|---|
| Visual | 5 m (concealed) – 800 m+ (exposed) | Affected by lighting, weather, camouflage |
| Thermal | 500 m+ | Affected by ambient temperature, concealment |
| Acoustic | 5 m – 300 m | Weapon fire, movement, voice |
| Electronic | 500 m – km+ | Radio emissions, active electronics |

Both player and AI are subject to identical detection rules.

---

## 10. Logistics & Sustainment

- **Ammunition** is finite and tracked per round, not per magazine
- **Resupply** requires physical access to supply points — no magic refills
- **Equipment degrades** with use and environmental exposure
- **Enemy supply lines** are legitimate targets — disrupting logistics weakens the enemy
- **Medical supplies** are limited; triage decisions matter
- **Weapons can jam or malfunction** based on maintenance state and conditions

---

## 11. Night, Weather & Environmental Warfare

Weather and time of day are tactical conditions, not cosmetic effects.

| Condition | Tactical Impact |
|---|---|
| **Night** | Mutual blindness without NVGs; NVGs limit FOV and depth perception |
| **Rain** | Masks sound signatures; reduces visual detection range |
| **Fog** | Reduces visibility equally for all combatants |
| **Extreme cold** | Fine motor degradation; equipment reliability reduced |
| **Wind** | Affects ballistic trajectory; masks or carries sound |

---

## 12. HUD Philosophy

Every HUD element must pass one test: **would this soldier actually have this information?**

- No enemy position markers unless confirmed by observation or intelligence
- No kill confirmation beyond what the player can see
- No health bar — injury is communicated through visual and audio symptoms
- No magical ammo counter — the player estimates remaining rounds
- No minimap with omniscient awareness
- No objective markers that trivialize navigation

---

## 13. Player Agency & Time Horizons

The player is one person in a larger conflict. A platoon is genuinely dangerous. Volume of fire forces suppression. Flanking is decisive.

### Decision Scales

| Time Horizon | Decisions |
|---|---|
| **Seconds** | Fire, move, take cover, reload |
| **Minutes** | Engage or bypass, flank or suppress, advance or withdraw |
| **Hours** | Choose area of operations, plan approach routes, manage resources |
| **Days** | Shift operational focus, respond to evolving campaign state |

The player chooses: where to go, what to engage, when to fight, how long to stay, and what to achieve. The world does not wait.

---

## 14. Body & Death Physics

- At death, all animation transfers instantly to physics ragdoll — no blend time
- Momentum from the killing force carries through the body
- Mass distribution affects fall behavior realistically
- **Zero tolerance** for: floating bodies, T-poses, bodies standing against walls, or animation glitches
- Contextual death responses based on wound location and caliber

---

## 15. Module Boundaries

Clear separation of responsibility between warfare subsystems:

| Module | Responsibility |
|---|---|
| Primordia AI Layer | Cognitive subsystems (Thinker, Mnemonic, Echo, Aletheia, Simulon, Astraea) |
| Combat Systems | Ballistics, damage model, suppression mechanics |
| World / Environment | Terrain, cover, destruction, weather |
| Animation / Ragdoll | Character animation, physics-driven death |
| Audio | Sound emission, propagation, spatial processing |
| HUD / UI | Player information display (doctrinal constraints) |
| Progression / Meta | Unlocks, session data, campaign state |

---

## 16. Implementation Priorities

### Critical (Must Ship)

1. True player physiological degradation (stress, fatigue, injury)
2. Full indirect fire physics
3. Operational-level AI with campaign memory
4. True information vacuum (no gamified HUD assists)

### High Priority

5. Wound persistence and medical doctrine
6. Electronic warfare as a primary gameplay system
7. Logistics as a binding constraint on operations

### Medium Priority

8. True-scale engagement distances (300 m – 800 m typical)
9. Indirect evidence and tracking systems (footprints, shell casings, disturbed terrain)

---

*"Every system is finished when a sophisticated player cannot tell the difference between this game and physical reality — and when that player cannot predict what the enemy will do next."*
