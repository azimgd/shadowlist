#!/usr/bin/env bash
#
# Compile the shared shadowlist-core (../shadowlist-core) together with the
# embind binding (cpp/Binding.cpp) into a WebAssembly ES module.
#
# Output: src/wasm/shadowlistCore.{js,wasm}
#
# Requires the Emscripten SDK. Set EMSDK to the emsdk root, or have emcc on PATH.
set -euo pipefail

PACKAGE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGES_DIR="$(cd "${PACKAGE_DIR}/.." && pwd)"
CORE_DIR="${PACKAGES_DIR}/shadowlist-core"
OUT_DIR="${PACKAGE_DIR}/src/wasm"

# Activate emsdk if emcc is not already on PATH.
if ! command -v emcc >/dev/null 2>&1; then
  EMSDK_ROOT="${EMSDK:-$HOME/emsdk}"
  if [ -f "${EMSDK_ROOT}/emsdk_env.sh" ]; then
    # shellcheck disable=SC1091
    source "${EMSDK_ROOT}/emsdk_env.sh" >/dev/null 2>&1
  fi
fi

if ! command -v emcc >/dev/null 2>&1; then
  echo "error: emcc not found. Install Emscripten and/or set EMSDK to its root." >&2
  exit 1
fi

mkdir -p "${OUT_DIR}"

echo "Compiling shadowlist-core -> WASM ..."
emcc \
  "${PACKAGE_DIR}/cpp/Binding.cpp" \
  "${CORE_DIR}"/*.cpp \
  -I "${PACKAGES_DIR}" \
  -std=c++20 \
  -O3 \
  -fexceptions \
  -lembind \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s EXPORT_NAME=createShadowlistCoreModule \
  -s ENVIRONMENT=web,worker \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s FILESYSTEM=0 \
  -s SINGLE_FILE=0 \
  -o "${OUT_DIR}/shadowlistCore.js"

echo "Done. Artifacts written to ${OUT_DIR}/shadowlistCore.{js,wasm}"
