# ShadowListKit for Android

A virtualized list for Android views, written in Kotlin on the shadowlist core in C++. No React
Native. `ShadowListKitListView` is a `ViewGroup` with a data source. The visible content stays
still while rows are measured, inserted above, removed or regrouped. Rows have stable keys.

## Source layout

| File                                                                                                                          | Role                                                                                                    |
| ----------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------- |
| `packages/shadowlist-core/host/ListDriver.{hpp,cpp}`                                                                          | The core driven for a native list: layout passes, measurement, keys, sticky math, scroll landing, drag. |
| `ShadowListKit/src/main/cpp/ShadowListKitCoreJNI.cpp`                                                                         | Thin JNI hops into the driver with reused arrays.                                                       |
| `ShadowListKitListView.kt`                                                                                                    | Public API, layout pass, mounting, sticky placement, scroll commands.                                   |
| `ShadowListKitScrollGesture.kt`                                                                                               | Touch scrolling, flings, snapping, animated scrolls, the gesture phase and nested scrolling.            |
| `ShadowListKitEdgeEffects.kt`                                                                                                 | The platform edge effect at both ends, glow or stretch.                                                 |
| `packages/shadowlist-core/host/ListSections`, `KeyDiff`, `SwipeReveal`                                                        | Sections over the rows, the key diff and batch plan, swipe offsets, reached through JNI.                |
| `ShadowListKitChangeAnimator.kt`, `ShadowListKitItemAnimator.kt`                                                              | What `animatesChanges` animates, and the pluggable animations.                                          |
| `ShadowListKitDragController.kt`                                                                                              | Touch and hold to reorder through the host layer's DragReorder, and the row menu on a hold.             |
| `ShadowListKitSwipeController.kt`                                                                                             | Swipe actions and the buttons behind a swiped row.                                                      |
| `ShadowListKitRefreshIndicator.kt`, `ShadowListKitSectionIndex.kt`                                                            | Pull to refresh without SwipeRefreshLayout, the section index.                                          |
| `ShadowListKitSwipeAction.kt`, `ShadowListKitAnchorState.kt`, `ShadowListKitListChanges.kt`, `ShadowListKitItemDecoration.kt` | Public value types and the decoration hook.                                                             |
| `ShadowListKitCore.kt`, `ShadowListKitListCell.kt`, `ShadowListKitText.kt`                                                    | JNI wrapper, row base view with its states, precomputed text.                                           |

`ShadowListKitTextLayout` breaks text into lines once with `StaticLayout`, on any thread, and
`ShadowListKitTextView` only draws it.

## API

`DataSource` gives `numberOfItems`, `keyForItem` and `cellForItem`, and `reconfigureCell` for payload reloads. A data
source that also implements `Sizing` gives sizes without views. Otherwise every row is measured through its cell.
The `Delegate` has `willDisplayCell`, `didEndDisplayingCell`, `didSelectItem`, `didDeselectItem`, `shouldSelectItem`,
`shouldHighlightItem`, `didScroll`, `didEndScrolling`, `canMoveItem`, `moveItem`, `didReachStart`, `didReachEnd`,
`leadingSwipeActionsForItem`, `trailingSwipeActionsForItem`, `contextMenuForItem`, `showsSeparatorAfterItem` and
`didBeginRefreshing`, all with defaults.

Properties: `inverted`, `followAppends`, `horizontal`, `numberOfColumns`, `estimatedItemSize`,
`overscan`, `mountOverscan`, `startReachedThreshold`, `endReachedThreshold`, `snapToItem`,
`snapAlignment`, `reorderEnabled`, `animatesChanges`, `itemAnimator`, `stickyIndices`, `stickySectionHeaders`,
`headerView`, `footerView`, `prefetchDataSource`, `allowsSelection`, `allowsMultipleSelection`, `editing`,
`refreshEnabled`, `refreshing`, `showsSeparators`, `separatorColor`, `separatorInsetStart`, `separatorInsetEnd`,
`separatorThickness`. Data: `reloadData`, `insertItems`, `deleteItems`, `reloadItems(indices, payload)`,
`moveItem`, `performBatchUpdates`, `applyChanges`. Selection: `selectedIndices`, `selectItem`, `deselectItem`,
`setEditing`. Queries and commands: `cellForItem`, `visibleCells`, `visibleRange`, `rectForItem`, `contentSize`,
`numberOfSections`, `sectionForItem`, `firstItemIndexInSection`, `rectForHeaderInSection`, `anchorState`,
`scrollToItem`, `scrollToSection`, `scrollToStart`, `scrollToEnd`, `restoreAnchorState`, `closeSwipeActions`,
`addItemDecoration`. Sizes are in pixels.

- Sections: a data source that implements `Sections` groups its items. Item indices still run across all
  sections. A section has a header or footer when `titleForHeaderInSection` or `titleForFooterInSection` returns a
  title, shown in a plain cell unless `cellForHeaderInSection` gives one. `sectionIndexTitles` adds the index along
  the trailing edge. A list with sections reads its sections and keys again on every change, and a dragged row
  stays in its section.
- Batches follow UITableView's index rules and are planned by the core's `planBatch`. A batch that does not add up
  reloads everything. `applyChanges` diffs every key with the core's `diffKeys`, reloads rows whose
  `ContentVersions.contentVersionForItem` changed since the last reload, and returns a `ShadowListKitListChanges`.
- `itemAnimator` (a `ShadowListKitItemAnimator`, `ShadowListKitDefaultItemAnimator` by default) animates inserts, removals
  and moves.
- Swipe actions: a `ShadowListKitSwipeActionsConfiguration` of `ShadowListKitSwipeAction`s per side. A full swipe runs
  the first action. Off while editing.
- Context menus: `contextMenuForItem` fills a `PopupMenu`'s menu on a hold. A row that can also be reordered lifts
  on the hold, and letting it go in place shows the menu.
- Prefetching: `PrefetchDataSource.prefetchItems` for items the core's measured window brought in without a cell,
  `cancelPrefetchingForItems` for ones that left it unseen.
- Pull to refresh draws its own spinner over the rows, without a SwipeRefreshLayout dependency.
- Separators draw after each item's cell, which keeps a pinned header above them. `ShadowListKitItemDecoration` draws
  below or above the rows, like RecyclerView's ItemDecoration without item offsets.
- Selection survives detach and is kept by key.
- The saved position goes into `onSaveInstanceState` as a `ShadowListKitAnchorState` when the list has an id, and lands
  again once the data has its key.

Masonry is round robin: row `i` goes in column `i % numberOfColumns`. Full width rows in a grid are not supported.

The view's own behavior:

- Padding works like content insets. The core's window is the padded area, rows start inside
  the padding, sticky headers pin below the top padding, and with `clipToPadding = false` rows
  scroll under it.
- The edge effect (a stretch on Android 12 and later, a glow before) shows when a drag pulls
  past an end or a fling hits one, along the scroll axis, following `overScrollMode`.
- It is a `NestedScrollingChild3`. Inside a `CoordinatorLayout` a collapsing `AppBarLayout`
  takes its share of drags and flings first.
- Accessibility: the list reports `CollectionInfo` for all rows and each cell its
  `CollectionItemInfo`. Page scrolls forward and backward and scroll to position work for rows
  that are not mounted, which lets TalkBack move past the mounted window.
- `animatesChanges`: inserted rows fade in, removed rows fade out where they were, and rows that
  stay slide to their new place.

## Install

Android API 24 or newer and Java 17. The AAR ships `libshadowlistkit.so` with the C++ runtime linked
statically, aligned for 16 KB pages.

Maven Central:

```kotlin
dependencies {
  implementation("io.github.azimgd:shadowlist-kit:0.9.0")
}
```

JitPack, with `maven("https://jitpack.io")` in the repositories:

```kotlin
dependencies {
  implementation("com.github.azimgd.shadowlist:shadowlist-kit:v0.9.0")
}
```

The core's `[SL]` debug log is off by default. Build the module from a checkout with `-PshadowlistDebugLog`
to compile it in and read it with `adb logcat -s SL`. It prints on every pass.

`./gradlew :ShadowListKit:publishToMavenLocal` installs the current checkout as
`io.github.azimgd:shadowlist-kit` with `VERSION_NAME` from `ShadowListKit/gradle.properties`.

## Run it

The example app lives in [`templates/shadowlist-android-example`](../../templates/shadowlist-android-example). From the repo root:

```sh
cd templates/shadowlist-android-example
./gradlew :app:assembleRelease
adb install -r app/build/outputs/apk/release/app-release.apk
adb shell am start -S -n shadowlist.android.example/com.shadowlist.kit.example.MainActivity \
  --es SLRoute Chat --es SLEngine sl
```

The release build is optimized, not debuggable and signed with the debug key.

`SLRoute` takes `Feed`, `Chat`, `SectionList` and `Masonry`, which run on any engine, and `Reorder`, `ReorderGrid`,
`Snap`, `Horizontal`, `Changes`, `Collapsing`, `Sections` and `Inbox`, which run on ShadowListKitListView only.

| `SLEngine`      | List                  | Row sizes                                       |
| --------------- | --------------------- | ----------------------------------------------- |
| `sl`            | ShadowListKitListView | precomputed layouts, off the UI thread          |
| `sl-auto`       | ShadowListKitListView | each cell measured on the UI thread             |
| `recycler`      | RecyclerView          | fixed heights from the same precomputed layouts |
| `recycler-auto` | RecyclerView          | `wrap_content` rows measured on the UI thread   |

Extras: `SLCount N` rows, `SLImages 0`, `SLPadding N` (dp of padding, rows under it),
`SLAnimate 1` (`animatesChanges` on the list screens), `SLAutoDrag 1` on the reorder screens.

All engines show the same row views. RecyclerView uses `LinearLayoutManager` (with
`stackFromEnd` for chat), `StaggeredGridLayoutManager` for the gallery, stable ids, its default
prefetch, and the usual item decoration for sticky headers.

## Benchmarks

`Bench/src/.../ShadowListKitBench.kt` drives the biggest vertical scroll view at a constant speed, one
step per Choreographer frame, and logs one JSON line: frame intervals, frames over their
deadline from FrameMetrics, UI thread, RenderThread and process CPU, memory, and how much of
the viewport no row covers.

```sh
Bench/bench.sh my-label          # ENGINES, SCREENS, COUNTS, RUNS, SPEED (dp/s), IMAGES, AXES
Bench/summarize.py results/my-label/runs.jsonl
```

`SLBenchAxes` (`AXES` in `bench.sh`) takes `y`, `x` or `xy`, comma separated: one line per axis with an `axis`
key, and a `skipped` line for an axis without a scroll view. A view that draws its content after its rows show
implements `ShadowListKitBenchProbe.benchBlankFraction()`, and the result adds `contentBlankAvg`, `contentBlankMax`
and `contentBlankFrames`.

The `rn` engine runs the React Native example (`shadowlist.example`) with ShadowListKitBench added by a Gradle
init script:

```sh
cd templates/shadowlist-fabric-example/android
./gradlew --init-script ../../../packages/shadowlist-android/Bench/rn/slbench-init.gradle \
  app:assembleRelease -PreactNativeArchitectures=arm64-v8a
adb install -r app/build/outputs/apk/release/app-release.apk
cd ../../../packages/shadowlist-android && ENGINES="sl recycler rn" Bench/bench.sh my-label
```

Scenarios run from the example itself:

```sh
adb shell am start -S -n shadowlist.android.example/com.shadowlist.kit.example.MainActivity \
  --es SLRoute SectionList --es SLEngine recycler --es SLScenario prepend --es SLBenchExit 1
adb logcat -s SLSCENARIO SLAUTODRAG
```

- `prepend`, `append`: rest mid list, change the data, report how far visible rows moved.
- `cost`: UI thread time of one update plus a synchronous layout, with the layout prefetch
  finished and paused. `prependMs`/`appendMs` count the list's own work (the update call and
  the layout), the `*TotalMs` fields add the screen building its rows. `SLCostRuns N` sets the
  updates of each kind. Compile the apps with `adb shell cmd package compile -m speed -f <pkg>`
  first. Interpreted code doubles the numbers and their noise.
- `a11y` on a list screen: the collection info, the actions and two accessibility scrolls.
- `snap` (Snap), `horizontal` (Horizontal), `animate` (Changes), `nested` (Collapsing): the
  feature checks, each through synthesized touches or the public API.
- `SLAutoDrag 1` (Reorder, ReorderGrid): touch and hold the second row, drag it and log the
  order before and after.
- `sections` (Sections) and `features` (Inbox): the feature screens' checks, one `SLSCENARIO` JSON line each. The
  Inbox one swipes, holds and pulls with synthesized touches and saves the position through `saveHierarchyState`.
