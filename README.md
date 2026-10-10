# ShadowList

Virtualized lists for React Native, UIKit and Android views. Layout, virtualization and scroll position live in a shared C++ core.

## React Native

[`shadowlist`](packages/shadowlist-fabric) gives Fabric apps `ShadowList`, `SectionList`, `TreeList` and `DraggableList`.
`ShadowList` runs on iOS, Android and macOS. [`shadowlist-utils`](templates/shadowlist-utils)
adds hooks and ready-made chat, feed and other list kits.

```sh
yarn add shadowlist
```

```tsx
const listRef = useRef<ShadowListCommands>(null);

<ShadowList
  ref={listRef}
  data={photos}
  renderItem={({ item }) => <Photo photo={item} />}
  numberOfColumns={3}
  reorderEnabled
  snapToItem
  snapAlignment="start"
/>;

listRef.current?.scrollToIndex({ index: 42, viewPosition: 0.5 });
```

## UIKit

[`ShadowListKit`](packages/shadowlist-uikit) is a `UIScrollView` subclass with a UITableView-like data source. The visible content stays still while rows are measured, inserted above or removed. Install it with Swift
Package Manager or CocoaPods.

```swift
import ShadowListKit

final class FeedController: UIViewController, ShadowListKitListViewDataSource {
  private let list = ShadowListKitListView()
  private var posts: [Post] = []

  override func viewDidLoad() {
    super.viewDidLoad()
    list.frame = view.bounds
    list.dataSource = self
    list.register(PostCell.self, forCellReuseIdentifier: "post")
    view.addSubview(list)
  }

  func numberOfItems(in listView: ShadowListKitListView) -> Int { posts.count }

  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { posts[index].id }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    let cell = listView.dequeueReusableCell(withIdentifier: "post") as! PostCell
    cell.configure(posts[index])
    return cell
  }
}
```

## Android

[`ShadowListKit`](packages/shadowlist-android) is a Kotlin `ViewGroup` with a data source. The visible
content stays still while rows are measured, inserted above, removed or regrouped. Install it from Maven Central or
JitPack.

```kotlin
class FeedScreen(context: Context, private val posts: List<Post>) : ShadowListKitListView.DataSource {
  val list = ShadowListKitListView(context).also {
    it.registerCell("post") { ctx -> PostCell(ctx) }
    it.dataSource = this
  }

  override fun numberOfItems(listView: ShadowListKitListView) = posts.size

  override fun keyForItem(listView: ShadowListKitListView, index: Int) = posts[index].id

  override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell {
    val cell = listView.dequeueReusableCell<PostCell>("post")
    cell.bind(posts[index])
    return cell
  }
}
```

## Repo layout

```text
packages/shadowlist-core              Shared C++ core
packages/shadowlist-core-tests        Core integration and perf tests
packages/shadowlist-core-bench        Core micro-benchmarks and device metrics scripts
packages/shadowlist-fabric            React Native (Fabric) package
packages/shadowlist-uikit             UIKit list (ShadowListKit)
packages/shadowlist-android           Android list (ShadowListKit, Kotlin)
templates/shadowlist-utils            List helpers, hooks and React Native list kits
templates/shadowlist-fabric-example   React Native example app for iOS and Android
templates/shadowlist-macos-example    React Native macOS example app (shares screens through .macos variants)
templates/shadowlist-uikit-example    UIKit example app (xcodegen)
templates/shadowlist-android-example  Android example app (Gradle)
```

## Example apps

The JS packages are workspaces of the repo root, and the apps build against the library sources in `packages/`.
Run `yarn install` at the repo root first.

| App                  | Command                                                                                                 |
| -------------------- | ------------------------------------------------------------------------------------------------------- |
| React Native iOS     | `yarn workspace shadowlist-example ios`                                                                 |
| React Native Android | `yarn workspace shadowlist-example android`                                                             |
| React Native macOS   | `yarn workspace shadowlist-macos-example pods && yarn workspace shadowlist-macos-example macos`         |
| UIKit                | `cd templates/shadowlist-uikit-example && ./build.sh sim <simulator-udid>` (or `./build.sh device any`) |
| Android              | `cd templates/shadowlist-android-example && ./gradlew :app:assembleRelease`                             |

The React Native iOS example runs `pod install` on its own.

iOS reads launch arguments (`-SLRoute Chat`). Android reads intent extras
(`adb shell am start -n shadowlist.example/.MainActivity --es SLRoute Chat`). The React Native examples take:

- `SLRoute <screen>`, `SLCount N` (start size), `SLTheme light|dark` and `SLOverscan N`.
- `SLDebug 1` for debug buttons, captions and status lines.
- `SLLatency a,b`, `SLSendFailureRate 0.5` and `SLNetLog 1` for the fake network.
- iOS `-SLEngineFlags NO` turns the engine flags off.

The UIKit and Android examples document their flags in their package READMEs:
[UIKit](packages/shadowlist-uikit/README.md#example-app) and [Android](packages/shadowlist-android/README.md#example-app).
