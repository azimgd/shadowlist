#!/usr/bin/env bash
set -uo pipefail
# Builds the example for this Mac. Prints only errors and the result.
#   ./build.sh mac [Release|Debug]
# SHADOWLIST_DEBUG_LOG=1 ./build.sh ... compiles in the core's [SL] debug log.
# Run a scenario: open -W <app> --args -SLRoute Feed -SLScenario prepend -SLBenchExit 1
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KIND="${1:?mac}"; CONFIG="${2:-Release}"
[[ "$KIND" == mac ]] || { echo "unknown kind: $KIND (mac)"; exit 2; }
cd "$HERE" && xcodegen generate >/dev/null
EXTRA=()
[[ "${SHADOWLIST_DEBUG_LOG:-0}" == 1 ]] && EXTRA+=('GCC_PREPROCESSOR_DEFINITIONS=$(inherited) SHADOWLIST_DEBUG_LOG=1')
mkdir -p "$HERE/build"
xcodebuild -project ShadowListKitAppKitExample.xcodeproj -scheme ShadowListKitAppKitExample -configuration "$CONFIG" \
  -destination "platform=macOS" -derivedDataPath "$HERE/build/dd-$KIND" ${EXTRA[@]+"${EXTRA[@]}"} build \
  > "$HERE/build/log-$KIND.txt" 2>&1
STATUS=$?
grep -E "error:" "$HERE/build/log-$KIND.txt" | sort -u | cut -c1-260 | head -40
[[ $STATUS == 0 ]] && echo "BUILD OK: $HERE/build/dd-$KIND/Build/Products/$CONFIG/ShadowListKitAppKitExample.app" || echo "BUILD FAILED ($STATUS)"
exit $STATUS
