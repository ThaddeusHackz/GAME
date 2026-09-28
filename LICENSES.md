# DIVIDED HORIZON — Licenses & Provenance

**Studio credit:** Built by AI under human direction.
**Product:** DIVIDED HORIZON (original IP).
**DRM:** None. No telemetry. No online requirement. (Spec 35, 36)

This document is the single source of truth for *what is in the build and who
made it*. Anything not listed here does not ship.

---

## 1. First-party code — all original

| Component | Path | License | Notes |
|---|---|---|---|
| Horizon Engine core (math, RNG, noise, geometry, frustum) | `game/src/core/dh_types.h`, `dh_math.c` | Original work, © DIVIDED HORIZON project | mulberry32 RNG, value-noise fBm, Gribb–Hartmann frustum: standard published algorithms, implemented from scratch |
| Logging / filesystem / crash log | `game/src/core/dh_log.[ch]` | Original work | |
| JSON parser + writer | `game/src/core/dh_json.[ch]` | Original work | No third-party JSON library is linked |
| SettingsManager | `game/src/core/settings.[ch]` | Original work | |
| SaveManager | `game/src/core/save.[ch]` | Original work | |
| Renderer (command list, registries, BMP/PNG writers) | `game/src/rend/rend.[ch]` | Original work | PNG uses zlib *stored* blocks; CRC-32 & Adler-32 implemented in-file. These are algorithms specified in RFC 1950/1951/PNG, not copied code |
| Software rasterizer | `game/src/rend/soft.c` | Original work | |
| Smoke test | `game/tests/smoke_core.c` | Original work | |
| Build + verification tooling | `tools/*` | Original work | `verify_pe.py` parses the PE format per the MS PE/COFF specification |

**No GPL/AGPL/viral code is present.** No code is copied from any engine
(Godot, Unity, Unreal, id Tech, raylib, stb, etc.).

## 2. Third-party *build-time* dependencies (not shipped)

| Tool | Used for | License | Shipped in the zip? |
|---|---|---|---|
| Zig (`zig cc -target x86_64-windows-gnu`) | cross-compilation & bundled mingw-w64 headers/libs | MIT (compiler); mingw-w64 runtime is public-domain-ish BSD/GPL-with-exception per its own terms | **No** — only the compiled output ships. The mingw C runtime is *statically* linked; see §4 |
| GCC | Linux verification build | GPL (compiler exception applies) | **No** |

> The compiler is a build dependency, never a runtime dependency. The
> produced `.exe` contains no Zig or GCC code beyond the standard C runtime
> start files described in §4.

## 3. Assets — zero external asset files (Spec 3)

Every texture, mesh, sound and music stem in the game is generated
**procedurally at load time by first-party code**, or is authored as data in
this repository under `game/data/*.json` (CC0-equivalent, original). There are
therefore **no third-party art, audio, font or model licenses to disclose**.

- **Fonts:** bitmap font atlas generated procedurally from first-party
  vector glyph outlines (no TTF/OTF is embedded or linked).
- **Music:** adaptive stems synthesized at runtime from first-party
  composition data (no samples, no MIDI files from outside).
- **Names:** "Isla Sombra", "Meridian City", "Poncho" and all character names
  are original to this project. Any resemblance to existing franchises is
  coincidental; no trademark is claimed or infringed.

## 4. Runtime dependencies of `DividedHorizon.exe`

The shipped executable imports **only** DLLs that ship with every supported
Windows release (verified by `tools/verify_pe.py` on every build):

`kernel32.dll`, `user32.dll`, `gdi32.dll`, `opengl32.dll`, `winmm.dll`,
`msvcrt.dll` (statically-linked C runtime start files where applicable).

No DirectX redistributable, no VC++ runtime installer, no vendor driver,
no `.NET`, no Electron/CEF. This is deliberate (Spec 17): the zip must run on
a clean, never-updated machine.

## 5. Public-domain / open specifications implemented (reference only)

These are *specifications*, not code, and impose no license on the build:

- PNG (W3C/ISO 15948), zlib stored-block format (RFC 1950), BMP (de-facto)
- PE/COFF (Microsoft public documentation)
- OpenGL 1.1 fixed-function pipeline (Khronos public specification — used via
  the OS-provided `opengl32.dll`; no Khronos code is copied)

## 6. What this project deliberately does **not** contain

- No ripped, scraped, or "inspired-by-extraction" assets from any commercial
  game (Spec 2, Section 1 Honesty Contract).
- No copyrighted music, voice lines, or dialogue from any source.
- No telemetry, analytics, ads, or always-online checks (Spec 35).
- No DRM of any kind (Spec 36).

---

*If you find a file in this repository whose provenance is not covered above,
that is a bug — please report it. The Honesty Contract (Spec 1) requires it be
removed or re-licensed immediately.*
