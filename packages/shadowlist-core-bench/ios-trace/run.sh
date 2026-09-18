#!/usr/bin/env bash
#
# Run one scripted scenario on an iOS simulator with the device trace on, then analyze it.
#
#   ./run.sh <scenario-file|name> [label]
#
# <scenario> is a step file (scenarios/<name>.steps, or a path). Lines:
#
#   route Chat              screen to open (launch argument -SLRoute); before any other step
#   latency 250,700         fake network latency range in ms (-SLLatency)
#   wait 1.5                sleep seconds
#   mark some-label         write a [SCN] marker into the trace
#   press X Y               tap a point (header buttons: prepend 296 80, append 333 80, random 372 80)
#   statusbar               tap the status bar (scroll to top)
#   pan X Y DX DY MS        one-finger drag; negative DY scrolls a vertical list forward
#   repeat N <step>         run a step N times back to back
#   ad <args...>            any other agent-device command, run in the session
#
# Environment: UDID + DEVICE (simulator udid and its agent-device name, default sl-iosfix), SESSION, OUT_DIR, AD (agent-device).
# Prints the analyzer summary; the raw log is $OUT_DIR/<label>.<scenario>.log.
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCENARIO="${1:?usage: run.sh <scenario> [label]}"
LABEL="${2:-run}"
UDID="${UDID:-3631E904-E223-4D1A-BDE7-2C11261920C0}"
DEVICE="${DEVICE:-sl-iosfix}"
SESSION="${SESSION:-sltrace-${DEVICE}}"
OUT_DIR="${OUT_DIR:-$HERE/../results/ios-trace}"
AD="${AD:-agent-device}"
PKG=shadowlist.example

if [[ -f "$SCENARIO" ]]; then
  STEPS="$SCENARIO"
else
  STEPS="$HERE/scenarios/$SCENARIO.steps"
fi
[[ -f "$STEPS" ]] || { echo "no scenario: $SCENARIO" >&2; exit 2; }
NAME="$(basename "$STEPS" .steps)"
mkdir -p "$OUT_DIR"
LOG="$OUT_DIR/$LABEL.$NAME.log"
MARKS="$LOG.marks"
: > "$MARKS"

ROUTE=Feed
LATENCY=250,700
while read -r command rest; do
  case "$command" in
    route) ROUTE="$rest" ;;
    latency) LATENCY="$rest" ;;
  esac
done < "$STEPS"

now() { python3 -c 'import time; print("%.4f" % (time.clock_gettime_ns(time.CLOCK_UPTIME_RAW) / 1e9))'; }
mark() { echo "[SCN] t=$(now) $*" >> "$MARKS"; }
ad() { "$AD" "$@" --session "$SESSION" --platform ios --device "$DEVICE" > /dev/null; }

echo "launching $PKG route=$ROUTE latency=$LATENCY -> $LOG"
SIMCTL_CHILD_SHADOWLIST_FRAME_TRACE=1 xcrun simctl launch --console-pty --terminate-running-process \
  "$UDID" "$PKG" -SLRoute "$ROUTE" -SLLatency "$LATENCY" > "$LOG" 2>&1 &
CONSOLE_PID=$!
trap 'kill $CONSOLE_PID 2>/dev/null || true' EXIT

# Ready once the list has committed from JS. A screen that opens on an empty state (the
# assistant's suggestions) renders n=0, so the row count is deliberately not required here.
for _ in $(seq 1 180); do
  if grep -qE '\[SLJ\] .*render id=[0-9]|\[SLF\] .*frame' "$LOG" 2>/dev/null; then break; fi
  sleep 0.5
done
# A screen that opens on an empty state (the assistant's suggestions) can commit before the
# JS trace function is installed and then sit still, so an absent render line is a warning,
# not a failure: the steps below produce the activity the trace needs.
grep -qE '\[SLJ\] .*render id=|\[SLF\] .*frame' "$LOG" ||
  echo "warning: no list trace yet (Metro up? build has trace?); running the steps anyway" >&2
sleep 1.5
# A stale session left holding the device (a crashed run, another tool) is closed once.
if ! OPEN_OUTPUT="$("$AD" open "$PKG" --session "$SESSION" --platform ios --device "$DEVICE" 2>&1)"; then
  STALE="$(printf '%s' "$OPEN_OUTPUT" | sed -n 's/.*in use by session "\([^"]*\)".*/\1/p' | head -1)"
  [[ -n "$STALE" ]] || { echo "$OPEN_OUTPUT" >&2; exit 1; }
  echo "closing stale agent-device session $STALE"
  "$AD" close --session "$STALE" > /dev/null 2>&1 || true
  ad open "$PKG"
fi
mark start

run_step() {
  local command="$1"; shift
  case "$command" in
    route|latency|"") ;;
    \#*) ;;
    wait) sleep "$1" ;;
    mark) mark "$*" ;;
    press) ad press "$1" "$2" ;;
    statusbar) ad press 200 14 ;;
    pan) ad gesture pan "$@" ;;
    repeat)
      local count="$1"; shift
      for _ in $(seq 1 "$count"); do run_step "$@"; done
      ;;
    ad) ad "$@" ;;
    *) echo "unknown step: $command $*" >&2; exit 2 ;;
  esac
}

while read -r line; do
  [[ -z "$line" || "$line" == \#* ]] && continue
  # shellcheck disable=SC2086
  run_step $line
done < "$STEPS"

mark end
sleep 1
kill $CONSOLE_PID 2>/dev/null || true
trap - EXIT
python3 "$HERE/analyze.py" "$LOG" --json "$LOG.json"
