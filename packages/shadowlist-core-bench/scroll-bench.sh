#!/usr/bin/env bash
set -euo pipefail
# Fling the benchmark screens the same way on every run and report frame times and CPU per screen.
#
#   ./scroll-bench.sh <ios|android> <label>
#
# For each screen in SCREENS and each run it launches the example straight on the screen with
# SLCount rows, SLJsFps 1 (JS [SLFPS] and iOS [SLUI] frame lines), flings FLINGS times forward and
# BACK times back, and writes results/scroll/<label>/<platform>/<screen>-<run>.json:
#   js:  JS thread frames per second while flinging, share of JS frames over 20 ms, longest gap
#   ui:  Android gfxinfo framestats (p50/p90/p99, janky %), iOS [SLUI] main thread frames
#   cpu: app process CPU as a share of one core over the fling window
#   blank (Android): tallest empty band in screenshots taken during the flings
#
# Environment:
#   SCREENS="Feed Chat"  RUNS=3  COUNT=1000  FLINGS=20  BACK=10
#   LOAD_MAX=10 (wait for a 1-minute load average below it before each run)
#   OVERSCAN=<rows> (SLOverscan, overscanRows on Feed and Chat)
#   ENGINE_FLAGS=0|1 (the example's React Native feature flags, off in a stock app. 0 runs iOS with
#     -SLEngineFlags NO. Android's switch is the build property -PshadowlistEngineFlags=false, and
#     the script only checks it. Each run writes the state the app logged to <screen>-<run>.flags)
#   iOS: UDID, IOS_FLING_SPEED=5000 (pt/s at the start of each in-app fling).  Android: ADB="adb -s emulator-5570", SHOT_COUNT=40,
#            EMULATOR_AVD=<avd name>, OTHER_EMU_MAX=30 (other emulators' CPU %)
#
# List comparison (LISTS set): the example's Bench route renders the same rows with each list
# library (src/bench/BenchScreen.tsx) and every run is list-agnostic: native viewport monitors
# score blank area by row testIDs, [SLUI]/[SLPHASE]/gfxinfo time UI frames, [SLFPS] JS frames.
#   LISTS="shadowlist legend flash flat"  SCENARIOS="fixed text image"  COUNTS="1000 10000"
#   RUNS=5 measured runs after WARMUP=1 discarded one. The list order rotates every run (A B C D,
#   B C D A, ...), so machine noise hits every list alike.
#   DRAW=500 render window past each viewport edge in pt/dp (SLDraw). SLOW_SPEED=450 pt/s for
#   SLOW_SECONDS=5, then FLINGS forward and BACK back. OPS=1 adds a second launch per run that
#   times data updates and follows the anchor row ([SLOP], [SLANCHOR]).
#   MEASURE_LOCK=<dir> takes that lock around each batch of BATCH_RUNS runs, never around builds.
#   VIDEO=1 records the warm-up run's scroll to <out>/video.
#   Results: results/compare/<label>/<platform>/<scenario>-<count>/<list>-<run>.{log,json,ops.json}

PLATFORM="${1:?usage: scroll-bench.sh <ios|android> <label>}"
LABEL="${2:?usage: scroll-bench.sh <ios|android> <label>}"
SCREENS="${SCREENS:-Feed Chat}"
RUNS="${RUNS:-3}"
COUNT="${COUNT:-1000}"
FLINGS="${FLINGS:-20}"
BACK="${BACK:-10}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$HERE/results/scroll/$LABEL/$PLATFORM"
PKG=shadowlist.example
mkdir -p "$OUT"

now() { python3 -c 'import time; print("%.4f" % time.time())'; }
chat() { [[ "$1" == *Chat ]]; }

# The engine flags line the app logged at start, as "on, commit branching off" or "off".
record_flags() { # <log text> <out file>
  local flags
  flags="$(grep -o '\[SL\] engine flags.*' <<<"$1" | tail -1 | sed 's/^\[SL\] engine flags //' | tr -d '\r' || true)"
  echo "${flags:-unknown}" > "$2"
  if [[ -n "${ENGINE_FLAGS:-}" && "${flags:-unknown}" != unknown ]]; then
    local want=on; [[ "$ENGINE_FLAGS" == 0 ]] && want=off
    [[ "${flags%%,*}" == "$want" ]] || echo "!! ENGINE_FLAGS=$ENGINE_FLAGS but the app logged engine flags $flags" >&2
  fi
  echo "   engine flags ${flags:-unknown}"
}

settings() { # key value pairs, shared by both platforms
  printf '%s\n' SLRoute "$1" SLCount "$COUNT" SLLatency 0,0 SLTheme dark SLJsFps 1
  [[ -n "${OVERSCAN:-}" ]] && printf '%s\n' SLOverscan "$OVERSCAN"
  return 0
}

# ---------------------------------------------------------------------------- iOS

IOS_FLING_SPEED="${IOS_FLING_SPEED:-5000}"

run_ios() {
  local screen="$1" run="$2" base="$OUT/$1-$2"
  local args=()
  while read -r key && read -r value; do args+=("-$key" "$value"); done < <(settings "$screen")
  # Flings come from the app itself (AutoFlingDriver in AppDelegate.swift): XCTest gestures
  # snapshot the accessibility tree on the main thread per gesture, which skews the numbers.
  # Forward is down a feed, up into a chat's history.
  local speed="$IOS_FLING_SPEED"
  chat "$screen" && speed="-$speed"
  args+=(-SLAutoFling "6,$FLINGS,$BACK,$speed")
  case "${ENGINE_FLAGS:-}" in
    0) args+=(-SLEngineFlags NO) ;;
    1) args+=(-SLEngineFlags YES) ;;
  esac
  echo "==> ios $screen run=$run"
  xcrun simctl terminate "$UDID" "$PKG" >/dev/null 2>&1 || true
  xcrun simctl launch --console-pty "$UDID" "$PKG" "${args[@]}" > "$base.log" 2>&1 &
  local console=$!
  local t0="" cpu0="" l0="" pid=""
  for _ in $(seq 1 600); do
    if [[ -z "$l0" ]] && grep -q '\[SLBENCH\] start' "$base.log"; then
      # NSLog lines start with "ShadowListExample[<pid>:" (simctl's own pid line is buffered).
      pid="$(grep -m1 -oE 'ShadowListExample\[[0-9]+' "$base.log" | tr -dc '0-9')"
      t0="$(now)"; cpu0="$(ps -o time= -p "$pid" | tr -d ' ')"; l0="$(wc -l < "$base.log")"
    fi
    grep -q '\[SLBENCH\] \(end\|no scroll view\)' "$base.log" && break
    sleep 0.2
  done
  local t1 cpu1 l1
  t1="$(now)"; cpu1="$(ps -o time= -p "$pid" | tr -d ' ')"; l1="$(wc -l < "$base.log")"
  sleep 1.2
  kill "$console" 2>/dev/null || true
  grep -q '\[SLBENCH\] end' "$base.log" || { echo "!! no [SLBENCH] end" >&2; return 1; }
  record_flags "$(cat "$base.log")" "$base.flags"
  python3 "$HERE/scroll-bench.py" ios "$base" --t0 "$t0" --t1 "$t1" --cpu0 "$cpu0" --cpu1 "$cpu1" \
    --lines "$l0 $l1"
}

# ---------------------------------------------------------------------------- Android

ADB="${ADB:-adb${ANDROID_SERIAL:+ -s $ANDROID_SERIAL}}"
adb_() { $ADB "$@"; }  # unquoted on purpose: ADB may carry "-s SERIAL"
# CPU ticks per thread, "comm|ticks" lines. Holds up under host contention better than frame times.
thread_ticks() {
  adb_ shell "for t in /proc/$1/task/*; do echo \"\$(cat \$t/comm)|\$(sed 's/.*) //' \$t/stat | cut -d' ' -f12,13)\"; done" | tr -d '\r'
}
SHOT_COUNT="${SHOT_COUNT:-40}"

run_android() {
  local screen="$1" run="$2" base="$OUT/$1-$2"
  local extras=()
  while read -r key && read -r value; do extras+=(--es "$key" "$value"); done < <(settings "$screen")
  read -r W H < <(adb_ shell wm size | tr -d '\r' | sed 's/.*: //' | tr 'x' ' ')
  local cx=$((W / 2)) ylo=$((H * 75 / 100)) yhi=$((H * 30 / 100))
  echo "==> android $screen run=$run"
  adb_ shell cmd uimode night yes >/dev/null 2>&1 || true
  adb_ shell am force-stop "$PKG" >/dev/null 2>&1 || true
  adb_ logcat -c >/dev/null 2>&1 || true
  adb_ shell am start -n "$PKG/.MainActivity" "${extras[@]}" >/dev/null
  sleep 8
  local pid; pid="$(adb_ shell pidof "$PKG" | tr -d '\r')"
  [[ -n "$pid" ]] || { echo "!! app not running" >&2; return 1; }
  local shots=/data/local/tmp/slbench-shots
  adb_ shell "rm -rf $shots; mkdir -p $shots"
  adb_ shell dumpsys gfxinfo "$PKG" reset >/dev/null
  local cpu0 cpu1 t0 t1
  cpu0="$(adb_ shell cat /proc/$pid/stat | tr -d '\r' | awk '{print $14+$15}')"
  thread_ticks "$pid" > "$base.threads0"
  t0="$(adb_ shell 'echo $EPOCHREALTIME' | tr -d '\r')"
  local capture=""
  if ((SHOT_COUNT > 0)); then
    adb_ shell "sleep 0.5; for i in \$(seq 1 $SHOT_COUNT); do screencap $shots/f\$i.raw; done" >/dev/null 2>&1 &
    capture=$!
  fi
  local fy1=$ylo fy2=$yhi
  if chat "$screen"; then fy1=$yhi; fy2=$ylo; fi
  for ((i = 0; i < FLINGS; i++)); do adb_ shell input swipe "$cx" "$fy1" "$cx" "$fy2" 60 >/dev/null; sleep 0.25; done
  for ((i = 0; i < BACK; i++)); do adb_ shell input swipe "$cx" "$fy2" "$cx" "$fy1" 60 >/dev/null; sleep 0.25; done
  sleep 1.5
  cpu1="$(adb_ shell cat /proc/$pid/stat | tr -d '\r' | awk '{print $14+$15}')"
  thread_ticks "$pid" > "$base.threads1"
  t1="$(adb_ shell 'echo $EPOCHREALTIME' | tr -d '\r')"
  [[ -n "$capture" ]] && { wait "$capture" 2>/dev/null || true; }
  adb_ shell dumpsys gfxinfo "$PKG" framestats | tr -d '\r' > "$base.gfx"
  adb_ shell dumpsys meminfo "$PKG" | tr -d '\r' > "$base.mem"
  adb_ logcat -d -v epoch -s ReactNativeJS | tr -d '\r' > "$base.log"
  record_flags "$(adb_ logcat -d -s SL:I 2>/dev/null | tr -d '\r')" "$base.flags"
  rm -rf "$base.shots"; mkdir -p "$base.shots"
  ((SHOT_COUNT > 0)) && adb_ pull "$shots" "$base.shots" >/dev/null 2>&1 || true
  local clk; clk="$(adb_ shell getconf CLK_TCK | tr -d '\r')"
  python3 "$HERE/scroll-bench.py" android "$base" --t0 "$t0" --t1 "$t1" \
    --cpu0 "$cpu0" --cpu1 "$cpu1" --clk "${clk:-100}" --width "$W" --height "$H" \
    --viewport "$(chat "$screen" && echo "0.16 0.80" || echo "0.12 0.95")"
  rm -rf "$base.shots"
}

# ---------------------------------------------------------------------------- list comparison

DRAW="${DRAW:-500}"
SLOW_SPEED="${SLOW_SPEED:-450}"
SLOW_SECONDS="${SLOW_SECONDS:-5}"
WARMUP="${WARMUP:-1}"
OPS="${OPS:-1}"
BATCH_RUNS="${BATCH_RUNS:-3}"
CMP="$HERE/results/compare/$LABEL/$PLATFORM"

bench_settings() { # list scenario count ops
  printf '%s\n' SLRoute Bench SLList "$1" SLBench "$2" SLCount "$3" SLDraw "$DRAW" SLTheme dark SLLatency 0,0 SLJsFps 1
  [[ "$4" == 1 ]] && printf '%s\n' SLOps 1
  return 0
}

LOCK_HELD=0
lock_take() {
  [[ -n "${MEASURE_LOCK:-}" ]] || return 0
  until mkdir "$MEASURE_LOCK" 2>/dev/null; do
    # A lock older than 25 minutes is stale.
    if [[ -n "$(find "$MEASURE_LOCK" -maxdepth 0 -mmin +25 2>/dev/null)" ]]; then rm -rf "$MEASURE_LOCK"; continue; fi
    sleep 0.3
  done
  echo "react-native $(date)" > "$MEASURE_LOCK/owner"
  LOCK_HELD=1
}
# Drops the lock only if this script took it and its owner file still names this lane.
lock_drop() {
  if [[ -n "${MEASURE_LOCK:-}" && "$LOCK_HELD" == 1 ]] &&
    grep -q "^react-native " "$MEASURE_LOCK/owner" 2>/dev/null; then
    rm -rf "$MEASURE_LOCK"
  fi
  LOCK_HELD=0
  return 0
}

# Waits for a line in a file, up to n tenths of a second. Returns 1 if it never came.
wait_line() { # file pattern tenths
  for _ in $(seq 1 "$3"); do grep -q "$2" "$1" 2>/dev/null && return 0; sleep 0.1; done
  return 1
}

ios_pid() { pgrep -f "$UDID/data/Containers/Bundle/Application/.*/ShadowListExample.app/ShadowListExample" | head -1; }

compare_ios() { # list scenario count run dir
  local list="$1" scenario="$2" count="$3" run="$4" dir="$5" base="$5/$1-$4"
  local args=()
  while read -r key && read -r value; do args+=("-$key" "$value"); done < <(bench_settings "$list" "$scenario" "$count" 0)
  args+=(-SLAutoSlow "$SLOW_SPEED,$SLOW_SECONDS" -SLAutoFling "1,$FLINGS,$BACK,$IOS_FLING_SPEED")
  case "${ENGINE_FLAGS:-}" in 0) args+=(-SLEngineFlags NO) ;; 1) args+=(-SLEngineFlags YES) ;; esac
  xcrun simctl terminate "$UDID" "$PKG" >/dev/null 2>&1 || true
  xcrun simctl launch --console-pty "$UDID" "$PKG" "${args[@]}" > "$base.log" 2>&1 &
  local console=$! pid="" video=""
  # RSS samples, 5 a second, for the peak.
  : > "$base.rss"
  ( while kill -0 "$console" 2>/dev/null; do p="$(ios_pid)"; [[ -n "$p" ]] && echo "$(now) $(ps -o rss=,time= -p "$p" 2>/dev/null)" >> "$base.rss"; sleep 0.2; done ) &
  local sampler=$!
  wait_line "$base.log" '\[SLBENCH\] start' 600 || { kill "$console" "$sampler" 2>/dev/null; echo "!! $list no start" >&2; return 1; }
  pid="$(ios_pid)"
  local cpu0 t0 cpu1 t1
  cpu0="$(ps -o time= -p "$pid" | tr -d ' ')"; t0="$(now)"
  if [[ "${VIDEO:-0}" == 1 && "$run" == 0 ]]; then
    mkdir -p "$CMP/video"
    xcrun simctl io "$UDID" recordVideo --codec h264 --force "$CMP/video/$scenario-$count-$list.mp4" >/dev/null 2>&1 &
    video=$!
  fi
  wait_line "$base.log" '\[SLBENCH\] \(end\|no scroll view\)' 1800 || echo "!! $list no end" >&2
  cpu1="$(ps -o time= -p "$pid" | tr -d ' ')"; t1="$(now)"
  [[ -n "$video" ]] && { kill -INT "$video"; wait "$video" 2>/dev/null || true; }
  kill "$console" "$sampler" 2>/dev/null || true
  wait "$sampler" 2>/dev/null || true
  record_flags "$(cat "$base.log")" "$base.flags"
  python3 "$HERE/scroll-bench.py" compare-ios "$base" --t0 "$t0" --t1 "$t1" --cpu0 "$cpu0" --cpu1 "$cpu1"
}

compare_ios_ops() { # list scenario count run dir
  local list="$1" base="$5/$1-$4.ops"
  local args=()
  while read -r key && read -r value; do args+=("-$key" "$value"); done < <(bench_settings "$1" "$2" "$3" 1)
  case "${ENGINE_FLAGS:-}" in 0) args+=(-SLEngineFlags NO) ;; 1) args+=(-SLEngineFlags YES) ;; esac
  xcrun simctl terminate "$UDID" "$PKG" >/dev/null 2>&1 || true
  xcrun simctl launch --console-pty "$UDID" "$PKG" "${args[@]}" > "$base.log" 2>&1 &
  local console=$!
  wait_line "$base.log" '\[SLBENCH\] ops-done' 900 || echo "!! $list ops never finished" >&2
  sleep 0.3
  kill "$console" 2>/dev/null || true
  python3 "$HERE/scroll-bench.py" compare-ops "$base"
}

# Device epoch seconds, the clock logcat -v epoch stamps lines with.
dev_now() { adb_ shell 'echo $EPOCHREALTIME' | tr -d '\r'; }
dev_ticks() { adb_ shell cat "/proc/$1/stat" | tr -d '\r' | awk '{print $14+$15}'; }

compare_android() { # list scenario count run dir
  local list="$1" scenario="$2" count="$3" run="$4" dir="$5" base="$5/$1-$4"
  local extras=()
  while read -r key && read -r value; do extras+=(--es "$key" "$value"); done < <(bench_settings "$list" "$scenario" "$count" 0)
  read -r W H < <(adb_ shell wm size | tr -d '\r' | sed 's/.*: //' | tr 'x' ' ')
  local cx=$((W / 2)) ylo=$((H * 75 / 100)) yhi=$((H * 30 / 100))
  # Slow drags: SLOW_SPEED dp/s over the middle of the screen, one second each.
  local density; density="$(adb_ shell wm density | tr -d '\r' | awk '{print $NF}' | tail -1)"
  local slow_px; slow_px="$(python3 -c "print(int($SLOW_SPEED * $density / 160))")"
  local sy1=$((H / 2 + slow_px / 2)) sy2=$((H / 2 - slow_px / 2))
  adb_ shell am force-stop "$PKG" >/dev/null 2>&1 || true
  adb_ logcat -c >/dev/null 2>&1 || true
  adb_ shell am start -n "$PKG/.MainActivity" "${extras[@]}" >/dev/null
  local ready=0
  for _ in $(seq 1 600); do
    adb_ logcat -d -s ReactNativeJS 2>/dev/null | grep -q '\[SLBENCH\] ready' && { ready=1; break; }
    sleep 0.1
  done
  ((ready)) || { echo "!! $list never ready" >&2; return 1; }
  local pid; pid="$(adb_ shell pidof "$PKG" | tr -d '\r')"
  local video=""
  if [[ "${VIDEO:-0}" == 1 && "$run" == 0 ]]; then
    mkdir -p "$CMP/video"
    adb_ shell "screenrecord --time-limit 60 /data/local/tmp/sl-cmp.mp4" >/dev/null 2>&1 &
    video=$!
  fi
  # Phase 1: slow drags.
  adb_ shell dumpsys gfxinfo "$PKG" reset >/dev/null
  local s0 c0 s1 c1 f0 f1 e0 e1
  s0="$(dev_now)"; c0="$(dev_ticks "$pid")"
  for ((i = 0; i < SLOW_SECONDS; i++)); do adb_ shell input swipe "$cx" "$sy1" "$cx" "$sy2" 1000 >/dev/null; done
  sleep 1
  s1="$(dev_now)"; c1="$(dev_ticks "$pid")"
  adb_ shell dumpsys gfxinfo "$PKG" framestats | tr -d '\r' > "$base.slow.gfx"
  # Phase 2: flings forward, then back.
  adb_ shell dumpsys gfxinfo "$PKG" reset >/dev/null
  thread_ticks "$pid" > "$base.threads0"
  f0="$(dev_now)"; e0="$(dev_ticks "$pid")"
  for ((i = 0; i < FLINGS; i++)); do adb_ shell input swipe "$cx" "$ylo" "$cx" "$yhi" 60 >/dev/null; sleep 0.25; done
  for ((i = 0; i < BACK; i++)); do adb_ shell input swipe "$cx" "$yhi" "$cx" "$ylo" 60 >/dev/null; sleep 0.25; done
  sleep 1.5
  f1="$(dev_now)"; e1="$(dev_ticks "$pid")"
  thread_ticks "$pid" > "$base.threads1"
  adb_ shell dumpsys gfxinfo "$PKG" framestats | tr -d '\r' > "$base.gfx"
  adb_ shell dumpsys meminfo "$PKG" | tr -d '\r' > "$base.mem"
  adb_ shell cat "/proc/$pid/status" | tr -d '\r' > "$base.status"
  adb_ logcat -d -v epoch -s ReactNativeJS SLBench SL | tr -d '\r' > "$base.log"
  if [[ -n "$video" ]]; then
    adb_ shell pkill -INT screenrecord >/dev/null 2>&1 || true
    wait "$video" 2>/dev/null || true
    sleep 1
    adb_ pull /data/local/tmp/sl-cmp.mp4 "$CMP/video/$scenario-$count-$list.mp4" >/dev/null 2>&1 || true
  fi
  record_flags "$(cat "$base.log")" "$base.flags"
  local clk; clk="$(adb_ shell getconf CLK_TCK | tr -d '\r')"
  python3 "$HERE/scroll-bench.py" compare-android "$base" --slow "$s0 $s1 $c0 $c1" --fling "$f0 $f1 $e0 $e1" --clk "${clk:-100}"
}

compare_android_ops() { # list scenario count run dir
  local list="$1" base="$5/$1-$4.ops"
  local extras=()
  while read -r key && read -r value; do extras+=(--es "$key" "$value"); done < <(bench_settings "$1" "$2" "$3" 1)
  adb_ shell am force-stop "$PKG" >/dev/null 2>&1 || true
  adb_ logcat -c >/dev/null 2>&1 || true
  adb_ shell am start -n "$PKG/.MainActivity" "${extras[@]}" >/dev/null
  for _ in $(seq 1 900); do
    adb_ logcat -d -s ReactNativeJS 2>/dev/null | grep -q '\[SLBENCH\] ops-done' && break
    sleep 0.1
  done
  sleep 0.3
  adb_ logcat -d -v epoch -s ReactNativeJS SLBench | tr -d '\r' > "$base.log"
  python3 "$HERE/scroll-bench.py" compare-ops "$base"
}

run_compare() {
  local lists=($LISTS) runs_total=$((WARMUP + RUNS))
  for scenario in ${SCENARIOS:-fixed text image}; do
    for count in ${COUNTS:-1000 10000}; do
      local dir="$CMP/$scenario-$count"
      mkdir -p "$dir"
      for ((run = 0; run < runs_total; run++)); do
        if (( run % BATCH_RUNS == 0 )); then lock_drop; wait_quiet; lock_take; fi
        local n=${#lists[@]}
        for ((k = 0; k < n; k++)); do
          local list="${lists[$(( (k + run) % n ))]}"
          echo "==> $PLATFORM $scenario $count run=$run $list"
          "compare_$PLATFORM" "$list" "$scenario" "$count" "$run" "$dir" || echo "!! $list run $run failed" >&2
          [[ "$OPS" == 1 ]] && { "compare_${PLATFORM}_ops" "$list" "$scenario" "$count" "$run" "$dir" || echo "!! $list ops $run failed" >&2; }
        done
      done
      lock_drop
    done
  done
  python3 "$HERE/scroll-bench.py" compare-summary "$CMP" --warmup "$WARMUP"
}

# Other projects' builds on this host starve the emulator and simulator. Start each run quiet.
LOAD_MAX="${LOAD_MAX:-10}"
# Other emulators share the host GPU. Their CPU (a proxy for their rendering) counts too.
OTHER_EMU_MAX="${OTHER_EMU_MAX:-30}"
# The AVD running the benchmark, left out of the other emulators' CPU.
EMULATOR_AVD="${EMULATOR_AVD:-}"
other_emulators_cpu() {
  ps -Ao pcpu=,args= | awk -v avd="$EMULATOR_AVD" '/qemu-system/ && !/awk/ && (avd == "" || index($0, avd) == 0) {s += $1} END {print int(s)}'
}
wait_quiet() {
  while (( $(sysctl -n vm.loadavg | awk '{print int($2)}') >= LOAD_MAX )) ||
    (( $(other_emulators_cpu) >= OTHER_EMU_MAX )); do sleep 10; done
}

if [[ -n "${LISTS:-}" ]]; then
  trap lock_drop EXIT
  run_compare
  exit 0
fi

RUN_START="${RUN_START:-1}"
for ((run = RUN_START; run < RUN_START + RUNS; run++)); do
  for screen in $SCREENS; do
    wait_quiet
    "run_$PLATFORM" "$screen" "$run" || echo "!! $screen run $run failed" >&2
  done
done
python3 "$HERE/scroll-bench.py" summary "$OUT"
