#!/usr/bin/env bash
#
# Scroll benchmark across list engines on Android. Each run appends one SLBENCH JSON line.
#
#   ./bench.sh <label>                     (ANDROID_SERIAL picks the device)
#
# Environment:
#   ENGINES="sl recycler recycler-auto sl-auto rn"
#     rn is the React Native example (shadowlist.example) built with ShadowListKitBench in it:
#     cd templates/shadowlist-fabric-example/android && ./gradlew \
#       --init-script ../../../packages/shadowlist-android/Bench/rn/slbench-init.gradle \
#       app:assembleRelease -PreactNativeArchitectures=arm64-v8a
#     then adb install -r app/build/outputs/apk/release/app-release.apk
#   SCREENS="Feed Chat SectionList Masonry"  COUNTS="1000"  RUNS=3
#   SPEED=4000 (dp/s)  SECONDS_PER_LEG=6  DELAY=4  IMAGES=1
#   AXES=y                                   y, x or xy, comma separated. One result line per axis.
#
# Results: ../results/<label>/runs.jsonl, then ./summarize.py ../results/<label>/runs.jsonl
#
set -uo pipefail
LABEL="${1:?label}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINES="${ENGINES:-sl recycler recycler-auto sl-auto}"
SCREENS="${SCREENS:-Feed Chat SectionList Masonry}"
COUNTS="${COUNTS:-1000}"
RUNS="${RUNS:-3}"
SPEED="${SPEED:-4000}"
SECONDS_PER_LEG="${SECONDS_PER_LEG:-6}"
DELAY="${DELAY:-4}"
IMAGES="${IMAGES:-1}"
AXES="${AXES:-y}"
OUT="$HERE/../results/$LABEL"
ACTIVITY=shadowlist.android.example/com.shadowlist.kit.example.MainActivity
RN_ACTIVITY=shadowlist.example/.MainActivity
mkdir -p "$OUT"
RESULTS="$OUT/runs.jsonl"

run_one() {
  local engine="$1" screen="$2" count="$3" run="$4"
  local log="$OUT/$engine-$screen-$count-$run.log"
  local axes_count
  axes_count=$(( $(tr -cd ',' <<< "$AXES" | wc -c) + 1 ))
  local limit=$(( DELAY + axes_count * (2 * SECONDS_PER_LEG + 1) + 30 ))
  local activity="$ACTIVITY"
  # The React Native example reads SLRoute and SLCount. It ignores SLEngine and SLImages.
  [[ "$engine" == rn ]] && activity="$RN_ACTIVITY"
  adb logcat -c
  adb shell am start -S -n "$activity" --es SLRoute "$screen" --es SLEngine "$engine" --es SLCount "$count" \
    --es SLImages "$IMAGES" --es SLBench 1 --es SLBenchExit 1 --es SLBenchSpeed "$SPEED" \
    --es SLBenchSeconds "$SECONDS_PER_LEG" --es SLBenchDelay "$DELAY" --es SLBenchAxes "$AXES" --es SLBenchLabel "$engine" >/dev/null
  local lines="" waited=0
  while [[ $(grep -c . <<< "$lines") -lt $axes_count && $waited -lt $limit ]]; do
    sleep 1
    waited=$((waited + 1))
    lines="$(adb logcat -d -s SLBENCH:I 2>/dev/null | grep -o '{.*}')"
  done
  adb logcat -d -s SLBENCH:I AndroidRuntime:E > "$log" 2>&1
  if [[ -z "$lines" ]]; then
    echo "!! $engine $screen $count run $run: no result (see $log)"
    return
  fi
  local line
  while IFS= read -r line; do
    python3 -c 'import json,sys; r=json.loads(sys.argv[1]); r.update(engine=sys.argv[2], screen=sys.argv[3], count=int(sys.argv[4]), run=int(sys.argv[5]), kind="android"); print(json.dumps(r))' \
      "$line" "$engine" "$screen" "$count" "$run" >> "$RESULTS"
    python3 -c 'import json,sys; r=json.loads(sys.argv[1]); a=r.get("axis","y"); print("   %-14s %-11s n=%-5s %-2s skipped: %s" % (sys.argv[2], sys.argv[3], sys.argv[4], a, r["skipped"]) if "skipped" in r else "   %-14s %-11s n=%-5s %-2s hitch=%6.1fms/s janky=%-4d ui=%6.1fms/s render=%6.1fms/s proc=%6.1fms/s blank=%.3f" % (sys.argv[2], sys.argv[3], sys.argv[4], a, r["hitchMsPerS"], r["jankyFrames"], r["mainCpuMsPerS"], r["renderCpuMsPerS"], r["processCpuMsPerS"], r["blankAvg"]))' \
      "$line" "$engine" "$screen" "$count"
  done <<< "$lines"
}

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
