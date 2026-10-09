#!/usr/bin/env bash
# Sample the main thread of a simulator app during a bench run with macOS sample.
#   ./profile-sim.sh <udid> <bundle> <out.txt> <launch args...>
set -uo pipefail
U="$1"; BUNDLE="$2"; OUT="$3"; shift 3
PID=$(xcrun simctl launch --terminate-running-process "$U" "$BUNDLE" -SLBench 1 -SLBenchExit 1 -SLBenchDelay 3 "$@" | awk '{print $2}')
sleep 3.3
sample "$PID" "${SAMPLE_SECONDS:-8}" 1 -mayDie -file "$OUT" >/dev/null 2>&1
echo "sampled $PID into $OUT"
