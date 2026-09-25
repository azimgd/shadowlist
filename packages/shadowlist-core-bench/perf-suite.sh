#!/usr/bin/env bash
#
# Perf and correctness suite: aggressive flings on the benchmark screens at several list
# sizes, on an iOS simulator or an Android device, with MVCP and virtualization checks.
#
#   ./perf-suite.sh <ios|android> <label>
#   ./perf.py compare <label-before> <label-after>
#
# For each screen x count x run it writes results/perf/<label>/<platform>/<screen>-<count>-<run>.*
# and at the end summary.json + summary.md (medians over runs).
#
# The app opens straight on the screen holding exactly <count> rows: launch settings
# SLRoute, SLCount, SLLatency 0,0 and SLDebug 1 (debug header buttons for inserts).
#
# iOS runs ios-trace/run.sh on scenarios/perf-<screen>.steps with the fling count sized to the
# list, samples the app with ps (CPU, RSS), and reduces the trace with ios-trace/analyze.py:
# findings by kind (explained or not), jsms / mountms, Fabric commits per second of scrolling.
# Android flings with adb input swipe and reads gfxinfo framestats, screenshot blank bands
# and meminfo through analyze-metrics.py, [SLC] commits from logcat, and checks MVCP with
# uiautomator dumps before and after a prepend and an append while scrolled.
#
# Environment:
#   SCREENS="Feed FeedNative Chat ChatNative SectionList Masonry"   COUNTS="50 200 1000"   RUNS=3
#   FLING_PT=2500      average travel of one fling in pt; fling count = count x row height / this
#   FLINGS=<n>         force a fling count instead
#   EXTRA_LAUNCH_ARGS  more iOS launch arguments, e.g. "-SLEngineFlags NO"; SIMCTL_CHILD_<NAME>
#                      in the environment sets an app env var, e.g. SIMCTL_CHILD_SHADOWLIST_SCROLL_BAND=0
#   iOS:     UDID, DEVICE (agent-device name), default sl-bench. Build must have the trace
#            (Debug, or Release with SHADOWLIST_FRAME_TRACE_COMPILED=1); Debug needs Metro.
#   Android: ADB="adb -s emulator-5554" (or ANDROID_SERIAL), SHOT_COUNT=60, BACKGROUND=000000.
#            [SLC] needs a debug build or a release built with -PshadowlistFrameTrace.
#
set -euo pipefail

PLATFORM="${1:?usage: perf-suite.sh <ios|android> <label>}"
LABEL="${2:?usage: perf-suite.sh <ios|android> <label>}"
SCREENS="${SCREENS:-Feed FeedNative Chat ChatNative SectionList Masonry}"
COUNTS="${COUNTS:-50 200 1000}"
RUNS="${RUNS:-3}"
FLING_PT="${FLING_PT:-2500}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$HERE/results/perf/$LABEL/$PLATFORM"
PKG=shadowlist.example
mkdir -p "$OUT"

now() { python3 -c 'import time; print("%.4f" % (time.clock_gettime_ns(time.CLOCK_UPTIME_RAW) / 1e9))'; }
lower() { printf '%s' "$1" | tr '[:upper:]' '[:lower:]'; }

# Rough row height in pt per screen, to size the fling count so a run reaches the far end.
row_pt() {
  case "$1" in
    Feed | FeedNative) echo 300 ;;
    Chat | ChatNative) echo 270 ;;
    SectionList) echo 70 ;;
    Masonry) echo 100 ;;
    *) echo 200 ;;
  esac
}

fling_count() {
  if [[ -n "${FLINGS:-}" ]]; then echo "$FLINGS"; return; fi
  local n=$(( $2 * $(row_pt "$1") / FLING_PT + 2 ))
  (( n < 4 )) && n=4
  echo "$n"
}

inverted() { [[ "$1" == Chat || "$1" == ChatNative ]]; }

# ---------------------------------------------------------------------------- iOS

run_ios() {
  local screen="$1" count="$2" run="$3"
  local name; name="$(lower "$screen")"
  local base="$OUT/$screen-$count-$run"
  local steps="$HERE/ios-trace/scenarios/perf-$name.steps"
  [[ -f "$steps" ]] || { echo "!! no scenario $steps" >&2; return 1; }
  local flings; flings="$(fling_count "$screen" "$count")"
  echo "==> ios $screen count=$count run=$run flings=$flings"

  # Sample the app process on the host: the simulator app is a plain macOS process.
  local stop="$base.ps.stop"
  rm -f "$stop" "$base.ps.tsv"
  (
    while [[ ! -f "$stop" ]]; do
      pid="$(pgrep -f "$UDID/data/Containers/Bundle/Application/.*/ShadowListExample.app/ShadowListExample" | head -1 || true)"
      if [[ -n "$pid" ]]; then
        line="$(ps -o pid=,%cpu=,rss=,time= -p "$pid" 2>/dev/null || true)"
        [[ -n "$line" ]] && echo "$(now) $line" >> "$base.ps.tsv"
      fi
      sleep 0.5
    done
  ) &
  local sampler=$!

  local status=0
  UDID="$UDID" DEVICE="$DEVICE" LOG="$base.log" FLINGS="$flings" \
    LAUNCH_ARGS="-SLCount $count ${EXTRA_LAUNCH_ARGS:-}" \
    "$HERE/ios-trace/run.sh" "$steps" "$LABEL" > "$base.analyze.txt" 2>&1 || status=$?
  touch "$stop"
  wait "$sampler" 2>/dev/null || true
  rm -f "$stop"
  if (( status != 0 )) || [[ ! -f "$base.log.json" ]]; then
    echo "!! run failed ($status), see $base.analyze.txt" >&2
    tail -5 "$base.analyze.txt" >&2
    return 1
  fi
  grep -qF '[SLC]' "$base.log" || echo "   warning: no [SLC] commit lines (build without the trace?)" >&2
  python3 "$HERE/perf.py" ios-run "$base.log" "$base.ps.tsv" "$base.run.json"
  grep -m1 -A3 '^list ' "$base.analyze.txt" || true
}

# ---------------------------------------------------------------------------- Android

ADB="${ADB:-adb${ANDROID_SERIAL:+ -s $ANDROID_SERIAL}}"
adb_() { $ADB "$@"; }  # unquoted on purpose: ADB may carry "-s SERIAL"

swipe_forward() { # finger up on a normal list, finger down (into history) on an inverted one
  if inverted "$1"; then adb_ shell input swipe "$CX" "$Y_HIGH" "$CX" "$Y_LOW" "$SWIPE_MS" >/dev/null
  else adb_ shell input swipe "$CX" "$Y_LOW" "$CX" "$Y_HIGH" "$SWIPE_MS" >/dev/null; fi
}
swipe_back() {
  if inverted "$1"; then adb_ shell input swipe "$CX" "$Y_LOW" "$CX" "$Y_HIGH" "$SWIPE_MS" >/dev/null
  else adb_ shell input swipe "$CX" "$Y_HIGH" "$CX" "$Y_LOW" "$SWIPE_MS" >/dev/null; fi
}

ui_dump() { # <local file>
  adb_ shell uiautomator dump /sdcard/sl-ui.xml >/dev/null 2>&1 || true
  adb_ shell cat /sdcard/sl-ui.xml > "$1" 2>/dev/null || true
}

# Centers of the debug header buttons, "px py ax ay": the two clickable nodes left of More.
header_buttons() {
  python3 - "$1" <<'PY'
import re, sys, xml.etree.ElementTree as ET
try:
    root = ET.parse(sys.argv[1]).getroot()
except Exception:
    sys.exit(0)
def box(node):
    m = re.match(r"\[(\d+),(\d+)\]\[(\d+),(\d+)\]", node.get("bounds", ""))
    return tuple(int(v) for v in m.groups()) if m else None
more = next((box(n) for n in root.iter("node") if n.get("content-desc") == "More"), None)
if not more:
    sys.exit(0)
cy = (more[1] + more[3]) // 2
row = sorted({box(n) for n in root.iter("node")
              if n.get("clickable") == "true" and box(n) and box(n) != more
              and box(n)[1] <= cy <= box(n)[3] and box(n)[2] <= more[0] and box(n)[3] - box(n)[1] < 300})
if len(row) >= 3:
    prepend, append = row[-3], row[-2]
    print((prepend[0] + prepend[2]) // 2, (prepend[1] + prepend[3]) // 2,
          (append[0] + append[2]) // 2, (append[1] + append[3]) // 2)
PY
}

# Where the list sits, as screen height fractions, for blank bands and MVCP probes.
viewport() {
  case "$1" in
    Chat | ChatNative) echo "0.16 0.76" ;;
    FeedNative) echo "0.12 0.86" ;;
    *) echo "0.12 0.95" ;;
  esac
}

run_android() {
  local screen="$1" count="$2" run="$3"
  local base="$OUT/$screen-$count-$run"
  local flings; flings="$(fling_count "$screen" "$count")"
  local back=$(( flings * 3 / 4 > 0 ? flings * 3 / 4 : 1 ))
  inverted "$screen" && back="$flings"
  local half=$(( flings / 2 > 0 ? flings / 2 : 1 ))
  read -r VP_TOP VP_BOTTOM <<<"$(viewport "$screen")"
  echo "==> android $screen count=$count run=$run flings=$flings"

  adb_ shell cmd uimode night yes >/dev/null 2>&1 || true
  adb_ shell setprop log.tag.SLC D >/dev/null 2>&1 || true
  adb_ shell am force-stop "$PKG" >/dev/null 2>&1 || true
  adb_ logcat -c >/dev/null 2>&1 || true
  adb_ shell am start -n "$PKG/.MainActivity" --es SLRoute "$screen" --es SLCount "$count" \
    --es SLLatency 0,0 --es SLDebug 1 --es SLTheme dark >/dev/null
  # Wait for the list's first commit (debug JS can take ~20s to start on a loaded host).
  local ready=0
  for _ in $(seq 1 120); do
    if adb_ logcat -d -s SLC 2>/dev/null | grep -qF '[SLC]'; then ready=1; break; fi
    sleep 0.5
  done
  (( ready )) || echo "   warning: no [SLC] line yet (release build without -PshadowlistFrameTrace?)" >&2
  sleep 3
  local pid; pid="$(adb_ shell pidof "$PKG" | tr -d '\r')"
  [[ -n "$pid" ]] || { echo "!! app not running" >&2; return 1; }

  ui_dump "$base.ui-start.xml"
  local buttons; buttons="$(header_buttons "$base.ui-start.xml")"
  [[ -n "$buttons" ]] || echo "   warning: debug header buttons not found, skipping MVCP taps" >&2

  # Phase 1: flings to the far end and back, measured.
  local shots=/data/local/tmp/sl-perf-shots
  adb_ shell "rm -rf $shots; mkdir -p $shots"
  adb_ shell "screencap $shots/cal.raw"
  adb_ shell dumpsys gfxinfo "$PKG" reset >/dev/null
  adb_ logcat -c >/dev/null 2>&1 || true
  local cpu_before; cpu_before="$(adb_ shell "cat /proc/$pid/stat" | tr -d '\r' | awk '{print $14+$15}')"
  local t_before; t_before="$(now)"
  # Spread the screenshots over the whole fling run (each takes ~170ms on device).
  local est; est="$(python3 -c "print(max(0.0, ($flings + $back) * 0.6 / max(1, $SHOT_COUNT) - 0.17))")"
  local capture=""
  if (( SHOT_COUNT > 0 )); then
    adb_ shell "for i in \$(seq 1 $SHOT_COUNT); do screencap $shots/f\$i.raw; sleep $est; done" >/dev/null 2>&1 &
    capture=$!
  fi
  for _ in $(seq 1 "$flings"); do swipe_forward "$screen"; sleep 0.35; done
  sleep 1.5
  for _ in $(seq 1 "$back"); do swipe_back "$screen"; sleep 0.35; done
  sleep 1.5
  local t_after; t_after="$(now)"
  local cpu_after; cpu_after="$(adb_ shell "cat /proc/$pid/stat" | tr -d '\r' | awk '{print $14+$15}')"
  adb_ logcat -d -s SLC > "$base.slc.txt" 2>/dev/null || true
  python3 -c "print('%.3f' % ($t_after - $t_before))" > "$base.fling-s"
  [[ -n "$capture" ]] && { wait "$capture" 2>/dev/null || true; }
  adb_ shell dumpsys gfxinfo "$PKG" framestats | tr -d '\r' > "$base.gfx"
  adb_ shell dumpsys meminfo "$PKG" | tr -d '\r' > "$base.mem"
  local local_shots; local_shots="$(mktemp -d)"
  adb_ pull "$shots" "$local_shots" >/dev/null 2>&1 || true
  adb_ shell "rm -rf $shots"
  local clk; clk="$(adb_ shell getconf CLK_TCK | tr -d '\r')"
  python3 "$HERE/analyze-metrics.py" \
    --label "$LABEL" --screen "$screen-$count" --gfx "$base.gfx" --mem "$base.mem" \
    --shots "$local_shots/$(basename "$shots")" \
    --cpu-ticks $((cpu_after - cpu_before)) --clk "${clk:-100}" \
    --elapsed-ns "$(python3 -c "print(int(($t_after - $t_before) * 1e9))")" \
    --height "$HEIGHT" --width "$WIDTH" --background "${BACKGROUND-000000}" \
    --viewport-top "$VP_TOP" --viewport-bottom "$VP_BOTTOM" > "$base.metrics.txt"
  rm -rf "$local_shots"

  # Phase 2: MVCP at rest, scrolled into the list, then inserts mid-fling as extra stress.
  if [[ -n "$buttons" ]]; then
    read -r PX PY AX AY <<<"$buttons"
    for _ in $(seq 1 "$half"); do swipe_forward "$screen"; sleep 0.35; done
    sleep 2.5
    ui_dump "$base.mvcp-prepend-before.xml"
    adb_ shell input tap "$PX" "$PY"
    sleep 2.5
    ui_dump "$base.mvcp-prepend-after.xml"
    cp "$base.mvcp-prepend-after.xml" "$base.mvcp-append-before.xml"
    adb_ shell input tap "$AX" "$AY"
    sleep 2.5
    ui_dump "$base.mvcp-append-after.xml"
    swipe_forward "$screen"; adb_ shell input tap "$PX" "$PY"; sleep 2
    swipe_back "$screen"; adb_ shell input tap "$AX" "$AY"; sleep 2
  fi
  adb_ shell dumpsys meminfo "$PKG" | tr -d '\r' > "$base.mem-end"

  python3 "$HERE/perf.py" android-run "$base" "$base.run.json" \
    --top "$(python3 -c "print(int($HEIGHT * $VP_TOP))")" \
    --bottom "$(python3 -c "print(int($HEIGHT * $VP_BOTTOM))")"
  sed -n '/-- frames --/,/frames > 33/p' "$base.metrics.txt" | sed -n '2,6p'
  python3 -c "import json,sys; r=json.load(open(sys.argv[1])); print('   commits/s', r['commits_per_s'], ' mvcp', r.get('mvcp_prepend'), r.get('mvcp_append'))" "$base.run.json"
}

# ---------------------------------------------------------------------------- main

case "$PLATFORM" in
  ios)
    UDID="${UDID:-DEDB0C9F-6F48-4813-A2AA-41949887265C}"
    DEVICE="${DEVICE:-sl-bench}"
    xcrun simctl boot "$UDID" 2>/dev/null || true
    ;;
  android)
    SHOT_COUNT="${SHOT_COUNT:-60}"
    SWIPE_MS="${SWIPE_MS:-60}"
    read -r WIDTH HEIGHT < <(adb_ shell wm size | tr -d '\r' | tail -1 | sed 's/.*: //' | tr 'x' ' ')
    CX=$((WIDTH / 2))
    Y_LOW=$((HEIGHT * 78 / 100))
    Y_HIGH=$((HEIGHT * 25 / 100))
    ;;
  *) echo "platform must be ios or android" >&2; exit 2 ;;
esac

failed=0
for screen in $SCREENS; do
  for count in $COUNTS; do
    for run in $(seq 1 "$RUNS"); do
      if [[ "$PLATFORM" == ios ]]; then
        run_ios "$screen" "$count" "$run" || failed=$((failed + 1))
      else
        run_android "$screen" "$count" "$run" || failed=$((failed + 1))
      fi
    done
  done
done

python3 "$HERE/perf.py" summary "$OUT"
echo "==> $OUT/summary.md ($failed failed runs)"
