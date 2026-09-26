# DIVIDED HORIZON

An original open-world action game in the spirit of Far Cry / GTA — two maps
(**Isla Sombra**, a tropical island; **Meridian City**, a crime-ridden
metropolis), built on the first-party **Horizon Engine** (C99, zero runtime
dependencies).

> **Studio credit:** Built by AI under human direction.
> **Honesty contract:** nothing in this repository is claimed to work unless a
> script in `tools/` runs it and prints the result. See `docs/reports/`.

---

## Status (read this first)

| Milestone | State |
|---|---|
| **M0 — Bootstrap** | ✅ engine core, JSON, settings, saves, software rasterizer, build + PE verification, 85-check smoke test |
| **M1 — Traversal** | ✅ run/jump/vault/mantle/ledge/zipline/swim over a 1 km² streaming island, 69-check traversal smoke |
| **M2 — Combat** | ✅ data-driven weapons, enemy FSM + detection, 30-enemy arena, 112-check combat smoke |
| **M3 — Island Slice** | ✅ outpost capture (stealth/loud/mixed), signal mast, wildlife, day/night, fog-of-war minimap, binoculars, 82-check island smoke |
| **M4 — City Slice** | ✅ Meridian City: 8 blocks + harbor, 5 data-driven vehicles, 15 traffic cars, 30 peds, heat 0-5 with cruisers + heli, odd job, hot-dog economy, ferry between acts, 154-check city smoke |
| **M5 — Systems & Progression** | ✅ XP/levels (100+50(n-1)), 24 skills in 4 trees wired to live systems, 2 data-driven vendors (economy.json), hunting/skinning, herbs, 4 crafting recipes, medkits/armor, island alert level, safehouses (rest+autosave), map screen + fast travel, saves/continue incl. wanted stars, island state persists across the ferry, 93-check systems smoke |
| M5–M13 | not started |
| M6 | Story: 11 data-driven missions, text cards, choices + 2 endings, tracker/waypoints, checkpoints | done (81 checks) |

A playable vertical slice exists: `dist/DividedHorizon.exe` boots into the island,
and `--outpost` drops you at the Punta Quemada capture DoD. Every milestone is
gated by headless smoke tests (`tools/build.sh test`, currently **502 checks**)
and a structurally-verified Win64 cross-build; see `docs/reports/`.

## Build

```bash
tools/fetch_toolchain.sh      # one-time: fetches the zig cross compiler (npm)
tools/build.sh test           # Linux headless build + runs the 85-check smoke test
tools/build.sh crosscheck     # cross-compile the same sources to a Win64 .exe + verify PE
tools/build.sh all            # both
tools/build.sh zip            # (once main.c exists) produce dist/DividedHorizon-*.zip
```

Requires only `gcc` (or zig) + `python3`. No package-manager access needed —
the build host here has none.

## Verify

```bash
tools/build.sh test
```

prints `SMOKE TEST PASSED: 85/85 checks` and writes
`build/smoke/smoke_cube.{bmp,png}` — a real rendered frame from the software
rasterizer, inspectable without a GPU.

## Layout

```
game/src/core/     math, RNG, noise, JSON, logging, settings, saves
game/src/rend/     renderer command list + software rasterizer (+ GL 1.1 backend)
game/src/plat/     platform layers (win32 / headless)   [M0→M1]
game/tests/        smoke tests (headless, no GPU required)
tools/             build driver, toolchain bootstrap, PE verifier
docs/reports/      per-milestone honest status reports
```

## Design constraints that shape the code

- **60 FPS @ 720p Low on integrated graphics** (Spec 15) — budgets are enforced
  against live counters in `RendState`, not aspirational.
- **Always playable** (P5) — no commit may leave the build broken.
- **Zero telemetry, zero DRM, offline-complete** (Spec 35/36).
- **Saves never in the install dir** (Spec 6.11); corrupt saves are quarantined
  and auto-repaired, never silently truncated (Spec 37.4).
- **Accessibility is core, not a toggle afterthought** (Spec 33): photosensitivity,
  colourblind remap, subtitles, remapping and more are first-class settings
  that persist and are covered by the smoke test.

See `MASTER_PROMPT.md` for the full design doctrine (Parts I–X).


## Controls (keyboard + mouse)

| Key | On foot | Driving |
|---|---|---|
| WASD | move | W throttle · S brake/reverse · A/D steer |
| Mouse | look | look around |
| Shift / Ctrl / Space / V | sprint / crouch / jump-vault / roll | Space = handbrake drift |
| F or LMB / C or RMB / R | fire / aim / reload | — |
| 1 2 3 | weapon slots | — |
| E | interact · enter car (carjack) · buy hot dog · job board | exit (below 3 m/s) |
| T / B | takedown / binoculars (hold) | — |
| Q | — | cycle radio (station names; audio arrives in M7) |
| K | Beto's ferry: Isla Sombra <-> Meridian City (not while wanted) | |
| TAB | Map screen + fast travel (UP/DOWN, E) | |
| I | Character: skills / crafting / bag (LEFT/RIGHT, UP/DOWN, E) | |
| H | Use medkit / bandage / armor plate | |
| E | Also: trade at vendors, rest+save at safehouses, skin kills | |
| L | Title screen: continue from the latest autosave | |
| F3 | Perf/debug overlay (was TAB) | |
| G | combat arena (island, dev) | |
| P / Tab / Esc | photo mode / debug overlay / pause | |

Launch flags: `--outpost` (island capture DoD), `--city` (start in Meridian), `--night`, `--arena [n]`, `--headless`.
