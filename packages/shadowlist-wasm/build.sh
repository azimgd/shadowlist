#!/usr/bin/env bash
set -uo pipefail
# Builds the wasm core and the TypeScript into dist/. Prints only errors and the result.
#   ./build.sh [Release|Debug]
# Emscripten comes from the PATH or from emsdk at $EMSDK (default ~/emsdk).
# SHADOWLIST_DEBUG_LOG=1 ./build.sh compiles in the core's [SL] debug log.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG="${1:-Release}"
if ! command -v emcmake >/dev/null; then
  # shellcheck disable=SC1091
  source "${EMSDK:-$HOME/emsdk}/emsdk_env.sh" >/dev/null 2>&1
fi
command -v emcmake >/dev/null || { echo "BUILD FAILED: emcmake not found, install emsdk"; exit 1; }
mkdir -p "$HERE/build" "$HERE/dist"
DEBUG_LOG=OFF
[[ "${SHADOWLIST_DEBUG_LOG:-0}" == 1 ]] && DEBUG_LOG=ON
{
  emcmake cmake -S "$HERE/cpp" -B "$HERE/build/$CONFIG" -DCMAKE_BUILD_TYPE="$CONFIG" -DSHADOWLIST_DEBUG_LOG="$DEBUG_LOG" &&
    cmake --build "$HERE/build/$CONFIG" -j 8
} > "$HERE/build/log.txt" 2>&1
STATUS=$?
if [[ $STATUS == 0 ]]; then
  cp "$HERE/build/$CONFIG/shadowlistkit.mjs" "$HERE/build/$CONFIG/shadowlistkit.wasm" "$HERE/src/shadowlistkit.d.mts" "$HERE/dist/"
  "$HERE/../../node_modules/.bin/tsc" -b "$HERE/tsconfig.json" >> "$HERE/build/log.txt" 2>&1
  STATUS=$?
fi
grep -E "error" "$HERE/build/log.txt" | sort -u | cut -c1-260 | head -40
[[ $STATUS == 0 ]] && echo "BUILD OK: $HERE/dist" || echo "BUILD FAILED ($STATUS)"
exit $STATUS
