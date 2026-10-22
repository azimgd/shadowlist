# ShadowList

Virtualized lists for React Native, UIKit, Android views and the web, on a shared C++ core.

An **item** is your data, a **row** is a layout entry (including section headers/footers), a **cell** is its mounted
view, and a **key** is its stable identity across updates. Sizes, cells, selection and the scroll position follow
keys, and inserts, deletes and size changes keep the visible content still.

## React Native

ShadowList is the React Native Fabric package: the ShadowList, SectionList, TreeList and DraggableList components.
ShadowListUtils (`shadowlist-utils` on npm) adds hooks and prebuilt list kits.

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

See more: [ShadowList](packages/shadowlist-fabric/README.md), [ShadowListUtils](templates/shadowlist-utils/README.md)

## UIKit

ShadowListKit is the UIKit package: ShadowListKitListView, a UIScrollView with a data source like UITableView's.
Install it with Swift Package Manager or CocoaPods.

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
    list.reloadData()
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

See more: [ShadowListKit for UIKit](packages/shadowlist-uikit/README.md)

## Android

ShadowListKit is the Android package: ShadowListKitListView, a ViewGroup with a data source.
Install it from Maven Central or JitPack.

```kotlin
class FeedScreen(context: Context, private val posts: List<Post>) : ShadowListKitListView.DataSource {
  val list = ShadowListKitListView(context).also {
    it.registerCell("post") { ctx -> PostCell(ctx) }
    it.dataSource = this
    it.reloadData()
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

See more: [ShadowListKit for Android](packages/shadowlist-android/README.md)

## Web

[`shadowlist-wasm`](packages/shadowlist-wasm) is a custom element, `<shadowlist-kit-list-view>`, on the core compiled
to WebAssembly. It has the same data source as the native kits. Build it with `./build.sh` and Emscripten.

```ts
await loadShadowListKit();

const list = new ShadowListKitListView();
list.registerCell('post', () => new PostCell());
list.dataSource = {
  numberOfItems: () => posts.length,
  keyForItem: (_, index) => posts[index].id,
  cellForItem: (listView, index) => {
    const cell = listView.dequeueReusableCell<PostCell>('post');
    cell.configure(posts[index]);
    return cell;
  },
};
document.body.append(list);
list.reloadData();
```

## Repo layout

| Path | Contents |
| --- | --- |
| `packages/shadowlist-core` | Shared C++ core |
| `packages/shadowlist-core-tests` | Core integration and perf tests |
| `packages/shadowlist-core-bench` | Core microbenchmarks, device metrics |
| `packages/shadowlist-fabric` | React Native Fabric package |
| `packages/shadowlist-uikit` | UIKit list |
| `packages/shadowlist-android` | Android list |
| `packages/shadowlist-wasm` | Web list, on the core in WebAssembly |
| `templates/shadowlist-utils` | List helpers, hooks and React Native list kits |
| `templates/shadowlist-fabric-example` | React Native example app for iOS and Android |
| `templates/shadowlist-macos-example` | React Native macOS example app |
| `templates/shadowlist-uikit-example` | UIKit example app |
| `templates/shadowlist-android-example` | Android example app |
| `templates/shadowlist-web-example` | Web example app |
| `docs` | Code style |

## Example apps

The JS packages are workspaces of the repo root, and the apps build against the library sources in `packages/`.
Run `yarn install` at the repo root first.

| App | Command |
| --- | --- |
| React Native iOS | `yarn workspace shadowlist-example ios` |
| React Native Android | `yarn workspace shadowlist-example android` |
| React Native macOS | `yarn workspace shadowlist-macos-example pods && yarn workspace shadowlist-macos-example macos` |
| UIKit | `cd templates/shadowlist-uikit-example && ./build.sh sim <simulator-udid>` (or `./build.sh device any`) |
| Android | `cd templates/shadowlist-android-example && ./gradlew :app:assembleRelease` |
| Web | `cd templates/shadowlist-web-example && npm run build && npm start` |

## Debug log

The C++ core can log every layout pass with an `[SL]` prefix. It is off by default, in debug builds too.

| Package | How to turn it on |
| --- | --- |
| React Native iOS and macOS | `SHADOWLIST_DEBUG_LOG=1 pod install`, then rebuild. The lines show in the Xcode console. |
| React Native Android | `arguments "-DSHADOWLIST_DEBUG_LOG=1"` in the app's `externalNativeBuild.cmake` block, then rebuild. |
| UIKit | The example's `build.sh` with `SHADOWLIST_DEBUG_LOG=1`. |
| Android | Build the module with `-PshadowlistDebugLog`. |

On Android read it with `adb logcat -s SL`.

## Benchmarks

`packages/shadowlist-core-bench` holds the core microbenchmarks. The UIKit and Android packages ship a scroll
benchmark in `Bench/`: `Bench/bench.sh` drives the example at a constant speed, and `Bench/summarize.py` reads the
results.

The web example documents its flags in [its package README](packages/shadowlist-wasm/README.md#example-app).

## Development

| Command | What it does |
| --- | --- |
| `yarn lint` | ESLint over the repo |
| `yarn typecheck` | TypeScript for the library, the utils, the example apps and `shadowlist-wasm` |
| `yarn test` | Jest for `shadowlist` and `shadowlist-utils` |
| `yarn test:core` | Builds `packages/shadowlist-core-tests` with CMake and runs it with ctest |
| `yarn build:wasm` | Builds `shadowlist-wasm` into its `dist/` with Emscripten |
| `yarn test:wasm` | Node tests for `shadowlist-wasm`, after `yarn build:wasm` |
| `yarn build` | Vendors the core into the package and builds it with bob |
| `yarn clean` | Removes the package's build output |
| `yarn format` | Prettier over the repo. `format:check` only checks. |
| `yarn docs:check` | Checks README links, headings, compact tables, requirements and examples |

Lefthook runs ESLint and Prettier on staged files before a commit and `yarn typecheck` before a push. CI runs lint,
formatting, typecheck, the README checks, jest, the core tests, the WebAssembly tests and the native builds.

## Docs

| Doc | Contents |
| --- | --- |
| [Code style](docs/code-style.md) | Naming, comments and the checklist for every package |
| [CHANGELOG.md](CHANGELOG.md) | Releases |
