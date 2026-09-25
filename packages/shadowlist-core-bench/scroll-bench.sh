#!/usr/bin/env bash
#
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
#   iOS: UDID, IOS_FLING_SPEED=5000 (pt/s at the start of each in-app fling).  Android: ADB="adb -s emulator-5570", SHOT_COUNT=40,
#            EMULATOR_AVD=sl_perf_android, OTHER_EMU_MAX=30 (other emulators' CPU %)
#
set -euo pipefail

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
  rm -rf "$base.shots"; mkdir -p "$base.shots"
  ((SHOT_COUNT > 0)) && adb_ pull "$shots" "$base.shots" >/dev/null 2>&1 || true
  local clk; clk="$(adb_ shell getconf CLK_TCK | tr -d '\r')"
  python3 "$HERE/scroll-bench.py" android "$base" --t0 "$t0" --t1 "$t1" \
    --cpu0 "$cpu0" --cpu1 "$cpu1" --clk "${clk:-100}" --width "$W" --height "$H" \
    --viewport "$(chat "$screen" && echo "0.16 0.80" || echo "0.12 0.95")"
  rm -rf "$base.shots"
}

# Other projects' builds on this host starve the emulator and simulator. Start each run quiet.
LOAD_MAX="${LOAD_MAX:-10}"
# Other emulators share the host GPU, so their CPU (a proxy for their rendering) counts too.
OTHER_EMU_MAX="${OTHER_EMU_MAX:-30}"
# The AVD running the benchmark, left out of the other emulators' CPU.
EMULATOR_AVD="${EMULATOR_AVD:-sl_perf_android}"
other_emulators_cpu() {
  ps -Ao pcpu=,args= | awk '/qemu-system/ && !/$EMULATOR_AVD/ && !/awk/ {s += $1} END {print int(s)}'
}
wait_quiet() {
  while (( $(sysctl -n vm.loadavg | awk '{print int($2)}') >= LOAD_MAX )) ||
    (( $(other_emulators_cpu) >= OTHER_EMU_MAX )); do sleep 10; done
}

RUN_START="${RUN_START:-1}"
for ((run = RUN_START; run < RUN_START + RUNS; run++)); do
  for screen in $SCREENS; do
    wait_quiet
    "run_$PLATFORM" "$screen" "$run" || echo "!! $screen run $run failed" >&2
  done
done
python3 "$HERE/scroll-bench.py" summary "$OUT"
