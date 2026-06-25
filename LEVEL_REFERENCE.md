# M01 Taoyuan Beach — Reference-Grounded Art Direction

Authentic art direction for the level, grounded in **real Taoyuan/Taiwan geography and
architecture** (researched + reference photos viewed 2026-06-25). Drives Wave 3 (visuals) and
Wave 6 (level composition) in [AAA_LEVEL_SPEC.md](AAA_LEVEL_SPEC.md), and concrete changes to the
headless build scripts (`Tools/LevelBuilder/build_beach_defense.py`, `build_urban.py`,
`build_base_map.py`). Reference images in `Tools/refs/` (attributions in [CREDITS.md](CREDITS.md)).

## Geographic truth (why the level makes sense)

- The real plausible amphibious-landing ("red") beaches near **Taoyuan are Haihu and Linkou** —
  close to **Taoyuan Airport** and the **Port of Taipei**, the objectives heavier reinforcements
  would target. Our fiction (a Taoyuan beach assault toward an urban objective) is authentic.
- Taiwan's beaches are **few, narrow, and overlooked by mountains** — the Central Mountain Range /
  coastal hills rise **immediately behind** the coastal plain. The defender holds the high ground.
- This is **exactly** what `level_plan.json` already models: ocean → surf → wet/dry sand → dunes →
  vegetation → **trench (defensive line)** → coastal road → rice paddies → **urban** → highway →
  **mountains start at Y=+12000**. The cross-section is correct; this doc makes the *surfaces and
  set-dressing* authentic.

## Reference photos (viewed)

| File | Shows | Use |
|---|---|---|
| `coast_tetrapods.jpg` | Gray-brown sand bay, **concrete tetrapod sea-defense blocks**, sea wall, rocky headland, green hills behind | Beach surface + obstacles |
| `coast_northtaiwan.jpg` | Golden sand, narrow strip, **mountains directly behind**, breakwater groin, driftwood debris, clear blue sky | Beach geography + palette |
| `taoyuan_street.jpg` | **Real Taoyuan** intersection: yellow box-junction markings, red no-park curbs, mid-rise tiled shops w/ red awnings, neon/banner signage, scooters, cantilever signals, cable tangles, walled compound w/ red-tile-capped wall | Urban vocabulary |
| `temple.jpg` | Taiwanese temple: **orange glazed-tile upturned-eave roofs**, ornate dragon ridges, **red columns**, polychrome dougong bracketing, white balustrade terraces | Hero landmark |

## BEACH ZONE — art direction (`build_beach_defense.py`)

- **Sand:** gray-brown to muddy-tan (Taiwan west coast is **not** tropical white). Wet darker band
  at the waterline, drier lighter dunes inland. Megascans beach/wet-sand surfaces.
- **Tetrapods (signature):** clusters of **concrete wave-dissipating tetrapods** along the
  waterline / sea wall — authentic *and* double as hard cover and an amphibious obstacle. Place in
  irregular piles at the surf/wet-sand boundary. (Single Nanite mesh, instanced.)
- **Sea wall / revetment:** low concrete sea wall + sloped revetment along the dune base — natural
  defensive line, ties into the trench.
- **Beach defenses (combat):** Czech-hedgehogs, concertina wire, dragon's-teeth, sandbag emplacements,
  HESCO barriers along the trench line — period-correct amphibious-defense kit.
- **Debris/story:** driftwood, kelp lines, wrecked landing-craft hulks, scattered gear — the
  aftermath of a contested landing.
- **Vegetation:** hardy dune grass / beach naupaka, then denser scrub toward the trench; green
  hills/palms toward the mountains. Poly Haven / Megascans foliage.
- **Surf:** strong multi-line breakers (single-layer water on Low). Groins/breakwaters offshore.
- **Backdrop:** green mountains immediately behind — already in the heightfield; ensure they read
  as a defensible ridge, not a flat wall.

## URBAN ZONE — art direction (`build_urban.py`)

The town is **suburban Taoyuan**, not Taipei skyscrapers: **4–6 story** density, wide roads.

- **Buildings:** mid-rise **tiled/painted concrete** (tan, ochre, cream, faded teal, pink), flat
  roofs cluttered with water tanks, rebar, AC units, laundry, rooftop shrines. Ground floor =
  shops/garages with roll-up shutters. Some Japanese-colonial baroque façades + red-tile pitched
  roofs on older blocks.
- **Qílóu arcades (signature):** continuous **covered arcade walkways** at ground level (columns +
  overhang) sheltering shopfronts and **parked scooters** — Taiwan's defining streetscape. Great
  for cover, sightline breaks, and interior fighting.
- **Signage (signature):** dense **vertical neon + banner signs**, red/yellow with Chinese
  characters, projecting over the street; red shop awnings; yellow horizontal banners. This is what
  reads "Taiwan" instantly.
- **Streets:** wide asphalt with **yellow box-junction cross-hatch**, white zebra crossings,
  **red-painted no-park curbs**, lane arrows; cantilever traffic signals; bus stops.
- **Clutter (signature):** **tangled overhead power/utility cables** between poles; scooters in
  tidy/messy lines along curbs and under arcades; betel-nut stands (neon glass booths); convenience
  stores; vending machines; street trees + palms in planters.
- **Walled compounds:** schools/temples behind **red-tile-capped masonry walls** with decorative
  gates (seen in the Taoyuan ref) — natural strongpoints.

## LANDMARKS (navigation + named in `shots.py`)

- **Temple (hero):** orange glazed-tile upturned-eave roof, red columns, white balustrade plaza,
  dragon ridge — visible from the beach as the player's objective beacon. Becomes a major
  fight/strongpoint.
- **Market:** covered/arcade market street — tight, vertical, signage-dense CQB.
- **Overpass/highway:** the elevated highway (Y −70000..−66000) — verticality + a flank route.
- **Radar/comms site, landing craft** — already named camera anchors; dress them.

## PALETTE & TIME OF DAY

- **Mood:** recommend **overcast or golden-hour** for drama and (critically) so the **Low path can
  bake** the lighting cheaply. Clear blue (ref `coast_northtaiwan`) is authentic too — keep as a
  High-path option.
- **Base palette:** cool blue-gray sea, gray-brown sand, green hills; **warm concrete grays** in
  town with **red/orange/yellow signage accents** and **red-tile** roofs; the temple's
  **orange/red/gold** as the focal pop.
- **Atmosphere:** humid haze, subtropical; smoke columns from the battle; god-rays at golden hour.

## BUILD-SCRIPT CHECKLIST (Wave 3/6 execution)

- [ ] `build_beach_defense.py`: tetrapod instances at surf line; sea wall/revetment; hedgehogs +
      wire + dragon's-teeth + sandbags on the trench line; wrecked landing craft; driftwood/debris.
- [ ] `build_urban.py`: arcade (qílóu) building variants; rooftop clutter; vertical neon/banner
      signage props; scooter instances along curbs + under arcades; overhead cable splines between
      poles; red-curb + box-junction road decals; walled temple/school compounds.
- [ ] `build_base_map.py` / materials: Taiwan-correct sand (gray-brown), wet/dry bands; Megascans
      surface set; mountain ridge reads as defensible.
- [ ] Landmark temple placed + lit as the beach-to-town objective beacon.
- [ ] Time-of-day set to golden-hour/overcast; Low path bakes it.

*All set-dressing assets sourced free/AI per [AAA_LEVEL_SPEC.md](AAA_LEVEL_SPEC.md) §3 (Megascans
free in Fab, Poly Haven/ambientCG CC0, Kenney, CitySample in-project, AI-gen signage/decals).*
