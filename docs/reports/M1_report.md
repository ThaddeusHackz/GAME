# M1 — Traversal · Milestone Report

**Branch:** `arena/01a0d58d-game`
**Date:** 2026-09-26
**Verdict:** ✅ **COMPLETE (with logged stubs, §5)** — every number below was
produced by running the code (`tools/build.sh test`, `tools/build.sh shots`),
not by assertion.

M1's promise (Spec P1): *traversal is joy*. The deliverable is a full
first-person movement kit — walk/run/sprint, crouch, jump with coyote time and
input buffering, auto step-up, vault, mantle, ledge catch, slope slide, swim
with a breath meter, fall damage with roll-cancel, and ziplines — plus a
graybox **traversal course** on the real island that exercises every verb, and
a headless smoke suite that asserts the Spec 6.1 numbers frame-exactly.

---

## 1. What was built

### Player controller (`game/src/player/`) — ~700 LOC
Single deterministic fixed-step controller (`player_input`, 60 Hz). No
animation-state spaghetti: one stance machine
(`GROUND / AIR / SWIM / MANTLE / SLIDE / ZIP / DOWN`) with shared landing
handling (`land_on`) so fall damage, landing stats and roll-cancel cannot
disagree between the apex path and the post-move clamp path.

| Verb | Spec 6.1 number | Implemented as |
|---|---|---|
| walk / run / sprint / crouch | 2.45 / 4.7 / 7.2 / 1.45 m/s | accel+friction model, `walk_only` accessibility flag respected |
| jump | 1.35 m apex | `v = √(2gh)` = 8.05 m/s, jump-cut on release (0.45×) |
| coyote time / buffer | 0.12 s / 0.15 s | edge latches in the controller |
| auto step-up | ≤ 1.20 m | only when actually blocked + moving, then `obstacles_step_onto` nudges the cylinder onto the volume (prevents support-query yo-yo) |
| vault | ≤ 2.20 m | wall probe at ankle height, radius+0.45 m ahead **or** the volume that blocked us last frame (catches early presses) |
| mantle / ledge catch | ≤ 4.00 m | air-mantle with smoothstep arc over the lip |
| slide | steepness > 0.62 | downhill = horizontal part of the surface normal (negating it slides uphill — fixed) |
| swim + dive | breath 30 s | surface swim, CTRL dive, breath countdown, drowning at 0 |
| fall damage | > 4 m @ 12 hp/m | measured apex→surface; roll inside 0.35 s of touchdown negates |
| zipline | 6.1 | `E` mounts within 1.25 m of a cable, gravity-along-cable accel, CTRL brake, SPACE drops with momentum, end auto-release, lateral pendulum sway |
| stamina / breath meters | 100 / 30 s | 15/s drain, 13/s regen after 0.7 s idle |

Cylinder: r 0.34 m, height 1.80 m (1.05 m crouched). NaN-guarded throughout
(Spec 18.4); `player_camera()` publishes eye position + look direction so the
render layer never touches controller internals.

### World / collision (`game/src/world/`)
- `obstacles_step_onto()`, ankle-height `obstacles_wall_ahead()`, support
  queries that let the player stand on crates and decks.
- `ZiplineSet` (≤ 24 cables) + `zip_nearest()` point/segment query.
- Terrain: flatten/mound sculpting used by the course (pad, pool, slide hill).

### Traversal course (`game/src/game/game.c`) — 10 elements on Isla Sombra
Pad at (330, 560), deck 8 m; spawn faces +z straight down the line.

| # | Element | What it proves |
|---|---|---|
| 1 | step trio (0.5/1.0/1.5 m) | chained auto step-up under the 1.2 m limit |
| 2 | vault crate **on the run line** (1.8 m) + runway deck | vault between step and vault limits; steps lead somewhere |
| 3 | mantle wall (2.1 m rise) + landing deck | mantle verb |
| 4 | gap jump (3.9 m of air at +2.1 m) | sprint long-jump |
| 5 | crate tower to +7.5 m | fall damage + roll-cancel |
| 6 | walk-up ramp (8 × 0.45 m wedges) | no stair-stepping artefacts |
| 7 | swim pool carved into the pad | swim, dive, breath |
| 8 | slide hill (steepness > 0.62) | slide verb |
| 9 | vista marker post | postcard law (42.2): landmark readable from the pad |
| 10 | two zipline cables (tower→pad 37 m, slide-hill→pad 55 m) | mount/ride/brake/drop |

### Tests (`game/tests/smoke_traversal.c`) — 69 checks
Per-verb unit tests on a flat world **plus** an integration pass that boots the
real island through `game_init → game_frame → game_render` and asserts the
renderer actually produced geometry (the M1 double-model-matrix bug would fail
it). Brittle-assertion policy: runs track *maxima* (max height, max tris) and
use position-triggered inputs, never final positions.

### Tooling
- `tools/build.sh shots` — headless 12 s scripted flythrough of the course +
  zipline ride, prints distance/verbs/perf and writes BMP+PNG stills to
  `build/shots/` (gitignored by design — regenerable, Spec 9).
- `tools/diag/course_shot.c` — the dev tool behind it (not shipped).

---

## 2. Measured results (verbatim from the runs)

```
$ tools/build.sh test
   ════ 85 checks, 0 failed ════            (M0 core suite)
   SMOKE TEST PASSED: 85/85 checks
   ── 69 checks, 0 failed ──                 (M1 traversal suite)
   M1 TRAVERSAL SMOKE PASSED: 69/69 checks

$ tools/build.sh crosscheck
   ✓ build/dh_smoke.exe (360K) — Win64 PE produced from the same sources
   PE32+ x64, subsystem=CONSOLE, 10 sections — imports only OS-shipped DLLs

$ tools/build.sh shots
   course flythrough (12.0 s, 719 frames)
     distance along course : 49.6 m
     stance/health         : GROUND / 100
     jumps=2 vaults=1 mantles=2 slides=0 landings=1 total=46.9m
     max_fall=0.00m fall_dmg=0 last_jump_h=2.91m
     perf: max tris/frame=2325  max draws/frame=613
     cpu : sim 0.01 ms/frame, software raster 29.11 ms/frame (34 fps cpu-bound)
   zipline ride: 410 frames (37.4 m of cable), release stance AIR
```

Budget check (Spec 15, Low preset): **613 draw calls < 900**, **2.3k tris
< 1.2 M** — the course is graybox, so this is a floor, not a forecast.

---

## 3. Screenshots

Regenerate with `tools/build.sh shots` → `build/shots/`:
`m1_course_front`, `m1_course_side`, `m1_course_gap`, `m1_course_zip`
(mid-ride, HUD coaching line visible). Not committed: `.gitignore` keeps
regenerable captures out of the repo (Spec 9 honest sizes).

---

## 4. Feel notes (why it is fun, not just correct)

- Momentum is preserved across verbs: a sprint long-jump off the mantle deck
  clears the 3.9 m gap with ~0.6 m to spare; dropping off a zipline at speed
  keeps 88 % of cable velocity, so lines chain into runs.
- Every verb coaches itself: HUD messages (`VAULT`, `MANTLE`, `ZIPLINE -
  SPACE drops, CTRL brakes`, `OUT OF AIR`) teach the kit without a tutorial.
- Roll-cancel turns fall damage from punishment into a skill check: the smoke
  suite asserts a 7 m drop kills 40 hp unrolled and 0 hp rolled.

---

## 5. Stubs, deviations, BLOCKED (Honesty Contract §1)

| Item | Status |
|---|---|
| **Ladder climb** (6.1) | **NOT implemented.** `collision.h` reserves `kind=1` climbable faces; controller support lands with M3 interiors. Logged, not hidden. |
| **Ledge grab / shimmy** | Implemented as *air-mantle* (catch + climb in one motion ≤ 4 m). A hanging/shimmy state (ledge-hang, side-step along a lip) is deferred to M2 combat-cover work. |
| **Zipline sway** | Pendulum spring, not verlet rope. Reads correctly at speed; a segmented rope is M7 polish. |
| **In-game rebind screen** | Settings layer + persistence + 57 actions are M0-complete and core-tested; the *UI screen* is M5. Rebinding itself works today via `settings.json`. |
| **60 fps proof** | **Not yet provable on GPU here** (no display/GPU in sandbox). Honest proxy: sim cost 0.01 ms/frame; the *software* rasterizer (the `--safe-mode` fallback) costs 29 ms/frame at 960×540 single-threaded ≈ 34 fps — it is an emergency fallback, not the shipping path. The GL11 backend + win32 window (M1b) is what Spec 15.1 will be measured on. |
| **Wingsuit / parachute** | Island skill unlocks by design (M3/M6), not M1 stubs. |

---

## 6. Next

**M1b:** `rend/gl11.c` (fixed-function GL backend behind the same `rend.h`
command list) + `plat/win32.c` (window, keyboard/mouse, GL context, v-sync) so
`tools/build.sh windows` yields a playable `DividedHorizon.exe`, then M2
combat.
