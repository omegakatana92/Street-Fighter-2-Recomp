#!/usr/bin/env bash
# Build the Steam Deck / Linux AppImage from a release build.
#
#   BUILD_DIR     build directory holding bin/StreetFighterII (default build/linux-release)
#   APPIMAGETOOL  path to appimagetool (default: on PATH)
#
# The release build should link SDL2 statically against an old glibc baseline;
# see docs/BUILD_LINUX.md ("Release build").  No ROM data is packaged.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

BUILD_DIR=${BUILD_DIR:-build/linux-release}
APPDIR=${APPDIR:-build/AppDir}
DIST=${DIST:-dist}
TOOL=${APPIMAGETOOL:-appimagetool}
OUT=StreetFighterII-SteamDeck-x86_64.AppImage

if [[ ! -x "$BUILD_DIR/bin/StreetFighterII" ]]; then
  echo "error: $BUILD_DIR/bin/StreetFighterII not found; build first" >&2
  exit 1
fi

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" "$APPDIR/usr/share/sf2/profiles" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps" "$APPDIR/usr/share/doc/sf2" "$DIST"
cp "$BUILD_DIR/bin/StreetFighterII" "$APPDIR/usr/bin/sf2"
strip "$APPDIR/usr/bin/sf2" 2>/dev/null || true
cp profiles/*.json "$APPDIR/usr/share/sf2/profiles/"
cp packaging/appimage/sf2.desktop "$APPDIR/sf2.desktop"
cp packaging/appimage/sf2.png "$APPDIR/sf2.png"
cp packaging/appimage/AppRun "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun" "$APPDIR/usr/bin/sf2"
cp packaging/appimage/sf2.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/sf2.png"
cp packaging/appimage/sf2.desktop "$APPDIR/usr/share/applications/sf2.desktop"
cp -r LICENSES LICENSE README.md docs/CONTROLS.md docs/ROM_SETUP.md docs/KNOWN_ISSUES.md docs/STEAM_DECK.md docs/SAVE_STATES.md "$APPDIR/usr/share/doc/sf2/"

if ldd "$APPDIR/usr/bin/sf2" | grep -q SDL2; then
  echo "warning: the binary links SDL2 dynamically; the host must provide libSDL2-2.0.so.0" >&2
fi

ARCH=x86_64 $TOOL --no-appstream "$APPDIR" "$DIST/$OUT"
echo "wrote $DIST/$OUT (no ROM included)"
