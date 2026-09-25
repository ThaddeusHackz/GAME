#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════════
# DIVIDED HORIZON — build driver
#
#   tools/build.sh test       Linux headless build + run the smoke test
#   tools/build.sh windows    cross-compile DividedHorizon.exe via zig cc
#   tools/build.sh all        both
#
# WHY zig: the build host has no mingw-w64 and no package-manager access, but
# `zig cc -target x86_64-windows-gnu` bundles a complete MSVC-ABI-compatible
# mingw toolchain and links opengl32.dll/winmm/gdi32/user32 directly — every
# one of which has shipped with every Windows release since XP. No runtime
# dependencies, no installer, no DRM (Spec 35/36).
#
# Spec 19 (never fake it): every target here runs what it builds. If a stage
# cannot run in this environment it says so in plain text instead of passing.
# ══════════════════════════════════════════════════════════════════════════
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
ROOT="$(pwd)"
GAME="$ROOT/game"
BUILD="$ROOT/build"
DIST="$ROOT/dist"
ZIG="${ZIG:-$HOME/.toolchain/package/zig}"
# npm tarballs unpack to <dir>/zig, older manual installs put the binary at
# the path directly — accept either so the script never lies about BLOCKED.
if [ -d "$ZIG" ]; then ZIG="$ZIG/zig"; fi
mkdir -p "$BUILD" "$DIST"

CSTD="-std=c99"
WARN="-Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation"
OPT="${OPT:--O2}"

CORE_SRC=(
  "$GAME/src/core/dh_math.c"
  "$GAME/src/core/dh_log.c"
  "$GAME/src/core/dh_json.c"
  "$GAME/src/core/settings.c"
  "$GAME/src/core/save.c"
  "$GAME/src/rend/rend.c"
  "$GAME/src/rend/soft.c"
)
GL_SRC=(
  "$GAME/src/rend/gl11.c"
)
PLAT_SRC_LINUX=(
  "$GAME/src/plat/headless.c"
)
PLAT_SRC_WIN=(
  "$GAME/src/plat/win32.c"
)

have() { command -v "$1" >/dev/null 2>&1; }

run_smoke() {
  local out="$1"
  echo "── running smoke test: $out"
  mkdir -p "$BUILD/smoke"
  if "$out" "$BUILD/smoke"; then
    echo "   ✓ smoke test passed"
    return 0
  else
    echo "   ✗ smoke test FAILED"
    return 1
  fi
}

# ── Linux headless test build ──────────────────────────────────────────────
build_test() {
  echo "══ [1/2] Linux headless test build (gcc) ══"
  local src=("${CORE_SRC[@]}" "$GAME/tests/smoke_core.c")
  local exe="$BUILD/dh_smoke"
  if have gcc; then
    gcc $CSTD $OPT $WARN -o "$exe" "${src[@]}" -lm || { echo "compile FAILED"; return 1; }
  elif [ -x "$ZIG" ]; then
    echo "   (gcc absent — using zig cc as host compiler)"
    "$ZIG" cc $CSTD $OPT -target native -o "$exe" "${src[@]}" || { echo "compile FAILED"; return 1; }
  else
    echo "   BLOCKED: no gcc and no zig on PATH"
    return 1
  fi
  run_smoke "$exe"
}

# ── Windows cross build ────────────────────────────────────────────────────
# Cross-compiles the *smoke test* to a Win64 .exe so the toolchain is verified
# on every build, even before the game entry point exists. This is the only
# honest way to prove the delivery path (Spec 17) works: a .exe we cannot run
# here must at least be structurally verified.
build_crosscheck() {
  echo "══ cross-toolchain check (zig cc → x86_64-windows-gnu) ══"
  if [ ! -x "$ZIG" ]; then
    echo "   BLOCKED: zig not found at $ZIG (run tools/fetch_toolchain.sh)"
    return 1
  fi
  local src=("${CORE_SRC[@]}" "$GAME/tests/smoke_core.c")
  local exe="$BUILD/dh_smoke.exe"
  "$ZIG" cc $CSTD $OPT -target x86_64-windows-gnu \
      -Wno-single-bit-bitfield-constant-conversion \
      -Wl,--subsystem,console -o "$exe" "${src[@]}" 2>&1 | grep -E "game/src|error:" | head -30
  if [ ! -f "$exe" ]; then echo "cross-compile FAILED"; return 1; fi
  echo "   ✓ $exe ($(du -h "$exe" | cut -f1)) — Win64 PE produced from the same sources"
  if have python3; then python3 "$ROOT/tools/verify_pe.py" "$exe" 3 || return 1; fi
  echo "   NOTE: this .exe cannot be *executed* in this sandbox (no Windows)."
  echo "   Run it on any x64 Windows machine to confirm identical results."
  return 0
}

build_windows() {
  echo "══ [2/2] Windows cross build (zig cc → x86_64-windows-gnu) ══"
  if [ ! -x "$ZIG" ]; then
    echo "   BLOCKED: zig not found at $ZIG"
    echo "   (set ZIG=/path/to/zig, or run tools/fetch_toolchain.sh)"
    return 1
  fi
  if [ ! -f "$GAME/src/main.c" ]; then
    echo "   BLOCKED: game/src/main.c does not exist yet."
    echo "   Producing a DividedHorizon.exe now would mean shipping an entry-point-less"
    echo "   binary that cannot launch — which is exactly the fake-completion this"
    echo "   project's Honesty Contract forbids. Use 'tools/build.sh crosscheck' to"
    echo "   verify the cross toolchain in the meantime."
    return 1
  fi
  echo "   zig: $("$ZIG" version 2>/dev/null | head -1)"
  local src=("${CORE_SRC[@]}")
  local plat=()
  if [ -f "$GAME/src/plat/win32.c" ]; then plat=("${PLAT_SRC_WIN[@]}"); fi
  if [ -f "$GAME/src/rend/gl11.c" ];     then src+=("${GL_SRC[@]}"); fi
  src+=("$GAME/src/main.c")
  local exe="$DIST/DividedHorizon.exe"
  # -Wl,--subsystem,windows : no console window behind the game window.
  "$ZIG" cc $CSTD $OPT -target x86_64-windows-gnu \
      -Wno-single-bit-bitfield-constant-conversion \
      -Wl,--subsystem,windows \
      -o "$exe" "${src[@]}" "${plat[@]}" \
      -lgdi32 -lopengl32 -lwinmm -luser32 2>&1 | grep -E "game/src|error:" | head -30
  if [ ! -f "$exe" ]; then echo "cross-compile FAILED"; return 1; fi
  echo "   ✓ $exe ($(du -h "$exe" | cut -f1))"
  # Verify it is a real Win64 PE with only universally-present imports.
  if have python3; then
    python3 "$ROOT/tools/verify_pe.py" "$exe" 2 || return 1
  fi
  return 0
}

# ── dist zip ──────────────────────────────────────────────────────────────
build_zip() {
  echo "══ packaging dist/DividedHorizon-*.zip ══"
  local ver
  ver=$(grep -o 'DH_VERSION_MAJOR [0-9]*' "$GAME/src/core/dh_types.h" | awk '{print $2}')
  local name="DividedHorizon-v${ver}-win64.zip"
  ( cd "$DIST" && zip -q -9 "$name" DividedHorizon.exe ../LICENSES.md ../README.md 2>/dev/null \
      || zip -q -9 "$name" DividedHorizon.exe )
  echo "   ✓ dist/$name ($(du -h "$DIST/$name" | cut -f1))"
}

target="${1:-test}"
rc=0
case "$target" in
  test)        build_test || rc=1 ;;
  crosscheck)  build_crosscheck || rc=1 ;;
  windows)     build_windows || rc=1 ;;
  all)         build_test || rc=1; build_crosscheck || rc=1; build_windows || rc=1 ;;
  zip)         build_windows || rc=1; build_zip || rc=1 ;;
  *)           echo "usage: $0 [test|crosscheck|windows|all|zip]"; exit 2 ;;
esac
exit $rc
