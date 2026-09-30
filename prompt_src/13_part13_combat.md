
═══════════════════════════════════════════════════════════════════════════
#  PART XIII — COMBAT MASTERY
═══════════════════════════════════════════════════════════════════════════

# SECTION 116 — GUNPLAY: BALLISTICS, RECOIL, FEEL

116.1 BALLISTIC MODEL: hybrid — hitscan with lag-compensated ballistic
      simulation for distances > 60 m: muzzle velocity per ammo (9mm 360 m/s,
      .44 440, 5.56 910, 7.62x51 840, 12ga slug 450, pellets 380 + spread),
      gravity drop, drag (G1 coefficient per ammo), wind deflection (weather
      controller), travel time visible via tracers at 1 of 3 rounds.
      Table: Section 142 (BALLISTIC DROP 25 m steps to 600 m).
116.2 PENETRATION & SURFACE RESPONSE: each impact resolves: surface (Chaos
      physical material, Section 145), angle of incidence (3 bins), ammo
      penetration rating vs material thickness → {stop, penetrate(energy loss),
      ricochet(angle), shatter}; spawns decals, particles, sounds, AI noise
      events, damage to destructibles; penetrates soft cover (wood, drywall,
      foliage) and not hard cover (concrete, engine block). Table: Section 142
      (Weapon × Material × Angle = 1,728 rows).
116.3 RECOIL: layered — (a) authored vertical/horizontal pattern per weapon
      (30-shot patterns in Section 142), (b) procedural noise scaled by
      stance/stamina/movement, (c) camera kick vs weapon kick separation
      (weapon kicks more than view), (d) recovery curves (return-to-aim 65%),
      (e) attachment modifiers. Controller mode: aim assist slowdown within 25%
      recoil deviation.
116.4 SPREAD/BLOOM: first-shot accuracy bonus after 250 ms idle; movement
      spread multipliers (walk ×1.4, sprint-fire ×2.4, crouch ×0.8, ADS ×0.4,
      prone ×0.6); hipfire cone visible by dynamic crosshair (option).
116.5 HIT REGISTRATION: server-style authoritative hit resolution even in
      single-player (for future co-op); hitboxes: 15 bones per humanoid (head,
      neck, chest, stomach, pelvis, 2 upper-arms, 2 forearms, 2 thighs, 2
      calves) mapped to damage zones (head ×2.5/torso ×1/limb ×0.7 per S68,
      neck ×2.0 (OPT)); Chaos physics asset per archetype.
116.6 WEAPON CLASSES & FEEL TARGETS (S68 numbers bind; this section binds the
      FEEL implementation): pistol — snappy, low muzzle rise, crisp slide
      animation; revolver — heavy rise, slow hammer, big muzzle light; SMG —
      high RPM chatter, climb + drift pattern; AR/carbine — balanced; LMG —
      weighty ramp-up; shotgun — chest thump, pump cycle, pellet spread with
      per-pellet decals; DMR/sniper — scope breath, bolt cycle, heavy kick,
      bullet-time-free tracer; bow/crossbow (OPT [LOVE]) — draw tension, arrow
      physics with stick/penetrate; flamethrower — fluid fire, fuel
      gauge. Each has: 6 audio layers (mech, crack, body, tail, distant, foley),
      3 muzzle flash variants, shell physics, barrel heat shimmer, ADS
      animation, inspect animation.
116.7 ATTACHMENTS (25): optics (red dot, holo, 2x, 4x, 8x scope, thermal-
      lite), barrels (suppressor, compensator, long), grips (vertical, angled),
      mags (extended, fast), stocks, lasers, flashlights; stats in Section 142.
      Attachment swaps are visible on the model (modular weapon meshes +
      sockets) and change handling (mass ↔ sway).
116.8 FEEL VERIFICATION: 44.2 impact table + per-weapon "feel sheet": video
      capture of 10 s sustained fire, 5 shots ADS, reload — reviewed against
      rubric (kick readability, sound punch, tracer clarity, hit confirmation).

# SECTION 117 — MELEE, TAKEDOWNS & CONTEXTUAL CLOSE COMBAT

117.1 MELEE: three weapon classes (M01 Cortesía, M02 Llave de Marisol, M03
      Bastón del Sereno per S68) + unarmed; light/heavy swings with
      anticipation/follow-through; melee hit detection via swept hitboxes on
      weapon mesh (per-frame trace in active window); hit-stop (44.2), camera
      punch, stagger states on enemies (light/heavy/knockdown).
117.2 TAKEDOWNS (28 types, Section 142): silent back, silent front, ledge
      (pull-off), water (drown), cover-over-wall, crouch-stealth from foliage,
      vehicle (window), aerial (from zipline/jump), knife throw, dog-assisted
      (Poncho), and 18 weapon-specific brutal/comedic variants; each
      SYNCHRONIZED: motion warping aligns attacker/victim to 1 cm; dual-actor
      animation sets with notifies for blood/foley/cloth; victim ragdoll
      hand-off blend at finish.
117.3 STEALTH ENTRY RULES: takedown eligibility cones (rear ±50°, front 30°
      only when enemy unaware in SUSPICIOUS <0.35 meter), quiet movement
      MM crouch (footstep noise 0.3×), body hide (drag anim + shadow
      volumes), bodies discovered by patrols → alert (S82).
117.4 CLOSE-QUARTERS SHOTGUN/PISTOL: "shove-and-shoot" contextual (E) moves, 
      enemy grab-as-shield (OPT [LOVE], 5 s), weapon-strip disarm.

# SECTION 118 — HIT REACTIONS, RAGDOLL, WOUNDS & GORE OPTIONS

118.1 LAYERS: (a) additive hit flinch by zone+direction (8 directional bins ×
      12 zones in Section 142: HIT REACTION MATRIX), (b) partial ragdoll via
      Physical Animation Component blending spine/limbs by impulse, (c) full
      ragdoll on kill with pre-kill death anim blends (forward/back/left/right/
      falling-from-height/in-water/in-vehicle), (d) stumble MM for heavy
      hits, (e) limp/injury gait after leg hits (injured MM DB).
118.2 RAGDOLL QUALITY: per-archetype physics assets with constraint limits
      tuned (no hyperextended elbows/knees), damping by surface; ragdoll
      settling sleep after 4 s; no jitter gate; stacking bodies behave;
      corpse cap 16 (11.3) with cleanup fade; cloth/gear attached stable.
118.3 WOUNDS: localized decals (entry/exit) on skin/cloth via projected decal
      on skeletal mesh (Substrate), blood types (spray, pool, drip, smear on
      walls); wound persistence on hero; dismemberment NOT in base game;
      GORE REDUCED option (default ON for T0, OFF for T3+ with mature flag).
118.4 REACTIONS TO NON-BULLETS: explosion shove (radius-based impulse,
      ear-ring audio, tinnitus filter on hero), fire panic (run + roll),
      vehicle impact (launch via capsule-to-ragdoll switch at > 18 km/h),
      fall damage (landing MM + ragdoll if > 10 m), drowning, electrocution
      (OPT).
118.5 NPC CIVILIAN REACTIONS: panic, hands-up, hide behind cover, call
      police (phone animation), film with phones, flee along graph, freeze;
      reaction depends on weapon-type and proximity (gunshot 80 dB falloff).

# SECTION 119 — ENEMY AI: SQUADS THAT ACT LIKE PEOPLE

119.1 STACK: StateTree (PATROL, SUSPICIOUS, SEARCH, COMBAT, FLEE, DEAD,
      DOWNED per S82) + EQS for cover/flank/vantage queries + Smart Objects
      for cover/ambush/lean/vault + AI Perception (sight with light/stance
      weighting, hearing with occlusion/propagation, smell (Poncho-era
      animals), damage, team) + Mass for background combatants.
119.2 SQUAD DIRECTOR: assigns roles per encounter: SUPPRESSOR (pins), FLANKER
      (routes via EQS flank graph), RUSHER (aggressive, low morale cost),
      SNIPER/OVERWATCH, BREACHER (grenade flush at 3+ enemies in cover), 
      MEDIC/RALLIER (cult archetype), COMMANDER (calls reinforcements, 2-wave
      cap S82). Roles are re-evaluated every 2 s; no more than 3 enemies fire
      at player simultaneously in "fair mode" (difficulty parameter),
      token system for attack slots.
119.3 HUMAN-LIKE FAILURE: reaction delay 180–450 ms by difficulty; aim error
      cone shrinks over 1.2 s of continuous LOS; enemies miss first shots
      visibly (whizz-by near miss for readability), lose track behind smoke,
      get flustered by flashbangs, hesitate on exposed reloads, run out of ammo,
      call for help, retreat when outgunned (morale 0.2 → FLEE).
119.4 COVER INTELLIGENCE: cover scoring = protection from known threat vectors
      + distance + flank exposure + teammates' crossfire overlap; enemies
      vault, lean, blind-fire, swap cover under suppression, abandon cover
      when grenaded, never stand in front of explosive barrels unless dumb
      archetype.
119.5 PERCEPTION FAIRNESS: detection meter truthful (6.3), rate base/(dist² ·
      light · stance), decay 0.2/s; team share 8 m; LKP honest (S82); foliage
      concealment volume (prone in grass = 0.35× visibility); night vision
      cones; binoculars tagging.
119.6 ARCHETYPES (Part IV faction dossiers bind): GRUNT, BRUISER, OFFICER,
      SNIPER, MEDIC, RUNNER (alarm), BREACHER, HEAVY (LMG), DRIVER, PILOT,
      ELITE (boss escort), CULTIST ZEALOT (melee charge), DOG-HANDLER.
      Encounter grid: Section 142 (AI ENCOUNTER GRID).
119.7 ANIMAL AI: predators stalk (MM crouch-stalk), ambush, flee, herd;
      prey flock/herd with Mass; pet dog Poncho (Part II S24) uses StateTree
      + Smart Objects for sniffing, tagging, distracting, attacking, reviving.
119.8 PERFORMANCE: AI LOD: within 40 m full StateTree at 30 Hz; 40–150 m 10
      Hz; beyond sleep w/ Mass proxy; max 24 fully-active combat AI + 200
      Mass-LOD (S0.7).

# SECTION 120 — FIRE, EXPLOSIONS & DESTRUCTION (THE SIGNATURE CHAOS)

120.1 FIRE PROPAGATION: grid-based ignition model per vegetation/material
      (Section 142 FIRE TABLE: material × wind × wetness): flammability,
      burn time, spread radius, smoke color; rain suppression; fire spreads
      across jungle with wind vector; burnt state swaps (char material,
      collapsed foliage), fire damage to NPC/player (DoT), extinguish via
      water/rain, fire audio (crackle layers by intensity), heat distortion.
120.2 EXPLOSIONS (14 types: grenade, frag, flash, smoke, molotov, C4, rocket,
      vehicle, barrel, fuel depot, gunship missile, mortar, dynamite,
      demolition): overpressure falloff (inverse-square w/ occlusion),
      fragmentation with ray-cast shrapnel, shockwave impulse to Chaos
      bodies, crater decals/landscape deformation (Landscape Patch stamps
      for big blasts), Niagara fireball/smoke (heterogeneous volume on T3+),
      dynamic light + Lumen injection, tinnitus + camera shake (33 safe).
      Table: Section 142 (EXPLOSIVE × ENVIRONMENT).
120.3 DESTRUCTION TIERS: T0 no-destroy; T1 prop-break (crates/barrels/
      glass); T2 fracture (walls/fences/windows; Chaos Geometry Collection,
      max 40 fragments active); T3 structural (shacks, towers, bridges
      sections: scripted sequences + GC); T4 set-piece (Signal Mast fall,
      bridge collapse, stadium section) authored for performance. Each
      destructible has tier ID in Section 142; debris LOD/cleanup at 20 s.
120.4 PHYSICS TOYS (S48 intent): ragdoll launch, explosive barrel chains,
      swinging cranes, rolling barrels, tumbling debris; all sleep-managed.
120.5 VEGETATION INTERACTION: characters/vehicles bend foliage (RVT/
      interaction textures + WPO pushing), cut vines with machete (M-class),
      tall grass hiding (119.5), trees falling from explosions (T3), wind-
      gust animation coupling.
120.6 BUDGET: physics bodies active 1,500; fracture chunks 600; Niagara fluid
      sims 2; auto-degrade at < 55 FPS (fewer chunks, sprites vs volumes).

# SECTION 121 — COMBAT FEEL, DIFFICULTY & FAIRNESS

121.1 DAMAGE MODEL: S68 numbers for weapons; armor reduces by flat+percent;
      player health regen only with food/meds (island mode) and partial regen
      (city mode, per difficulty); death → checkpoint (no lost progress).
121.2 FEEDBACK STACK (44.2 binds): hit-marker (visual+audio variants by zone),
      kill confirm, damage direction indicator, screen edge blood (photosens-
      safe), controller haptics (adaptive triggers), audio mix duck, slow
      micro-freeze (hit-stop), camera punch, enemy reaction visible ≥ 100 ms.
121.3 TTK TABLES: Section 92 (legacy) + Section 142 (hit reaction matrix);
      difficulties: Story, Easy, Normal, Hard, Survival (Permadeath OPT) — scaling:
      enemy damage, aim accuracy, detection speed, reinforcement cap, ammo.
121.4 AIM ASSIST (gamepad): slowdown, friction, magnetism, rotational assist;
      options off by default on mouse; accessibility fine control (33).
121.5 READABILITY: enemy muzzle flashes readable in foliage; threat indicators;
      dynamic music states (S32) tied to alert; sound design localizes
      direction within ±15°; binaural HRTF option.
121.6 COMBAT AUTOMATION: bot-driven arena test (Gauntlet) runs 30-enemy fight
      x 10 loadouts x 3 difficulties nightly; asserts: no NaN damage, no stuck
      AI, TTK within ±10% of table, frame-time gate; output to dashboard.
