# M2 REPORT — COMBAT v1

**Branch:** `arena/01a0d58d-game` · **Date:** 2026-09-26 · **Status:** COMPLETE (all gates green)
**Build:** `DividedHorizon.exe` v0 "M2 Combat" (420 KB PE32+ GUI) + `DividedHorizon-v0-win64.zip` (204 KB, ships `weapons.json` next to the exe)

---

## 1. Definition of Done — result

| DoD (§16, M2) | Result |
|---|---|
| 30-enemy arena playable end-to-end | ✅ `--arena [n]` launch flag + `G` hotkey + `game_arena_start()` API. 30 hostiles (20 grunts / 7 bruisers / 3 officers), cover crates, low walls, two crate-stair platforms, field medkits + ammo. Boots, fights, clears. |
| Kill loop satisfying (pistol/rifle/shotgun) | ✅ Auto-aim bot in `smoke_combat` kills 3 duelists in <1 s of trigger time and **survives at 64/100 HP** — enemies shoot back hard enough to matter, not enough to delete you. Headless demo frames show tracers, hitmarkers, reload bar, corpses. |
| No NaN damage bugs (§18.4) | ✅ NaN distance → 0 damage, NaN falloff → 0, NaN damage rejected by `enemy_apply_damage`, validation rejects NaN balance rows, post-arena NaN sweep over player + all enemies + mags. |
| Grunts PATROL → COMBAT (§82) | ✅ Full FSM incl. SUSPICIOUS/SEARCH/FLEE; inverse-square detection meter, 0.35/0.8 thresholds, 0.2/s decay, ≤8 m team share, gunshot noise through walls ≤40 m, LKP grid search. |
| Damage zones + health pickups + death/respawn | ✅ head ×2.5 / torso ×1.0 / limb ×0.7 bands on the hitscan; medkits +40 HP and ammo boxes as world pickups and kill loot (30/45/25 roll); death → 2.5 s → checkpoint respawn **keeps the whole loadout**, corpses stay dead. |

Test gates: **smoke 85/85 · traversal 69/69 · combat 112/112 = 266 checks**, headless demo SELF-CHECK PASSED, Windows cross-build PE-verified, `crosscheck` green.

## 2. What shipped

| File | Lines | Content |
|---|---|---|
| `game/data/weapons.json` | 38 | Data-driven balance (§10.4). Schema v1: dmg/rpm/mag/reload/recoil/spread/ads/zones/falloff/pellets/ammo/price + `audio` hook reserved for M5. |
| `game/src/combat/weapon.{h,c}` | 292 | Pure ballistics: falloff curve, zone damage, posture spread (ADS ×0.25, crouch ×0.85, move ×1.6), deterministic recoil pattern, rpm cadence, `weapon_valid()` NaN gate, embedded fallback table if the JSON is missing (WARN-logged, Honesty Contract). |
| `game/src/ai/enemy.{h,c}` | 420 | EnemySet ≤64, archetypes (grunt 100 / bruiser 160 / officer 130 HP §65), FSM, perception (LOS = AABB segments + terrain ridge march), burst-fire control with reaction delay + pauses, team share, flee (cartel only; cult fights to the death). |
| `game/src/world/collision.c` (+h) | +70 | `obstacles_ray_hit()` (slab method, normals, tmax) + `obstacles_segment_clear()` — hitscan and AI LOS share one ray path. |
| `game/src/game/game.c` (+~560) | — | Arena builder, loadout/inventory (3 slots + reserve pools), reload FSM, hitscan vs world+enemies with zone bands, recoil = permanent climb + decaying view punch + ADS FOV 78→61°, tracers, impact/blood-lite particles, hitmarkers (red X on kill), damage vignette, pickup bob/collect, enemy corpse/flash rendering, arena-clear flow, combat HUD. |
| `game/src/plat/*` | — | New buttons `BTN_SLOT1..3`, `BTN_ARENA`; Win32 map: **R reload** (was roll), **V roll**, 1/2/3 slots, G arena, F/LMB fire, RMB/C aim (mouse edges already existed). |
| `game/tests/smoke_combat.c` | 579 | 112 checks: §68 number rows, falloff/TTK math, spread posture, recoil determinism, NaN guards, ray/LOS unit tests, FSM transitions, team share, noise alerts, corpse persistence, determinism (same seed ⇒ same AI), full integration (duel, reload conservation, slot switch, dry-fire, death→respawn inventory, pickups, arena clear, perf, render submit, NaN sweep). |
| `tools/diag/arena_demo.txt` | 13 | Headless combat showcase script (used for the M2 screenshots). |

Sim cost with 30 live enemies: **0.01 ms/frame** (sim-only, measured in CI). Render: 741 draw calls / 903 items worst observed frame — inside the §15 Low budget (<900 draws).

## 3. Balance conflict log (§95: prose wins)

The §92 matrix and §68 prose disagree on the Culebra. We implemented **§68 prose** and flag the matrix rows stale:

| Row | §92 matrix says | §68 prose says | Implemented |
|---|---|---|---|
| B-0005/6 (W01 falloff R2/R3) | torso 18 @R2, 12 @R3 (75 %/50 %) | 100 % <25 m → **70 % @50 m**, flat beyond | 16.8 @50 m+ (`falloff a25 b50 pct0.70`) |
| B-0004 (W01 hits-to-kill grunt) | 4 | 24 × 4 = 96 < 100 ⇒ **5 torso shots** | TTK 5 torso / 2 head (asserted in tests) |

Rationale: 4-shot torso TTK on a 100 HP grunt is arithmetically impossible at 24 dmg; the prose row is the self-consistent one, and §95 says prose beats tables on conflict. `weapons.json` carries the same note so future balance passes see the decision.

## 4. Feel decisions (why it's fun, not just correct)

- **Every shot answers you**: tracer line, impact spark or blood-lite puff, hitmarker tick (red X on kill), corpse that stays in the yard, reload progress bar. No shot is silent (§33 readability).
- **Recoil you can learn**: vertical climb partially baked into aim (you correct it), plus a decaying view punch; horizontal wobble is a deterministic sine of the shot index — patterns, not noise.
- **Stance matters**: crouch tightens your cone *and* halves enemy detection; ADS trades 45 % move speed for a 0.25× cone and 61° FOV.
- **Enemies are people-ish**: they patrol, get suspicious, call it in (≤8 m share), search your last seen position, hold preferred ranges, strafe in bursts, and bruisers push to 7 m while officers snipe from 25 m.
- **Death is a setback, not a reset**: loadout and kill count persist; only *your* position resets. Dead hostiles stay dead — the arena remembers.

## 5. Controls (M2 delta)

| Action | Key |
|---|---|
| Fire | F or LMB (full-auto cadence = weapon rpm) |
| Aim | C or RMB |
| Reload | **R** (changed from roll) |
| Roll | **V** (changed from R) |
| Weapon slots 1/2/3 | 1 / 2 / 3 |
| Deploy to combat arena | G (or `--arena [n]`) |

## 6. Honest limitations (carried, not hidden)

- **No audio yet** — weapon/pain/impact events exist only as log + particle feedback; `weapons.json` reserves `audio` ids for the M5 audio pass. The kill loop is *visually* loud and aurally silent.
- **Enemies are graybox capsules** (box + head + facing nub, faction/state tinted). Readable, not pretty; M7 art pass replaces them with stylized meshes on the same struct.
- **Lighting is constant golden hour** — `EnemyView.light = 1.0`. Night/stealth modifiers from §82 are implemented in the formula but never vary until the M3 day/night cycle lands.
- **Melee, grenades, stealth takedowns, vehicles** untouched — later milestones.
- **The .exe still has never run on real Windows hardware** (no Windows/GPU in sandbox). Structural verification only: PE32+ GUI, imports = gdi32/opengl32/user32/kernel32/msvcrt. First real-hardware boot remains a hard gate — now with `--headless --arena 30` available on the shipped binary as the no-GPU self-test.

## 7. Evidence

- `build/smoke/m2_arena_*.png` — 4 showcase frames (first-kill HUD at t=0.9 s shows `HOSTILES 5 / 6`, reload bar mid-mag, enemies + cover in the yard, damage vignette).
- CI log lines: `arena cleared: 3 kills`, `player survived the duel (hp 64)`, `sim with 30 enemies costs 0.01 ms/frame`, `renderer submitted 903 items / 741 draws`.

## 8. Next: M3 — Island Slice

Outpost capture loop (12 outposts start here), day/night + weather feeding the §82 light term, stealth crouch-vision cones, first 3 story missions on Isla Sombra, Poncho prototype. Combat v2 backlog from this milestone: enemy flanking, suppressive-fire accuracy decay, gib-free death animation states, weapon-sway/bob, audio.
