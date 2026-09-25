#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════════
# DIVIDED HORIZON — cross-toolchain bootstrap
#
# The production build host has no package-manager access and GitHub release
# assets are unreachable from it, so Godot / Electron / mingw packages cannot
# be downloaded the normal way. The npm registry IS reachable, and the Zig
# compiler is published there as a plain tarball. Zig ships a complete
# mingw-w64 compatible cross toolchain, which is exactly what we need to
# produce a native Win64 PE with zero runtime dependencies.
#
# Installs to $HOME/.toolchain/package/zig (outside the repo — the compiler
# is a build dependency, not source, and must never bloat the zip).
# ══════════════════════════════════════════════════════════════════════════
set -euo pipefail
DEST="${DEST:-$HOME/.toolchain/package/zig}"
PKG="${PKG:-@oven/zig-linux-x64}"

if [ -x "$DEST/zig" ] || [ -x "$DEST" ]; then
  BIN="$DEST/zig"; [ -x "$BIN" ] || BIN="$DEST"
  echo "zig already present: $("$BIN" version)"
  exit 0
fi

echo "fetching $PKG from the npm registry…"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cd "$TMP"
npm pack "$PKG" --silent
TGZ=$(ls *.tgz | head -1)
tar -xzf "$TGZ"
mkdir -p "$(dirname "$DEST")"
rm -rf "$DEST"
mv package "$DEST"
chmod +x "$DEST/zig"
echo "installed: $DEST/zig ($("$DEST/zig" version))"
echo
echo "next: tools/build.sh windows"
