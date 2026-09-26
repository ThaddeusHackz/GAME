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
| M4–M13 | not started |

A playable vertical slice exists: `dist/DividedHorizon.exe` boots into the island,
and `--outpost` drops you at the Punta Quemada capture DoD. Every milestone is
gated by headless smoke tests (`tools/build.sh test`, currently **348 checks**)
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
