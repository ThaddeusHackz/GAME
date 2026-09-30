
═══════════════════════════════════════════════════════════════════════════
#  PART XV — AI-SPEED PRODUCTION, DELIVERY, ACCEPTANCE & LEGAL
═══════════════════════════════════════════════════════════════════════════

# SECTION 131 — THE AI-SWARM PRODUCTION METHOD

131.1 PREMISE: human studios needed years because work was serial, manual and
      communication-bound. An AI swarm works in parallel lanes around the
      clock; the binding constraints become (a) verification, (b) tool/
      licence access, (c) integration conflicts, (d) GPU/CPU build time.
      Plan around THOSE. "Months of calendar time" is a fine outcome;
      "years" is a planning failure.
131.2 LANES (each lane = an agent role with its own backlog in Section 149):
      GAMEPLAY · MOVEMENT/ANIM · COMBAT · AI · VEHICLES · WORLD/ENV · CITY ·
      CHARACTERS · RENDERING/TA · VFX · AUDIO · MUSIC/VO · UI/UX · MISSIONS/
      NARRATIVE · CINEMATICS · TOOLS/PIPELINE · BUILD/CI · QA/AUTOMATION ·
      PERFORMANCE · LOCALIZATION · LEGAL/LICENSE · PRODUCER (integrator).
131.3 CONTRACTS: each lane owns specific folders (Section 102) and publishes
      interface contracts (headers/DataAssets). Cross-lane changes go through
      PRs with automated checks; the PRODUCER lane arbitrates. Daily integration
      build, nightly full automation, weekly milestone candidate.
131.4 WORK LOOP per task: (1) read the relevant spec rows (Section 149 task →
      links to sections/matrix IDs), (2) implement, (3) author tests/gates,
      (4) run gates, (5) capture evidence (screens/video/trace), (6) PR with
      evidence, (7) merge on green, (8) update the task + report.
131.5 AUTOMATION-FIRST: every acceptance item in Section 134 has an automated
      or evidence-capturing test; "look at it and say it's fine" is allowed
      only for art review, and always produces screenshots in the PR.
131.6 CONTENT FACTORY: batch pipelines for props, materials, foliage, buildings,
      crowd costumes, audio variants — scripts with deterministic seeds and
      validators (104.9); per-asset metadata; nightly content health report.
131.7 HUMAN-IN-THE-LOOP: where only a human can act (hardware, studio mocap,
      paid licences, legal sign-off, Epic account/EULA acceptance, signing
      certificates, native-language review), the agent prepares a precise
      checklist, blocks ONLY that item, and continues elsewhere.
131.8 ANTI-PATTERNS: serial bottlenecks, unreviewed generated assets,
      unmeasured "optimizations", feature-creep without a gate, hiding
      placeholders, merging on red, silent scope cuts.

# SECTION 132 — MILESTONES v2 (ALWAYS PLAYABLE, ALWAYS MEASURED)

(Replaces legacy §16/§40/§63. Durations are SWARM-CALENDAR targets; gates, not
dates, close a milestone.)

 M0  FOUNDATION (wk 0–1): UE5 project, modules (102), CI (build, cook,
     automation), Git LFS, validators, ADR-0001..0003, LICENSES skeleton,
     test range level, capsule player. ✅ Gate: CI green; packaged Win64
     build boots to menu and walks the test range; nightly dashboard live.
 M1  MOVEMENT SLICE (wk 1–4): Mover character, Motion Matching v0 (2 DBs),
     foot IK, traversal (vault/mantle/climb), FP body + camera feel, sprint/
     slide/crouch/swim v0, camera FP/TP. ✅ Gate: 60-s parkour course passes
     flow + slide + pop tests; input-latency gate.
 M2  GUNPLAY SLICE (wk 3–7): 4 weapons (pistol, rifle, shotgun, sniper) with
     ballistics, penetration v1, recoil patterns, reload rigs, hit reactions
     v1, 2 enemy archetypes with cover AI, training range. ✅ Gate: 30-enemy
     arena Gauntlet green; feel sheets approved.
 M3  PHOTOREAL ISLAND BLOCK (wk 4–10): 1 km2 of Isla Sombra at FINAL QUALITY:
     Landscape + PCG rainforest/beach/mangrove, Lumen/VSM/Nanite tuned, ocean,
     weather v1, 1 outpost (3 valid approaches), 1 mast, 2 wildlife species,
     day/night. ✅ Gate: Art Lock passes; Blind Bench rubric >= 40% wins at
     T4; frame budgets T1..T5 met.  ★ THIS IS THE QUALITY BAR.
 M4  PHOTOREAL CITY BLOCK (wk 8–14): 12 blocks of Meridian (harbor +
     downtown fringe), modular kits, enterable interiors (10), Mass traffic
     (60)/peds (150), 3 vehicles with Chaos handling + damage, heat 0→5.
     ✅ Gate: steal-car→evade-3-stars→buy-hot-dog loop grins; night neon
     Art Lock; budgets met.
 M5  SYSTEMS (wk 10–18): economy, skills, crafting, safehouse, persistence
     (129), map/fog, fast travel with streaming, weather full (12 states),
     wildlife suite, Poncho v1. ✅ Gate: 60-min play session with progression;
     save/load edge grid (S98) green.
 M6  WORLD SCALE-UP (wk 14–30): full landscape (6x6 km) + city (8x8 km) in
     greybox-to-final passes per district/biome; PCG full; HLOD builds;
     streaming tuned; 24 outposts/8 masts; 900 city blocks. ✅ Gate: world
     cell grid (S137) 100% cells reviewed; hitch gate on all traversal routes.
 M7  CAMPAIGN & CINEMATICS (wk 18–36): 11 missions + strongholds + bosses
     (S73/74/75), performance-captured cutscenes (S150), VO, dynamic music,
     Herald/radio, all side content. ✅ Gate: complete 12–20 h playthrough;
     checkpoint grid (S87) green.
 M8  COMBAT & ANIMATION DEPTH (wk 20–40): 12 firearms + melee + 28
     takedowns, full ballistics tables, AI squads/roles, fire/destruction,
     vehicles (12) polish, motion-matching DBs complete. ✅ Gate: Section 142
     tables 100% implemented; encounter grid Gauntlet green.
 M9  LIFE & POLISH (wk 30–46): Mass crowds full, ecology, audio pass, UI/UX,
     accessibility (33), i18n (34), photo mode, minigames, NG+, mutators,
     contracts. ✅ Gate: legacy Section 39 [SHIP]+[LOVE] green.
 M10 OPTIMIZATION & HARDENING (wk 40–52): profiling vs 109 for all tiers,
     PSO/shader hitch elimination, memory, soak (4 h), crash-proofing,
     content size/dedup, Blind Bench >= 50% at T4. ✅ Gate: Section 134 ALL
     green; zero placeholders (G-PH).
 M11 RELEASE CANDIDATE & SHIP (wk 50–56): package (133), sign, LICENSES,
     README, CHANGELOG, demo cut, trailer via Movie Render Graph. ✅ Gate:
     clean-machine install/launch on T1/T3/T5 test rigs; legal gate.
 M12+ LIVE/LOVE PATCHES: S59 feedback loop; mod kit; DLC-scale epics.

Milestone report (docs/reports/MX_report.md) MUST include: done / stubbed /
deviations / perf table per tier / Blind Bench scores / size report /
placeholder count / open risks / next-week plan.

# SECTION 133 — BUILD, PACKAGING & DELIVERY v2 (THE .EXE PROMISE)

133.1 PIPELINE (`tools/build.sh` / `build.ps1`):
   1. Clean checkout → restore LFS → generate project files
   2. Compile (Development Editor + Shipping Game, Win64)
   3. Validators (naming, PBR, texel, budgets, licenses, placeholders)
   4. Cook (-iterate in dev; full in RC), PSO cache gather + bundle
   5. Automation: Smoke → System → Gauntlet perf → screenshot diff
   6. Stage + pak/IoStore + Oodle compress; chunk optional packs
   7. Archive → `dist/DividedHorizon-win64/` → installer (zip + optional
      Inno/MSIX) → checksums (SHA-256) → symbols (PDB) archived separately
133.2 DELIVERABLE TREE:
   DividedHorizon-win64/
     DividedHorizon.exe            ← launcher/bootstrapper (double-click)
     DividedHorizon/Binaries/Win64/DividedHorizon-Win64-Shipping.exe
     DividedHorizon/Content/Paks/  (pakchunk0 base, chunk1 4K+, chunk2 audio-EN,
                                    chunkN language packs, optional Ultra textures)
     Engine/Binaries/ThirdParty/   (DLSS/FSR/XeSS, Oodle, redists)
     /docs  README_PLAYER.md (controls, tiers, settings), LICENSES.md,
            CHANGELOG.md, ACCESSIBILITY.md, TIER_PLEDGE.md
     /tools  benchmark.bat (-benchmark), safe_mode.bat (-safe)
   README states: Windows 10 22H2/11 x64, DX12 GPU (SM6) T1+, DX11/SM5 T0
   compat (-dx11), 16 GB RAM, SSD recommended, 90–140 GB disk, per-tier numbers.
133.3 FALLBACK LADDER (if the build environment cannot run UE5 cooks/packaging):
   (a) deliver the full UE5 project + Linux/Windows build scripts + exact CI
       recipe + ADR explaining the gap → the human runs `build.ps1` on a
       Windows/Epic-account machine;
   (b) run a cloud Windows GPU builder (self-hosted runner) if the owner
       provides access;
   (c) last resort: Vertical-slice exe (M3/M4) built with whatever toolchain
       is available, CLEARLY labeled SLICE, plus (a).
   NEVER deliver a tree with no runnable path to an .exe; ALWAYS report which
   rung was used and what the human must do (EULA acceptance, account linking,
   GPU builder access).
133.4 SIGNING & INTEGRITY: Authenticode signing (owner certificate) — if absent,
   document SmartScreen warning; SHA-256 sums; reproducible build notes.
133.5 UPDATING: delta patches via IoStore; version stamp in main menu; crash
   reporter; opt-in telemetry.

# SECTION 134 — ACCEPTANCE v2 ("PERFECT" = THESE, ALL GREEN)

134.0 EXTENDS legacy §18/§39/§62 (all gameplay/content/UX/legal items remain
      binding, read through Part 0 tables). NEW/CHANGED GATES:
134.1 G-STAB: 4 h soak (Gauntlet bot roams + fights + drives + saves); zero
      crashes; < 5% memory growth; no GPU hang/TDR; alt-tab/resolution change
      safe; HDR toggle safe.
134.2 G-PERF: per-tier frame-time gates (109): avg, 1% low, 0.1% low across
      12 traversal routes × 3 TODs × 4 weather on T1/T3/T5 rigs (or
      emulated budgets if only one rig: state it); hitch gate (no frame > 33
      ms); input-latency gate.
134.3 G-ART (Art Lock checklists per biome/district): no visible tiling; no
      floating props; no light leaks; shadows stable; reflections coherent;
      foliage density meets 105.4; wetness coherent; LOD/HLOD pop-free at walk
      & vehicle speed; no default/placeholder materials; histogram bands OK
      (108.8); color script fidelity (43.1).
134.4 G-BENCH (Blind Bench, S0.8): matched-scene A/B vs internal perception
      references; rubric of 15 categories (S0.8); pass = >= 50% category wins
      in >= 80% scenes at T4; report per-category deltas; failing categories
      spawn epics.
134.5 G-ANIM: foot-slide ≤ 1 cm avg; no pops; transitions ≤ 3 frames of
      visible blend artifacts; traversal landing error ≤ 2 cm; takedown sync
      error ≤ 1 cm; ragdoll jitter-free; cloth stable at 120 FPS; IK no
      hyperextension; facial lip-sync error ≤ 2 frames on ≥ 98% of VO lines.
134.6 G-COMBAT: S68 numbers honored ±1%; Section 142 tables implemented and
      data-verified; TTK within ±10%; penetration behaves per table (Gauntlet
      shooting range with every weapon × material × angle); recoil patterns
      match; squad AI passes encounter grid; fairness tokens respected.
134.7 G-WORLD: world cell grid (S137) fully loaded/visited by a bot (no fall-
      through, no stuck, no unreachable-reachable violations); 312-cell events
      fire; traffic/ped soak 15 min per district; AI nav coverage ≥ 99.5% of
      walkable area; ecology counts stable over 1 h sim.
134.8 G-CONTENT: 11/11 missions + 3 strongholds + 24 outposts + 8 masts +
      side content; both endings; 100% tasks in Section 149 closed or
      explicitly deferred with rationale.
134.9 G-ACCESS: legacy 33 fully green; subtitles/contrast/remap/holds;
      photosensitivity path verified with flash-analyzer (Harding test).
134.10 G-LEGAL: zero third-party IP; LICENSES.md complete; AI-generated asset
      disclosure file; no placeholder flags (G-PH); music/voice rights.
134.11 G-SHIP: clean-machine install + launch on T1/T3/T5; size report; README
      truthful; symbols archived; checksums.
134.12 G-EVIDENCE: every gate produces artifacts (logs/CSV/screens/video) stored
      under docs/evidence/MX/; the final report links each.

# SECTION 135 — LEGAL & LICENSING v2

135.1 ORIGINALITY (hard): no Rockstar/Ubisoft/other IP (names, characters,
      logos, maps, dialogue, music, models, textures, scripts). "Comparable
      quality" is a goal for craft and scale; it never licenses imitation of
      specific protected expression. Title, factions, locations and story are
      original (Parts I–VII).
135.2 ALLOWED THIRD-PARTY: Epic/Unreal content usable in shipped UE games per
      the current UE EULA and Fab license (verify per asset; record the
      license text hash); CC0 libraries (Poly Haven, ambientCG); CC-BY with
      attribution; purchased commercial packs with redistribution rights;
      self-authored; human-performed capture with signed releases.
135.3 MUSIC & VO: original or properly licensed only; Sonniss GDC bundles
      (royalty-free license) allowed; radio stations original compositions or
      CC/licensed tracks with written permission; VO from consented performers
      or consented voice-cloning with signed agreements (no cloning without
      consent); TTS placeholders removed by RC.
135.4 GENERATIVE AI DISCLOSURE: every AI-assisted asset carries provenance in
      metadata (tool, version, prompt class, date); AI disclosure statement
      generated for store pages (e.g., Steam) from that metadata; no training
      on or output imitating identifiable third-party IP/artists; faces of real
      people never generated/used.
135.5 LICENSES.md AUTOMATION: tools/gen_licenses.py builds the file from asset
      metadata; CI fails if a third-party asset lacks license metadata.
135.6 HUMAN-ONLY LEGAL TASKS: EULA acceptances, store compliance, age ratings
      (IARC), trademark search for the title, privacy policy — the LEGAL lane
      prepares drafts and checklists; a human signs.

# SECTION 136 — REPORTING, EVIDENCE & RISK REGISTER

136.1 REPORT FORMAT (every milestone): Headline → Done → Stubbed → Deviations →
      Evidence links → Perf table (tiers) → Size table → Blind Bench table →
      Risks → Asks of the human → Next.
136.2 NEVER-FABRICATE RULE (from 1.4b): a gate is "green" only if its artifact
      exists; partial = amber with numbers; red shows failing cases.
136.3 RISK REGISTER (top 12, update weekly):
   R1  UE5 build environment/licensing access → 133.3 ladder
   R2  Content volume vs quality (too many assets, uneven) → content factory
       validators + Art Lock gates; cut breadth before lowering bar
   R3  Frame-time budgets (Lumen/Nanite/foliage/crowds) → budgets per pass,
       nightly profiling, scalability groups, upscaling
   R4  Motion-capture access/cost → 115.2 ladder, procedural/ML fill, cleanup
   R5  Integration conflicts between 20 lanes → contracts + CI gating
   R6  Shader compile/PSO hitching → 103.12 + warmup + caches
   R7  Streaming hitches at vehicle/aircraft speeds → predictive streaming,
       HLOD tuning, hitch gate
   R8  Physics/destruction cost spikes → budgets (120.6), auto-degrade
   R9  Uncanny faces/animation → Art gates, MetaHuman pipeline, facial QA
   R10 Legal (asset provenance, AI disclosure) → 135 automation
   R11 Scope creep → WBS change control; [SHIP]/[LOVE]/[DREAM] tags (S22)
   R12 Save corruption at scale → 129 + edge matrix + corruption repair
136.4 COMMUNICATION: daily stand-up summary (auto-generated from PRs/tasks),
      weekly "Build of the Week" video (Movie Render Graph) for the owner.

═══════════════════════════════════════════════════════════════════════════
#  PART XVI — THE PRODUCTION LEDGERS (machine-generated, deterministic)
#  Everything below this line is generated by tools/gen_v2/*.py from
#  hand-authored vocabularies + formulas. Rows are binding targets; tune in
#  profiling and log changes in M-reports. Prose wins on conflicts (S0.2).
═══════════════════════════════════════════════════════════════════════════
