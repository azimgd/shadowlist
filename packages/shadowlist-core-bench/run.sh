#!/usr/bin/env bash
#
# Build and run the ShadowList core benchmark.
#
#   ./run.sh <label> [--quick]
#
# Builds the benchmark twice: once for this host and once for Android arm64 (pushed to
# the connected device and run there). Results land in results/<label>.{host,device}.txt
# so a baseline and an optimized run can be diffed with compare.py.
#
# Environment:
#   ADB=adb                  adb command; may carry a serial, e.g. "adb -s emulator-5554"
#   ANDROID_NDK_HOME=<path>  NDK to cross-compile with (default: newest under ANDROID_HOME)
#
set -euo pipefail

LABEL="${1:?usage: run.sh <label> [--quick]}"
shift || true
EXTRA_ARGS=("$@")

# Intentionally expanded unquoted below: ADB may carry arguments such as "-s SERIAL".
ADB="${ADB:-adb}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESULTS="$HERE/results"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
mkdir -p "$RESULTS"

# ---------------------------------------------------------------- host
echo "==> building host benchmark"
cmake -S "$HERE" -B "$HERE/build/host" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$HERE/build/host" -j"$JOBS" >/dev/null

echo "==> running host benchmark"
"$HERE/build/host/shadowlist_core_bench" ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} | tee "$RESULTS/$LABEL.host.txt"

# -------------------------------------------------------------- device
NDK_ROOT="${ANDROID_NDK_HOME:-}"
if [[ -z "$NDK_ROOT" ]]; then
  # Newest installed NDK that ships the CMake toolchain file.
  NDK_DIR="${ANDROID_HOME:-$HOME/Library/Android/sdk}/ndk"
  if [[ -d "$NDK_DIR" ]]; then
    while IFS= read -r candidate; do
      if [[ -f "$candidate/build/cmake/android.toolchain.cmake" ]]; then
        NDK_ROOT="$candidate"
        break
      fi
    done < <(printf '%s\n' "$NDK_DIR"/* | sort -Vr)
  fi
fi

if [[ -z "$NDK_ROOT" || ! -d "$NDK_ROOT" ]]; then
  echo "!! no Android NDK found; skipping the device run" >&2
  exit 0
fi

if ! $ADB get-state >/dev/null 2>&1; then
  echo "!! no Android device connected; skipping the device run" >&2
  exit 0
fi

echo "==> building Android arm64 benchmark (NDK: $NDK_ROOT)"
cmake -S "$HERE" -B "$HERE/build/android" \
  -DCMAKE_TOOLCHAIN_FILE="$NDK_ROOT/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$HERE/build/android" -j"$JOBS" >/dev/null

DEVICE_DIR=/data/local/tmp/shadowlist_bench
echo "==> running on device: $($ADB shell getprop ro.product.model | tr -d '\r')"
$ADB shell "mkdir -p $DEVICE_DIR"
$ADB push "$HERE/build/android/shadowlist_core_bench" "$DEVICE_DIR/" >/dev/null
$ADB shell "chmod 755 $DEVICE_DIR/shadowlist_core_bench"
$ADB shell "cd $DEVICE_DIR && ./shadowlist_core_bench ${EXTRA_ARGS[*]-}" | tr -d '\r' | tee "$RESULTS/$LABEL.device.txt"

echo
echo "==> wrote $RESULTS/$LABEL.host.txt and $RESULTS/$LABEL.device.txt"
