#!/usr/bin/env bash
# Launch the example on a simulator and save a small screenshot.
#   ./snap.sh <udid> <out.png> <launch args...>
set -uo pipefail
U="$1"; OUT="$2"; shift 2
xcrun simctl launch --terminate-running-process "$U" shadowlist.uikit.example "$@" >/dev/null
sleep "${SNAP_WAIT:-4}"
xcrun simctl io "$U" screenshot "$OUT.full.png" >/dev/null 2>&1
sips -Z 700 "$OUT.full.png" --out "$OUT" >/dev/null && rm "$OUT.full.png"
