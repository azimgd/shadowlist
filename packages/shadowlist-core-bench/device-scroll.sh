#!/usr/bin/env bash
#
# On-device scroll benchmark for the example app.
#
#   ./device-scroll.sh <label> <screen> [flings] [swipe_ms]
#
# Drives real flings on the connected Android device and reports what the display
# pipeline actually did, straight from `dumpsys gfxinfo framestats`:
#
#   * total frames drawn during the run
#   * janky frames (Android's own definition) and the jank rate
#   * 50th / 90th / 95th / 99th percentile frame times
#   * frames over 16.7ms / 33.3ms (one and two dropped frames at 60Hz)
#
# Unlike the core microbenchmark this includes React, Yoga, mounting, JNI and GPU
# work, so it is the number a user feels. It is noisier: run it more than once.
#
set -euo pipefail

LABEL="${1:?usage: device-scroll.sh <label> <screen> [flings] [swipe_ms]}"
SCREEN="${2:?screen name, e.g. Feed / Chat / Masonry / Contacts}"
FLINGS="${3:-12}"
SWIPE_MS="${4:-90}"

PKG=shadowlist.example
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESULTS="$HERE/results"
mkdir -p "$RESULTS"

OUT="$RESULTS/$LABEL.$SCREEN.scroll.txt"

# Screen geometry, so the swipe path is proportional on any device.
read -r WIDTH HEIGHT < <(adb shell wm size | tr -d '\r' | sed 's/.*: //' | tr 'x' ' ')
CX=$((WIDTH / 2))
Y_LOW=$((HEIGHT * 78 / 100))
Y_HIGH=$((HEIGHT * 25 / 100))

echo "==> $LABEL / $SCREEN : $FLINGS flings of ${SWIPE_MS}ms on ${WIDTH}x${HEIGHT}"

# Reset the frame-stats history so only this run is counted.
adb shell dumpsys gfxinfo "$PKG" reset >/dev/null

for ((i = 0; i < FLINGS; i++)); do
  # Alternate direction so the list is exercised both ways and never parks at an edge.
  if ((i % 4 < 2)); then
    adb shell input swipe "$CX" "$Y_LOW" "$CX" "$Y_HIGH" "$SWIPE_MS" >/dev/null
  else
    adb shell input swipe "$CX" "$Y_HIGH" "$CX" "$Y_LOW" "$SWIPE_MS" >/dev/null
  fi
done

# Let the last fling settle before reading the counters.
adb shell sleep 1.5

adb shell dumpsys gfxinfo "$PKG" framestats | tr -d '\r' > "$OUT.raw"

python3 - "$OUT.raw" "$LABEL" "$SCREEN" <<'PY' | tee "$OUT"
import re, sys, statistics

raw_path, label, screen = sys.argv[1], sys.argv[2], sys.argv[3]
text = open(raw_path, errors="replace").read()

def scalar(pattern):
    match = re.search(pattern, text)
    return int(match.group(1)) if match else None

total  = scalar(r"Total frames rendered:\s+(\d+)")
janky  = scalar(r"Janky frames:\s+(\d+)")

# The framestats CSV block: one row per frame, columns documented by AOSP. Frame
# duration a user perceives is FRAME_COMPLETED (last column) minus INTENDED_VSYNC
# (column 1); rows with a 0 or sentinel timestamp are discarded.
durations = []
for block in re.findall(r"---PROFILEDATA---(.*?)---PROFILEDATA---", text, re.S):
    for line in block.strip().splitlines():
        if not line or line.startswith("Flags"):
            continue
        cells = line.strip().rstrip(",").split(",")
        if len(cells) < 14:
            continue
        try:
            values = [int(c) for c in cells]
        except ValueError:
            continue
        if values[0] != 0:      # skip frames the platform marked as not-a-normal-frame
            continue
        intended, completed = values[1], values[-1]
        if completed <= intended:
            continue
        ms = (completed - intended) / 1e6
        if 0 < ms < 2000:
            durations.append(ms)

durations.sort()
def pct(p):
    if not durations:
        return float("nan")
    return durations[min(len(durations) - 1, int(len(durations) * p))]

over16 = sum(1 for d in durations if d > 16.7)
over33 = sum(1 for d in durations if d > 33.3)
n = len(durations)

print(f"scroll benchmark: {label} / {screen}")
print(f"  total frames rendered      {total}")
print(f"  janky frames (platform)    {janky}"
      + (f"  ({100.0*janky/total:.1f}%)" if total and janky is not None else ""))
print(f"  sampled frames             {n}")
if n:
    print(f"  frame time p50             {pct(0.50):.2f} ms")
    print(f"  frame time p90             {pct(0.90):.2f} ms")
    print(f"  frame time p95             {pct(0.95):.2f} ms")
    print(f"  frame time p99             {pct(0.99):.2f} ms")
    print(f"  frame time mean            {statistics.mean(durations):.2f} ms")
    print(f"  frame time worst           {durations[-1]:.2f} ms")
    print(f"  frames > 16.7ms            {over16}  ({100.0*over16/n:.1f}%)")
    print(f"  frames > 33.3ms            {over33}  ({100.0*over33/n:.1f}%)")
print()
print("#BEGIN_TSV")
print("label\tscreen\ttotal_frames\tjanky_frames\tsampled\tp50_ms\tp90_ms\tp95_ms\tp99_ms\tmean_ms\tworst_ms\tover16\tover33")
print(f"{label}\t{screen}\t{total}\t{janky}\t{n}\t{pct(0.50):.2f}\t{pct(0.90):.2f}\t{pct(0.95):.2f}\t"
      f"{pct(0.99):.2f}\t{statistics.mean(durations) if n else float('nan'):.2f}\t"
      f"{durations[-1] if n else float('nan'):.2f}\t{over16}\t{over33}")
print("#END_TSV")
PY

echo "==> wrote $OUT"
