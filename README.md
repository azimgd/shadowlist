# ShadowList

Virtualized lists for React Native, UIKit, AppKit, Android views and the web, on a shared C++ core.

An **item** is your data, a **row** is a layout entry (including section headers/footers), a **cell** is its mounted
view, and a **key** is its stable identity across updates. Sizes, cells, selection and the scroll position follow
keys, and inserts, deletes and size changes keep the visible content still. In a grid only the anchor row's
column is held still. Right-to-left layout is not supported: leading is always the left and trailing the right.

## React Native

ShadowList is the React Native Fabric package: the ShadowList, SectionList, TreeList and DraggableList components.

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

See more: [ShadowList](packages/shadowlist-fabric/README.md)

## Templates and example apps

The list templates, visual components and example apps live in
[shadowlist-templates](https://github.com/komplete-io/shadowlist-templates): `shadowlist-utils` (hooks and list kits for
React Native), ShadowListUtils for UIKit and Android views, the template logic in C++ and the example apps for every
kit. They build against a checkout of this repository.

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

## AppKit

ShadowListKit for AppKit is the Mac package: ShadowListKitListView, a flipped NSView with a data source like
NSTableView's and the UIKit kit's class names and selectors. It scrolls itself with the trackpad and the mouse
wheel, or a host such as a two axis grid drives it. Install it with Swift Package Manager (the ShadowListKitAppKit
product) or CocoaPods.

```swift
import ShadowListKitAppKit

final class FeedController: NSViewController, ShadowListKitListViewDataSource {
  private let list = ShadowListKitListView()
  private var posts: [Post] = []

  override func loadView() {
    view = NSView()
    list.autoresizingMask = [.width, .height]
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

See more: [ShadowListKit for AppKit](packages/shadowlist-appkit/README.md)

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
| `packages/shadowlist-core` | Shared C++ core: the list engine and `host/` for what the platforms' lists share |
| `packages/shadowlist-core-tests` | Core integration and perf tests |
| `packages/shadowlist-core-bench` | Core microbenchmarks, device metrics |
| `packages/shadowlist-fabric` | React Native Fabric package |
| `packages/shadowlist-uikit` | UIKit list |
| `packages/shadowlist-appkit` | AppKit list |
| `packages/shadowlist-android` | Android list |
| `packages/shadowlist-wasm` | Web list, on the core in WebAssembly |
| `docs` | Code style |

## Example apps

The example apps are in the `examples/` folder of [shadowlist-templates](https://github.com/komplete-io/shadowlist-templates).
Clone it next to this repository, or set `SHADOWLIST_ROOT` to this checkout. Its README gives the build commands for
each app. The scripts here that drive an app find the templates checkout at `SHADOWLIST_TEMPLATES_ROOT`, by default
`../shadowlist-templates`.

## Debug log

The C++ core can log every layout pass with an `[SL]` prefix. It is off by default, in debug builds too.

| Package | How to turn it on |
| --- | --- |
| React Native iOS and macOS | `SHADOWLIST_DEBUG_LOG=1 pod install`, then rebuild. The lines show in the Xcode console. |
| React Native Android | `arguments "-DSHADOWLIST_DEBUG_LOG=1"` in the app's `externalNativeBuild.cmake` block, then rebuild. |
| UIKit | The shadowlist-templates example's `build.sh` with `SHADOWLIST_DEBUG_LOG=1`. |
| AppKit | The shadowlist-templates example's `build.sh` with `SHADOWLIST_DEBUG_LOG=1`. |
| Android | Build the module with `-PshadowlistDebugLog`. |

On Android read it with `adb logcat -s SL`.

## Benchmarks

`packages/shadowlist-core-bench` holds the core microbenchmarks. The UIKit and Android packages ship a scroll
benchmark in `Bench/`: `Bench/bench.sh` drives the example app from shadowlist-templates at a constant speed, and `Bench/summarize.py` reads the
results. The React Native example turns on React Native feature flags that are off in a stock app, and every run
records their state. On iOS `ENGINE_FLAGS=0` turns them off. On Android the build takes
`-PshadowlistEngineFlags=false`.

The web example documents its flags in [its package README](packages/shadowlist-wasm/README.md#example-app).

## Development

| Command | What it does |
| --- | --- |
| `yarn lint` | ESLint over the repo |
| `yarn typecheck` | TypeScript for the library and `shadowlist-wasm` |
| `yarn test` | Jest for `shadowlist` |
| `yarn test:core` | Builds `packages/shadowlist-core-tests` with CMake and runs it with ctest |
| `yarn build:wasm` | Builds `shadowlist-wasm` into its `dist/` with Emscripten |
| `yarn test:wasm` | Node tests for `shadowlist-wasm`, after `yarn build:wasm` |
| `yarn build` | Vendors the core into the package and builds it with bob |
| `yarn clean` | Removes the package's build output |
| `yarn format` | Prettier over the repo. `format:check` only checks. |
| `yarn docs:check` | Checks README links, headings, compact tables, requirements and examples |

The core tests in `packages/shadowlist-core-tests` read their environment. A variable they do not know, or a value
they cannot read, stops the run with a message and exit status 2 before any test runs.

| Variable | What it does |
| --- | --- |
| `SLT_ONLY=<text>` | Runs only the tests whose name contains the text |
| `SLT_SKIP=<text>` | Leaves out the tests whose name contains the text. CI passes `SLT_SKIP=fuzz_` to the ThreadSanitizer run |
| `SLT_FUZZ_TRACE=<seed>` | Prints every frame of that seed |
| `SLT_FUZZ_SEEDS=<count>` | Runs that many seeds per fuzz test, in place of each test's own count |
| `SLT_FUZZ_FIRST_SEED=<seed>` | Starts at that seed, in place of 1 |
| `SLT_FUZZ_DEFAULT_MIX=1` | Keeps a campaign to the tests, actions and checks of a default run |

The fuzz tests are the ones named `fuzz_`: `tests_fuzz.cpp`, `tests_fuzz_axis.cpp` and `tests_fuzz_faults.cpp` for the
core, `tests_host_fuzz.cpp` and `tests_host_mount_driver.cpp` for the host layer, and one each in
`tests_host_selection.cpp`, `tests_host_swipe.cpp`, `tests_host_drag_board.cpp` and `tests_host_row_keys.cpp`. A seed is the same episode on every platform.

`SLT_FUZZ_SEEDS` or `SLT_FUZZ_FIRST_SEED` turns the run into a campaign. A campaign keeps going past a failing seed
and runs every seed twice to check that both runs see the same frames. It ends with a summary: per test the seeds run,
the seeds that failed and a digest of the run, then per kind of failure its first seed, how many seeds hit it, its
message and the command that replays that seed alone with the trace on. It also lists how often each action and each
core operation was reached.

Every registered fuzz test runs by default. A Release build keeps a long campaign short:

```sh
cmake -S packages/shadowlist-core-tests -B packages/shadowlist-core-tests/build/campaign -DCMAKE_BUILD_TYPE=Release
cmake --build packages/shadowlist-core-tests/build/campaign
SLT_ONLY=fuzz_ SLT_FUZZ_SEEDS=20000 packages/shadowlist-core-tests/build/campaign/shadowlist_core_tests
```

The files in `packages/shadowlist-core-tests/vectors` hold cases that a function of the core's `host/` and its twin in
TypeScript must answer alike. `yarn test:core` replays them through the C++, and `yarn test` through the TypeScript. `UPDATE_VECTORS=1 yarn test` writes the outputs from the TypeScript, after a change both sides make. A file
without a replay on either side fails that side's tests.

Lefthook runs ESLint and Prettier on staged files before a commit and `yarn typecheck` before a push. CI runs lint,
formatting, typecheck, the README checks, jest, the core tests, the WebAssembly tests and the native builds. The
example app builds check out shadowlist-templates next to this repository.

## Docs

| Doc | Contents |
| --- | --- |
| [Code style](docs/code-style.md) | Naming, comments and the checklist for every package |
