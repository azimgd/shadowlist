#!/usr/bin/env bash
# Build the example for a simulator or a device. Prints only errors and the result.
#   ./build.sh sim <udid> [Release|Debug]
#   ./build.sh device <udid> [Release|Debug]
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KIND="${1:?sim|device}"; UDID="${2:?udid}"; CONFIG="${3:-Release}"
cd "$HERE" && xcodegen generate >/dev/null
if [[ "$KIND" == sim ]]; then
  SDK=iphonesimulator; DEST="id=$UDID,arch=arm64"
else
  SDK=iphoneos; DEST="generic/platform=iOS"
fi
mkdir -p "$HERE/build"
xcodebuild -project ShadowListKitExample.xcodeproj -scheme ShadowListKitExample -configuration "$CONFIG" \
  -sdk "$SDK" -destination "$DEST" -derivedDataPath "$HERE/build/dd-$KIND" -allowProvisioningUpdates build \
  > "$HERE/build/log-$KIND.txt" 2>&1
STATUS=$?
grep -E "error:" "$HERE/build/log-$KIND.txt" | sort -u | cut -c1-260 | head -40
[[ $STATUS == 0 ]] && echo "BUILD OK: $HERE/build/dd-$KIND/Build/Products/$CONFIG-$SDK/ShadowListKitExample.app" || echo "BUILD FAILED ($STATUS)"
exit $STATUS
