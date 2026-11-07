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

## API

The table summarizes the API; [ShadowListKitListView.kt](ShadowListKit/src/main/java/com/shadowlist/kit/ShadowListKitListView.kt)
has full signatures and defaults.

| Area | API | Notes |
| --- | --- | --- |
| Data source | `DataSource`: `numberOfItems`, `keyForItem`, `cellForItem`, `reconfigureCell` | `reconfigureCell` updates a shown cell for a payload reload. Return `false` to reload the item in full. |
| Sizing | `Sizing`: `sizeForItem`, `exactSizeForItem` | Optional. Without it every item is measured once through its cell. `exactSizeForItem` gives a size with its fraction of a pixel, like 21dp at a density of 2.625, and defaults to `sizeForItem`. |
| Sections | `Sections`: `numberOfSections`, `numberOfItemsInSection`, `keyForSection`, `titleForHeaderInSection`, `titleForFooterInSection`, `cellForHeaderInSection`, `sizeForHeaderInSection`, `sectionIndexTitles` | A title, an empty one too, gives a section a header or footer, shown in a plain cell unless a cell is given. The section key defaults to the key of its first item. |
| Content versions | `ContentVersions`: `contentVersionForItem` | A revision or hash. `applyChanges` reloads items whose value changed. |
| Delegate | `willDisplayCell`, `didEndDisplayingCell`, `didSelectItem`, `didDeselectItem`, `shouldSelectItem`, `shouldHighlightItem`, `canMoveItem`, `moveItem`, `didReachStart`, `didReachEnd`, `didScroll`, `didEndScrolling` | Every method has a default. |
| Cells | `registerCell`, `dequeueReusableCell` |  |
| Layout | `inverted`, `followAppends`, `horizontal`, `numberOfColumns`, `estimatedItemSize`, `overscan`, `mountOverscan`, `startReachedThreshold`, `endReachedThreshold`, `snapToItem`, `snapAlignment`, `reorderEnabled`, `stickyIndices`, `stickySectionHeaders`, `headerView`, `footerView` | `followAppends` is off by default, which keeps the visible items in place. Call `requestLayout` on the list after `headerView` or `footerView` changes size. |
| Changes | `reloadData`, `insertItems`, `deleteItems`, `reloadItems(indices, payload)`, `moveItem(sourceIndex, destinationIndex)`, `performBatchUpdates`, `applyChanges` | An insert past the end goes at the end, a delete past the end is dropped. `applyChanges` diffs every key and returns a `ShadowListKitListChanges`. |
| Animations | `animatesChanges`, `itemAnimator` | Off by default. Inserted items fade in, deleted items fade out and items that stay slide. `ShadowListKitDefaultItemAnimator` unless replaced. |
| Selection | `allowsSelection`, `allowsMultipleSelection`, `editing`, `selectedIndices`, `selectItem`, `deselectItem`, `setEditing` | Selection follows keys and survives detach. The select and deselect calls skip the delegate. `editing` turns swipe actions and reordering off. |
| Swipe actions | Delegate `leadingSwipeActionsForItem`, `trailingSwipeActionsForItem`, `closeSwipeActions` | A `ShadowListKitSwipeActionsConfiguration` per side. A full swipe performs the first action. |
| Context menus | Delegate `contextMenuForItem` | Fills a `PopupMenu` on a hold. A reorderable item lifts on the hold and shows the menu when let go in place. |
| Prefetching | `prefetchDataSource`: `prefetchItems`, `cancelPrefetchingForItems` | Items in the measured range past the mounted ones, prefetched by the layout pass that brings them in, after it measures the items it will show and before it measures these. Without `sizeForItem` that is before `cellForItem` makes their cells. Cancelled when one leaves the range unseen. |
| Pull to refresh | `refreshEnabled`, `refreshing`, delegate `didBeginRefreshing` | Draws its own spinner, without a SwipeRefreshLayout dependency. Works in horizontal lists too. |
| Separators | `showsSeparators`, `separatorColor`, `separatorInsetStart`, `separatorInsetEnd`, `separatorThickness`, delegate `showsSeparatorAfterItem`, `addItemDecoration` | Only between items of one section. `ShadowListKitItemDecoration` draws below or above the rows, like RecyclerView's ItemDecoration without item offsets. |
| Queries | `cellForItem`, `visibleCells`, `visibleRange`, `rectForItem`, `contentSize`, `numberOfSections`, `sectionForItem`, `firstItemIndexInSection`, `rectForHeaderInSection`, `rectForFooterInSection` | `visibleRange` runs low to high, null before the first layout. Section header and footer rows do not count. |
| Scrolling | `scrollToItem(index, viewPosition, animated)`, `scrollToSection`, `scrollToStart`, `scrollToEnd`, `adjustOffsetBy(delta)`, `scrollGestureEnabled` | `viewPosition` goes from 0 at the viewport start to 1 at the end. The core keeps correcting until the item lands. See [Hosting the list](#hosting-the-list) for the last two. |
| Saved position | `anchorState`, `restoreAnchorState` | The key at the viewport start and the distance into it. Lands now or once a reload brings the key. Saved in `onSaveInstanceState` when the list has an id. |
| Accessibility | `CollectionInfo` for all rows, `CollectionItemInfo` per cell | Page scrolls and scroll to position reach rows that are not mounted, which lets TalkBack move past the mounted rows. |

Rows sit on whole pixels rounded from the exact sum of the sizes before them, and the offset
keeps the fraction of a pixel the core's corrections leave. Rows of a fractional size meet without
gaps, end where their sizes add up to and stay still when the core moves them and the offset
together.

Masonry is round robin by row: row `r`, counting section header and footer rows, goes in column
`r % numberOfColumns`, and each column stacks its own rows. Full width rows in a grid are not supported.

These combinations do nothing:

- `showsSeparators` in a grid.
- `sectionIndexTitles` in a horizontal list.
- `followAppends` without `inverted`.

## Hosting the list

A view that owns the scrolling, like a spreadsheet grid that pans on two axes, can put the list
inside itself and drive it:

- `scrollGestureEnabled = false` turns off the list's touch scrolling, flings, edge pulls and
  nested scrolling. The list takes no drag and a touch no row took goes back to the parent. The
  host moves the list with `scrollTo`, `scrollToItem` and `adjustOffsetBy`. Taps, swipe actions and
  reordering stay with their own switches.
- `adjustOffsetBy(delta: Double)` moves the offset by `delta` pixels, fractions included, and lays
  out right away. `scrollTo` holds the offset inside the content size, which counts estimates for
  rows not measured yet. This one leaves that to the layout pass, which measures the rows at the
  new offset first.
- A subclass overrides `didLayOut()`, which runs after every layout pass once the rows it measured
  and the cells it mounted are in place, before the frame is drawn.
- `performBatchUpdates` that deletes rows at one end and inserts rows at the other keeps the
  visible rows where they are on screen by key, in the layout pass that follows.
- `importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS` hides the list
  and its cells from TalkBack when the host gives its own accessibility nodes. The list does not
  change it.

### Building the kit in a host library

`ShadowListKit/src/main/cpp/CMakeLists.txt` builds `libshadowlistkit.so`. A host's CMake can add
it with `add_subdirectory` and set two variables first:

| Variable | Meaning |
| --- | --- |
| `SHADOWLIST_KIT_CORE_DIR` | A copy of the core to compile against, in a folder named `shadowlist-core`. Defaults to `packages/shadowlist-core` in this repo. |
| `SHADOWLIST_KIT_COMPILE_DEFINITIONS` | Definitions added to the kit target, like `azimgd=host_kit_azimgd` to rename the core's namespace when the app links a second copy of the core. |

```cmake
set(SHADOWLIST_KIT_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/shadowlist-core)
set(SHADOWLIST_KIT_COMPILE_DEFINITIONS "azimgd=host_kit_azimgd")
add_subdirectory(vendor/shadowlistkit/cpp shadowlistkit)
```
