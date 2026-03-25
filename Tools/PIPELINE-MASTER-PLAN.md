# Shattered Horizon 2032 — AI Asset Pipeline Master Plan
## Primordia Games — Automated Production Pipelines

**Goal:** Minimize human-in-the-loop for all game asset creation.
**Total Assets Needed:** ~1,500+ files across 9 pipelines
**Assets Referenced in Code:** 238+ discrete slots currently unfilled

---

## Pipeline Overview

| # | Pipeline | Assets | Automatable | Priority | Status |
|---|----------|--------|-------------|----------|--------|
| 1 | Terrain Textures (PBR) | 375 textures | 95% | DONE | Built |
| 2 | Audio — Sound Effects | 180+ files | 85% | Critical | Not Built |
| 3 | Audio — Music/Soundtrack | 20-30 tracks | 90% | Critical | Not Built |
| 4 | Audio — Voice Lines | 260+ lines | 90% | Critical | Not Built |
| 5 | UI/HUD Art | 50+ textures/icons | 80% | High | Not Built |
| 6 | VFX/Particle Configs | 65+ systems | 70% | High | Not Built |
| 7 | 3D Models | 30+ meshes | 50% | High | Not Built |
| 8 | Weapon Data Assets | 3 weapons | 100% (code) | Critical | Not Built |
| 9 | Animations | 55+ montages | 40% | Medium | Not Built |

---

## Pipeline 1: Terrain Textures (PBR) — BUILT

**Location:** `Tools/ArtPipeline/`
**What it does:** Sources reference photos via APIs → AI generates textures → processes into PBR sets (albedo, normal, roughness, AO, height) → exports UE5-ready with material configs.

**Output:** 15 terrain types × 5 destruction stages × 5 PBR maps = 375 textures

**How to run:**
```bash
cd Tools/ArtPipeline
pip install -r requirements.txt

# Set API keys (get free keys at each site)
export UNSPLASH_ACCESS_KEY="your_key"      # unsplash.com/developers
export PEXELS_API_KEY="your_key"           # pexels.com/api
export STABILITY_API_KEY="your_key"        # stability.ai (for AI generation)

# Option A: Full automated pipeline (source + AI + process + export)
python generate.py --full

# Option B: Process your own photos (no API keys needed)
# Place .jpg/.png files in reference_photos/ directory
python generate.py --process-local

# Option C: Process a single photo
python generate.py --photo path/to/photo.jpg --name concrete_wall

# Option D: Only source reference photos (no AI generation)
python generate.py --source-only

# Option E: Skip API sourcing, just AI generate + process
python generate.py --full --skip-sourcing

# Option F: Single terrain type only
python generate.py --terrain concrete
```

**Output location:** `Tools/ArtPipeline/output/ue5_export/`
**Import into UE5:** Run `output/ue5_export/ue5_batch_import.py` in UE5 Editor Python console.

---

## Pipeline 2: Audio — Sound Effects

**What it produces:** All non-voice, non-music audio assets.

### Assets (180+ files):

**Weapon Audio (66 files):**
Per weapon (11 weapons × 6 sounds each):
- `FireSound` — unsuppressed gunshot
- `FireSoundSuppressed` — suppressed gunshot
- `DryFireSound` — empty chamber click
- `ReloadSound` — magazine insertion + chamber
- `FireModeSwitchSound` — fire selector click
- `MalfunctionSound` — weapon jam sound

**Projectile Audio (4 base × variants = ~20 files):**
- `SupersonicCrackSound` — per caliber (5.56, 7.62, 9mm, .338, .50 BMG)
- `RicochetSound` — per surface type (concrete, metal, rock)
- `ImpactSound` — per surface type (8 types: concrete, metal, wood, flesh, water, glass, dirt, vegetation)
- `ExplosionSound` — grenades, mortars, vehicle

**Terrain Footsteps (60 files):**
15 terrain types × 4 movement types (walk, run, crawl, land):
- wet_sand, dry_sand, mud, grass, dirt, concrete, asphalt, gravel, rock,
  shallow_water, deep_water, rubble, metal, wood, vegetation

**Destruction Audio (13 files):**
One per destructible type: wall, roof, floor, column, window, door, sandbag,
fortification, vehicle_wreck, tree, furniture, fence, generic

**Drone Audio (7 files):**
- Motor loop, dive whine, explosion, ISR buzz, detection beep, close warning, jamming active

**EW/Comms Audio (4 files):**
- Radio static, radio crackle, comms restored, comms jammed

**UI/Misc Audio (10+ files):**
- Menu clicks, compass ticks, squad command wheel selection, heartbeat (stress),
  tinnitus (post-explosion), breathing (exertion)

### Approach:
- **ElevenLabs Sound Effects API** — generate SFX from text descriptions
- **Stable Audio API** — generate ambient/environmental sounds
- **Code synthesis (numpy/scipy)** — generate mechanical sounds (clicks, beeps, radio static)
- **Freesound.org API** — source Creative Commons reference audio
- **Post-processing** — normalize, trim, loop, format to UE5 specs (.wav, 48kHz, 16-bit)

---

## Pipeline 3: Audio — Music/Soundtrack

**What it produces:** Dynamic music system with layered tracks.

### Assets (20-30 tracks):

**Ambient/Exploration (4-6 tracks):**
- Pre-combat patrol ambience
- Beach environment atmosphere
- Urban environment atmosphere
- Night operations ambience

**Combat Music (6-8 tracks, layered):**
- Low tension layer (distant gunfire, unease)
- Medium tension layer (contact spotted, squad alert)
- High intensity layer (active firefight)
- Suppression/stress layer (pinned down, heavy incoming)
- Each track must loop seamlessly and crossfade between layers

**Event Stingers (6-8 short clips):**
- Mission start
- First contact
- Squad casualty
- Mission victory
- Mission failure
- Objective complete

**Menu/UI Music (2-3 tracks):**
- Main menu theme
- Loading screen ambient
- Briefing/debrief theme

### Approach:
- **Suno API** or **Stable Audio API** — generate full tracks from descriptions
- **Code post-processing** — trim to loop points, normalize loudness, add crossfade markers
- **Export** — .wav stems for UE5 MetaSound dynamic mixing system

---

## Pipeline 4: Audio — Voice Lines

**What it produces:** All character voice acting via AI TTS.

### Assets (260+ lines):

**Friendly Squad — English (210 lines):**
5 squad roles × 15 voice contexts × 3 stress variants (calm/normal/stressed):
- Contexts: ContactReport, OrderAcknowledge, Reloading, Suppressed, WoundedSelf,
  WoundedFriendly, ManDown, AmmoLow, GrenadeThrowing, AllClear, MovingUp,
  CoverMe, TargetDown, EnemyGrenade, FriendlyFire

Each role gets a distinct voice (pitch, timbre, speaking style):
- Team Leader — authoritative, composed
- Rifleman — standard military cadence
- Automatic Rifleman (SAW) — gruff, steady
- Grenadier — calm, methodical
- Designated Marksman — quiet, precise

**Enemy PLA — Mandarin Chinese (50+ lines):**
15 voice contexts × 3-4 speaker variants:
- Contexts: ContactFront/Left/Right/Rear, Reloading, ThrowingGrenade, Suppressed,
  ManDown, Retreating, AdvanceOrder, HoldPosition, Surrendering, EnemyDown,
  RequestingSupport

### Approach:
- **ElevenLabs Voice API** — generate distinct character voices with emotion control
- **Azure Cognitive Services TTS** — alternative, supports Mandarin natively
- **Script generation** — AI writes tactical dialogue scripts per context
- **Post-processing** — add radio filter effect, distance attenuation variants, compression

---

## Pipeline 5: UI/HUD Art

**What it produces:** All HUD textures, icons, and UI elements.

### Assets (50+ files):

**HUD Elements:**
- Damage direction indicator texture (512×512)
- Suppression vignette overlay (screen-sized)
- Stance icons: standing, crouching, prone (64×64 each)
- Body silhouette injury diagram (256×256)
- Compass cardinal markers and tick marks

**Squad Command Radial Menu (20+ icons):**
- Move To, Suppress, Flank Left, Flank Right, Hold Position, Regroup,
  Advance, Cover Me, Fall Back, Cease Fire
- Formation icons: Wedge, Line, Column, Echelon
- ROE icons: Hold Fire, Return Fire, Fire at Will, Weapons Free

**Menu Screens:**
- Main menu background (Taoyuan Beach concept art)
- Loading screen artwork (5-10 variations)
- Settings screen icons (video, audio, gameplay, controls)
- Mission select map overlay

### Approach:
- **Stability AI / DALL-E** — generate concept art (menu backgrounds, loading screens)
- **Code-generated SVG → PNG** — clean geometric icons (stance, formation, compass)
- **AI + post-process** — injury diagrams, damage indicators
- **Color-grading** — match HUD color scheme: military green (0.6, 0.8, 0.6, 0.85)

---

## Pipeline 6: VFX/Particle System Configs

**What it produces:** Niagara system configurations (JSON/code, not visual assets).

### Assets (65+ configs):

**Weapon VFX (22 systems):**
- Muzzle flash per weapon (11) — caliber-specific flash size/color
- Shell casing eject per weapon (11) — direction, velocity, mesh reference

**Projectile Impact VFX (10 systems):**
- Per surface: concrete, metal, wood, flesh, water, glass, dirt, rock, vegetation
- Default fallback impact

**Destruction VFX (13 systems):**
- Per destructible type: wall, roof, floor, column, window, door, sandbag,
  fortification, vehicle, tree, furniture, fence, generic

**Terrain Footstep VFX (15 systems):**
- Dust puff per terrain type (sand = large, concrete = small, water = splash)

**Drone VFX (5 systems):**
- Rotor wash, trail, destruction explosion, debris cascade, FPV fragmentation

### Approach:
- **Pure code generation** — write Python that outputs Niagara system parameter JSON
- **Template-based** — define base Niagara templates, vary parameters per asset
- **No AI needed** — this is deterministic config generation
- **Import** — UE5 Python script creates Niagara systems from parameter tables

---

## Pipeline 7: 3D Models

**What it produces:** Character, weapon, drone, and environment meshes.

### Assets (30+ meshes):

**Characters (3 base + role variants):**
- Player first-person arms (USkeletalMesh)
- Enemy soldier base (7 role equipment variants)
- Friendly squad base (5 role equipment variants)

**Weapons (11+ skeletal meshes):**
- M4A1, M27 IAR, M249 SAW, M110 SASS, M17 SIG, M320 GL, Mossberg 590
- QBZ-95 (enemy), .338 Lapua rifle, .50 BMG system
- Attachment meshes: scopes, suppressors, grips, lasers

**Drones (3 drone types × 3 components):**
- ISR drone: body, gimbal, rotors
- FPV strike drone: body, warhead, rotors
- Counter-drone system: emitter mesh

**Environment Props:**
- Sandbag wall, concrete barrier, ammo crate, fuel drum, radio equipment
- Destructible building sections (5 stages each)

### Approach:
- **Meshy AI API** — text/image to 3D model generation
- **Tripo3D API** — image-to-3D mesh generation
- **Rodin (Hyper3D)** — high-quality AI 3D generation
- **Post-processing code** — auto-LOD generation, UV unwrap validation, polygon count checks
- **Manual review recommended** — 3D models are the hardest to fully automate; AI gets 70-80% there, human polish needed for rigging/animation-ready quality

---

## Pipeline 8: Weapon Data Assets

**What it produces:** Missing weapon platform data (code + UE5 DataAssets).

### Assets (3 weapon definitions):

These are **code-only** — no art required (just ballistic data):

1. **.338 Lapua Magnum** — 250gr, 915 m/s, BC 0.675, 1,500m effective range
2. **12.7×99mm .50 BMG** — 660gr, 930 m/s, BC 0.620, 1,800m effective range
3. **7.62×39mm (AK variant)** — 123gr, 715 m/s, BC 0.300, 400m effective range

### Approach:
- **100% code** — Python script generates UE5 DataAsset configs
- **Ballistic tables** — real-world data encoded into weapon data structs
- **Immediate** — no external API needed, pure data generation

---

## Pipeline 9: Animations

**What it produces:** Weapon and character animation montages.

### Assets (55+ montages):

**Per Weapon (11 weapons × 5 each = 55):**
- `FireMontage_FP` — fire/recoil animation
- `ReloadMontage_FP` — standard reload
- `ReloadEmptyMontage_FP` — empty magazine reload (bolt catch)
- `ADSInMontage_FP` — aim down sights transition
- `MalfunctionClearMontage_FP` — tap-rack-bang clearance

**Character Animations (20-30):**
- Movement states: walk, run, sprint, crouch, prone per stance
- Death/ragdoll blend transitions
- Injury limp/stagger
- Grenade throw
- Vault/mantle

### Approach:
- **Mixamo** — free Adobe motion capture library, auto-rigged to characters
- **DeepMotion API** — AI-powered motion capture from video reference
- **Cascadeur** — AI-assisted animation tool (physics-based poses)
- **Procedural in code** — some animations (recoil, sway, bob) can be generated as curves
- **Hardest to automate** — animation is the most human-dependent pipeline. Recommend using Mixamo for base movement, procedural code for weapon mechanics.

---

## Quick Start — Running All Pipelines

### Prerequisites

```bash
# Python 3.10+
pip install -r Tools/ArtPipeline/requirements.txt

# API Keys (set in your shell profile or .env)
# Free tier available for all of these:
export UNSPLASH_ACCESS_KEY=""       # unsplash.com/developers
export PEXELS_API_KEY=""            # pexels.com/api

# Paid (usage-based, typically $5-20 per full run):
export STABILITY_API_KEY=""         # platform.stability.ai
export ELEVENLABS_API_KEY=""        # elevenlabs.io
export SUNO_API_KEY=""              # suno.com (or use Stable Audio)
export REPLICATE_API_TOKEN=""       # replicate.com (optional)
export MESHY_API_KEY=""             # meshy.ai (optional, for 3D)
```

### Run Order (Recommended)

```bash
# Step 1: Terrain Textures (already built)
cd Tools/ArtPipeline
python generate.py --full
# Or without AI: python generate.py --process-local

# Step 2: Sound Effects
cd Tools/AudioPipeline
python generate_sfx.py --full

# Step 3: Music
python generate_music.py --full

# Step 4: Voice Lines
python generate_voices.py --full

# Step 5: UI/HUD Art
cd Tools/UIPipeline
python generate_ui.py --full

# Step 6: VFX Configs
cd Tools/VFXPipeline
python generate_vfx.py --full

# Step 7: Weapon Data
cd Tools/WeaponDataPipeline
python generate_weapon_data.py --full

# Step 8: 3D Models (semi-automated, review recommended)
cd Tools/ModelPipeline
python generate_models.py --full

# Step 9: Animations (most manual, uses Mixamo + procedural)
cd Tools/AnimationPipeline
python generate_animations.py --full
```

### Import Everything Into UE5

```bash
# After all pipelines complete, run the master import:
cd Tools
python import_all_to_ue5.py
# This generates a single UE5 Python script that imports every asset.
# Run that script in UE5 Editor: Edit > Editor Preferences > Python > Execute
```

---

## Cost Estimate (API Usage)

| Pipeline | API | Est. Cost per Full Run |
|----------|-----|----------------------|
| Terrain Textures | Stability AI | $5-15 (75 images) |
| Sound Effects | ElevenLabs SFX | $10-20 (180 generations) |
| Music | Suno/Stable Audio | $10-30 (25 tracks) |
| Voice Lines | ElevenLabs Voice | $15-30 (260 lines) |
| UI Art | Stability AI | $3-8 (40 images) |
| VFX Configs | None (code only) | $0 |
| Weapon Data | None (code only) | $0 |
| 3D Models | Meshy AI | $20-50 (30 models) |
| Animations | Mixamo (free) | $0 |
| **TOTAL** | | **$63-153 per full run** |

All pipelines can also run **without paid APIs** using:
- Local Stable Diffusion (ComfyUI) for images
- Free reference photo sources for textures
- Code-synthesized audio for basic SFX
- Free Mixamo for animations

---

## File Structure (After All Pipelines Built)

```
Tools/
├── ArtPipeline/            ← BUILT
│   ├── config/
│   ├── modules/
│   ├── generate.py
│   └── requirements.txt
├── AudioPipeline/          ← TO BUILD
│   ├── config/
│   ├── modules/
│   ├── generate_sfx.py
│   ├── generate_music.py
│   ├── generate_voices.py
│   └── requirements.txt
├── UIPipeline/             ← TO BUILD
│   ├── config/
│   ├── modules/
│   ├── generate_ui.py
│   └── requirements.txt
├── VFXPipeline/            ← TO BUILD
│   ├── config/
│   ├── modules/
│   ├── generate_vfx.py
│   └── requirements.txt
├── ModelPipeline/          ← TO BUILD
│   ├── config/
│   ├── modules/
│   ├── generate_models.py
│   └── requirements.txt
├── WeaponDataPipeline/     ← TO BUILD
│   ├── config/
│   ├── modules/
│   ├── generate_weapon_data.py
│   └── requirements.txt
├── AnimationPipeline/      ← TO BUILD
│   ├── config/
│   ├── modules/
│   ├── generate_animations.py
│   └── requirements.txt
└── import_all_to_ue5.py    ← TO BUILD (master importer)
```

---

## What Can Run TODAY (No API Keys)

Even without any API keys, these work right now:

1. **Terrain Textures** — `python generate.py --process-local` (drop photos in `reference_photos/`)
2. **VFX Configs** — pure code generation, no API needed
3. **Weapon Data** — pure code generation, no API needed
4. **Animations (procedural)** — recoil curves, weapon bob, sway can be code-generated

Everything else needs at least one API key, but all have free tiers to start.
