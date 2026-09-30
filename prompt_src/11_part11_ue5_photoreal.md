
═══════════════════════════════════════════════════════════════════════════
#  PART XI — THE UE5 MIGRATION & PHOTOREAL CHARTER
═══════════════════════════════════════════════════════════════════════════

# SECTION 101 — ENGINE & TOOLCHAIN DECISION

101.1 ENGINE: Unreal Engine 5, latest stable 5.x at project start (>= 5.6;
      prefer the newest release where Nanite Foliage, Nanite Skeletal Meshes,
      Substrate and MegaLights are production-supported — VERIFY each feature's
      maturity against release notes on day 0 and record the decision in
      docs/adr/0001-engine-version.md). Lock the version per milestone; upgrade
      only at milestone boundaries with a regression pass.
101.2 WHY UE5 (decision record): it is the only widely available engine whose
      shipping feature set already contains the photoreal pillars required —
      virtualized geometry (Nanite), fully dynamic GI + reflections (Lumen),
      virtual shadow maps, strand hair, Chaos physics/vehicles/destruction,
      Mass entity simulation, World Partition + HLOD, PCG, MetaHuman,
      Motion Matching (Pose Search), Niagara, MetaSounds — so the swarm spends
      effort on CONTENT and GAMEPLAY QUALITY instead of rebuilding an engine.
101.3 LANGUAGES: C++20 (engine modules), Blueprints (glue/tuning/UI),
      Python (editor automation, content validation), Houdini/Blender Python
      (DCC pipelines), HLSL only via Material Graph custom nodes or engine
      plugins, Verse: NOT used.
101.4 TOOLCHAIN (all required unless marked OPT):
      - Visual Studio 2022 (MSVC v14.4x, Win SDK 10.0.22621+), .NET 8 SDK
      - Git + Git LFS (binary assets) — or Perforce/Diversion (OPT, if a human
        owner prefers); `.gitattributes` tracks uasset/umap/fbx/exr/wav/etc.
      - Blender 4.x (modeling/retopo/rigging), Houdini Indie/Apprentice or
        Houdini Engine (PCG/city/destruction/VAT) , Substance 3D Painter/
        Designer OR Materialize/ComfyUI PBR generation (see 104.6),
        RizomUV/Blender UV, Gaea or World Creator (terrain, OPT),
        SpeedTree or Fab foliage (vegetation), Marvelous Designer (cloth, OPT)
      - Audio: Reaper, Wwise OR native MetaSounds (decision ADR-0002; default
        native MetaSounds + UE Audio Modulation), Sonniss GDC bundles
      - Capture/QA: Unreal Insights, RenderDoc, PIX, NVIDIA Nsight, Intel GPA,
        CSV Profiler, Gauntlet, Automation Driver, ffmpeg (video diffing)
101.5 PLUGINS (enable explicitly; each has an ADR line if shipped):
      Mover (or CMC w/ extensions), Pose Search, Motion Warping, Control Rig,
      IK Rig/Retargeter, Chaos Vehicles, Chaos Destruction (Geometry
      Collection), Chaos Cloth, Chaos Physical Animation/Control, Niagara,
      Niagara Fluids (smoke/water sims), Mass (Entity, Crowd, Traffic, AI,
      Gameplay, Movement), Smart Objects, State Tree, Gameplay Abilities,
      Gameplay Tags, Enhanced Input, Common UI, World Partition, PCG, Water,
      Landscape Patch, Procedural Content Generation Framework, Sequencer,
      Movie Render Graph, MetaHuman Creator/Animator, Live Link, Audio
      Modulation, MetaSounds, Audio Synesthesia, Steam Audio OR Project Acoustics
      (OPT), Online Subsystem (for achievements; optional), DLSS/FSR/XeSS
      plugins, Streamline (frame-gen), Hardware RT, Path Tracer (showcase).
101.6 NON-GOALS (explicit): multiplayer, VR, console SKUs at launch (code must
      not block them: no Windows-only hard dependencies in gameplay modules).

# SECTION 102 — REPOSITORY, MODULES & CODE STANDARDS

102.1 REPO LAYOUT (Git LFS for content):
   /DividedHorizon.uproject
   /Source
     /DHCore        GameInstance/World subsystems, save, settings, data registry,
                    tags, message bus, time-of-day + weather controller, telemetry
     /DHMovement    Mover modes, traversal (mantle/vault/climb/swim/zip/glide),
                    motion-matching glue, foot IK, warping providers
     /DHCombat      weapons, ballistics, damage, hit reactions, melee/takedown,
                    explosives, fire, destruction hooks, aim assist, recoil
     /DHAI          StateTree tasks, perception, cover/Smart Objects, squad
                    director, EQS contexts, wildlife, dog companion
     /DHVehicle     Chaos vehicle model, damage/deformation, radio, boats/air
     /DHWorld       streaming rules, PCG graphs glue, ecology, population,
                    traffic, events, wanted/law, persistence
     /DHMission     objective system, triggers, checkpoints, dialogue runtime
     /DHUI          CommonUI layer: HUD, map, menus, photo mode, accessibility
     /DHAudio       MetaSounds glue, propagation, mix states, dynamic music
     /DHTools       editor utilities, validators, batch importers, Python bridge
     /DHTests       Automation specs, functional test actors, Gauntlet configs
   /Plugins         3rd-party + internal plugins (each with README + license)
   /Content
     /Art           /Characters /Creatures /Weapons /Vehicles /Props /Architecture
                    /Landscape /Foliage /Materials /Decals /Textures /VFX
     /Animation     /MotionMatching /Weapons /Traversal /Vehicles /Takedowns
                    /Facial /Cinematic /Retarget
     /Audio         /Weapons /Foley /Ambience /Music /VO /Radio /Vehicles
     /Maps          IslaSombra (WP), Meridian (WP), Interiors, Sandbox, TestRange
     /Data          DataAssets, DataTables, JSON imports (balance numbers 10.4)
     /UI            /Widgets /Fonts /Icons
   /Saved, /Intermediate, /DerivedDataCache, /Binaries  → gitignored
   /tools           build.sh/.ps1, validate_content.py, gen scripts, CI configs
   /docs            ADRs, reports/MX_report.md, LICENSES.md, CHANGELOG.md
102.2 NAMING (enforced by validator): SM_ static mesh, SK_ skeletal, M_/MI_/MF_
      materials/instances/functions, T_ texture (suffix _BC _N _ORM _E _H _M),
      NS_ Niagara, ABP_ anim BP, PSD_ pose-search DB, AS_/AM_/BS_ anim assets,
      CR_ control rig, IKR_ IK rig, BP_ blueprint, DA_ data asset, DT_ table,
      WP_/L_ maps, MS_ MetaSound, SC_ sound cue, SW_ sound wave, A_ audio attn.
102.3 C++ STANDARDS: Epic coding standard; UCLASS/USTRUCT reflection for all
      tunables; no raw new/delete; TObjectPtr; async loading via
      StreamableManager/AssetManager; no blocking loads on game thread; tick
      only where required (prefer timers, Mass processors, event-driven);
      `SCOPED_NAMED_EVENT` + `TRACE_CPUPROFILER_EVENT_SCOPE` in every system
      update; no hard references from gameplay modules to content (soft only).
102.4 ACTOR/COMPONENT LAYOUT (replaces legacy §84):
   PLAYER  ADHPlayerCharacter : ACharacter/Mover
     ├ USkeletalMeshComponent Body (full-body, first-person capable)
     ├ UDHFirstPersonArmsComponent (FP arms proxy, shared skeleton)
     ├ UDHMovementComponent, UDHTraversalComponent, UDHFootIKComponent
     ├ UDHWeaponManager → ADHWeapon[] (skeletal, attachments as child comps)
     ├ UDHHealthComponent, UDHStaminaComponent, UDHStatusEffects
     ├ UDHInteractionComponent, UDHCompanionBond (Poncho)
     ├ UCameraComponent + UDHCameraDirector (FP/TP blend, recoil, sway)
     └ UDHClothHandle (poncho cloth: Chaos Cloth asset)
   ENEMY   ADHAICharacter : ACharacter + UStateTreeAIComponent, UAIPerception,
           UDHSquadMember, UDHCoverUser (SmartObject), UDHHitReactionComponent,
           UPhysicalAnimationComponent, UDHLootDropper, UDHBarkComponent
   VEHICLE ADHVehicle : AChaosWheeledVehiclePawn (or custom) + seats[],
           UDHDamageModel (zone deform + part detach), UDHRadioComponent,
           UDHVehicleAudio (MetaSound), UDHVehicleLights
   CROWD   Mass entities (fragments: Transform, Velocity, Appearance, Schedule,
           Panic, Faction) → ISM → skeletal LOD actors within 25–40 m
   WORLD   ADHWorldController: time-of-day, weather, ecology, events, law
102.5 DATA-DRIVEN RULE (10.4 stays): balance/economy/loot/spawn tables are
      DataAssets + JSON-importable DataTables. Hot-reload in editor; validated
      by tools/validate_content.py in CI (schema + range checks).

# SECTION 103 — RENDERING CHARTER (THE PHOTOREAL STACK)

103.1 GEOMETRY: Nanite for ALL opaque static geometry (architecture, props,
      rocks, landscape-adjacent meshes, foliage trunks/branches/leaves where
      Nanite Foliage is available). Non-Nanite allowed only for: skinned
      meshes without Nanite skeletal support, translucency, WPO-heavy assets
      not yet Nanite-compatible (document each in the validator whitelist).
      Displacement: Nanite tessellation/displacement on terrain-blend hero
      surfaces (cobble, bark, rock faces) where supported.
103.2 GLOBAL ILLUMINATION & REFLECTIONS: Lumen (software on T1–T3, hardware RT
      on T4–T5 for reflections/GI), Lumen Reflections for water/wet asphalt;
      screen-space fallbacks only on T0. MegaLights (where production-ready)
      for the neon city at night: hundreds of shadow-casting emissive/area
      lights on screen without per-light cost explosions. Emissive meshes from
      signs/windows feed Lumen; all night city lighting is physically motivated.
103.3 SHADOWS: Virtual Shadow Maps everywhere; directional sun at 16k virtual;
      local lights with VSM; contact shadows + capsule shadows on characters;
      distance-field shadows for far-terrain fallback on T1.
103.4 MATERIALS: Substrate (layered: clear coat car paint, wet asphalt film,
      skin SSS + fuzz for fabrics, rough-dielectric glass, anisotropic brushed
      metal). Master materials: M_Master_Opaque, _Foliage, _Landscape, _Skin,
      _Hair, _Cloth, _Glass, _Water, _Decal, _Emissive, _Vehicle_Paint (24
      total, permutation budget in Section 155). Virtual Textures (RVT) for
      terrain/road blending; SVT for large atlases; Nanite-friendly
      masked/programmable raster only where unavoidable.
103.5 ATMOSPHERE: Sky Atmosphere + Volumetric Clouds (3 cloud layers, weather-
      blended) + Exponential Height Fog w/ volumetric fog + Local Fog Volumes
      (mist in jungle valleys, harbor haze) + Sky Light real-time capture.
      Rain: Niagara + GPU-driven wetness (global wetness parameter in M_*),
      puddle mask via RVT height, ripples, lens droplets (post). Heterogeneous
      Volumes for smoke/fire/explosions on T3+; sprite fallback on T0–T2.
103.6 UPSCALING & AA: TSR default; DLSS 4 / FSR 4 / XeSS 2 selectable; Frame
      Generation optional (never enabled in automated perf gates; reported
      separately); DLAA/TSR native for T5; Temporal stability budget: no
      visible ghosting on foliage/wires at 100% test-pan (screenshot-diff
      gate Section 148).
103.7 POST: physically based camera — aperture/shutter/ISO driven exposure
      with auto-exposure histogram, eye adaptation (tuned: 1.2 s dark→bright),
      bloom (convolution on T4+), lens flares (subtle), chromatic aberration
      (<= 0.15, user option), film grain (<= 0.2, option), vignette (<= 0.15),
      color grading via ACES-like OCIO tone-mapper + per-act LUTs from 43.1
      color scripts, HDR10/Dolby Vision output (scRGB) with calibration UI.
103.8 WATER: Water plugin + custom far-ocean FFT cascades (T3+), Gerstner
      fallback (T0–T2); shoreline foam, wake/bow-wave sim via RT, underwater
      volumetric with caustics + god rays; river flow maps authored via splines;
      refraction/SSS through Substrate water (Section 128).
103.9 HAIR/FUR: Groom (strand-based) for hero + 40 close-range NPCs; cards for
      crowd; fur for wildlife via groom-lite; wet-hair response.
103.10 DECALS: DBuffer/Nanite-aware decals: bullet holes (per-surface 16
      variants), blood (wet/dry), scorch, mud, puddle splashes; decal budget by
      tier; RVT "world-space grime" under props for contact realism.
103.11 CAMERA/LENS LANGUAGE: Cine Camera with real focal lengths; gameplay FOV
      slider 70–110 (horizontal) with lens-correct distortion at edges (<= 2%);
      motion blur per-object + camera at 180° shutter equivalent, optional.
103.12 SHADER/PSO: PSO precaching enabled; shader compile hitch gate = zero
      stutter > 33 ms after first-run warmup scene; bundled PSO cache for
      Shipping; compile step part of build (Section 133).
103.13 RENDER-FEATURE TIER MAP (binding):
   T0 SM5, no Lumen, baked-ish ambient fallbacks, sprite smoke, cards-only hair
   T1 SM6, Lumen software low-res, VSM low, TSR-Perf, reduced Nanite budget
   T2 SM6, Lumen software med, VSM med, strands on hero only
   T3 SM6, Lumen software high + HW reflections optional, VSM high, strands 40 NPC
   T4 SM6, Lumen HW GI/reflections, VSM epic, heterogeneous volumes, MegaLights
   T5 as T4 + path-traced showcase mode (photo mode + trailer), 4K native

# SECTION 104 — MATERIAL, SCAN & ASSET PIPELINE (PHOTOREAL SOURCES)

104.1 SOURCE HIERARCHY (attempt in this order per asset; first that meets the
      gate wins; record pipeline id in asset metadata `DH_SourceMethod`):
   P1  PHOTOGRAMMETRY / SCAN LIBRARIES — Fab/Quixel Megascans (surfaces, 3D
       scans, atlases, vegetation), other CC0/CC-BY scan libraries
       (Poly Haven, ambientCG — CC0), own captured scans when a human supplies
       photos. Best for rocks, surfaces, vegetation, debris, set dressing.
   P2  HAND/PROCEDURAL AUTHORING — Blender/Houdini modeling with PBR-calibrated
       textures (Substance/Designer/Materialize). Best for architecture kits,
       signs, vehicles, weapons (hard-surface), props needing variation.
   P3  GENERATIVE ASSISTED — image→PBR-map generators, text/image→3D
       generators (for blockout/hero prop base meshes), upscalers, texture
       synthesis. ALWAYS followed by retopo/cleanup + PBR calibration + a
       human-auditable provenance record; generated assets containing
       recognizable third-party IP are rejected. (License/AI-disclosure: 135.4)
   P4  MARKETPLACE HERO PACKS — Fab vehicles/characters/weapons only when the
       license allows redistribution in a shipped game and assets are
       re-skinned/modified to our art direction; logged in LICENSES.md.
   P5  PLACEHOLDER — allowed only at M0–M3; must carry `DH_Placeholder=true`;
       zero placeholders permitted in Release Candidate build (gate G-PH).
104.2 TEXEL DENSITY LAW: hero (weapons, hands, faces) 1024–2048 px/m;
      characters' body 512–1024; vehicles 512–768; architecture 256–512
      (trim sheets + unique hero faces); terrain detail 256 (RVT 4 cm/px near,
      blended to 32 cm/px far); foliage leaf atlases 256–512 px/m. A validator
      measures texel density and fails assets outside ±30% of class.
104.3 PBR CALIBRATION (albedo ranges, sRGB 0–255 for base color, measured):
      charcoal 50 · fresh asphalt 55–75 · worn asphalt 75–95 · dry dirt 85–120 ·
      wet dirt 45–70 · concrete 120–150 · brick 100–140 · bark 40–80 ·
      leaves 40–70 (bright young 60–90) · sand 150–190 · snow N/A ·
      skin (light) 170–220 · skin (dark) 55–115 · clean white paint <= 240 ·
      no albedo < 30 or > 240 except emissive. Roughness maps contain real
      variation (std-dev >= 0.06); metallic only 0/1 + edge-wear gradients.
      Validator script `validate_pbr.py` samples histograms; fails outside.
104.4 VARIATION & ANTI-TILING: world-aligned macro variation maps, per-instance
      random color/roughness via Custom Primitive Data, 4 decal "grime" layers,
      stochastic tiling for terrain layers, vertex-paint/trim wear masks.
      Gate: 50 m screenshot test shows no repeat pattern at 10% contrast boost.
104.5 MODULAR KITS: Architecture built from modular kits (walls, corners,
      balconies, shopfronts, roofs, AC units, cables, awnings...) on a 50 cm
      snap grid; every module has 3 wear levels and 2 graffiti/poster passes;
      kit piece count targets per district in Section 106.3.
104.6 TEXTURE GENERATION RULES (for P2/P3): 16-bit height, OpenGL/DirectX normal
      convention = DirectX, ORM packing (R=AO, G=Roughness, B=Metal), BC7
      for color/ORM, BC5 for normal, BC4 for masks; UDIM for characters
      (skin 4 tiles 4K). Streaming mip bias by tier; texture pool sized by
      tier (T1 2.5 GB, T3 6 GB, T5 10 GB).
104.7 LODs/NANITE BUDGETS: Nanite meshes 50k–2M tris hero, 5k–50k prop (Nanite
      handles LOD); non-Nanite skinned LOD chain 100%/50%/25%/10%/imposter;
      crowd LOD: LOD0 25k, LOD1 12k, LOD2 5k, LOD3 1.5k, impostor sprite.
104.8 COLLISION: simple collision primitives for props; complex-as-simple only
      for non-moving architecture; Chaos physical materials per surface type
      (Section 145) drive audio, VFX, decals, vehicle grip, AI noise.
104.9 ASSET DEFINITION OF DONE (validated automatically): naming ✔, pivot ✔,
      scale (1 uu = 1 cm) ✔, texel ✔, PBR ✔, collision ✔, physical material ✔,
      Nanite setting ✔, LOD/impostor ✔, source-method tag ✔, license row ✔,
      sound-surface tag ✔, destruction tier ✔, thumbnail ✔, placed-instance count
      within budget ✔.

# SECTION 105 — LANDSCAPE & WORLD BUILDING (ISLA SOMBRA + MERIDIAN)

105.1 ISLA SOMBRA: 6 x 6 km footprint (36 km2) within a 10 x 10 km sea
      footprint. Elevation 0–520 m (Cresta Sombria ridge), volcanic core with a
      caldera lake (Laguna Quieta), 3 river systems, 11 waterfalls (4 jumpable),
      6 beaches, mangrove estuary, cloud forest above 300 m, 140 caves/overhangs
      (4 full cavern interiors), 2 sugar-mill valleys, cult hillside villages.
      Authored heightfield at 1 m/px (6,096 x 6,096 + 2 px border) in Gaea/
      World Creator/Houdini with erosion, then sculpt pass for hero vistas;
      Landscape with Nanite enabled; World Partition 256 m runtime cells,
      1 km HLOD tiles, OFPA (one file per actor).
105.2 LANDMARK LAW: every 500 m of travel presents a new readable landmark or
      vista; 4 Signal Masts visible from 70% of the island; lighthouse visible
      from the sea ring; silhouettes tested with 1-color-clay render.
105.3 BIOME TABLE (binding; drives PCG + audio + wildlife + weather):
      B01 Coral Beach · B02 Mangrove · B03 Lowland Rainforest · B04 Cloud Forest ·
      B05 Volcanic Ridge · B06 Sugar-Cane Fields · B07 River Gorge ·
      B08 Cult Village · B09 Cave Mouth · B10 Shallow Reef · B11 Deep Ocean ·
      B12 Caldera Lake.
105.4 PCG: Procedural Content Generation framework graphs place vegetation,
      rocks, debris, ruins, fences, cables, litter from biome + slope + wetness +
      proximity-to-path masks; hero dressing is hand-placed in a separate
      data layer. Density targets: rainforest 3,500 Nanite foliage instances/ha
      (5 layers: canopy, sub-canopy, shrub, ground-cover, litter), beach 400,
      cloud forest 2,800, cane fields 6,000 (stalks via ISM+WPO wind), cult
      village 900 props/ha. Performance gated by Section 109 instance budgets.
105.5 WIND: global wind vector field (from weather controller) drives foliage
      WPO (Nanite-compatible), cloth, flags, water surface, rain slant, audio
      (leaf rustle layers). Gusts propagate as traveling waves (visible across
      canopy). Per-species sway parameters in Section 140.
105.6 ROADS & PATHS: spline-deformed landscape (Landscape Patch) for roads,
      trails and river beds; RVT blend for asphalt edges, tire tracks; road
      furniture via PCG (signs, guardrails, bollards, litter).
105.7 OCEAN & ISLAND RING: 10x10 km ocean with shelf depth data, reef patches,
      wrecks (8), kelp/seagrass PCG, bioluminescent plankton at night (toggle),
      distant islets as HLOD backdrops (reachable by boat: 3).
105.8 CAVES & INTERIORS: four authored cavern levels (Cueva del Trueno, Bóveda
      Sumergida, Colina Hueca, Mina Vieja) streamed as sub-levels with
      volumetric fog, dripping audio, lightless-with-flashlight sections tuned
      for Lumen (emissive bounce minimum), bats Mass crowd.

# SECTION 106 — CITY & ARCHITECTURE (MERIDIAN)

106.1 FOOTPRINT: 8 x 8 km (64 km2) incl. harbor, airport strip, highway loop,
      hillside Alto Barro, industrial La Herrería, Solada Beach strip,
      Downtown glass core, residential rings, countryside fringe, 2 bridges,
      rail yard, stadium, university campus, hospital, prison island.
106.2 ROAD NETWORK: ~420 km of lanes across 6 road classes (Section 152);
      intersections with real signal phasing (2 s amber, 1 s all-red);
      one-way/two-way graph; parking (street + garages); highway ramps with
      merge logic; rail crossings.
106.3 BUILDING SYSTEM: 2,400 building archetypes (Section 154) composed from
      modular kits; 1,100 enterable interiors (seamless, streamed) of which 140
      are fully hand-dressed "hero interiors"; remaining use 60 room templates
      × dressing permutations (Section 154). Window interiors (parallax/
      interior-mapping) on all non-enterable facades; enterable = real rooms.
106.4 SIGNAGE & NEON: 3,200 sign assets (neon tubes emissive + MegaLights
      sources), bilingual EN/ES; flicker/buzz audio; wet-street reflection
      tuned via Lumen Reflections + screen-space hit improvement.
106.5 URBAN DETAIL DENSITY (per city block, minimums): 60 street furniture,
      120 litter/debris, 40 cables/wires (spline Nanite), 25 vehicles parked,
      12 vendors/stalls (evening), 8 graffiti/posters, 6 AC/vent props per
      facade, 4 trees, 2 dynamic hero props (flag, sign, laundry).
106.6 DAY/NIGHT CITY: streetlight circuits (sodium/LED by district, random
      outages), building-window schedules driven by occupancy simulation,
      headlight/taillight bloom, rain-slick reflections, steam vents,
      traffic-light glows.
106.7 DISTRICT CHARACTER (visual identity table: palettes + materials + props):
      Old Harbor — rusted steel, rope, tarps, sodium; Downtown — glass/steel/
      marble, cool LED, reflections; Solada — pastel stucco, palm, neon pink;
      Alto Barro — corrugated metal, exposed brick, laundry, cables, tight
      alleys, warm bulbs; La Herrería — steel, freight, sulfur lamps, steam.

# SECTION 107 — CHARACTERS, FACES, HAIR & CLOTH

107.1 HERO CHARACTERS (Ren, Tobias, Marisol, El Sereno, bosses): MetaHuman
      Creator base → sculpt/customize in Blender/ZBrush to original designs;
      LOD0 ~120k tris body+head; 8K skin across UDIM tiles (albedo, normal,
      cavity, roughness, SSS, micro-normals); 4 wrinkle-map states; eyes with
      refractive cornea, caustics fake, wetness meniscus; teeth/tongue; strand
      hair (groom) with physics and wetness; eyebrows/lashes strands.
107.2 NPC CAST: 25 named NPCs at hero-lite (MetaHuman + custom costume), 240
      crowd archetypes (Section 144) built from 32 body bases × 120 heads ×
      costume-piece kit (12 slots) with color/pattern masks; per-NPC voice +
      facial set. Crowd LODs per 104.7.
107.3 FACIAL ANIMATION: audio-driven facial animation for all VO (MetaHuman
      Animator / audio-to-face) + authored emotional layer (brow/eyes/blink/
      saccades); hero cinematics use performance capture (face + body);
      saccades/micro-expressions/blink rates procedural; gaze IK (44.3).
107.4 CLOTH: Chaos Cloth for poncho, jackets, skirts, flags; wind-coupled;
      collision capsules per body; wet cloth darkens + drags (shader+sim param);
      gear (straps, holster) physics via Chaos Physical Animation.
107.5 SKIN & DAMAGE STATES: dirt/sweat/blood/wetness layers (Substrate) per
      character; wound decals by hit zone; scars persist for hero per NG+;
      sun-tan progression over game-time (subtle).
107.6 BODY VARIETY & FIT: size/height/mass variance ±10%; clothing morph/
      retarget correct across 32 bases; no clipping gate (automated pose sweep
      across 20 animations × all costume pieces, Section 144).
107.7 ANIMALS: wildlife (boar, chanchos, capybara-likes, birds, reef fish,
      crocs, jaguars (2 bosses-lite), lizards, snakes, bats, crabs, monkeys) via
      skeletal meshes + fur groom-lite, motion matching for quadrupeds (DB >= 1 h
      each quadruped class), species list Section 151.
107.8 POSE & SILHOUETTE: 43.4 silhouette rules still bind (hats, coats, poncho).

# SECTION 108 — LIGHTING, TIME OF DAY & WEATHER

108.1 TIME OF DAY: 24 h cycle, 1 in-game hour = 2 real minutes default (user
      option 1–10 min). Sun/moon ephemeris for Isla Sombra latitude (11° N
      fictional), seasonal tilt fixed (equatorial rainy/dry mix). Moon phase
      cycle 12 days. Stars with correct density; Milky Way texture HDR.
108.2 WEATHER STATES (12, blendable over 90–300 s): Clear, Hazy, Partly Cloudy,
      Overcast, Drizzle, Rain, Storm (lightning), Tropical Squall, Fog/Mist,
      Heat Shimmer, Night Clear, Night Storm. Each defines sun intensity,
      cloud coverage/type, fog density, wind, wetness target, audio bed,
      VFX set, gameplay modifiers (visibility 44, noise masking 6.3, vehicle
      grip, NPC behavior) — table in Section 138.
108.3 EXPOSURE & GRADE: EV100 targets (noon clear 14.5, overcast 12.5, dusk
      8, night city 3–5, night jungle 0–1) with auto-exposure clamped to keep
      gameplay readable; HDR peak 1000–4000 nits configurable.
108.4 INTERIOR LIGHTING: motivated sources only (43.2), Lumen GI through
      windows, light shafts via volumetric fog, portal-less; interior/exterior
      exposure transition adaptive (1.2 s).
108.5 LIGHTNING & PYRO LIGHT: lightning = 3 flash strobes (photosens-safe
      alt 33), real sound propagation delay (distance/343 m/s); explosions and
      muzzle flashes emit dynamic light into Lumen with color temperature.
108.6 FIRE: Niagara + heterogeneous volume/sprite hybrid, emissive into Lumen,
      propagation rules (Section 120), wind-driven, smoke columns visible 2 km.
108.7 COLOR SCRIPTS: 43.1 palettes apply as LUT + light color grammar
      (e.g., Act II neon cyan/magenta). Grading never crushes blacks below
      0.02 or clips highlights except explosions; HDR-safe.
108.8 AUTOMATED LIGHTING QA: Section 138 grid renders each TOD×weather×zone
      camera and validates luminance histogram bands (no crushed/blown > 2%).

# SECTION 109 — PERFORMANCE CONTRACT v2 (TIERS, BUDGETS, GATES)

109.1 HARDWARE TIERS (gates):
   T0 COMPAT   GTX 960/iGPU-class — 720p, SM5, 30 FPS best-effort (NOT a gate)
   T1 MIN      GTX 1060 6GB / RX 580 — 1080p, TSR-Perf (720p internal), 30 FPS
   T2 REC-LOW  GTX 1660S/RTX 2060 — 1080p, TSR-Quality, 45–60 FPS
   T2H DECK    Steam-Deck-class — 800p, FSR, 40 FPS locked
   T3 REC      RTX 3070 / RX 6800 — 1440p, TSR-Quality/DLSS-Q, 60 FPS avg
   T4 ENTH     RTX 4080 / RX 7900 XT — 4K, DLSS-Q/TSR-Q, 60 FPS avg; 1440p 120
   T5 HALO     RTX 5090-class — 4K native/DLAA 60–90, 120 w/ upscaling
   CPU baseline: i7-class 6c/12t @ 3.5+ GHz (T1–T3); 8c+ recommended T4+.
   RAM: 16 GB min, 32 GB rec. Storage: SSD REQUIRED for T2+, HDD supported at T1
   (streaming budgets reduced). VRAM: 6 GB (T1) / 8 GB (T3) / 16 GB (T5).
109.2 FRAME BUDGET (T3 @ 60 FPS = 16.67 ms; GPU pass budgets, ms):
   Nanite visibility+raster 2.6 · Base/material pass 2.4 · Lumen GI 2.8 ·
   Lumen reflections 1.2 · VSM 2.2 · Volumetric fog/clouds 0.9 ·
   Translucency + VFX 1.1 · Post + TSR 1.8 · Hair/strands 0.5 · UI 0.3 ·
   Headroom 0.87 ms. CPU (game thread 10 ms, render thread 9 ms, RHI 8 ms).
   Budget table per tier/scene: Section 147. Overbudget = blocking bug.
109.3 CPU BUDGET (game thread @ T3, ms): Character/anim 2.4 · AI (24 full +
   Mass) 2.0 · Physics (Chaos) 1.8 · Vehicles 0.8 · World sim (traffic, ped,
   ecology) 1.5 · Gameplay/UI/audio 1.0 · Headroom 0.5. Worker-thread offload
   mandatory for animation (parallel eval), Mass, Chaos, audio DSP.
109.4 MEMORY: (T3) total process <= 14 GB; GPU VRAM <= 7.2 GB; streaming pool
   cap 3.5 GB textures, Nanite pool 1 GB; leaks: <5% growth over 4 h soak.
109.5 STREAMING & HITCH: zero loading screens inside an act; traversal at 160
   km/h (aircraft) without pop: HLOD + Nanite; hitch gate: no frame > 33 ms
   during 1-hour scripted traversal route; first-minute shader warmup scene.
   World Partition load range: 1.2 km (T3), 0.8 km (T1), 2 km (T5).
109.6 TIER PLEDGE (replaces old-laptop pledge; print in README): "Every tier
   has a number, and we publish it. If your hardware meets a tier and misses
   its number, it's a bug — file it." (Report tables via built-in Benchmark.)
109.7 INSTANCE BUDGETS (visible, T3): Nanite instances 400k · foliage 2.5M
   (clustered) · dynamic lights w/ shadows 64 (MegaLights: 500+ unshadowed-
   cost) · skinned characters full 60, LOD'd 400 · Niagara systems 120,
   particles 1.2M GPU · decals 8,000 · physics bodies active 1,500 · audio
   voices 128 (virtual 512).
109.8 SCALABILITY GROUPS: r.ViewDistance, Shadow, GI, Reflections, PostProcess,
   Texture, Effects, Foliage, Shading, Landscape, Nanite, Hair, Crowd, Traffic,
   Water, Volumetrics — each 0..4 (Low..Cinematic), mapped per tier; first-run
   benchmark recommends tier; user can override each (exposed in UI, S50).
109.9 PROFILING RITUAL: nightly automated run — 12 traversal routes × 4 weather ×
   3 TODs × 3 tiers produce CSV + Insights traces; dashboard regressions >3%
   fail the nightly; top-10 offenders auto-assigned as tasks (Section 149).

# SECTION 110 — ART DIRECTION v2 (PHOTOREAL, WITH A SIGNATURE)

110.1 PHILOSOPHY: photoreal is the substrate, not the identity. Identity =
      composition, color script, silhouette language, and restraint. The
      goal is "a location scout's dream, shot by a great DP".
110.2 REFERENCE DISCIPLINE: art leads build mood boards from real-world
      photography (tropical Caribbean/Central American/West African coasts,
      Latin-American and Mediterranean port cities, tropical hillside
      settlements) and cinematography; shipped-game screenshots may be used
      for perceptual benchmarking ONLY (S0.8); NEVER as trace/copy sources.
110.3 ACT I — ISLA SOMBRA: jade & gold; humidity visible as haze; god-rays
      through canopy; wet leaves with specular glints; sea turquoise→
      ultramarine with physically plausible depth absorption; skin with
      sweat+sun.
110.4 ACT II — MERIDIAN: rain-slick neon, sodium harbor, cold marble core;
      reflections are the hero (wet asphalt, glass towers, car paint); night
      city emissive richness; heat shimmer in day; heavy atmospherics.
110.5 ACT III — STORM RETURN: violet storm, lighthouse gold; volumetric cloud
      drama; rain interaction with every surface; exhaustion on the hero's
      face (progressive grime/fatigue layers).
110.6 "NO UNCANNY" RULES: eyes wet + gaze-correct; teeth not too white;
      micro-motion always on (breathing, weight shift); hands posed naturally;
      no plastic skin (SSS tuned per tone); clothing wrinkles match motion;
      hair not helmet; beards/stubble strands.
110.7 "NO CG TELLS": no perfect edges (chamfer/wear everywhere), no uniform
      dirt, no symmetrical damage, no sterile clean cars, no loop-perfect
      crowd, no floating dust-free air (particulates in light shafts),
      no mirrored foliage, no billboard-tree silhouettes at < 500 m.
110.8 ART GATES: each biome/district must pass a 12-camera "Art Lock" review
      (Section 148 screenshots) with checklist in 134.3 before its feature
      lock. Art leads (human if available; else the ART lane + rubric) sign.
110.9 UI & PHOTO MODE: HUD remains minimal, diegetic where possible; photo
      mode with full physical camera controls (focal length, aperture, ISO,
      shutter, focus pull, exposure comp), HDR capture, path-traced capture
      on T4+ (accumulated samples), time/weather scrub.
