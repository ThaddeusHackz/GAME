
═══════════════════════════════════════════════════════════════════════════
#  PART 0 — THE SUPREME DIRECTIVE (v2) — READ BEFORE ANYTHING ELSE
#  Precedence: Part 0 > Parts XI–XV (v2 hand-written) > Parts I–X (legacy)
#  > machine ledgers (Part XVI, Sections 137–156) and legacy matrices (IX–X). Matrices never override prose.
═══════════════════════════════════════════════════════════════════════════

# SECTION S0.1 — WHAT THIS DOCUMENT IS

This is the complete production order for DIVIDED HORIZON v2: an ORIGINAL,
photoreal, fully 3D open-world action game built in Unreal Engine 5 — island
first-person combat (Act I), city crime sandbox (Act II), island finale
(Act III). v1 of this document specified a stylized Godot game and was
delivered. v2 is the UPGRADE ORDER: take the finished game to a level where its
graphics, character movement, combat and open world are engineered to match or
exceed the best open-world action games ever shipped.

What changed in v2 (audit summary — the "forensic scan"):
  F1  Engine: Godot 4 / GDScript / Compatibility renderer  →  Unreal Engine 5
      (C++ + Blueprints), Nanite + Lumen + Virtual Shadow Maps + Substrate.
  F2  Visuals: painterly stylized, low-poly, "NOT photoreal — proudly"  →
      photoreal cinematic realism with physically based everything.
  F3  Honesty contract: declared photoreal impossible and benchmark quality
      unreachable  →  REALITY CONTRACT v2: ambition is default; completion
      claims must be gate-verified; originality + license hygiene stay hard.
  F4  Performance: 60 FPS on integrated GPU @720p  →  tiered contract T0–T5
      with upscaling (TSR/DLSS/FSR/XeSS), frame generation optional, and
      per-pass millisecond budgets (Section 109).
  F5  World scale: 2.5 km island + 3 km city  →  6 km island + 8 km city,
      World Partition streaming, Mass-simulated life (Sections 105, 122–125).
  F6  Animation: Mixamo clips + stubby rigs  →  Motion Matching, IK warping,
      MetaHuman-class faces, performance capture, contextual traversal and
      synchronized takedowns (Sections 107, 111–115).
  F7  Combat: hit-stop + damage zones  →  full ballistics (penetration, drop,
      surface response), procedural recoil + authored patterns, active-ragdoll
      hit reactions, squad AI with roles, fire/destruction systems
      (Sections 116–121).
  F8  Delivery: ~10 GB Godot zip  →  cooked/pak'd UE5 Shipping build, real
      content at real scale (Sections 130, 133).
  F9  Method: single-agent milestones  →  AI-SWARM production with parallel
      lanes, automated gates and a ~90,000-line machine-generated ledger set
      (WBS, world cells, animation, combat, QA; Sections 131, 137–156).

# SECTION S0.2 — READING PROTOCOL (this file is ~100,000 lines)

No model context window holds 100,000 lines at once. That is by design: this
file is a LIBRARY with a fixed entry point, not a single monolithic paste.
  1. ALWAYS load first: the INDEX (top of file), Part 0, and Sections 101–136.
     (~1,500 lines — this is "the brief".) Everything in it is binding.
  2. Load legacy Parts I–X by SECTION NUMBER when a system needs its lore,
     balance numbers or beat sheets (Part IV codex, V armory, VI campaign).
  3. Machine ledgers (Part XVI) and legacy matrices (IX–X) are LOOK-UP TABLES. Never read them
     linearly. Use the INDEX line ranges: `sed -n 'START,ENDp' MASTER_PROMPT.md`
     or `grep -n "^| ANM-0421 " MASTER_PROMPT.md`. Every row has a stable ID.
  4. When a matrix row and prose disagree, PROSE WINS; log the row as stale.
  5. Rebuild: `python3 tools/build_prompt.py` regenerates this file from
     prompt_src/ and tools/gen_v2/ (deterministic; no RNG; byte-identical).

# SECTION S0.3 — PRIME DIRECTIVES (the five laws of v2)

L1  FIDELITY: Every frame a player sees must survive a pause-and-inspect test
    at 4K on T5 and look like a photograph of a believable place. No visible
    tiling, no floating props, no T-poses, no texture pops, no light leaks,
    no foot-sliding. (Gates in Section 134.)
L2  MOTION: Every movement must have weight, anticipation and follow-through
    at 120 Hz input-to-photon latency budget (Section 111.6). Motion must never
    feel floaty, never snap, never slide.
L3  COMBAT: Every shot has a cause and a consequence you can see, hear and feel
    — surface-specific impact, penetration, reaction, sound, rumble — and the
    enemy fights back like a squad of people, not a shooting gallery.
L4  WORLD: The world exists when you are not looking: schedules, weather,
    ecology, traffic, law, rumor. Nothing is a painted backdrop inside the
    playable area; everything you can see, you can reach or it is deliberately
    framed (Section 122.2 "the reach rule").
L5  PROOF: Nothing is "done" until a gate says so (automation test, screenshot
    diff, Insights trace, Blind Bench). Claims without evidence are bugs.

# SECTION S0.4 — SUPERSESSION REGISTER (every legacy statement that changes)

| LEGACY § | v1 STATEMENT (summary) | v2 STATUS | REPLACED BY |
|----------|------------------------|-----------|-------------|
| 0 | role: Godot TD | REWRITTEN in place | S0, S131 |
| 1 | honesty contract: photoreal impossible | REWRITTEN in place | S1 (Reality Contract v2) |
| 2.3 | assets from CC0 low-poly packs | REPLACED | S104, S135 |
| 3 P4 | runs on anything @ integrated | REWRITTEN in place | S109 tiers |
| 4.1/4.2 | 2.5 km island, 3 km city | ENLARGED (4.0 override) | S105, S122, S137 |
| 8 | stubby stylized Ren | REWRITTEN in place | S107, S111 |
| 9 | ~10 GB budget | TOMBSTONED | S130 |
| 10 | Godot 4.3 constitution | TOMBSTONED (10.4/10.5/10.6 survive) | S101–S103 |
| 11 | typed GDScript | REWRITTEN in place | S11, S102 |
| 13 | Kenney/Quaternius/Mixamo packs | TOMBSTONED | S104, S135 |
| 15 | 60 FPS @720p Low integrated | TOMBSTONED | S109 |
| 16 | milestones M0–M8 | TOMBSTONED | S132 |
| 17 | Godot export, .exe promise | TOMBSTONED (.exe promise survives) | S133 |
| 18.1/18.3/18.11 | min-spec, integrated, Godot | PATCHED in place | S134 |
| 40, 63 | M9–M13 plans | TOMBSTONED (intent → epics E29–E33) | S132, S149 |
| 43.3 | painterly stylized | REWRITTEN in place | S110 |
| 44.1/44.3 | animation law for stylized rigs | PATCHED in place | S111–S115 |
| 58 | hardware matrix, old-laptop pledge | TOMBSTONED | S109.6 |
| 84 | Godot scene trees | TOMBSTONED | S102 + table S0.5 |
| 85 (QA matrix) | platforms UHD620…DECK | KEPT; platform IDs REMAP per S0.6 | S134 |
| all other | gameplay, lore, balance, campaign, scripts | KEPT, BINDING | — |

# SECTION S0.5 — GODOT → UNREAL TRANSLATION TABLE (read legacy text through this)

| LEGACY (Godot term) | UE5 EQUIVALENT |
|---------------------|----------------|
| project.godot / autoload singleton | .uproject / GameInstanceSubsystem, WorldSubsystem |
| EventBus signal | Gameplay Message Subsystem, multicast delegates, Gameplay Tags |
| CharacterBody3D | ACharacter + Mover (or CMC) with custom movement modes |
| VehicleBody3D | Chaos Vehicles (custom wheel/suspension model, Section 114) |
| Camera3D / CameraPivot | Gameplay Camera System / CameraComponent + physical camera settings |
| NavAgent / NavigationRegion3D | NavMesh + NavLinks + Smart Objects + Mass Navigation |
| FSM / behavior tree | StateTree (top-level) + Behavior Tree leaves + EQS |
| AnimTree | Animation Blueprint + Pose Search (Motion Matching) + Control Rig |
| Area3D / BondArea | Overlap volumes / Gameplay Ability System tasks / Mass processors |
| .tscn scenes | Levels + World Partition cells + Blueprint classes + Data Layers |
| GPUParticles3D | Niagara |
| ShaderMaterial / StandardMaterial3D | Material Graph, Substrate, Material Instances |
| DirectionalLight3D + SSAO | Directional Light + Lumen GI + VSM + Sky Atmosphere |
| Resource (.tres) | Data Assets, Data Tables, Primary Data Assets |
| GUT tests | Automation Spec + Functional Tests + Gauntlet |
| FastNoiseLite terrain | Landscape + heightfields (Gaea/Houdini/World Creator) + PCG |
| glTF import | FBX/USD/glTF via Interchange; Datasmith for DCC scenes |
| TTS voice placeholders | Final VO (studio/AI-cloned with consent) + MetaHuman Animator / audio-driven facial |

# SECTION S0.6 — LEGACY PLATFORM ID REMAP (for Section 85 / 99 matrices)

| LEGACY ID | v2 TIER | MEANING |
|-----------|---------|---------|
| UHD620 | T0 | Compatibility tier: best-effort, 720p, SM5, no Lumen; NOT a launch gate |
| VEGA8 | T0 | as above |
| GTX750TI | T1 | Minimum: GTX 1060-class remap, 1080p30-ish Low, software Lumen off |
| GTX1060 | T1 | Minimum tier gate: 1080p TSR-Performance, 30 FPS |
| RX580 | T2 | Recommended-low: 1080p, 45–60 FPS Medium |
| DECK | T2H | Handheld: 800p, 40 FPS locked, FSR |
| (new) RTX3070/RX6800 | T3 | Recommended: 1440p60, High, Lumen software/HW hybrid |
| (new) RTX4080/RX7900XT | T4 | Enthusiast: 4K60 TSR/DLSS, Epic, HW Lumen |
| (new) RTX5090-class | T5 | Halo: 4K/120 + path-tracing showcase mode |

# SECTION S0.7 — THE QUALITY BAR, STATED AS NUMBERS

Every adjective in this document is backed by a number somewhere in
Sections 101–136. The headline numbers:

| DIMENSION | MEASUREMENT | v2 TARGET |
|-----------|-------------|-----------|
| Geometry | unique Nanite tris on screen | 20–50 M visible, <= 4 ms raster budget T3 |
| Materials | texel density (hero / world) | 1024 px/m hero · 512 px/m props · 256 px/m terrain detail |
| Lighting | GI | fully dynamic Lumen; no baked dependency; HW RT on T4+ |
| Shadows | resolution | VSM 16k virtual, contact shadows on characters |
| Characters | hero LOD0 | ~120k tris + strand hair, 8K/4K-UDIM skin |
| Animation | locomotion | Motion Matching DB >= 4 h mocap-class, foot slide <= 1 cm avg |
| Latency | input → photon | <= 50 ms @ 60 FPS (T3), <= 30 ms @ 120 (T5) |
| Gunplay | firearms | 12 firearms, 1,728-row penetration table, per-weapon patterns |
| AI | simultaneous combatants | 24 full-fidelity + 200 Mass-LOD in combat zone |
| World | playable area | 140 km2 footprint (36 island + 64 city + 40 sea) |
| Streaming | hitch | zero loading screens inside an act; no hitch > 33 ms in traversal |
| Life | population | 1,500+ simultaneous Mass agents in city core (LOD'd) |
| Weather | states | 12, blendable, all Lumen/Niagara/Audio-integrated |
| Stability | soak | 4 h no crash; < 5% memory growth |

# SECTION S0.8 — HOW "BETTER THAN" IS EARNED (and judged)

"Match or exceed" is not a slogan; it is a blind A/B protocol (Section 134.4):
reference captures of comparable scenes from best-in-class shipped games are used
ONLY as internal perception benchmarks (never as assets), against matched
captures of our build, scored blind by a rubric. Exceed = ours wins >= 50% of
rubric categories in >= 80% of matched scenes at T4. The rubric categories:
  foliage density & lighting · water · skin/face/eye · hair · cloth ·
  locomotion weight · gun feel · vehicle feel · destruction · fire · crowd life ·
  night/neon city · weather · draw-distance coherence · art-direction identity.
Where an area scores worse, the WBS (Section 149) auto-spawns a fix epic.

═══════════════════════════════════════════════════════════════════════════
#  PART I–X (LEGACY, PATCHED) — game soul, lore, balance, campaign, scripts
#  Read through the translation tables S0.5 and S0.6.
═══════════════════════════════════════════════════════════════════════════
