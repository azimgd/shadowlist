#!/usr/bin/env bash
#
# Scroll benchmark across list engines. Each run launches the app on one screen, lets SLKBench
# drive the list at a constant speed and appends its [SLBENCH] JSON line to a results file.
#
#   ./bench.sh <sim|device> <udid> <label>
#
# Environment:
#   ENGINES="sl table table-auto sl-auto"   UIKit example engines, "sl+async" for background
#                                           text drawing, plus "rn" for the
#                                           React Native example (simulator only, needs a
#                                           Release build installed as shadowlist.example)
#   SCREENS="Feed Chat SectionList Masonry"  COUNTS="1000"  RUNS=3
#   SPEED=4000  SECONDS_PER_LEG=6  DELAY=4
#   AXES=y                                   y, x or xy, comma separated. One result line per axis.
#
# Runs are interleaved engine by engine inside each round. Host load hits every engine alike.
# Results: ../results/<label>/runs.jsonl, then ./summarize.py ../results/<label>/runs.jsonl
#
set -uo pipefail
KIND="${1:?sim|device}"; UDID="${2:?udid}"; LABEL="${3:?label}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINES="${ENGINES:-sl table table-auto sl-auto}"
SCREENS="${SCREENS:-Feed Chat SectionList Masonry}"
COUNTS="${COUNTS:-1000}"
RUNS="${RUNS:-3}"
SPEED="${SPEED:-4000}"
SECONDS_PER_LEG="${SECONDS_PER_LEG:-6}"
DELAY="${DELAY:-4}"
AXES="${AXES:-y}"
OUT="$HERE/../results/$LABEL"
mkdir -p "$OUT"
RESULTS="$OUT/runs.jsonl"
DYLIB="$HERE/../build/SLKBench-sim.dylib"

build_dylib() {
  [[ -f "$DYLIB" && "$DYLIB" -nt "$HERE/SLKBench.m" ]] && return
  mkdir -p "$(dirname "$DYLIB")"
  xcrun --sdk iphonesimulator clang -dynamiclib -fobjc-arc -O2 -target arm64-apple-ios16.0-simulator \
    -framework UIKit -framework QuartzCore -framework CoreGraphics -framework Foundation "$HERE/SLKBench.m" -o "$DYLIB"
  codesign -f -s - "$DYLIB" >/dev/null 2>&1
}

run_one() {
  local engine="$1" screen="$2" count="$3" run="$4"
  local bundle=shadowlist.uikit.example
  local args=(-SLRoute "$screen" -SLCount "$count" -SLLatency 0,0 -SLDebug 1
    -SLBench 1 -SLBenchExit 1 -SLBenchSpeed "$SPEED" -SLBenchSeconds "$SECONDS_PER_LEG" -SLBenchDelay "$DELAY"
    -SLBenchAxes "$AXES" -SLBenchLabel "$engine")
  # An engine name can carry options after a plus: sl+async draws text on a background queue.
  local base="${engine%%+*}"
  if [[ "$base" == rn ]]; then
    bundle=shadowlist.example
  else
    args+=(-SLEngine "$base")
  fi
  [[ "$engine" == *+async* ]] && args+=(-SLTextAsync 1)
  local log="$OUT/$engine-$screen-$count-$run.log"
  local axes_count
  axes_count=$(( $(tr -cd ',' <<< "$AXES" | wc -c) + 1 ))
  local limit=$(( DELAY + axes_count * (2 * SECONDS_PER_LEG + 1) + 40 ))
  if [[ "$KIND" == sim ]]; then
    if [[ "$base" == rn ]]; then
      SIMCTL_CHILD_DYLD_INSERT_LIBRARIES="$DYLIB" timeout "$limit" \
        xcrun simctl launch --console-pty --terminate-running-process "$UDID" "$bundle" "${args[@]}" > "$log" 2>&1
    else
      timeout "$limit" xcrun simctl launch --console-pty --terminate-running-process "$UDID" "$bundle" "${args[@]}" > "$log" 2>&1
    fi
  else
    timeout "$limit" xcrun devicectl device process launch --console --terminate-existing --device "$UDID" "$bundle" -- "${args[@]}" > "$log" 2>&1
  fi
  local lines
  lines="$(grep -o '\[SLBENCH\] {.*}' "$log" | sed 's/^\[SLBENCH\] //')"
  if [[ -z "$lines" ]]; then
    echo "!! $engine $screen $count run $run: no result (see $log)"
    return
  fi
  local line
  while IFS= read -r line; do
    python3 -c 'import json,sys; r=json.loads(sys.argv[1]); r.update(engine=sys.argv[2], screen=sys.argv[3], count=int(sys.argv[4]), run=int(sys.argv[5]), kind=sys.argv[6]); print(json.dumps(r))' \
      "$line" "$engine" "$screen" "$count" "$run" "$KIND" >> "$RESULTS"
    python3 -c 'import json,sys; r=json.loads(sys.argv[1]); a=r.get("axis","y"); print("   %-10s %-11s n=%-5s %-2s skipped: %s" % (sys.argv[2], sys.argv[3], sys.argv[4], a, r["skipped"]) if "skipped" in r else "   %-10s %-11s n=%-5s %-2s hitch=%6.1fms/s dropped=%-4d p99=%5.1fms main=%6.1fms/s blank=%.3f" % (sys.argv[2], sys.argv[3], sys.argv[4], a, r["hitchMsPerS"], r["dropped"], r["p99"], r["mainCpuMsPerS"], r["blankAvg"]))' \
      "$line" "$engine" "$screen" "$count"
  done <<< "$lines"
}

[[ " $ENGINES " == *" rn "* ]] && build_dylib
for run in $(seq 1 "$RUNS"); do
  for count in $COUNTS; do
    for screen in $SCREENS; do
      for engine in $ENGINES; do
        run_one "$engine" "$screen" "$count" "$run"
      done
    done
  done
done
echo "results: $RESULTS"
