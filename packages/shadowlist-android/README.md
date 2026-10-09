# ShadowListKit for Android (prototype)

A virtualized list for Android views, written in Kotlin on the shadowlist core in C++. No React
Native. `SLKListView` is a `ViewGroup` with a data source shaped like the UIKit `SLKListView`.
It adds what the shadowlist core does well: the visible content stays still while rows are
measured, inserted above, removed or regrouped. Rows have stable keys.

## How it works

One layout pass on the UI thread does what Fabric spreads over commits:

1. `ListDriver::runPasses` in C++ calls `Virtualizer::update` with the current offset.
   `ListDriver` lives in `packages/shadowlist-core/host` and also drives the UIKit list.
2. Every row in the core's window that has no size gets measured. The driver calls back into
   Kotlin once per unmeasured row. The size comes from the data source's `Sizing`, a cache
   lookup when layouts are precomputed, or from measuring the cell. Then one
   `commitElementSizes` reflow runs.
3. Any offset correction the core asks for is applied inside the same call, and the core gets
   more passes to confirm it. Corrections land before the frame is drawn.
4. The window's frames are copied to Kotlin. Cells within `mountOverscan` of the viewport are
   mounted by key and the rest go back to the reuse pool. A cell that only moved is offset
   without a measure or layout. The range follows `ListDriver::planMount`, run in Kotlin over
   the copied frames.

Data changes cross JNI once. `insertItems` and `deleteItems` send only the changed keys.
`reloadData` compares the new keys with the old ones and sends only the changed middle, all
keys joined in one char array. The driver passes an edit at one end, like a prepend, to the core
as one edit, which skips comparing every key.

A scroll frame inside the core's `computeOffsetBand` skips steps 1 to 3 and makes no JNI call.
Most frames only run the mount pass over the copied frames. Rows binding during the pass do
not ask the whole window for a layout traversal, the pass measures them itself. Sticky headers
are pinned by the host, which keeps the band usable for section lists.

`SLKTextLayout` breaks text into lines once with `StaticLayout`, on any thread, and
`SLKTextView` only draws it. Row layouts computed off the UI thread carry their text.

| File                                                                                  | Role                                                                                                                                                   |
| ------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `packages/shadowlist-core/host/ListDriver.{hpp,cpp}`                                  | The core driven for a native list: layout passes, measurement, keys, sticky math, scroll landing, drag. No platform types. Shared with the UIKit list. |
| `ShadowListKit/src/main/cpp/SLKCoreJNI.cpp`                                           | Thin JNI hops into the driver with reused arrays.                                                                                                      |
| `SLKListView.kt`                                                                      | Public API, layout pass, mounting, sticky placement, scroll commands.                                                                                  |
| `SLKScrollGesture.kt`                                                                 | Touch scrolling, flings, snapping, animated scrolls, the gesture phase and nested scrolling.                                                           |
| `SLKEdgeEffects.kt`                                                                   | The platform edge effect at both ends, glow or stretch.                                                                                                |
| `packages/shadowlist-core/host/ListSections`, `KeyDiff`, `SwipeReveal`                | Sections over the rows, the key diff and batch plan, swipe offsets. Shared with the UIKit list, reached through JNI.                                   |
| `SLKChangeAnimator.kt`, `SLKItemAnimator.kt`                                          | What `animatesChanges` animates, and the pluggable animations.                                                                                         |
| `SLKDragController.kt`                                                                | Touch and hold to reorder through the host layer's DragReorder, and the row menu on a hold.                                                            |
| `SLKSwipeController.kt`                                                               | Swipe actions and the buttons behind a swiped row.                                                                                                     |
| `SLKRefreshIndicator.kt`, `SLKSectionIndex.kt`                                        | Pull to refresh without SwipeRefreshLayout, the section index.                                                                                         |
| `SLKSwipeAction.kt`, `SLKAnchorState.kt`, `SLKListChanges.kt`, `SLKItemDecoration.kt` | Public value types and the decoration hook.                                                                                                            |
| `SLKCore.kt`, `SLKListCell.kt`, `SLKText.kt`                                          | JNI wrapper, row base view with its states, precomputed text.                                                                                          |

## API

The same names as the UIKit list. `DataSource` gives `numberOfItems`, `keyForItem` and `cellForItem`, and
`reconfigureCell` for payload reloads. A data source that also implements `Sizing` gives sizes without views.
Otherwise every row is measured through its cell. The `Delegate` has `willDisplayCell`, `didEndDisplayingCell`,
`didSelectItem`, `didDeselectItem`, `shouldSelectItem`, `shouldHighlightItem`, `didScroll`, `didEndScrolling`,
`canMoveItem`, `moveItem`, `didReachStart`, `didReachEnd`, `leadingSwipeActionsForItem`,
`trailingSwipeActionsForItem`, `contextMenuForItem`, `showsSeparatorAfterItem` and `didBeginRefreshing`, all with
defaults.

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
  stays in its section. The rows' items cross JNI once per change, which keeps scroll frames free of JNI calls.
- Batches follow UITableView's index rules and are planned by the core's `planBatch`. A batch that does not add up
  reloads everything. `applyChanges` diffs every key with the core's `diffKeys`, reloads rows whose
  `ContentVersions.contentVersionForItem` changed since the last reload, and returns an `SLKListChanges`, like
  `ListAdapter.submitList` with `DiffUtil` but on keys.
- `itemAnimator` (an `SLKItemAnimator`, `SLKDefaultItemAnimator` by default) animates inserts, removals and moves,
  like RecyclerView's ItemAnimator.
- Swipe actions: an `SLKSwipeActionsConfiguration` of `SLKSwipeAction`s per side, like ItemTouchHelper's swipe to
  dismiss when the first action runs on a full swipe. Off while editing.
- Context menus: `contextMenuForItem` fills a `PopupMenu`'s menu on a hold. A row that can also be reordered lifts
  on the hold, and letting it go in place shows the menu.
- Prefetching: `PrefetchDataSource.prefetchItems` for items the core's measured window brought in without a cell,
  `cancelPrefetchingForItems` for ones that left it unseen. One JNI call per mount pass, only with a prefetch data
  source.
- Pull to refresh draws its own spinner over the rows, without a SwipeRefreshLayout dependency.
- Separators draw after each item's cell, which keeps a pinned header above them. `SLKItemDecoration` draws below
  or above the rows, like RecyclerView's ItemDecoration without item offsets. Android only.
- Selection survives detach and is kept by key in Kotlin with the rules of the core's `ListSelection`, because the
  core peer is dropped when the list detaches.
- The saved position goes into `onSaveInstanceState` as an `SLKAnchorState` when the list has an id, and lands
  again once the data has its key.

Masonry stays round robin, the same as the UIKit list: a shortest column layout would move rows between columns
whenever an earlier row is measured. Full width rows in a grid are not supported because the core's grid relies on
row `i` being in column `i % numberOfColumns`.

The view's own behavior works like a platform list:

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
  stay slide to their new place. The core anchors the content as without it.

## Add it to an app

There is no Maven artifact yet. Include the Gradle module from a checkout, the way the example's
`settings.gradle.kts` does:

```kotlin
include(":ShadowListKit")
project(":ShadowListKit").projectDir = file("<repo>/packages/shadowlist-android/ShadowListKit")
```

The module builds the core from `packages/shadowlist-core` with CMake and the NDK, which needs the
repo layout around it. It needs Android API 24 or newer and Java 17.

The core's `[SL]` debug log is off by default. Build with `-PshadowlistDebugLog` to compile it in
and read it with `adb logcat -s SL`. It prints on every pass.

## Run it

The example app lives in `templates/shadowlist-android-example`. From the repo root:

```sh
cd templates/shadowlist-android-example
./gradlew :app:assembleRelease
adb install -r app/build/outputs/apk/release/app-release.apk
adb shell am start -S -n shadowlist.android.example/com.shadowlist.kit.example.MainActivity \
  --es SLRoute Chat --es SLEngine sl
```

The release build is optimized, not debuggable and signed with the debug key. The library
compiles the canonical core in `packages/shadowlist-core` directly.

The example has the Feed, Chat, Directory (`SectionList`) and Gallery (`Masonry`) screens of the
UIKit and React Native examples, with the same fixtures. Feature screens run on SLKListView
only: `Reorder` and `ReorderGrid` (touch and hold to reorder), `Snap` (`snapToItem`),
`Horizontal` (a horizontal strip with pinned section letters and estimated widths), `Changes`
(`animatesChanges`), `Collapsing` (a collapsing app bar over the list), `Sections` (section headers, footers,
the index and measured cells) and `Inbox` (swipe actions, menus, selection, refresh, batches and `applyChanges`). The list screens run
on any engine:

| `SLEngine`      | List         | Row sizes                                       |
| --------------- | ------------ | ----------------------------------------------- |
| `sl`            | SLKListView  | precomputed layouts, off the UI thread          |
| `sl-auto`       | SLKListView  | each cell measured on the UI thread             |
| `recycler`      | RecyclerView | fixed heights from the same precomputed layouts |
| `recycler-auto` | RecyclerView | `wrap_content` rows measured on the UI thread   |

Extras: `SLCount N` rows, `SLImages 0`, `SLPadding N` (dp of padding, rows under it),
`SLAnimate 1` (`animatesChanges` on the list screens), `SLAutoDrag 1` on the reorder screens.

All engines show the same row views. RecyclerView uses `LinearLayoutManager` (with
`stackFromEnd` for chat), `StaggeredGridLayoutManager` for the gallery, stable ids, its default
prefetch, and the usual item decoration for sticky headers.

## Benchmarks

`Bench/src/.../SLKBench.kt` drives the biggest vertical scroll view at a constant speed, one
step per Choreographer frame, and logs one JSON line: frame intervals, frames over their
deadline from FrameMetrics, UI thread, RenderThread and process CPU, memory, and how much of
the viewport no row covers.

```sh
Bench/bench.sh my-label          # ENGINES, SCREENS, COUNTS, RUNS, SPEED (dp/s), IMAGES, AXES
Bench/summarize.py results/my-label/runs.jsonl
```

`SLBenchAxes` (`AXES` in `bench.sh`) takes `y`, `x` or `xy`, comma separated, the same as iOS: one
line per axis with an `axis` key, and a `skipped` line for an axis without a scroll view. A view
that draws its content after its rows show implements `SLKBenchProbe.benchBlankFraction()`, and the
result adds `contentBlankAvg`, `contentBlankMax` and `contentBlankFrames`.

The `rn` engine runs the React Native example (`shadowlist.example`) with the same SLKBench.
A Gradle init script adds the bench to its release build without any change to that project:
it compiles `Bench/src` and `Bench/rn/src` into the release variant and merges
`Bench/rn/AndroidManifest.xml`, whose content provider starts SLKBench in the launched activity.
The example reads `SLRoute` and `SLCount` from the same extras.

```sh
cd templates/shadowlist-fabric-example/android
./gradlew --init-script ../../../packages/shadowlist-android/Bench/rn/slbench-init.gradle \
  app:assembleRelease -PreactNativeArchitectures=arm64-v8a
adb install -r app/build/outputs/apk/release/app-release.apk
cd ../../../packages/shadowlist-android && ENGINES="sl recycler rn" Bench/bench.sh my-label
```

Scenarios from the example itself, the same as the UIKit ones:

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
  order before and after, like the UIKit `-SLAutoDrag`.
- `sections` (Sections) and `features` (Inbox): the same checks as the UIKit scenarios. The Inbox one swipes,
  holds and pulls with synthesized touches and saves the position through `saveHierarchyState`.

## Results

Android 15 emulator (`sl_perf_android`, arm64 on Apple Silicon, host GPU, 60 Hz), release builds,
1000 rows, images on, the list driven at a constant speed for 6 s each way, 3 interleaved runs,
medians. UI ms per frame is the UI thread part of each drawn frame from FrameMetrics (input,
animation, measure and layout, draw). Lower is better. Raw runs are in `results/v2-4000` and
`results/v2-12000`.

| UI ms per frame, mean / p95 | SLK            | RecyclerView | SLK self-sizing | RecyclerView self-sizing |
| --------------------------- | -------------- | ------------ | --------------- | ------------------------ |
| Feed, 4000 dp/s             | **1.21** / 3.2 | 1.43 / 4.3   | 1.64 / 4.6      | 1.78 / 7.1               |
| Chat, 4000 dp/s             | **1.16** / 2.8 | 1.31 / 3.9   | 1.62 / 4.4      | 1.84 / 6.8               |
| Directory, 4000 dp/s        | **1.81** / 3.6 | 2.08 / 5.0   | 3.74 / 9.2      | 3.51 / 9.5               |
| Gallery, 4000 dp/s          | **1.49** / 3.3 | 1.84 / 5.6   | 1.92 / 4.2      | 2.17 / 5.2               |
| Feed, 12000 dp/s            | **1.53** / 4.0 | 1.90 / 5.9   | 3.01 / 8.2      | 3.06 / 9.4               |
| Chat, 12000 dp/s            | **1.72** / 4.0 | 2.21 / 5.6   | 3.27 / 8.2      | 3.05 / 7.8               |
| Directory, 12000 dp/s       | **2.45** / 5.7 | 4.42 / 9.0   | 4.82 / 10.5     | 4.83 / 10.2              |
| Gallery, 12000 dp/s         | **1.75** / 3.6 | 2.30 / 5.1   | 3.21 / 7.4      | 3.64 / 8.8               |

- With the same row views and the same precomputed layouts, SLKListView spends 11 to 19 percent
  less UI thread time per frame than RecyclerView at 4000 dp/s, and 19 to 45 percent less at
  12000 dp/s. Its p95 frame is lower on every screen. The gap grows with speed because a frame
  inside the core's band makes no JNI call and a moved row is only offset, not laid out.
- Self-sizing evens it out. Both lists then spend most of the frame measuring text on the UI
  thread. SLK is ahead on Feed and Gallery, behind on Chat at 12000 dp/s and on Directory at 4000.
- No engine showed a blank area. Dropped frames and RenderThread time are the same for all
  engines within noise. The emulator draws on the host GPU and is RenderThread bound.
- The host load average was 7 to 25 during these runs, which inflates absolute times. Runs are
  interleaved, which spreads that load across engines. Treat the ratios as the result. A
  physical device run is still to do.

React Native, the same bench on the RN example (`rn`) next to SLK and RecyclerView, 1000 rows,
3 interleaved runs, medians of UI ms per frame (mean / p95) and process CPU ms per second.
The emulator is the one above, on 2026-10-07. Raw runs are in `results/rn-4000` and
`results/rn-12000`.

| UI ms per frame, process CPU ms/s | SLK                 | RecyclerView    | shadowlist RN     |
| --------------------------------- | ------------------- | --------------- | ----------------- |
| Feed, 4000 dp/s                   | **0.85** / 2.2, 369 | 0.98 / 2.8, 392 | 3.25 / 13.9, 572  |
| Chat, 4000 dp/s                   | **0.82** / 1.8, 338 | 0.90 / 2.3, 339 | 2.23 / 8.6, 574   |
| Directory, 4000 dp/s              | **1.35** / 2.4, 328 | 1.47 / 2.7, 340 | 6.91 / 24.1, 1563 |
| Gallery, 4000 dp/s                | **0.98** / 1.9, 335 | 1.06 / 2.4, 343 | 3.13 / 11.7, 607  |
| Feed, 12000 dp/s                  | **1.41** / 4.0, 185 | 1.57 / 4.4, 193 | 7.78 / 21.4, 643  |
| Chat, 12000 dp/s                  | **1.52** / 3.9, 181 | 1.97 / 4.3, 193 | 5.95 / 16.5, 601  |
| Directory, 12000 dp/s             | **2.07** / 4.8, 187 | 2.29 / 5.5, 187 | 8.27 / 39.4, 1444 |
| Gallery, 12000 dp/s               | **1.73** / 4.3, 160 | 1.77 / 4.3, 175 | 9.33 / 26.2, 812  |

- RN spends 2.5 to 5 times the UI thread time per frame of either native list, and its p95
  frame is 3 to 8 times longer. Its process CPU is 1.5 to 4.5 times higher, because the JS thread
  and Fabric commits run next to the UI thread. Directory is RN's worst screen: 1.4 to 1.6 s of
  CPU per second and a 230 MB peak against 35 MB for SLK.
- No engine showed a blank area.
- A game ran on the host during the 12000 dp/s runs (load average 10 to 22). Every engine then
  missed about half its frames (hitch 360 to 520 ms/s), and the 12000 numbers are only good as
  ratios between engines. The 4000 dp/s runs had a load of 4 to 12, after a short spike to 26.

Data updates, `SLScenario cost`, 10k rows, `SLImages 0`, apps compiled with `-m speed`, the
list's own UI ms for one update plus a synchronous layout, 3 interleaved rounds of 30 updates,
medians. Before is the list before the key and reconcile work of 2026-10-07.

| 10k rows                        | SLK before | SLK  | RecyclerView |
| ------------------------------- | ---------- | ---- | ------------ |
| Chat, prepend 50                | 2.91       | 0.43 | 0.06         |
| Chat, append 10                 | 1.45       | 0.12 | 0.07         |
| Feed, prepend 10                | 1.22       | 0.46 | 0.03 to 0.07 |
| Feed, append 20                 | 0.92       | 0.19 | 0.05 to 0.09 |
| Directory, regroup after 10 new | 11.5       | 5.5  | 0.4 to 0.6   |
| Directory, 10 more              | 12.4       | 5.2  | 0.5          |

- The old numbers in this table (RecyclerView 0.21 ms) measured almost nothing for
  RecyclerView: with a fixed size it defers adapter updates to the next frame, past the timed
  span. The scenario now forces the layout, and the screen's own row building is timed apart.
- Where the time went before: the core rehashed its whole key map on every append or prepend
  (`reserve` with the exact size), reallocated the row vector on every append, rebuilt the key
  map for every other change and compared every key, and the driver copied every key. JNI
  crossed one string per key on `reloadData`.
- Now the key map grows like any hash map, trims at either end keep it, other changes update
  it in place, an edit at one end skips the key comparison, and `reloadData` sends only the
  changed middle in one char array. A host microbenchmark of the driver at 10k rows: prepend
  of 50 0.151 to 0.042 ms, trim of 50 at the end 0.58 to 0.018 ms, 10 scattered inserts 0.64
  to 0.27 ms.
- RecyclerView still costs less per update. Its range notifications only touch the visible
  rows. SLK reflows every row's offset after a change, which is O(n) by design of the core.
  The Directory regroup is a full `notifyDataSetChanged` for RecyclerView, which loses the
  position (177 px). SLK keeps it (0 px).
