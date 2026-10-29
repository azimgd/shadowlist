# ShadowListKit for UIKit

ShadowListKit is the UIKit package: ShadowListKitListView, a UIScrollView with a data source like UITableView's.
It is written in Objective-C++ on the shared C++ core.

## Install

Swift Package Manager: add `https://github.com/azimgd/shadowlist` and the ShadowListKit product.

```swift
.package(url: "https://github.com/azimgd/shadowlist", from: "0.9.0")
```

CocoaPods:

```ruby
pod 'ShadowListKit', '~> 0.9'
```

Then `import ShadowListKit` in Swift, or `#import <ShadowListKit/ShadowListKit.h>` in Objective-C.

### Requirements

| Dependency | Version |
| --- | --- |
| iOS | 16 or newer |

## Usage

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

`PostCell` subclasses `ShadowListKitListCell` and configures a `Post`.

## API

The table summarizes the API; [ShadowListKitListView.h](Sources/ShadowListKit/include/ShadowListKit/ShadowListKitListView.h)
has full signatures and defaults.

| Area | API | Notes |
| --- | --- | --- |
| Data source | `numberOfItemsInListView:`, `listView:keyForItemAtIndex:`, `listView:cellForItemAtIndex:`, `listView:reconfigureCell:atIndex:payload:` | `listView:reconfigureCell:atIndex:payload:` updates a shown cell for a payload reload. Return `NO` to reload the item in full. |
| Sizing | `listView:sizeForItemAtIndex:crossSize:` | Optional. Without it every item is measured once through its cell's `sizeThatFits:`, which fits Auto Layout constraints. |
| Sections | `numberOfSectionsInListView:`, `listView:numberOfItemsInSection:`, `listView:keyForSection:`, `listView:titleForHeaderInSection:`, `listView:titleForFooterInSection:`, `listView:cellForHeaderInSection:`, `listView:sizeForHeaderInSection:crossSize:`, `sectionIndexTitlesForListView:` | A title, an empty one too, gives a section a header or footer, shown in a plain cell unless a cell is given. The section key defaults to the key of its first item. |
| Content versions | `listView:contentVersionForItemAtIndex:` | A revision or hash. `applyChanges` reloads items whose value changed. |
| Delegate | `listView:willDisplayCell:atIndex:`, `listView:didEndDisplayingCell:atIndex:`, `listView:didSelectItemAtIndex:`, `listView:didDeselectItemAtIndex:`, `listView:shouldSelectItemAtIndex:`, `listView:shouldHighlightItemAtIndex:`, `listView:canMoveItemAtIndex:`, `listView:moveItemAtIndex:toIndex:`, `listViewDidReachStart:`, `listViewDidReachEnd:` | Every method is optional. Inherits `UIScrollViewDelegate` for scroll events. |
| Cells | `registerClass:forCellReuseIdentifier:`, `dequeueReusableCellWithIdentifier:` | `prepareForReuse` runs before a cell is handed out again. |
| Layout | `inverted`, `followAppends`, `horizontal`, `numberOfColumns`, `estimatedItemSize`, `overscan`, `mountOverscan`, `startReachedThreshold`, `endReachedThreshold`, `snapToItem`, `snapAlignment`, `reorderEnabled`, `stickyIndices`, `stickySectionHeaders`, `headerView`, `footerView` | `followAppends` is off by default, which keeps the visible items in place. Call `setNeedsLayout` on the list after `headerView` or `footerView` changes size. |
| Changes | `reloadData`, `insertItemsAtIndices:`, `deleteItemsAtIndices:`, `reloadItemsAtIndices:`, `reloadItemsAtIndices:payload:`, `moveItemAtIndex:toIndex:`, `performBatchUpdates:completion:`, `applyChanges` | An insert past the end goes at the end, a delete past the end is dropped. `applyChanges` diffs every key and returns a `ShadowListKitListChanges`. |
| Animations | `animatesChanges`, `itemAnimator` | Off by default. Inserted items fade in, deleted items fade out and items that stay slide. `ShadowListKitDefaultItemAnimator` unless replaced. |
| Selection | `allowsSelection`, `allowsMultipleSelection`, `editing`, `selectedIndices`, `selectItemAtIndex:animated:`, `deselectItemAtIndex:animated:`, `setEditing:animated:` | Selection follows keys. The select and deselect calls skip the delegate. `editing` turns swipe actions and reordering off. |
| Swipe actions | Delegate `listView:leadingSwipeActionsForItemAtIndex:`, `listView:trailingSwipeActionsForItemAtIndex:`, `closeSwipeActionsAnimated:` | A `ShadowListKitSwipeActionsConfiguration` per side. A full swipe performs the first action. |
| Context menus | Delegate `listView:contextMenuForItemAtIndex:` | A `UIMenu` through `UIContextMenuInteraction`. A reorderable item lifts on the hold and shows the menu when let go in place. |
| Prefetching | `prefetchDataSource`: `listView:prefetchItemsAtIndices:`, `listView:cancelPrefetchingForItemsAtIndices:` | Items in the measured range past the mounted ones, prefetched by the layout pass that brings them in, after it measures the items it will show and before it measures these. Without `listView:sizeForItemAtIndex:crossSize:` that is before `listView:cellForItemAtIndex:` makes their cells. Cancelled when one leaves the range unseen. |
| Pull to refresh | `refreshEnabled`, `refreshing`, delegate `listViewDidBeginRefreshing:` | A `UIRefreshControl` the list owns. |
| Separators | `showsSeparators`, `separatorColor`, `separatorInsetStart`, `separatorInsetEnd`, `separatorThickness`, delegate `listView:showsSeparatorAfterItemAtIndex:` | Only between items of one section. One device pixel thick by default. |
| Queries | `cellForItemAtIndex:`, `visibleCells`, `visibleRange`, `rectForItemAtIndex:`, `numberOfSections`, `sectionForItemAtIndex:`, `firstItemIndexInSection:`, `rectForHeaderInSection:`, `rectForFooterInSection:` | `visibleRange` runs low to high. |
| Scrolling | `scrollToItemAtIndex:viewPosition:animated:`, `scrollToSection:animated:`, `scrollToStartAnimated:`, `scrollToEndAnimated:` | `viewPosition` goes from 0 at the viewport start to 1 at the end. The core keeps correcting until the item lands. |
| Saved position | `anchorState`, `restoreAnchorState:` | The key at the viewport start and the distance into it. Lands now or once a reload brings the key. Saved with UIKit state restoration when the list has a `restorationIdentifier`. |
| Accessibility | Every item is an accessibility element | VoiceOver page scrolls move past the mounted items. The rotor is not supported. |

Masonry is round robin by row: row `r`, counting section header and footer rows, goes in column
`r % numberOfColumns`, and each column stacks its own rows. Full width rows in a grid are not supported.

These combinations do nothing:

- `showsSeparators` in a grid.
- `sectionIndexTitlesForListView:` in a horizontal list.
- `refreshEnabled` in a horizontal list. `UIRefreshControl` only pulls vertically.
- `followAppends` without `inverted`.
