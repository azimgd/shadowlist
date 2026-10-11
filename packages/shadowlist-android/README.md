# ShadowListKit for Android

ShadowListKit is the Android package: ShadowListKitListView, a ViewGroup with a data source.
It is written in Kotlin on the shared C++ core.

## Install

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

### Requirements

| Dependency | Version |
| --- | --- |
| Android | API 24 or newer |
| Java | 17 or newer |

The AAR ships `libshadowlistkit.so` with the C++ runtime linked statically, aligned for 16 KB pages.
`./gradlew :ShadowListKit:publishToMavenLocal` installs the current checkout as
`io.github.azimgd:shadowlist-kit` with `VERSION_NAME` from `ShadowListKit/gradle.properties`.

## Usage

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

`PostCell` subclasses `ShadowListKitListCell`. Add `list` to your screen's view hierarchy.

## Essential behavior

An item is data, a row is a layout entry, a cell is its mounted view, and a key is stable identity.

**Keys.** `keyForItem` is the item's stable identity. Sizes, cells, selection and the scroll position follow
keys. Content that changes under the same key needs `reloadItems` or a content version.

**Data first.** Update the data source, then tell the list. Every change call reads the data source as it is
now.

**Change indices.** An insert names positions in the new data, a delete positions in the old data. In a batch,
deletes, reloads and move sources are in the data before, inserts and move destinations in the data after. A
batch that does not add up reloads everything.

**Section indices.** Item indices run across all sections, the same as without sections. In a list with
sections every change reads the sections and keys again, like `reloadData`.

**Threading.** Measurement, offset corrections and mounting run in one layout pass on the UI thread. Sizes
from `Sizing.sizeForItem` can come from layouts precomputed off the UI thread.

**Units.** Sizes and offsets are in pixels.

**Coordinates.** `rectForItem` and the section rects are in content coordinates: from the start of the
content, `headerView` included, without the padding or the scroll offset. An item not measured yet has an
estimated rect.

**Visible content.** Measuring, inserting above, deleting and regrouping keep the visible content still.

## API

See [ShadowListKitListView.kt](ShadowListKit/src/main/java/com/shadowlist/kit/ShadowListKitListView.kt) for
full signatures and defaults.

- **Data source:** `DataSource`: `numberOfItems`, `keyForItem`, `cellForItem`, `reconfigureCell`.
  `reconfigureCell` updates a shown cell for a payload reload. Return `false` to reload the item in full.
- **Sizing:** `Sizing`: `sizeForItem`. Optional. Without it every item is measured once through its cell.
- **Sections:** `Sections`: `numberOfSections`, `numberOfItemsInSection`, `keyForSection`,
  `titleForHeaderInSection`, `titleForFooterInSection`, `cellForHeaderInSection`, `sizeForHeaderInSection`,
  `sectionIndexTitles`. A title, an empty one too, gives a section a header or footer, shown in a plain cell
  unless a cell is given. The section key defaults to the key of its first item.
- **Content versions:** `ContentVersions`: `contentVersionForItem`. A revision or hash. `applyChanges` reloads
  items whose value changed.
- **Delegate:** `willDisplayCell`, `didEndDisplayingCell`, `didSelectItem`, `didDeselectItem`,
  `shouldSelectItem`, `shouldHighlightItem`, `canMoveItem`, `moveItem`, `didReachStart`, `didReachEnd`,
  `didScroll`, `didEndScrolling`. Every method has a default.
- **Cells:** `registerCell`, `dequeueReusableCell`.
- **Layout:** `inverted`, `followAppends`, `horizontal`, `numberOfColumns`, `estimatedItemSize`, `overscan`,
  `mountOverscan`, `startReachedThreshold`, `endReachedThreshold`, `snapToItem`, `snapAlignment`,
  `reorderEnabled`, `stickyIndices`, `stickySectionHeaders`, `headerView`, `footerView`. `followAppends` is
  off by default, which keeps the visible items in place. Call `requestLayout` on the list after `headerView`
  or `footerView` changes size.
- **Changes:** `reloadData`, `insertItems`, `deleteItems`, `reloadItems(indices, payload)`,
  `moveItem(sourceIndex, destinationIndex)`, `performBatchUpdates`, `applyChanges`. An insert past the end
  goes at the end, a delete past the end is dropped. `applyChanges` diffs every key and returns a
  `ShadowListKitListChanges`.
- **Animations:** `animatesChanges`, `itemAnimator`. Off by default. Inserted items fade in, deleted items
  fade out and items that stay slide. `ShadowListKitDefaultItemAnimator` unless replaced.
- **Selection:** `allowsSelection`, `allowsMultipleSelection`, `editing`, `selectedIndices`, `selectItem`,
  `deselectItem`, `setEditing`. Selection follows keys and survives detach. The select and deselect calls skip
  the delegate. `editing` turns swipe actions and reordering off.
- **Swipe actions:** Delegate `leadingSwipeActionsForItem`, `trailingSwipeActionsForItem`,
  `closeSwipeActions`. A `ShadowListKitSwipeActionsConfiguration` per side. A full swipe performs the first
  action.
- **Context menus:** Delegate `contextMenuForItem`. Fills a `PopupMenu` on a hold. A reorderable item lifts on
  the hold and shows the menu when let go in place.
- **Prefetching:** `prefetchDataSource`: `prefetchItems`, `cancelPrefetchingForItems`. Items in the measured
  range without a cell yet. Cancelled when one leaves the range unseen.
- **Pull to refresh:** `refreshEnabled`, `refreshing`, delegate `didBeginRefreshing`. Draws its own spinner,
  without a SwipeRefreshLayout dependency.
- **Separators:** `showsSeparators`, `separatorColor`, `separatorInsetStart`, `separatorInsetEnd`,
  `separatorThickness`, delegate `showsSeparatorAfterItem`, `addItemDecoration`. Only between items of one
  section. `ShadowListKitItemDecoration` draws below or above the rows, like RecyclerView's ItemDecoration
  without item offsets.
- **Queries:** `cellForItem`, `visibleCells`, `visibleRange`, `rectForItem`, `contentSize`,
  `numberOfSections`, `sectionForItem`, `firstItemIndexInSection`, `rectForHeaderInSection`,
  `rectForFooterInSection`. `visibleRange` runs low to high, null before the first layout. Section header and
  footer rows do not count.
- **Scrolling:** `scrollToItem(index, viewPosition, animated)`, `scrollToSection`, `scrollToStart`,
  `scrollToEnd`. `viewPosition` goes from 0 at the viewport start to 1 at the end. The core keeps correcting
  until the item lands.
- **Saved position:** `anchorState`, `restoreAnchorState`. The key at the viewport start and the distance into
  it. Lands now or once a reload brings the key. Saved in `onSaveInstanceState` when the list has an id.
- **Accessibility:** `CollectionInfo` for all rows, `CollectionItemInfo` per cell. Page scrolls and scroll to
  position reach rows that are not mounted, which lets TalkBack move past the mounted rows.

Masonry is round robin: item `i` goes in column `i % numberOfColumns`. Full width rows in a grid are not
supported.

### View behavior

| Behavior | Details |
| --- | --- |
| Padding | Works like content insets. The core's viewport is the padded area, rows start inside the padding, sticky headers pin below the top padding, and with `clipToPadding = false` rows scroll under it. |
| Edge effect | A stretch on Android 12 and later, a glow before. Shows when a drag pulls past an end or a fling hits one, along the scroll axis, following `overScrollMode`. |
| Nested scrolling | It is a `NestedScrollingChild3`. Inside a `CoordinatorLayout` a collapsing `AppBarLayout` takes its share of drags and flings first. |

## Example app

The example app lives in `templates/shadowlist-android-example`. From the repo root:

```sh
cd templates/shadowlist-android-example
./gradlew :app:assembleRelease
adb install -r app/build/outputs/apk/release/app-release.apk
adb shell am start -S -n shadowlist.android.example/com.shadowlist.kit.example.MainActivity \
  --es SLRoute Chat --es SLEngine sl
```

The release build is optimized, not debuggable and signed with the debug key.

`SLRoute` takes `Feed`, `Chat`, `SectionList` and `Masonry`, which run on any engine, and `Reorder`,
`ReorderGrid`,
`Snap`, `Horizontal`, `Changes`, `Collapsing`, `Sections` and `Inbox`, which run on ShadowListKitListView
only.

| `SLEngine` | List | Row sizes |
| --- | --- | --- |
| `sl` | ShadowListKitListView | precomputed layouts, off the UI thread |
| `sl-auto` | ShadowListKitListView | each cell measured on the UI thread |
| `recycler` | RecyclerView | fixed heights from the same precomputed layouts |
| `recycler-auto` | RecyclerView | `wrap_content` rows measured on the UI thread |

Extras: `SLCount N` rows, `SLImages 0`, `SLPadding N` (dp of padding, rows under it),
`SLAnimate 1` (`animatesChanges` on the list screens), `SLAutoDrag 1` on the reorder screens.

All engines show the same row views. RecyclerView uses `LinearLayoutManager` (with
`stackFromEnd` for chat), `StaggeredGridLayoutManager` for the gallery, stable ids, its default
prefetch, and the usual item decoration for sticky headers.

See more: [Example apps](../../README.md#example-apps)

## Debug log

The core's `[SL]` log is off by default. It prints on every pass.

| Platform | How to turn it on |
| --- | --- |
| Android | Build the module from a checkout with `-PshadowlistDebugLog` to compile it in. Read it with `adb logcat -s SL`. |

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
implements `ShadowListKitBenchProbe.benchBlankFraction()`, and the result adds `contentBlankAvg`,
`contentBlankMax`
and `contentBlankFrames`.

Scenarios run from the example itself:

```sh
adb shell am start -S -n shadowlist.android.example/com.shadowlist.kit.example.MainActivity \
  --es SLRoute SectionList --es SLEngine recycler --es SLScenario prepend --es SLBenchExit 1
adb logcat -s SLSCENARIO SLAUTODRAG
```

| Scenario | Route | What it checks |
| --- | --- | --- |
| `prepend`, `append` | any | Rest mid list, change the data, report how far visible rows moved. |
| `cost` | any | UI thread time of one update plus a synchronous layout, with the layout prefetch finished and paused. `prependMs`/`appendMs` count the list's own work (the update call and the layout), the `*TotalMs` fields add the screen building its rows. `SLCostRuns N` sets the updates of each kind. Compile the apps with `adb shell cmd package compile -m speed -f <pkg>` first. Interpreted code doubles the numbers and their noise. |
| `a11y` | any list screen | The collection info, the actions and two accessibility scrolls. |
| `snap` | `Snap` | Feature check through synthesized touches or the public API. |
| `horizontal` | `Horizontal` | Feature check through synthesized touches or the public API. |
| `animate` | `Changes` | Feature check through synthesized touches or the public API. |
| `nested` | `Collapsing` | Feature check through synthesized touches or the public API. |
| `sections` | `Sections` | The feature screen's checks, one `SLSCENARIO` JSON line. |
| `features` | `Inbox` | The feature screen's checks, one `SLSCENARIO` JSON line. Swipes, holds and pulls with synthesized touches and saves the position through `saveHierarchyState`. |
| `SLAutoDrag 1` | `Reorder`, `ReorderGrid` | Touch and hold the second row, drag it and log the order before and after. |

## Source layout

| File | Role |
| --- | --- |
| `packages/shadowlist-core/host/ListDriver.{hpp,cpp}` | The core driven for a native list: layout passes, measurement, keys, sticky math, scroll landing, drag. |
| `ShadowListKit/src/main/cpp/ShadowListKitCoreJNI.cpp` | Thin JNI hops into the driver with reused arrays. |
| `ShadowListKitListView.kt` | Public API, layout pass, mounting, sticky placement, scroll commands. |
| `ShadowListKitScrollGesture.kt` | Touch scrolling, flings, snapping, animated scrolls, the gesture phase and nested scrolling. |
| `ShadowListKitEdgeEffects.kt` | The platform edge effect at both ends, glow or stretch. |
| `packages/shadowlist-core/host/ListSections`, `KeyDiff`, `SwipeReveal` | Sections over the rows, the key diff and batch plan, swipe offsets, reached through JNI. |
| `ShadowListKitChangeAnimator.kt`, `ShadowListKitItemAnimator.kt` | What `animatesChanges` animates, and the pluggable animations. |
| `ShadowListKitDragController.kt` | Touch and hold to reorder through the host layer's DragReorder, and the row menu on a hold. |
| `ShadowListKitSwipeController.kt` | Swipe actions and the buttons behind a swiped row. |
| `ShadowListKitRefreshIndicator.kt`, `ShadowListKitSectionIndex.kt` | Pull to refresh without SwipeRefreshLayout, the section index. |
| `ShadowListKitSwipeAction.kt`, `ShadowListKitAnchorState.kt`, `ShadowListKitListChanges.kt`, `ShadowListKitItemDecoration.kt` | Public value types and the decoration hook. |
| `ShadowListKitCore.kt`, `ShadowListKitListCell.kt`, `ShadowListKitText.kt` | JNI wrapper, row base view with its states, precomputed text. |

`ShadowListKitTextLayout` breaks text into lines once with `StaticLayout`, on any thread, and
`ShadowListKitTextView` only draws it.
