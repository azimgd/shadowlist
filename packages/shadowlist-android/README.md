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
| Sizing | `Sizing`: `sizeForItem` | Optional. Without it every item is measured once through its cell. |
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
| Prefetching | `prefetchDataSource`: `prefetchItems`, `cancelPrefetchingForItems` | Items in the measured range without a cell yet. Cancelled when one leaves the range unseen. |
| Pull to refresh | `refreshEnabled`, `refreshing`, delegate `didBeginRefreshing` | Draws its own spinner, without a SwipeRefreshLayout dependency. |
| Separators | `showsSeparators`, `separatorColor`, `separatorInsetStart`, `separatorInsetEnd`, `separatorThickness`, delegate `showsSeparatorAfterItem`, `addItemDecoration` | Only between items of one section. `ShadowListKitItemDecoration` draws below or above the rows, like RecyclerView's ItemDecoration without item offsets. |
| Queries | `cellForItem`, `visibleCells`, `visibleRange`, `rectForItem`, `contentSize`, `numberOfSections`, `sectionForItem`, `firstItemIndexInSection`, `rectForHeaderInSection`, `rectForFooterInSection` | `visibleRange` runs low to high, null before the first layout. Section header and footer rows do not count. |
| Scrolling | `scrollToItem(index, viewPosition, animated)`, `scrollToSection`, `scrollToStart`, `scrollToEnd` | `viewPosition` goes from 0 at the viewport start to 1 at the end. The core keeps correcting until the item lands. |
| Saved position | `anchorState`, `restoreAnchorState` | The key at the viewport start and the distance into it. Lands now or once a reload brings the key. Saved in `onSaveInstanceState` when the list has an id. |
| Accessibility | `CollectionInfo` for all rows, `CollectionItemInfo` per cell | Page scrolls and scroll to position reach rows that are not mounted, which lets TalkBack move past the mounted rows. |

Masonry is round robin: item `i` goes in column `i % numberOfColumns`. Full width rows in a grid are not
supported.
