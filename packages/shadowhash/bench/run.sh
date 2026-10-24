#!/bin/sh
set -eu

# Compiles the package and the CoreText oracle, then runs the size and fidelity benches.
# The fidelity bench needs macOS for swiftc; size runs anywhere Node does.
cd "$(dirname "$0")"

# tsc 6 refuses root files under a ..-prefixed relative path. Absolute paths work.
npx tsc "$PWD"/../src/*.ts --outDir "$PWD/dist" --module commonjs --target es2022 --skipLibCheck --ignoreConfig
node size.cjs

if command -v swiftc >/dev/null 2>&1; then
  swiftc -O layout.swift -o layout
  node fidelity.cjs
else
  echo 'swiftc not found: skipping the fidelity bench'
fi
