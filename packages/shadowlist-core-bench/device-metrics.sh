#!/usr/bin/env bash
#
# Measure the example app on an Android device while it scrolls fast.
#
#   ./device-metrics.sh <label> <screen> [flings] [swipe_ms]
#
# The screen is its route name in the example app, like Chat, Feed or FeedNative. The
# script opens it from the home list, flings the list and reports:
#
#   Frames: timing and jank from dumpsys gfxinfo framestats.
#   Blank cells: the tallest empty band in the list while flinging, meaning rows that
#     weren't drawn yet. See analyze-metrics.py.
#   Memory: PSS, native heap and the live view count, which the native view band should move.
#   CPU: process time from /proc, as a share of one core over the run.
#
# This covers React, Yoga, mounting, JNI and the GPU too, unlike run.sh, so it's noisier.
# Run it more than once.
#
# Environment:
#   ADB=adb            adb command; may carry a serial, e.g. "adb -s emulator-5554"
#   BACKGROUND=000000  list background as hex; set it empty to calibrate from a settled frame
#   MODE=sweep         sweep or local, see below
#   SHOT_COUNT=90      screenshots to take; 0 skips blank cell sampling and only times frames
#   VIEWPORT_TOP=0.16 VIEWPORT_BOTTOM=0.76
#                      where the list sits, as fractions of screen height. Defaults fit Chat.
#
set -euo pipefail

LABEL="${1:?usage: device-metrics.sh <label> <screen> [flings] [swipe_ms]}"
SCREEN="${2:?usage: device-metrics.sh <label> <screen> [flings] [swipe_ms]}"
FLINGS="${3:-14}"
SWIPE_MS="${4:-60}"
# The theme background color. Fixed by default so every build is scored the same way.
BACKGROUND="${BACKGROUND-000000}"
# sweep flings the same way every time, so the run goes deep into fresh rows where
# slow per frame work shows up. local alternates and stays in one area.
MODE="${MODE:-sweep}"
# Each screenshot takes about 170ms on device, so this should cover the whole run.
SHOT_COUNT="${SHOT_COUNT:-90}"
VIEWPORT_TOP="${VIEWPORT_TOP:-0.16}"
VIEWPORT_BOTTOM="${VIEWPORT_BOTTOM:-0.76}"

PKG=shadowlist.example
# Left unquoted on purpose below, since ADB may carry arguments like "-s SERIAL".
ADB="${ADB:-adb}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESULTS="$HERE/results"
SHOTS=/data/local/tmp/shots
mkdir -p "$RESULTS"
OUT="$RESULTS/$LABEL.$SCREEN.metrics.txt"
LOCAL_SHOTS="${TMPDIR:-/tmp}/slshots.$$"

read -r WIDTH HEIGHT < <($ADB shell wm size | tr -d '\r' | sed 's/.*: //' | tr 'x' ' ')
CX=$((WIDTH / 2))
Y_LOW=$((HEIGHT * 78 / 100))
Y_HIGH=$((HEIGHT * 25 / 100))

# Wall clock in nanoseconds. date +%s%N doesn't work on older macOS.
now_ns() {
  python3 -c 'import time; print(time.time_ns())'
}

# Print the bounds of the first view matching the pattern, or nothing.
# The caller decides what a miss means.
find_bounds() {
  $ADB shell uiautomator dump /sdcard/ui.xml >/dev/null 2>&1 || true
  $ADB shell cat /sdcard/ui.xml | tr '>' '\n' \
    | grep -F "$1" \
    | grep -oE 'bounds="\[[0-9]+,[0-9]+\]\[[0-9]+,[0-9]+\]"' | head -1 \
    | grep -oE '[0-9]+' | tr '\n' ' ' || true
}

# Restart the app on its home list and tap the row for $SCREEN.
navigate() {
  # BACKGROUND assumes the dark theme.
  $ADB shell cmd uimode night yes >/dev/null 2>&1 || true
  $ADB shell am force-stop "$PKG" >/dev/null 2>&1 || true
  $ADB shell am start -n "$PKG/.MainActivity" >/dev/null 2>&1 || true
  sleep 4
  # Home rows use the route name as testID.
  local bounds=""
  for _ in 1 2 3; do
    bounds=$(find_bounds "resource-id=\"$SCREEN\"")
    [[ -n "$bounds" ]] && break
    $ADB shell input swipe "$CX" "$Y_LOW" "$CX" "$Y_HIGH" 300 >/dev/null
    sleep 1
  done
  if [[ -z "$bounds" ]]; then
    echo "!! could not find '$SCREEN' on the home list" >&2
    exit 1
  fi
  read -r x1 y1 x2 y2 <<<"$bounds"
  $ADB shell input tap $(((x1 + x2) / 2)) $(((y1 + y2) / 2)) >/dev/null
  sleep 2.5
}

echo "==> $LABEL / $SCREEN : $MODE, $FLINGS flings of ${SWIPE_MS}ms on ${WIDTH}x${HEIGHT}"
navigate

PID=$($ADB shell pidof "$PKG" | tr -d '\r')
[[ -n "$PID" ]] || { echo "!! app not running" >&2; exit 1; }

# Warm up the list so we measure normal scrolling, not the first render.
for _ in 1 2 3; do
  $ADB shell input swipe "$CX" "$Y_LOW" "$CX" "$Y_HIGH" 200 >/dev/null
done
sleep 2

# Take a reference screenshot and reset the counters.
$ADB shell "rm -rf $SHOTS; mkdir -p $SHOTS"
$ADB shell "screencap $SHOTS/cal.raw"
$ADB shell dumpsys gfxinfo "$PKG" reset >/dev/null
CPU_BEFORE=$($ADB shell "cat /proc/$PID/stat" | tr -d '\r' | awk '{print $14+$15}')
T_BEFORE=$(now_ns)

# Keep taking screenshots on the device for the whole run.
CAPTURE_PID=""
if ((SHOT_COUNT > 0)); then
  $ADB shell "for i in \$(seq 1 $SHOT_COUNT); do screencap $SHOTS/f\$i.raw; done" >/dev/null 2>&1 &
  CAPTURE_PID=$!
fi

for ((i = 0; i < FLINGS; i++)); do
  if [[ "$MODE" == "sweep" ]] || ((i % 4 < 2)); then
    $ADB shell input swipe "$CX" "$Y_LOW" "$CX" "$Y_HIGH" "$SWIPE_MS" >/dev/null
  else
    $ADB shell input swipe "$CX" "$Y_HIGH" "$CX" "$Y_LOW" "$SWIPE_MS" >/dev/null
  fi
  # The swipe command returns right away but the fling runs for most of a second.
  # Wait, or the run ends before the list has moved.
  sleep 0.35
done

T_AFTER=$(now_ns)
CPU_AFTER=$($ADB shell "cat /proc/$PID/stat" | tr -d '\r' | awk '{print $14+$15}')
if [[ -n "$CAPTURE_PID" ]]; then
  wait "$CAPTURE_PID" 2>/dev/null || true
fi
sleep 1.5

$ADB shell dumpsys gfxinfo "$PKG" framestats | tr -d '\r' > "$OUT.gfx"
$ADB shell dumpsys meminfo "$PKG" | tr -d '\r' > "$OUT.mem"

rm -rf "$LOCAL_SHOTS"
mkdir -p "$LOCAL_SHOTS"
$ADB pull "$SHOTS" "$LOCAL_SHOTS" >/dev/null 2>&1 || true
$ADB shell "rm -rf $SHOTS"

CLK=$($ADB shell getconf CLK_TCK | tr -d '\r')
CLK=${CLK:-100}

python3 "$HERE/analyze-metrics.py" \
  --label "$LABEL" --screen "$SCREEN" \
  --gfx "$OUT.gfx" --mem "$OUT.mem" \
  --shots "$LOCAL_SHOTS/shots" \
  --cpu-ticks $((CPU_AFTER - CPU_BEFORE)) --clk "$CLK" \
  --elapsed-ns $((T_AFTER - T_BEFORE)) \
  --height "$HEIGHT" --width "$WIDTH" --background "$BACKGROUND" \
  --viewport-top "$VIEWPORT_TOP" --viewport-bottom "$VIEWPORT_BOTTOM" | tee "$OUT"

rm -rf "$LOCAL_SHOTS"
echo "==> wrote $OUT"
