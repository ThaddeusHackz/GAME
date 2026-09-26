#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════════
# DIVIDED HORIZON — build driver
#
#   tools/build.sh test       Linux headless build + run BOTH smoke tests
#   tools/build.sh run        build the headless game and run the demo course
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
# M1+: world / player / app layers. Shared by the traversal test, the headless
# game binary and the Windows exe — one source of truth for gameplay code.
GAME_SRC=(
  "$GAME/src/rend/font.c"
  "$GAME/src/rend/proc_tex.c"
  "$GAME/src/world/terrain.c"
  "$GAME/src/world/collision.c"
  "$GAME/src/player/player.c"
  "$GAME/src/combat/weapon.c"
  "$GAME/src/ai/enemy.c"
  "$GAME/src/city/vehicle.c"
  "$GAME/src/city/heat.c"
  "$GAME/src/city/city.c"
  "$GAME/src/game/game.c"
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
compile_host() {
  local out="$1"; shift
  if have gcc; then
    gcc $CSTD $OPT $WARN -I"$GAME/src" -o "$out" "$@" -lm
  elif [ -x "$ZIG" ]; then
    echo "   (gcc absent — using zig cc as host compiler)"
    "$ZIG" cc $CSTD $OPT -I"$GAME/src" -target native -o "$out" "$@"
  else
    echo "   BLOCKED: no gcc and no zig on PATH"
    return 1
  fi
}

build_test() {
  echo "══ [1/5] Linux headless test build (gcc) ══"
  local rc=0
  local exe="$BUILD/dh_smoke"
  compile_host "$exe" "${CORE_SRC[@]}" "$GAME/tests/smoke_core.c" \
      || { echo "compile FAILED"; return 1; }
  run_smoke "$exe" || rc=1
  local exe2="$BUILD/dh_smoke_traversal"
  compile_host "$exe2" "${CORE_SRC[@]}" "${GAME_SRC[@]}" "${PLAT_SRC_LINUX[@]}" \
      "$GAME/tests/smoke_traversal.c" || { echo "compile FAILED"; return 1; }
  run_smoke "$exe2" || rc=1
  local exe3="$BUILD/dh_smoke_combat"
  compile_host "$exe3" "${CORE_SRC[@]}" "${GAME_SRC[@]}" "${PLAT_SRC_LINUX[@]}" \
      "$GAME/tests/smoke_combat.c" || { echo "compile FAILED"; return 1; }
  run_smoke "$exe3" || rc=1
  local exe4="$BUILD/dh_smoke_island"
  compile_host "$exe4" "${CORE_SRC[@]}" "${GAME_SRC[@]}" "${PLAT_SRC_LINUX[@]}" \
      "$GAME/tests/smoke_island.c" || { echo "compile FAILED"; return 1; }
  run_smoke "$exe4" || rc=1
  local exe5="$BUILD/dh_smoke_city"
  compile_host "$exe5" "${CORE_SRC[@]}" "${GAME_SRC[@]}" "${PLAT_SRC_LINUX[@]}" \
      "$GAME/tests/smoke_city.c" || { echo "compile FAILED"; return 1; }
  run_smoke "$exe5" || rc=1
  return $rc
}

# ── headless game binary (the shipping exe's simulation, minus the window) ──
build_run() {
  echo "══ headless game build + demo run ══"
  local exe="$BUILD/dh_headless"
  compile_host "$exe" "${CORE_SRC[@]}" "${GAME_SRC[@]}" "${PLAT_SRC_LINUX[@]}" \
      "$GAME/src/main.c" || { echo "compile FAILED"; return 1; }
  mkdir -p "$BUILD/shots"
  echo "── running 14 s scripted traversal demo → $BUILD/shots"
  "$exe" --headless --shots "$BUILD/shots" --fast
}

# ── course stills for the milestone reports (dev tool, not shipped) ───────
build_shots() {
  echo "══ headless course flythrough + stills ══"
  local exe="$BUILD/dh_course_shots"
  compile_host "$exe" "${CORE_SRC[@]}" "${GAME_SRC[@]}" "${PLAT_SRC_LINUX[@]}" \
      "$ROOT/tools/diag/course_shot.c" || { echo "compile FAILED"; return 1; }
  mkdir -p "$BUILD/shots"
  "$exe"
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
  # The Windows exe ships the GL11 backend, so soft.c (same rend_be_* symbols)
  # must stay out of this link; headless/Linux builds keep soft.c.
  local src=()
  for f in "${CORE_SRC[@]}"; do
    case "$f" in *"/soft.c") ;; *) src+=("$f") ;; esac
  done
  src+=("${GAME_SRC[@]}")
  # headless stays in the Windows build too: `DividedHorizon.exe --headless`
  # is how the shipped binary is verified on machines with no display.
  local plat=("${PLAT_SRC_LINUX[@]}")
  if [ -f "$GAME/src/plat/win32.c" ]; then plat+=("${PLAT_SRC_WIN[@]}"); fi
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
  if [ ! -f "$DIST/DividedHorizon.exe" ]; then
    echo "   BLOCKED: no DividedHorizon.exe - refusing to ship a zip without the game (Spec 17.3)"
    rm -f "$DIST"/DividedHorizon-*.zip
    return 1
  fi
  local ver
  ver=$(grep -o 'DH_VERSION_MAJOR [0-9]*' "$GAME/src/core/dh_types.h" | awk '{print $2}')
  local name="DividedHorizon-v${ver}-win64.zip"
  rm -f "$DIST/$name"
  ( cd "$DIST" && zip -q -9 "$name" DividedHorizon.exe )
  # docs + balance table ship NEXT TO the exe (data dir resolves to ".")
  ( cd "$ROOT" && [ -f LICENSES.md ] && zip -q -9 "$DIST/$name" LICENSES.md )
  ( cd "$ROOT" && [ -f README.md ]   && zip -q -9 "$DIST/$name" README.md )
  ( cd "$GAME/data" && [ -f weapons.json ] && zip -q -9 "$DIST/$name" weapons.json )
  ( cd "$GAME/data" && [ -f vehicles.json ] && zip -q -9 "$DIST/$name" vehicles.json )
  echo "   ✓ dist/$name ($(du -h "$DIST/$name" | cut -f1))"
}

target="${1:-test}"
rc=0
case "$target" in
  test)        build_test || rc=1 ;;
  run)         build_run || rc=1 ;;
  shots)       build_shots || rc=1 ;;
  crosscheck)  build_crosscheck || rc=1 ;;
  windows)     build_windows || rc=1 ;;
  all)         build_test || rc=1; build_crosscheck || rc=1; build_windows || rc=1 ;;
  zip)         build_windows || rc=1; build_zip || rc=1 ;;
  *)           echo "usage: $0 [test|run|shots|crosscheck|windows|all|zip]"; exit 2 ;;
esac
exit $rc
