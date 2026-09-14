#!/usr/bin/env bash
#
# Full-stack device metrics for the example app under fast scrolling.
#
#   ./device-metrics.sh <label> <screen> [flings] [swipe_ms]
#
# <screen> is the drawer label of the example app screen to measure (e.g. Chat, Feed).
# The script opens it on the connected Android device, flings the list and reports what
# a user notices or pays for:
#
#   * FRAMES. Frame timing and jank straight from `dumpsys gfxinfo framestats`.
#   * BLANK CELLS. Sampled from the framebuffer during the fling: the tallest full-width
#     band of bare background inside the list viewport, i.e. rows the pipeline has not
#     produced yet. See analyze-metrics.py.
#   * MEMORY. PSS and native heap, plus the live View count, which is what the native
#     view band is supposed to move.
#   * CPU. utime+stime straight from /proc, as a share of one core over the run.
#
# Unlike the core microbenchmark (run.sh) this includes React, Yoga, mounting, JNI and
# GPU work, so it is noisier: run it more than once.
#
# Environment:
#   ADB=adb            adb command; may carry a serial, e.g. "adb -s emulator-5554"
#   BACKGROUND=000000  list background as hex; set it empty to calibrate from a settled frame
#   MODE=sweep         sweep or local, see below
#   SHOT_COUNT=90      framebuffer samples; 0 skips blank-cell sampling and its screencap
#                      load, for a frame-timing-only run
#   VIEWPORT_TOP=0.16 VIEWPORT_BOTTOM=0.76
#                      list viewport as fractions of screen height (defaults fit Chat)
#
set -euo pipefail

LABEL="${1:?usage: device-metrics.sh <label> <screen> [flings] [swipe_ms]}"
SCREEN="${2:?usage: device-metrics.sh <label> <screen> [flings] [swipe_ms]}"
FLINGS="${3:-14}"
SWIPE_MS="${4:-60}"
# App background (shadowlist-utils theme colors.background). Pinned rather than sampled
# by default so every build is scored against the same reference.
BACKGROUND="${BACKGROUND-000000}"
# sweep = every fling in the same direction, so the run travels deep into the dataset
# instead of oscillating around one spot. A list only reveals its scaling behaviour when
# the scroll actually leaves the region it started in: offsets stop being cached, the
# measured window keeps moving into fresh rows, and any per-frame O(N) work shows up.
# local = alternating, which measures steady-state scrolling in one neighbourhood.
MODE="${MODE:-sweep}"
# Framebuffer samples. ~170ms each on-device, so this should span the whole run.
SHOT_COUNT="${SHOT_COUNT:-90}"
VIEWPORT_TOP="${VIEWPORT_TOP:-0.16}"
VIEWPORT_BOTTOM="${VIEWPORT_BOTTOM:-0.76}"

PKG=shadowlist.example
# Intentionally expanded unquoted below: ADB may carry arguments such as "-s SERIAL".
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

# Wall clock in nanoseconds. `date +%s%N` is not portable (older macOS prints a literal N).
now_ns() {
  python3 -c 'import time; print(time.time_ns())'
}

# ------------------------------------------------------------------ navigation
# Dump the view tree and return "x1 y1 x2 y2" for the first node matching an attribute
# pattern, or nothing. Tolerant by design: a miss is a caller decision, not a hard exit.
find_bounds() {
  $ADB shell uiautomator dump /sdcard/ui.xml >/dev/null 2>&1 || true
  $ADB shell cat /sdcard/ui.xml | tr '>' '\n' \
    | grep -F "$1" \
    | grep -oE 'bounds="\[[0-9]+,[0-9]+\]\[[0-9]+,[0-9]+\]"' | head -1 \
    | grep -oE '[0-9]+' | tr '\n' ' ' || true
}

# Open the drawer and tap the entry labelled $SCREEN.
navigate() {
  # Bring the app to the front first: a drawer gesture on the launcher silently does nothing.
  $ADB shell am start -n "$PKG/.MainActivity" >/dev/null 2>&1 || true
  sleep 4
  # Tap the header's drawer button rather than swiping from the edge: on a device using
  # gesture navigation the system back gesture owns the screen edge and swallows the swipe,
  # which navigates out of the app instead of opening the drawer.
  local menu
  menu=$(find_bounds 'content-desc="Show navigation menu"')
  if [[ -n "$menu" ]]; then
    read -r mx1 my1 mx2 my2 <<<"$menu"
    $ADB shell input tap $(((mx1 + mx2) / 2)) $(((my1 + my2) / 2)) >/dev/null
  fi
  sleep 1.5
  local bounds
  bounds=$(find_bounds "text=\"$SCREEN\"")
  if [[ -z "$bounds" ]]; then
    echo "!! could not find '$SCREEN' in the drawer" >&2
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

# Warm the list so we measure steady-state scrolling, not first-render.
for _ in 1 2 3; do
  $ADB shell input swipe "$CX" "$Y_LOW" "$CX" "$Y_HIGH" 200 >/dev/null
done
sleep 2

# ------------------------------------------------------- calibration + counters
$ADB shell "rm -rf $SHOTS; mkdir -p $SHOTS"
$ADB shell "screencap $SHOTS/cal.raw"
$ADB shell dumpsys gfxinfo "$PKG" reset >/dev/null
CPU_BEFORE=$($ADB shell "cat /proc/$PID/stat" | tr -d '\r' | awk '{print $14+$15}')
T_BEFORE=$(now_ns)

# Sample the framebuffer continuously on-device (~170ms/frame) for the whole run.
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
  # `input swipe` returns as soon as the events are injected, but the fling it starts
  # animates for the best part of a second. Without this the whole run is over before
  # the list has moved, and the samples describe a settled list.
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
