# M17 — Boss B4: El Fraile

**Status:** DONE. 16 suites / 983 checks green (smoke_fraile 27/27). Win64 PE verified; zip rebuilt.

## What was built
- **Trigger:** a bell rope in the combat yard's north-east corner (arena +24,-38). Press [E] to start the fight, or a rematch.
- **P1 SILENT:** he never speaks. A bell rings every 0.8 s. On the **third ring** a 0.5 s parry window opens (HUD: `THIRD BELL - [T] PARRY NOW!`, and three bell markers over his head light up gold).
  - Parry with **T** within 4.5 m and he staggers for 3 s, taking full damage.
  - Miss it inside 4.5 m and you take 20 damage. At range the swing whiffs.
  - Outside a stagger he shrugs off 95% of damage.
- **P2 TOLL (<66%):** the rings speed up to 0.55 s and the window shrinks to 0.35 s. Two acolytes join.
- **P3 LAST BELL (<33%):** every 8 s a chain wrap drags you toward him from up to 14 m.
- **Mercy:** at 15% health he kneels. [E] within 3 m **subdues** (spares) him, or you can strike him down.
  - Spared: "El Fraile kneels, and rings the bell once. Softly." No line from him. +10 stature, and clean-mask bit 3 is set.
  - Struck: "The bell rope swings, unrung." +5 stature.
- **Feat LA TERCER CAMPANA:** spare El Fraile. That makes 30 feats. The Herald reports the fight under the name EL FRAILE.
- Saved through the existing boss flag (beaten | clean_mask<<8).

## Honest deviations
- He is fought in the combat yard, not a bell tower.
- The P2 smoke and noise-decoy mechanic was **not built**. Two acolytes pressure you instead.
- Bells are a pitched synth tone (the JINGLE SFX at a lower pitch), not a sampled bell.
- The bruiser mesh is reused, and the bell markers are boxes.
- Bosses 5 and 6 are still not built. Slot 4 refuses to start, and the tests check this.

## Screens
`img/M17_fraile_parry.png`, `img/M17_fraile_kneel.png` (headless soft-render; washed-out colours are a known capture artefact).
