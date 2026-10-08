# Templates

Example apps, demo screens and list templates for shadowlist. The JS packages are workspaces of the
repo root. The apps build against the library sources in `../packages`.

```text
shadowlist-utils            List helpers, hooks and React Native list templates
shadowlist-fabric-example   React Native example app for iOS and Android
shadowlist-macos-example    React Native macOS example app (shares screens through .macos variants)
shadowlist-uikit-example    UIKit example app for ShadowListKit (xcodegen)
shadowlist-android-example  Android example app for ShadowListKit (Gradle)
```

## Run

Run `yarn install` at the repo root first.

| App                  | Command                                                                                         |
| -------------------- | ----------------------------------------------------------------------------------------------- |
| React Native iOS     | `yarn workspace shadowlist-example ios`                                                         |
| React Native Android | `yarn workspace shadowlist-example android`                                                     |
| React Native macOS   | `yarn workspace shadowlist-macos-example pods && yarn workspace shadowlist-macos-example macos` |
| UIKit                | `cd shadowlist-uikit-example && ./build.sh sim <simulator-udid>` (or `./build.sh device any`)   |
| Android (Kotlin)     | `cd shadowlist-android-example && ./gradlew :app:assembleRelease`                               |

The React Native iOS example runs `pod install` on its own. It compiles a copy of the core that the
podspec syncs from `packages/shadowlist-core` on every build. Android builds compile the core directly.

## Launch flags

iOS reads launch arguments (`-SLRoute Chat`). Android reads intent extras
(`adb shell am start -n shadowlist.example/.MainActivity --es SLRoute Chat`).

- React Native: `SLRoute <screen>`, `SLCount N` (start size), `SLDebug 1` (debug buttons, captions and
  status lines), `SLTheme light|dark`, `SLLatency a,b`, `SLSendFailureRate 0.5`, `SLNetLog 1`, `SLOverscan N`.
  iOS `-SLEngineFlags NO` turns the engine flags off.
- UIKit and Android kits: `SLRoute`, `SLEngine sl|sl-auto|table|table-auto` (iOS) or
  `sl|sl-auto|recycler|recycler-auto` (Android), `SLCount`, `SLScenario prepend|append|jump|cost|...`,
  `SLAutoDrag 1`, `SLBench 1`. See the kit READMEs in `packages/shadowlist-uikit` and `packages/shadowlist-android`.
