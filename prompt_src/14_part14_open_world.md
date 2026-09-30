
═══════════════════════════════════════════════════════════════════════════
#  PART XIV — THE OPEN WORLD: SCALE, LIFE, STREAMING, PERSISTENCE
═══════════════════════════════════════════════════════════════════════════

# SECTION 122 — WORLD DESIGN LAW & SCALE

122.1 SCALE: Isla Sombra 6 x 6 km (36 km2) + Meridian 8 x 8 km (64 km2) +
      sea 40 km2 = 140 km2 of authored space, one unified streaming world per
      act. Travel-time targets (fastest sensible mode): island crossing 4 min
      sprint-less by vehicle; city crossing 5 min by car; island↔city ferry or
      plane cinematic-free streaming (3 min by boat, loading-free).
122.2 THE REACH RULE: any surface within the playable border that looks
      reachable IS reachable (or has an in-fiction barrier: cliffs, fences,
      patrols). Border handled diegetically (open sea hazard, military cordon,
      storm wall) — no invisible walls within the visible island.
122.3 CONTENT DENSITY (v2 multipliers over 4.1/4.2 minimums):
      Island: 12 → 24 outposts (12 primary + 12 micro), 4 → 8 Signal Masts,
        3 strongholds (unchanged, deeper), 8 → 20 hunting grounds, 4 → 8
        caves, 20 → 120 caches/collectibles, 40 dynamic-event slots,
        30 side missions, 60 points of interest (wrecks, ruins, shrines).
      City: 30 → 60 hero blocks, 120 → 900 city blocks (8 km2 grid), 12 → 48
        side-mission givers, 25 → 100 ducks/secrets, 18 street races, 12
        stunt jumps → 50, 10 heist-lite set-ups, 60 shops/vendors enterable,
        6 radio stations (3 original + 3 ambient), 140 hero interiors.
      Quality gate: no content bloat — each POI must pass "3-READ LAW" (47.1)
      and a 90-second fun test (video reviewed).
122.4 GUIDANCE: landmark-led navigation, compass + tagged waypoint, map with
      fog-of-war, NPC directions, route ribbons in vehicle (GPS option), 
      minimap (option: off for exploration purists).
122.5 CHARTED TRAVERSAL: fast travel points 40 (island 12, city 28), mode
      "Fast Travel with Streaming" (no loading screen: cinematic travel
      montage while streaming), vehicle call via phone.

# SECTION 123 — STREAMING ARCHITECTURE

123.1 WORLD PARTITION: 256 m runtime grid cells (Landscape, PCG output, props),
      512 m for actors with large bounds; data layers: Base, Story_ActI/II/III,
      Events, Night, Destruction, Interiors, Debug. HLOD: 3 levels (merged
      Nanite proxies w/ baked impostor materials), HLOD build in CI.
123.2 LOADING RULES: player-centric load range per tier (109.5); predictive
      streaming along vehicle/aircraft velocity vector; async PSO precache per
      cell; Nanite/VT streaming prioritized by screen coverage; audio bank
      streaming aligned to cell loads.
123.3 INTERIORS: seamless — interior cells stream when door approached
      (intent radius), exterior stays visible through windows (lightweight
      exterior proxy); Lumen handles GI via portals-lite; AI follows via
      Smart Objects; saves respect interior states.
123.4 ACT TRANSITIONS: Act I→II (cinematic ferry, streaming behind video of
      sequence), Act II→III (storm voyage). Map swap only at act boundaries;
      carry world state via save schema (10.5) with world.persistence blob.
123.5 TELEMETRY: streaming heatmap (hitches, pop-ins) collected in QA runs;
      thresholds in Section 147; top offenders → WBS.

# SECTION 124 — POPULATION, TRAFFIC & CROWDS (MASS)

124.1 MASS ARCHITECTURE: Mass Entity fragments for pedestrians, traffic,
      wildlife flocks, ambient birds/insects, boats; LOD: Actor (≤ 30 m,
      full skeletal + MM), Instanced-skinned (30–120 m, vertex animation
      textures/Animation Sharing), Dot/Impostor (120–400 m), Sim-only beyond.
124.2 DENSITIES (city, per km2 at peak hour): 2,500 pedestrians sim / 300
      visible-rendered at T3; 800 traffic agents sim / 120 visible; night
      weight shifts to venues (46.1); rain reduces pedestrian count 40% and
      increases umbrella/cover behavior; island village life (farming, fishing,
      laundry) per S46 schedules.
124.3 SCHEDULES: every Mass agent has a daily routine (home, work, leisure,
      transit) from archetype tables (Section 144) with 15-min resolution;
      gameplay consequences (shops open/closed, bank hours, nightlife).
124.4 CROWD BEHAVIOR: flow fields at crossings, queueing at vendors, group
      conversation clusters (Smart Objects), phone use, eating, dancing at
      venues; reactions to gunshots propagate as panic waves (radius 80 m,
      speed 6 m/s) with door-hiding and police calling.
124.5 TRAFFIC: road graph (Section 152) with lane-level Mass Traffic, junction
      controllers, bus routes, taxis (V05) with hail, emergency vehicles, funeral
      processions and parade events (event grid Section 96 legacy).
124.6 WILDLIFE (Section 151): ecosystems w/ predator-prey chains, hunting
      grounds, time-of-day activity, weather response, plausible spawn/
      despawn (never pop in view).

# SECTION 125 — LAW, HEAT, FACTIONS & EMERGENT EVENTS

125.1 WANTED SYSTEM (6.5 ladder binds): heat 0–5, police response tiers with
      cruisers, roadblocks (spikes), helicopters, tactical units (Task Force
      Limpio), search cones and LKP logic, escape by cone break + hide timer;
      witnesses + phone reports + CCTV (OPT) raise heat; bribes (S72).
125.2 FACTION WAR: territory control (S65), faction reputation, retaliation
      squads, reinforcement, alliance shifts, propaganda radio; outposts
      recapture timers; alert levels persisted in save world blob.
125.3 EMERGENCE ENGINE (S26 binds): dynamic events (convoy ambush, robbery,
      hostage, animal attack, vehicle chase, wildfire, collapsed bridge,
      street race, protest) on the 312-cell event grid (S96); event director
      ensures pacing: ≤ 1 high-intensity event per 3 min near player;
      events can chain and be hijacked by the player or wildlife.
125.4 REACTIVE MEDIA (S27): Herald headlines and radio news reference player
      deeds with slot grammar (S79, S89); TV in bars show news clips.
125.5 WORLD MEMORY: the world state diff (destroyed things, bodies, graffiti,
      tags, captured outposts, plantings) persists per save; budget 8 MB/save
      (compressed).

# SECTION 126 — INTERACTION DEPTH & LIFE DETAILS

126.1 DOORS/WINDOWS/ITEMS: physics-driven doors with handle animation; NPCs
      open doors; windows breakable (glass stage 3); items pick-up with
      arm-reach IK; containers with loot animation; shop counters interactive.
126.2 SURVIVAL-CRAFT (S6.7 binds): hunting (wildlife drops), plant gathering,
      crafting at workbench (hand animation), syringes, bandage animations.
126.3 FISHING & MINIGAMES (S31): rod cast physics, fish AI, fight-line tension;
      darts/poker/race-cards minigames with mocap hand animations.
126.4 PHONE & GADGETS: in-world phone (3D model) UI render-target; camera
      gadget for photo missions; binoculars with tagging & range-finder.
126.5 COMPANION (Poncho, S24): StateTree + motion matching quadrupeds; leash-
      free follow with 8 s teleport safety; commands; revive rules; bond.

# SECTION 127 — SPATIAL AUDIO & WORLD SOUND

127.1 PROPAGATION: geometry-aware audio occlusion/diffraction via Steam Audio
      or Project Acoustics (ADR-0003) or UE built-in occlusion + reverb
      volumes at T0–T2; reverb zones (Section 153): open, forest, canyon, city
      canyon, alley, interior-small/large, tunnel, cave, underwater, vehicle
      interior.
127.2 WEAPON AUDIO LAYERS: close/mid/far/tail convolved by environment;
      supersonic crack vs muzzle blast separated by distance; ambient duck
      (S32); distant gunfire responds to topography (echo slapback off
      cliffs).
127.3 AMBIENCE: per-biome 4-layer beds (base, insects/birds by TOD, weather
      overlays, sparse one-shots); city: traffic beds by road class, pedestrian
      murmurs, venue music bleed (occluded/muffled), sirens with Doppler.
127.4 FOLEY: footsteps by surface × footwear × gait × wetness (Section 145);
      cloth swishes by costume; gear rattle; body falls; vehicle interior
      rattles; hand-on-surface touches.
127.5 MUSIC: S32/S51 dynamic music stems (combat layers, stealth drones,
      exploration motifs) crossfade on bar boundaries; leitmotifs; radio
      stations licensed/original only (135).
127.6 MIX: dynamic range profiles (Cinema/Standard/Night), loudness target -23
      LUFS (Cinema) / -16 LUFS (Night), HDR audio-style ducking, subtitle cues,
      accessibility audio cues (33).

# SECTION 128 — WATER, OCEAN & UNDERWATER

128.1 SURFACE: Water plugin river/lake/ocean bodies; FFT ocean cascades (T3+)
      with whitecaps, foam persistence, SSS on wave tops; shore breaking via
      depth-based wave shaping; boat wakes leave foam trails.
128.2 INTERACTION: characters/vehicles displace water (RT ripple sim), rain
      ripples, splashes by impact energy, footstep splashes in shallows, wet
      footprints on sand fade; wetness on characters persists 60 s.
128.3 UNDERWATER: Post volume (absorption + scattering with depth-color shift),
      caustics projected, god-rays, particulates, marine life (Mass fish
      schools, rays, turtles, sharks (threat)), underwater caves, sound filter.
128.4 REEFS & SHIPWRECKS: 8 wrecks, 3 reef systems with coral PCG (Nanite);
      hidden cache dives; photo mode underwater.

# SECTION 129 — SAVE, PERSISTENCE & ROBUSTNESS AT SCALE

129.1 SAVE (10.5 binds): JSON schema=1 envelope + binary world-diff blob;
      rolling-5 autosave (37.4), manual slots 3+; load restores position,
      inventory, vehicles, dog, outposts, destroyed props, time, weather;
      forward migration with version ladder and corruption repair.
129.2 EDGE CASES: Section 98 (legacy edge matrix) remains binding; v2 adds:
      save during streaming hitch, during explosion chain, in vehicle in water,
      mid-takedown, during cinematic (blocked), low-disk, cloud-sync conflict.
129.3 CRASH HANDLING: CrashReportClient customized; minidump + logs to
      crash-log.txt; autosave integrity check on next boot; opt-in telemetry.
129.4 DETERMINISM TOOLS: replay recording (input + seed) for bug repro and
      speedrun kit (S30), in-editor replay scrubber, per-frame state hash in
      debug builds.

# SECTION 130 — CONTENT BUDGET & INSTALL SIZE v2 (HONEST SIZING)

130.1 EXPECTED INSTALL (Shipping, compressed Oodle Kraken): 90–140 GB:
      Landscape/terrain/VT ~14 GB · Nanite architecture + props ~26 GB ·
      vegetation ~9 GB · characters (+ UDIM skins, grooms) ~12 GB · weapons +
      vehicles ~7 GB · animation (ACL) ~1.2 GB · VFX ~2 GB · audio (music 6 h,
      VO 40 h, SFX, ambience, radio) ~22 GB · cinematics (in-engine) ~6 GB ·
      localization audio/text ~8 GB · PSO caches, shaders, misc ~4 GB.
130.2 PACKAGING: optional texture packs (Ultra-res 8K hero) as separate
      chunks; language audio packs separate; HDD-friendly chunk ordering;
      delta patches (IoStore).
130.3 RULES (unchanged spirit of 1.5): no zero-byte padding, no duplicated
      assets, no junk. If genuine content lands at 71 GB, SHIP 71 GB and state
      it. A size report (by asset class, by map, by biggest 50 assets) is part
      of every milestone report.
130.4 DEDUP & BUDGET GATES: texture duplication detector, unused asset
      sweeper, per-asset size budget by class (Section 139), CI fails on
      budget regression > 2% without rationale.
