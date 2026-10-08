#!/usr/bin/env bash
# Launch a route on an engine and save a screenshot after an optional swipe.
#   ./shot.sh <route> <engine> <out.png> [swipe-ms]
set -uo pipefail
ROUTE="$1"; ENGINE="$2"; OUT="$3"; SWIPE="${4:-}"
adb shell am start -S -n shadowlist.android.example/com.shadowlist.kit.example.MainActivity \
  --es SLRoute "$ROUTE" --es SLEngine "$ENGINE" >/dev/null
sleep 4
if [[ -n "$SWIPE" ]]; then
  adb shell input swipe 540 1800 540 600 "$SWIPE"
  sleep 3
fi
adb exec-out screencap -p > "$OUT"
