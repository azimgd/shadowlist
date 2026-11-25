#!/usr/bin/env bash
set -uo pipefail
# Measure every example screen with ShadowRuler on an iOS simulator and score the layout.
#
#   ./ruler-check.sh <out-dir> [routes...]
#
# Needs the example built with SHADOWRULER=1 (see its react-native.config.js) and installed on
# UDID, or with PLATFORM=android on ANDROID_SERIAL (gradle -PshadowrulerAllowRelease=true; the
# capture goes to the app's external files folder, SLRulerDir). For each route it launches the app with SLRuler 1, waits for the [SLRULER] line and copies
# the capture JSON and the PNG with the ruler drawn to <out-dir>/<route>.{json,png}. Then
# ruler-check.py scores them: off-pixel geometry, odd insets and unequal sibling gaps.
OUT="${1:?usage: ruler-check.sh <out-dir> [routes...]}"
shift
PLATFORM="${PLATFORM:-ios}"
[[ "$PLATFORM" == ios ]] && UDID="${UDID:?set UDID}"
PKG=shadowlist.example
ADIR=/sdcard/Android/data/$PKG/files
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROUTES=("$@")
(( ${#ROUTES[@]} )) || ROUTES=(Home Feed Masonry Nested Chat Assistant Inbox Activity Contacts SectionList Reorder ReorderGrid Board Tree Snap ScrollLab Templates Poll Horizontal Changes Collapsing Sections)
mkdir -p "$OUT"
android_route() { # route
  local route="$1" log="$OUT/$1.log" extras=(--es SLRuler 1 --es SLRulerDir "$ADIR" --es SLLatency 0,0 --es SLTheme "${THEME:-light}")
  [[ "$route" != Home ]] && extras+=(--es SLRoute "$route")
  adb logcat -c
  adb shell am start -S -n "$PKG/.MainActivity" "${extras[@]}" >/dev/null
  for _ in $(seq 1 300); do adb logcat -d -s ReactNativeJS | grep -q '\[SLRULER\]' && break; sleep 0.1; done
  adb logcat -d -s ReactNativeJS | tr -d '\r' > "$log"
  local capture; capture="$(grep -o 'capture=[^ ]*' "$log" | head -1 | cut -d= -f2)"
  if [[ -z "$capture" ]]; then echo "!! $route: $(grep -o '\[SLRULER\].*' "$log" | head -1)"; return; fi
  adb pull "$capture" "$OUT/$route.json" >/dev/null; adb pull "${capture%.json}.png" "$OUT/$route.png" >/dev/null
  echo "==> $route"
}

for route in "${ROUTES[@]}"; do
  if [[ "$PLATFORM" == android ]]; then android_route "$route"; continue; fi
  log="$OUT/$route.log"
  args=(-SLRuler 1 -SLLatency 0,0 -SLTheme "${THEME:-light}")
  [[ "$route" != Home ]] && args+=(-SLRoute "$route")
  xcrun simctl launch --console-pty --terminate-running-process "$UDID" "$PKG" "${args[@]}" > "$log" 2>&1 &
  console=$!
  for _ in $(seq 1 300); do grep -q '\[SLRULER\]' "$log" && break; sleep 0.1; done
  kill "$console" 2>/dev/null || true
  capture="$(grep -o 'capture=[^ ]*' "$log" | head -1 | cut -d= -f2 | tr -d '\r')"
  if [[ -z "$capture" ]]; then echo "!! $route: $(grep -o '\[SLRULER\].*' "$log" | head -1)"; continue; fi
  cp "$capture" "$OUT/$route.json"; cp "${capture%.json}.png" "$OUT/$route.png"
  echo "==> $route"
done
python3 "$HERE/ruler-check.py" "$OUT"
