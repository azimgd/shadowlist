# ShadowList

Virtualized list for React Native. Layout, virtualization and scroll position live in a shared C++ core that runs on iOS and Android.

## Packages

- `shadowlist-core`
  Shared C++ virtualization engine used by every integration.

- `shadowlist`
  React Native Fabric list components: `ShadowList`, `SectionList`, `TreeList` and
  `DraggableList`.

- `ShadowListKit`
  The list for UIKit ([packages/shadowlist-uikit](packages/shadowlist-uikit)) and Android views
  ([packages/shadowlist-android](packages/shadowlist-android)), on the same core. Swift Package Manager, CocoaPods,
  Maven Central and JitPack. [RELEASING.md](RELEASING.md) covers publishing.

## Repo Layout

```text
packages/shadowlist-core            Shared C++ core
packages/shadowlist-core-tests      Core integration and perf tests
packages/shadowlist-core-bench      Core micro-benchmarks and device metrics scripts
packages/shadowlist-fabric          React Native (Fabric) package
packages/shadowlist-uikit           UIKit list (ShadowListKit)
packages/shadowlist-android         Android list (ShadowListKit, Kotlin)
templates/                          Example apps, demo screens and shadowlist-utils
```

## Examples

Example apps for React Native (iOS, Android, macOS), UIKit and Android live in `templates/`.
See [templates/README.md](templates/README.md) for how to run each one.
