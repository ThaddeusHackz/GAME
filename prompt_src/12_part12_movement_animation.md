
═══════════════════════════════════════════════════════════════════════════
#  PART XII — MOVEMENT & ANIMATION MASTERY
═══════════════════════════════════════════════════════════════════════════

# SECTION 111 — LOCOMOTION (THE FEEL OF BEING THERE)

111.1 ARCHITECTURE: Mover/CMC-driven capsule with predicted root motion blend;
      animation = Motion Matching (Pose Search) as the base locomotion layer,
      then layered: Stride/Orientation/Slope Warping → Foot IK (two-bone +
      ground-normal alignment + pelvis offset) → Control Rig procedural layer
      (breathing, weight shift, hand IK) → additive hit/recoil/lean layers →
      Physical Animation (Chaos) for gear/ragdoll blend. Inertialization on all
      transitions (no crossfade pops, sub-frame-latency responses).
111.2 MOTION MATCHING DATABASES (Pose Search schemas; capture-class data):
      - Unarmed/relaxed, pistol-ready, rifle-ready, heavy-ready, stealth
        (crouch), injured/limp, swim-surface, swim-dive, wade, slope-heavy,
        stairs (up/down, 3 riser heights), ladder, rope, cover-left/right
        shuffle, vehicle-exit transitions, carrying (body/crate), dog-lead.
      - Coverage per DB: 16 directions × speeds (idle, 0.5, 1, 1.5, 2, 3, 4,
        5, 6 m/s) × turns (in-place 45/90/135/180), starts/stops/pivots,
        ≥ 25 min of clean capture per major DB; total MM data ≥ 4 h.
      - Sources: performance capture (human-supplied studio or optical/inertial
        suit), licensed mocap libraries (verified license), video-to-mocap
        (monocular) with manual cleanup, Epic's free Game Animation Sample
        Project as a technical reference ONLY when license allows use.
        Every clip passes the ANIM VALIDATOR (foot-contact labels, root drift,
        pop test, loop test). Matrix of clips: Section 141.
111.3 SPEEDS (binding): walk 1.6 m/s · jog 3.4 · sprint 6.2 (stamina) ·
      crouch-walk 1.3 · prone-crawl 0.8 (OPT) · swim surface 1.8 · dive 1.4 ·
      wade 1.0–1.6 by depth · ladder 1.1 · mantle auto speed varies by height.
      Accel/decel profiles via curves; turn-in-place at > 70° aim offset.
111.4 FOOT-SLIDE & CONTACT: measured, gated — avg slide <= 1.0 cm/step, max
      <= 3 cm, at every speed/direction/slope in the validator sweep; foot
      lock during pivot; toe-bend IK; heel-strike gait dust/particles by surface.
111.5 CAMERA: first-person (Acts I, III combat) uses full-body awareness with
      head-bob/sway tuned by surface and speed, weapon-inertia lag, breath
      sway, sprint cant, landing impact dip (spring-damper, variable
      amplitude by fall height), and strafing lean; third-person (city
      driving/on-foot option, cover) uses over-shoulder spring-arm with
      collision probe, FOV kick, look-ahead, aim-offset. FP⇄TP switch is
      seamless (no teleport). Camera shake via Camera Modifiers with
      photosens-safe alternates (33).
111.6 LATENCY: input sampled at render-start (late-latching), Reflex/Anti-Lag
      supported; input→photon gate measured with LDAT-class rig or in-engine
      markers: <= 50 ms at 60 FPS, <= 30 ms at 120 FPS on T3/T5.
111.7 STAMINA & BREATH: sprint drains 100 over 9 s, regen 1.8 s delay then
      14/s; breath hold (aim) 6 s; swim breath 40 s dive; HUD diegetic cue
      (breathing audio, vignette) before UI bar.
111.8 FALL & IMPACT: fall damage > 6 m; soft-landing roll (input window 180
      ms) cancels damage up to 10 m; landing MM transitions by speed;
      physics-driven camera dip; dust/foliage displacement on impact.
111.9 NETWORK-READY DISCIPLINE: movement code deterministic at fixed sub-step
      (10.6), so future co-op is not blocked even though not shipping now.

# SECTION 112 — TRAVERSAL: CLIMB, VAULT, MANTLE, SWIM, ZIP, GLIDE

112.1 CONTEXTUAL TRAVERSAL: ledge/obstacle probing (multi-trace +
      sphere sweeps + heightfield queries) classifies: step-up (<0.5 m),
      vault (0.5–1.2 m), mantle (1.2–2.2 m), hang-climb (2.2–3.2 m with
      hang), wall-run-lite (OPT), ladder, pipe-climb, rope-climb, cliff-climb
      (marked climbable surfaces), rappel (island cliffs), window-enter (city).
      Each uses MOTION WARPING to land hands/feet on the detected geometry with
      ≤ 2 cm error; animation sets in Section 141 (traversal clip ledger).
112.2 SWIM & WATER: surface swim, dive, underwater movement (6-DOF), surfacing
      with breath, water entry/exit animations across 12 heights, buoyancy,
      wading drag, hands-touch-water IK, swim wake via RT; boat board/leave.
112.3 ZIPLINE: verlet-sway cable (48), rider hang IK, speed physics with
      slope, dismount on timer/impact; 14 ziplines on island; city cranes (OPT).
112.4 GLIDE/WINGSUIT-LITE (Act I): launch from mast/cliff, lift/drag model with
      tunable glide ratio 3:1, turn banking animation, camera FOV/roll, landing
      flare; cloth wind-coupled; 6 designed glide corridors.
112.5 PARKOUR FLOW (city): sprint-vault chain with momentum preservation;
      slide (crouch-sprint, 1.2 s, gunfire-enabled), roll, wall-jump NOT
      included; stairs/rooftop route validation via nav links; "flow score"
      telemetry in tests (no dead-stops in standard routes).
112.6 CONTEXTUAL INTERACTIONS: doors (physics/animated), windows, gates,
      crates, levers, ATMs, vendors, vehicles, dog petting, fishing — each
      has approach MM + motion-warped interaction clip; interaction prompt
      within 0.25 s of eligibility; no stuck states (QA grid Section 141/148).
112.7 COVER: Smart-Object cover nodes (low/high/corner/window), auto-snap
      when sprinting into cover, blind-fire (inaccurate), lean-peek with
      camera, hand IK to surface, vault-over-cover.

# SECTION 113 — FIRST-PERSON BODY & WEAPON HANDLING RIG

113.1 FULL-BODY FP: the player body is one skeletal mesh; FP camera on head
      bone with neck blending; arms rendered via FP-FOV material (separate
      FOV param) so weapons don't distort at 110° FOV; shadow cast by full
      body in VSM; look-down shows legs + feet; vehicle seats show full body.
113.2 WEAPON RIG: per-weapon Control Rig: hand IK (left support hand placement
      per weapon socket), trigger finger curl, magwell hand-to-magazine IK,
      charging handle animation, bolt/slide physics (moving parts driven by
      state machine + springs), safety, shell ejection socket, ADS alignment
      (sight picture centered on reticle with < 0.5 mm error), sight rail
      compatibility for attachments.
113.3 WEAPON SWAY/INERTIA: weapon lags camera with spring-damper (mass-based
      per weapon), sway from breathing (sinusoid + noise), stamina-shaken
      when tired, walking cycle bob synced to footfalls, sprint low-ready
      pose, weapon collision avoidance (raise muzzle near walls: 0.6 m probe).
113.4 RELOAD SYSTEM: tactical vs empty reloads, cancelable phases with
      notifies (mag out / new mag in / bolt release), procedural magazine
      physics (drop mag with Chaos), shotgun shell-by-shell interruptible,
      revolver speedloader, ammo count diegetic (mag weight, HUD optional).
113.5 CLOTHING & GEAR: holsters, slings, straps animated; when sprinting gear
      bounces (physical animation); poncho cloth sim reacts to ADS/aim.
113.6 ANIMATION NOTIFIES: standardized notify set for gameplay/audio/VFX:
      Footstep(L/R,Surface), Foley(Cloth/Gear), ShellEject, MagOut, MagIn,
      BoltRelease, HandOnSurface, Impact, Grunt(type), Breath, Whoosh,
      MotionWarpWindow, SoundDuck, CameraShake(id), RumblePattern(id).
      Validator ensures required notifies exist in every clip class.

# SECTION 114 — VEHICLES: HANDLING, DAMAGE, SOUND

114.1 MODEL: Chaos Vehicles with custom extensions: multi-sample tire model
      (brush-tire: slip-ratio/slip-angle curves per surface × wetness),
      anti-roll bars, limited-slip diff, turbo/boost, automatic gearbox with
      shift logic, engine torque curves per vehicle (12 vehicles, Part V/S69
      numbers bind), aerodynamic drag/downforce, weight transfer, ABS/TCS
      assists (toggle), handbrake drift with sideslip thresholds (S69: drift
      when slip > 0.35), arcade-to-sim slider (difficulty option).
114.2 BOATS/JET-SKI: buoyancy multi-point, hydrodynamic drag, planing, wake
      VFX, propeller cavitation audio, throttle/trim; waves affect handling.
114.3 AIRCRAFT (Act III optional [LOVE]): light plane + helicopter with
      simplified flight model; gunship boss uses scripted path + physics.
114.4 DAMAGE: zone-based: 24 zones per car (Section 143 ledger); deformation
      via mesh morph/vertex-shader crumple + part detach (hood, doors, bumpers,
      mirrors, wheels) via Chaos Geometry Collection or swap-to-damaged SMs;
      glass shatter stages (3); tire burst; fluid leaks; engine smoke →
      fire → explosion (S69 thresholds); damage affects handling (alignment
      pull, power loss, steering slop).
114.5 SOUND: MetaSounds engine model: RPM-layered granular loops (16 layers),
      load/throttle crossfade, turbo spool/whine, gear-shift pops, tire squeal
      per surface, wind noise by speed, interior/exterior filter, collision
      layers by material, horn, radio (S32/S78), doppler on passes.
114.6 CAMERA & FEEL: chase camera with speed-dependent FOV/distance, look-
      behind, cockpit view with full interior model; steering wheel IK with
      hand-over-hand animation; camera shake from road surface; body roll.
114.7 ENTER/EXIT: walk-up MM + door open (motion-warped), carjack variants (4),
      NPC pull-out, passengers, seat shuffle, lean-out drive-by (aim with
      weapon IK), vehicle-weapon mounts on select V11/V12.
114.8 TRAFFIC AI: Mass Traffic with lane-following, signal compliance,
      overtaking, yielding to sirens, panic reaction, parked-vehicle doors,
      collisions with consequences (minor fender-bender chatter + insurance
      comedy hooks).

# SECTION 115 — ANIMATION PRODUCTION PIPELINE

115.1 SKELETON STANDARD: one master humanoid skeleton (UE5 Mannequin-compatible
      naming) for player/NPCs; IK Rig + Retargeter for all sources; twist/
      corrective bones; fingers full; face via MetaHuman facial rig; animals
      have species skeletons + retarget families (quad-small/quad-large/bird).
115.2 CAPTURE STRATEGY (priority order): (1) human-supplied professional
      mocap (if the owner has studio access) — locomotion/combat/takedowns/
      cinematics; (2) licensed mocap libraries (verify redistribution in
      shipped games); (3) inertial suit capture (ownership by the project);
      (4) monocular video-to-mocap + manual cleanup for long-tail clips;
      (5) procedural/ML-synthesized motion (diffusion motion models) for
      variations — always validated by the same gates and cleaned by animators.
115.3 CLEANUP & QA: automatic: foot-contact detection, sliding score, jitter
      (jerk) threshold, self-penetration test, loop seam test, pose pop test,
      root-motion drift, retarget reach test; manual: 20% sample per batch.
115.4 ANIMATION DATA LEDGER: every clip has an ID (ANM-xxxx), class, frames,
      fps (30 source → 60 runtime resample), notifies, compression (ACL),
      retarget tags, memory budget: total animation memory <= 600 MB (T3)
      via ACL + curve compression; streaming for cinematics.
115.5 CINEMATIC CAPTURE: Sequencer + Live Link; hero scenes use face+body
      capture; crowd/background via MM-driven actors; Movie Render Graph for
      trailers/in-engine cutscenes at 24/30 fps with motion blur and DoF.
115.6 EMOTE/GESTURE SET: 120 conversational gestures × 5 intensity layers as
      additive clips + procedural head/eye saccade; radio/phone/walkie hand
      poses; 60 emotional facial states × 12 visemes blend (Section 141).
115.7 ML ASSISTS (allowed, optional): ML Deformer for muscle/cloth corrective
      shapes on hero characters; learned foot-IK refinement; motion in-betweening
      for transitions; all ML assets version-locked with training data
      provenance in docs/adr.
