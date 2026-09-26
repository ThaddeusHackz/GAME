# M6 — Story & Content (report)

## Delivered
- **Data-driven mission system** (`game/src/meta/mission.c/.h`): a pure state machine that takes a `MissionCtx` snapshot every frame. There are 18 objective kinds, all built on mechanics that already work: card, choice, goto, herbs, hunt, craft, buy, rest, kills, capture, mast, ferry, drive, heat, lose_heat, survive, job, assault.
- **The campaign** (`game/data/missions.json`): 11 main missions, M01 LANDFALL to M11 HIGH LIGHTHOUSE, with 49 objectives and 17 text cards. It crosses both maps twice on Beto's ferry. There are 3 player choices, and the final one leads to **2 endings** (OPEN HORIZON / STILL WATER). All story content is original.
- **Embedded fallback:** `tools/gen_embed.py` bakes the JSON into `missions_embed.h`. If the data file is missing, or fails to parse into any missions, the game uses this embedded copy of the campaign (tested).
- **Screens:** a modal text-card screen (E to page, 1/2 or UP/DOWN+E to choose), an objective tracker (top-left), a mission banner, a 3D waypoint with distance, an arrow at the screen edge when the target is off-screen, and a minimap waypoint clamped to the edge.
- **Checkpoints:** when you die, you retry the current objective (its counter resets and the survive clock restarts). Saves store the campaign position, done mask, choices, ending and counters. Loading resumes at the start of the saved objective.
- Rewards are paid on each mission's completion ($ and XP). The story runs in the windowed game by default. `--story` / `--no-story` override this, and headless, DoD and test modes stay in sandbox mode.
- `missions.json` now ships in the zip.

## Verification
- New `tests/smoke_missions.c`: **81/81**. It checks the data, the rules, and plays the **whole campaign through `game_frame`**, including a save/load round-trip in the middle and the assault spawning 14 hostiles.
- The earlier suites are unchanged: **676/676 total** (85+69+112+82+154+93+81).
- Windows exe: 712 KB PE verified. Zip: 320 KB.

## Shortcuts in the test (it does not play these steps for real)
The live campaign test sets these states directly instead of playing them: the mast sync, shooting an animal, 3 kills, the outpost capture, completing the odd job, one purchase in M07, and entering a car. The M3/M4/M5 test suites already cover those actions played for real.

## Honest deviations / gaps
- Cutscenes are **text cards only**. There is no voice, no camera direction and no animation.
- It is roughly a 45–90 minute campaign, not 8–12 hours. There is still **1 outpost and 1 mast**, not 12 outposts and 3 strongholds.
- **No boss fights.** The finale is a 14-hostile wave assault at the lighthouse yard, which reuses the combat arena. The companion dog Poncho and the side content are not built yet.
- Choices only change dialogue and which ending you get. The world does not change based on them.
- Starting the assault teleports you to the yard's south gate.
- **Text rendering:** the headless (software) renderer shows underscore-like marks between some words at 640×360. This also happens in older HUD text, so it is not new. It is logged for M7.
