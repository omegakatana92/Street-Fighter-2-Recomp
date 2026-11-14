#!/usr/bin/env bash
# Development build + headless smoke tests for every ROM set you pass.
# usage: scripts/build-local.sh [--sf2 sf2.zip] [--sf2ce sf2ce.zip] [--sf2hf sf2hf.zip] [build-dir]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
ARGS=()
BUILD_DIR=build/cmake
while [[ $# -gt 0 ]]; do
  case "$1" in
    --sf2) ARGS+=("-DSF2_ROM_SF2=$2"); shift 2;;
    --sf2ce) ARGS+=("-DSF2_ROM_SF2CE=$2"); shift 2;;
    --sf2hf) ARGS+=("-DSF2_ROM_SF2HF=$2"); shift 2;;
    *) BUILD_DIR=$1; shift;;
  esac
done
cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release "${ARGS[@]}"
cmake --build "$BUILD_DIR"
(cd "$BUILD_DIR" && ctest --output-on-failure)
echo "Build and headless smoke tests passed."
