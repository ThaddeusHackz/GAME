# M0 — Bootstrap · Milestone Report

**Branch:** `arena/01a0d58d-game`
**Date:** 2026-09-25
**Verdict:** ✅ **COMPLETE** — every claim below was produced by running the
code, not by assertion.

---

## 1. What was built

### Engine core (`game/src/core/`) — ~2,400 LOC
| File | Contents |
|---|---|
| `dh_types.h` / `dh_math.c` | Vec/Mat (column-major, GL-compatible), `m4_look_at`, `m4_perspective`, `m4_trs` (YXZ FPS order), rigid inverse; mulberry32 RNG (Spec 82 seed law); value-noise + fBm + ridge; AABB/plane/ray primitives; Gribb–Hartmann frustum; pooled `DynArray`; NaN guards (`v3_valid`/`v3_safe`, Spec 18.4) |
| `dh_log.[ch]` | Rolling crash log with human-readable recent-action ring (Spec 37.4); monotonic clock; atomic file writes; `user://` resolution (Spec 6.11) |
| `dh_json.[ch]` | Original DOM JSON parser (comments + `\uXXXX`→UTF-8 allowed) + streaming writer. No third-party JSON linked |
| `settings.[ch]` | SettingsManager: quality presets (Spec 15.3), full accessibility block (Spec 33), 57 rebindable actions with hold/toggle, 13 languages incl. RTL flag (Spec 34), 3-mutator cap with NG+ lockout (Spec 29.2), first-run GPU recommendation (Spec 37.2), clamped load/save with corrupt-file self-repair |
| `save.[ch]` | SaveManager: 3 auto + 10 manual slots, JSON, atomic writes, corrupt→quarantine, header-only slot listing, playthrough-id stability (Spec 10.5), full stats block incl. `poncho_pets` (Spec 10.10) |

### Renderer (`game/src/rend/`) — ~1,600 LOC
- **`rend.[ch]`** — backend-agnostic command list (`RenderItem`), texture & mesh
  registries with mip generation, frustum + distance culling, opaque→transparent
  sort, **BMP and PNG writers** (PNG via zlib *stored* blocks with in-file
  CRC-32/Adler-32 — no third-party codec), colourblind + brightness post pass.
- **`soft.c`** — complete CPU rasterizer: perspective-correct interpolation,
  z-buffer, per-triangle mip selection, wrap lighting, hemisphere ambient,
  dynamic point lights, smoothstep fog, alpha blend + alpha test, scissored 2D
  quads for UI, camera-facing particle billboards, Bresenham 3D lines.

### Tooling (`tools/`)
- `build.sh` — `test` / `crosscheck` / `windows` / `all` / `zip` targets.
- `fetch_toolchain.sh` — fetches the zig cross compiler from the npm registry
  (the only reachable distribution channel from this host).
- `verify_pe.py` — structural verifier for the delivered `.exe` (PE32+, GUI
  subsystem, **imports restricted to OS-shipped DLLs**).

### Tests (`game/tests/smoke_core.c`) — 85 assertions
Math · JSON round-trip + malformed rejection · settings presets/clamps/mutator
cap/persistence/corrupt-repair · save round-trip incl. flags, missions,
outposts, collectibles, stats, playthrough id · slot headers · corrupt-save
quarantine · rasterizer (draw calls, triangles, pixel diff, colour variance,
z-buffer) · BMP/PNG emission · frustum culling · post-process identity &
saturation · colourblind remap · winding self-calibration.

---

## 2. Verified results (actual output)

```
SMOKE TEST PASSED: 85/85 checks
```
- Software frame 320×180: **8 triangles rasterized, 7,322 px differ from the
  clear colour, 64 distinct colours sampled, 7,322 z-buffer writes** — i.e. a
  real shaded image, not a flat buffer. The frame was visually inspected
  (`build/smoke/smoke_cube.png`): correct perspective, two visible faces with
  distinct Lambert response to the sun direction, texture mapped without
  swim.
- Save serialization: **3,237 bytes** of JSON for a fully-populated save.
- Cross-compile of the *same sources*: **PE32+ x64, 367,104 bytes**,
  subsystem CONSOLE, imports `kernel32.dll, msvcrt.dll` — verified by
  `verify_pe.py`.

## 3. Bugs found & fixed by building (not by reading)

1. **Invisible-world footgun.** A winding-agnostic backface test culled *every*
   triangle of a correctly-authored but CW-wound mesh. Fixed by making
   `mesh_finalize()` **vote on the winding convention** and store it on the
   mesh, so any consistently-wound procedural mesh renders regardless of which
   convention its generator used. This matters because 100% of this game's
   meshes are generated in code.
2. **`rumble_strength` declared `int`** while assigned `0.7f` — the player's
   chosen rumble strength would have silently truncated to 0 forever. gcc
   accepted it; clang (via zig) flagged it. Fixed to `float`.
3. **`verify_pe.py` read the export directory as the import directory** and
   `OriginalFirstThunk` as `Name` — it reported `imports: (none)`, which would
   have let a broken binary pass as good. Both offsets fixed; the verifier now
   fails loudly on non-universal imports.
4. **Post-pass false alarm.** An early "white frame" looked like a rasterizer
   failure; it was the accessibility post pass faithfully applying
   brightness=2.0 left over from the clamping test. Kept as an explicit
   assertion so the behaviour stays intentional.

## 4. BLOCKED / limitations (stated plainly, Spec 19)

- **The `.exe` cannot be executed here.** The sandbox has no Windows. It is
  verified *structurally* only (PE magic, machine, subsystem, import table).
  Running `build/dh_smoke.exe` on any x64 Windows machine is the remaining
  confirmation step; it should print the same 85/85.
- **No GPU or display server.** The software rasterizer is the only visual
  verification path until M1 adds a windowed GL context on Windows.
- **No package manager / CDNs / GitHub release assets.** zig-via-npm is the
  only cross toolchain obtainable; documented in `tools/fetch_toolchain.sh`.
- **The toolchain lives outside the workspace** (`~/.toolchain`), so it does
  not persist between sessions. Re-run `tools/fetch_toolchain.sh` (~7 s) after
  a fresh checkout.
- **No playable game yet.** M0 is the foundation; gameplay systems land from M1.

## 5. Next up — M1 (Traversal)

Player capsule controller (run/sprint/jump/crouch/vault/mantle/climb/swim),
procedural island terrain + heightfield collision, camera (FPS/TPS/third-person
vehicle), procedural texture set, GL 1.1 backend on Windows + Win32
window/input, `main.c` game loop with menu state. Acceptance: walk the island
coast-to-coast at 60 FPS @ 720p Low on the software budget without a crash.
