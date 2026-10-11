# ShadowListKit for UIKit

ShadowListKit is the UIKit package: ShadowListKitListView, a UIScrollView with a UITableView-like data source.
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

## Essential behavior

An item is data, a row is a layout entry, a cell is its mounted view, and a key is stable identity.

**Keys.** `listView:keyForItemAtIndex:` is the item's stable identity. Sizes, cells, selection and the scroll
position follow keys. Content that changes under the same key needs `reloadItemsAtIndices:` or a content
version.

**Data first.** Update the data source, then tell the list. Every change call reads the data source as it is
now.

**Change indices.** An insert names positions in the new data, a delete positions in the old data. In a batch,
deletes, reloads and move sources are in the data before, inserts and move destinations in the data after. A
batch that does not add up reloads everything.

**Section indices.** Item indices run across all sections, the same as without sections. In a list with
sections every change reads the sections and keys again, like `reloadData`.

**Threading.** Measurement, offset corrections and mounting run in one layout pass on the main thread. Sizes
from `listView:sizeForItemAtIndex:crossSize:` can come from layouts precomputed off the main thread.

**Units.** Sizes and offsets are in points.

**Coordinates.** `rectForItemAtIndex:` and the section rects are in content coordinates: from the start of the
content, `headerView` included, without the content insets or the scroll offset. An item not measured yet has
an estimated rect.

**Visible content.** Measuring, inserting above and deleting keep the visible content still.

## API

See [ShadowListKitListView.h](Sources/ShadowListKit/include/ShadowListKit/ShadowListKitListView.h) for full
signatures and defaults.

- **Data source:** `numberOfItemsInListView:`, `listView:keyForItemAtIndex:`, `listView:cellForItemAtIndex:`,
  `listView:reconfigureCell:atIndex:payload:`. `listView:reconfigureCell:atIndex:payload:` updates a shown
  cell for a payload reload. Return `NO` to reload the item in full.
- **Sizing:** `listView:sizeForItemAtIndex:crossSize:`. Optional. Without it every item is measured once
  through its cell's `sizeThatFits:`, which fits Auto Layout constraints.
- **Sections:** `numberOfSectionsInListView:`, `listView:numberOfItemsInSection:`, `listView:keyForSection:`,
  `listView:titleForHeaderInSection:`, `listView:titleForFooterInSection:`,
  `listView:cellForHeaderInSection:`, `listView:sizeForHeaderInSection:crossSize:`,
  `sectionIndexTitlesForListView:`. A title, an empty one too, gives a section a header or footer, shown in a
  plain cell unless a cell is given. The section key defaults to the key of its first item.
- **Content versions:** `listView:contentVersionForItemAtIndex:`. A revision or hash. `applyChanges` reloads
  items whose value changed.
- **Delegate:** `listView:willDisplayCell:atIndex:`, `listView:didEndDisplayingCell:atIndex:`,
  `listView:didSelectItemAtIndex:`, `listView:didDeselectItemAtIndex:`, `listView:shouldSelectItemAtIndex:`,
  `listView:shouldHighlightItemAtIndex:`, `listView:canMoveItemAtIndex:`, `listView:moveItemAtIndex:toIndex:`,
  `listViewDidReachStart:`, `listViewDidReachEnd:`. Every method is optional. Inherits `UIScrollViewDelegate`
  for scroll events.
- **Cells:** `registerClass:forCellReuseIdentifier:`, `dequeueReusableCellWithIdentifier:`. `prepareForReuse`
  runs before a cell is handed out again.
- **Layout:** `inverted`, `followAppends`, `horizontal`, `numberOfColumns`, `estimatedItemSize`, `overscan`,
  `mountOverscan`, `startReachedThreshold`, `endReachedThreshold`, `snapToItem`, `snapAlignment`,
  `reorderEnabled`, `stickyIndices`, `stickySectionHeaders`, `headerView`, `footerView`. `followAppends` is
  off by default, which keeps the visible items in place. Call `setNeedsLayout` on the list after `headerView`
  or `footerView` changes size.
- **Changes:** `reloadData`, `insertItemsAtIndices:`, `deleteItemsAtIndices:`, `reloadItemsAtIndices:`,
  `reloadItemsAtIndices:payload:`, `moveItemAtIndex:toIndex:`, `performBatchUpdates:completion:`,
  `applyChanges`. An insert past the end goes at the end, a delete past the end is dropped. `applyChanges`
  diffs every key and returns a `ShadowListKitListChanges`.
- **Animations:** `animatesChanges`, `itemAnimator`. Off by default. Inserted items fade in, deleted items
  fade out and items that stay slide. `ShadowListKitDefaultItemAnimator` unless replaced.
- **Selection:** `allowsSelection`, `allowsMultipleSelection`, `editing`, `selectedIndices`,
  `selectItemAtIndex:animated:`, `deselectItemAtIndex:animated:`, `setEditing:animated:`. Selection follows
  keys. The select and deselect calls skip the delegate. `editing` turns swipe actions off.
- **Swipe actions:** Delegate `listView:leadingSwipeActionsForItemAtIndex:`,
  `listView:trailingSwipeActionsForItemAtIndex:`, `closeSwipeActionsAnimated:`. A
  `ShadowListKitSwipeActionsConfiguration` per side. A full swipe performs the first action.
- **Context menus:** Delegate `listView:contextMenuForItemAtIndex:`. A `UIMenu` through
  `UIContextMenuInteraction`. A reorderable item lifts on the hold and shows the menu when let go in place.
- **Prefetching:** `prefetchDataSource`: `listView:prefetchItemsAtIndices:`,
  `listView:cancelPrefetchingForItemsAtIndices:`. Items in the measured range without a cell yet. Cancelled
  when one leaves the range unseen.
- **Pull to refresh:** `refreshEnabled`, `refreshing`, delegate `listViewDidBeginRefreshing:`. A
  `UIRefreshControl` the list owns.
- **Separators:** `showsSeparators`, `separatorColor`, `separatorInsetStart`, `separatorInsetEnd`,
  `separatorThickness`, delegate `listView:showsSeparatorAfterItemAtIndex:`. Only between items of one
  section. One device pixel thick by default.
- **Queries:** `cellForItemAtIndex:`, `visibleCells`, `visibleRange`, `rectForItemAtIndex:`,
  `numberOfSections`, `sectionForItemAtIndex:`, `firstItemIndexInSection:`, `rectForHeaderInSection:`,
  `rectForFooterInSection:`. `visibleRange` runs low to high.
- **Scrolling:** `scrollToItemAtIndex:viewPosition:animated:`, `scrollToSection:animated:`,
  `scrollToStartAnimated:`, `scrollToEndAnimated:`. `viewPosition` goes from 0 at the viewport start to 1 at
  the end. The core keeps correcting until the item lands.
- **Saved position:** `anchorState`, `restoreAnchorState:`. The key at the viewport start and the distance
  into it. Lands now or once a reload brings the key. Saved with UIKit state restoration when the list has a
  `restorationIdentifier`.
- **Accessibility:** Every item is an accessibility element. VoiceOver page scrolls move past the mounted
  items. The rotor is not supported.

Masonry is round robin: item `i` goes in column `i % numberOfColumns`. Full width rows in a grid are not
supported.

## Example app

The example app lives in `templates/shadowlist-uikit-example`. From the repo root:

```sh
cd templates/shadowlist-uikit-example
./build.sh sim <simulator-udid>      # or: ./build.sh device any
xcrun simctl install <udid> build/dd-sim/Build/Products/Release-iphonesimulator/ShadowListKitExample.app
xcrun simctl launch <udid> shadowlist.uikit.example -SLRoute Chat -SLEngine sl
```

`xcodegen` generates `ShadowListKitExample.xcodeproj` from its `project.yml`.

`-SLRoute` takes `Feed`, `Masonry`, `Chat`, `SectionList`, `Reorder` and `Snap`, which run on any engine, and
`Sections` and `Inbox`, which run on ShadowListKitListView only.

| `-SLEngine` | List | Row sizes |
| --- | --- | --- |
| `sl` | ShadowListKitListView | precomputed layouts, off the main thread |
| `sl-auto` | ShadowListKitListView | each cell's `sizeThatFits:` on the main thread |
| `table` | UITableView | `heightForRowAt` from the same precomputed layouts |
| `table-auto` | UITableView | self-sizing cells with an estimated height |

All engines show the same row views.
`-SLTextAsync 1` draws text on a background queue, and `-SLCount N` sets the list size.

See more: [Example apps](../../README.md#example-apps)

## Debug log

The core's `[SL]` log is off by default. It prints on every pass.

| Platform | How to turn it on |
| --- | --- |
| iOS | Run the example's `build.sh` with `SHADOWLIST_DEBUG_LOG=1` to compile it in. |

## Benchmarks

`Bench/ShadowListKitBench.m` drives an app's main scroll view at a constant speed and logs one JSON line with
frame intervals, hitch time, main thread and process CPU, memory, and how much of the viewport no row covers.

```sh
Bench/bench.sh device <devicectl-udid> my-label      # ENGINES, SCREENS, COUNTS, RUNS, SPEED, AXES
Bench/summarize.py results/my-label/runs.jsonl
```

`-SLBenchAxes y,x,xy` (`AXES` in `bench.sh`) runs one leg pair per axis and logs one line each with an
`axis` key. y drives the vertical scroll view with the most content, x the horizontal one, xy both at
once. An axis without a scroll view logs a line with `skipped`. A view that draws its content after
its rows show, like tiles, can adopt `ShadowListKitBenchProbe` and return the share of the viewport not drawn
yet from `shadowListKit_benchBlankFraction`. The bench asks the driven views and their superviews, and the
result then adds `contentBlankAvg`, `contentBlankMax` and `contentBlankFrames`. `blankAvg` stays the
share no row covers.

Correctness and update cost checks run from the example itself:

```sh
xcrun simctl launch --console-pty <udid> shadowlist.uikit.example -SLRoute SectionList -SLEngine table \
  -SLScenario prepend -SLBenchExit 1     # prepend | append | jump | cost | animate | a11y | sections | features
```

| Scenario | Route | What it checks |
| --- | --- | --- |
| `prepend`, `append` | any | Rest mid list, change the data, report how far visible rows moved. |
| `jump` | any | `scrollToItem` to a far item, report where it lands. |
| `cost` | any | Main thread time of one update plus its layout, with the layout prefetch finished and paused. `prependMs`/`appendMs` count the list's own work, the `*TotalMs` fields add the screen building its rows. `-SLCostRuns N` sets the updates of each kind. |
| `animate` | `Feed` | With `animatesChanges`, remove a visible row and add one, then log the fade and the slide while they run and whether they settled. |
| `a11y` | any | VoiceOver's view of the list: element count, the element of a far row and whether it is on screen after, and a page scroll. |
| `sections` | `Sections` | Section lookups, self-sized Auto Layout rows, `scrollToSection:`, the pinned header, separators, the index, and a regroup that keeps the visible rows still. |
| `features` | `Inbox` | Selection by key, a batch, `applyChanges`, a payload reload, a full and a partial swipe, the context menu, refresh, the saved position, prefetching and the item animator. |
| `-SLAutoDrag 1` | `Reorder` | Drags a row four rows down and logs the order. |
| `-SLSwipeDemo 1` | `Inbox` | Opens a row's trailing actions for a screenshot. |

## Source layout

| File | Role |
| --- | --- |
| `packages/shadowlist-core/host/ListDriver.{hpp,cpp}` | The core driven for a native list: layout passes, measurement, keys, mount plan, sticky math, scroll landing, drag. |
| `ShadowListKitListView.mm` | Public API, layout pass, mounting, sticky placement, separators, scroll events. |
| `ShadowListKitListView+Data.mm` | Data changes: reloads, inserts, deletes, moves, batches and `applyChanges`. |
| `ShadowListKitListView+Sections.mm` | Section queries and scrolling, sticky rows and the section index. |
| `ShadowListKitListView+Selection.mm` | Selection by key and the highlight of a touched row. |
| `ShadowListKitListView+Commands.mm` | Scroll commands and the saved position. |
| `ShadowListKitListView+Accessibility.mm` | Rows as accessibility elements and VoiceOver page scrolls. |
| `ShadowListKitListSupport.mm` | The settle display link target, the section title cell and the delegate proxy. |
| `packages/shadowlist-core/host/ListSections.{hpp,cpp}` | Sections over the rows: item and row indices, header and footer rows, row keys, drops inside a section. |
| `packages/shadowlist-core/host/KeyDiff.{hpp,cpp}` | `diffKeys` for `applyChanges` and `planBatch` for `performBatchUpdates:completion:`. |
| `packages/shadowlist-core/host/SwipeReveal.{hpp,cpp}`, `ListSelection.{hpp,cpp}` | Swipe action offsets and rests, selection by key. |
| `ShadowListKitListView+Drag.mm` | Touch and hold to reorder, and the testing hooks in `ShadowListKitListView+Testing.h`. |
| `ShadowListKitListView+Actions.mm` | Swipe actions and context menus. |
| `ShadowListKitSwipeActionsView.mm` | The buttons behind a swiped row. |
| `ShadowListKitListCell.mm` | Row base view: highlight, selection and editing states, Auto Layout fitting. |
| `ShadowListKitListModels.mm` | `ShadowListKitSwipeAction`, `ShadowListKitAnchorState`, `ShadowListKitListChanges`, `ShadowListKitDefaultItemAnimator`. |
| `ShadowListKitChangeAnimator.mm` | Works out what `animatesChanges` animates and hands it to the `itemAnimator`. |
| `ShadowListKitSectionIndexView.mm` | The section index along the trailing edge. |
| `include/ShadowListKit/*.h` | The public headers, Objective-C without C++. |
| `Internal/*.h` | State the list shares with its categories, the change animator and the support classes. Not public headers. |
| `ShadowListKitText.{h,mm}` | Precomputed text layout and drawing. |

`ShadowListKitTextLayout` breaks lines on any thread. `ShadowListKitTextView` draws the ready lines, or with
`displaysAsynchronously` a bitmap drawn on a background queue.
