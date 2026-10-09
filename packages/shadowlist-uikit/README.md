# ShadowListKit

A virtualized list for UIKit, written in Objective-C++ on the shadowlist core. No React Native.
`ShadowListKitListView` is a `UIScrollView` subclass with a UITableView-like data source. The visible
content stays still while rows are measured, inserted above or removed. Rows have stable keys.

## Source layout

| File                                                                             | Role                                                                                                                    |
| -------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| `packages/shadowlist-core/host/ListDriver.{hpp,cpp}`                             | The core driven for a native list: layout passes, measurement, keys, mount plan, sticky math, scroll landing, drag.     |
| `ShadowListKitListView.mm`                                                       | Public API, layout pass, mounting, sticky placement, scroll commands, delegate proxy.                                   |
| `packages/shadowlist-core/host/ListSections.{hpp,cpp}`                           | Sections over the rows: item and row indices, header and footer rows, row keys, drops inside a section.                 |
| `packages/shadowlist-core/host/KeyDiff.{hpp,cpp}`                                | `diffKeys` for `applyChanges` and `planBatch` for `performBatchUpdates:completion:`.                                    |
| `packages/shadowlist-core/host/SwipeReveal.{hpp,cpp}`, `ListSelection.{hpp,cpp}` | Swipe action offsets and rests, selection by key.                                                                       |
| `ShadowListKitListView+Drag.mm`                                                  | Touch and hold to reorder, and the testing hooks in `ShadowListKitListView+Testing.h`.                                  |
| `ShadowListKitListView+Actions.mm`                                               | Swipe actions and context menus.                                                                                        |
| `ShadowListKitListCell.mm`                                                       | Row base view: highlight, selection and editing states, Auto Layout fitting.                                            |
| `ShadowListKitListModels.mm`                                                     | `ShadowListKitSwipeAction`, `ShadowListKitAnchorState`, `ShadowListKitListChanges`, `ShadowListKitDefaultItemAnimator`. |
| `ShadowListKitChangeAnimator.mm`                                                 | Works out what `animatesChanges` animates and hands it to the `itemAnimator`.                                           |
| `ShadowListKitSectionIndexView.mm`                                               | The section index along the trailing edge.                                                                              |
| `include/ShadowListKit/*.h`                                                      | The public headers, Objective-C without C++.                                                                            |
| `Internal/*.h`                                                                   | State the list shares with its drag category and the change animator. Not public headers.                               |
| `ShadowListKitText.{h,mm}`                                                       | Precomputed text layout and drawing.                                                                                    |

`ShadowListKitTextLayout` breaks lines on any thread. `ShadowListKitTextView` draws the ready lines, or with
`displaysAsynchronously` a bitmap drawn on a background queue.

## API

The header, `ShadowListKitListView.h`, documents every member.

- Properties: `inverted`, `followAppends`, `horizontal`, `numberOfColumns` (round robin), `estimatedItemSize`,
  `overscan`, `mountOverscan`, `startReachedThreshold`, `endReachedThreshold`, `snapToItem`, `snapAlignment`,
  `reorderEnabled`, `animatesChanges`, `itemAnimator`, `stickyIndices`, `stickySectionHeaders`, `headerView`,
  `footerView`. Delegate `listViewDidReachStart:` and `listViewDidReachEnd:`. Every row is an accessibility
  element, and VoiceOver page scrolls move past the mounted rows.
- Data source: `numberOfItemsInListView:`, `listView:keyForItemAtIndex:`, `listView:cellForItemAtIndex:`, optional
  `listView:sizeForItemAtIndex:crossSize:`. Without sizes every row is measured through its cell's `sizeThatFits:`,
  which fits a cell's Auto Layout constraints unless the cell overrides it.
- Sections: `numberOfSectionsInListView:` and `listView:numberOfItemsInSection:` group the items. Item indices still
  run across all sections. `listView:titleForHeaderInSection:` and `...FooterInSection:` decide which sections have a
  header or footer, shown in a plain title cell unless `listView:cellForHeaderInSection:` gives one. Sizes come from
  `listView:sizeForHeaderInSection:crossSize:` or measuring. `listView:keyForSection:` defaults to the key of the
  section's first item. `sectionIndexTitlesForListView:` adds the index. `stickySectionHeaders` pins them.
  `numberOfSections`, `sectionForItemAtIndex:`, `firstItemIndexInSection:`, `rectForHeaderInSection:`,
  `scrollToSection:animated:`. A list with sections reads its sections and keys again on every change, like
  `reloadData`. A dragged row stays in its section.
- Changes: `reloadData`, `insertItemsAtIndices:`, `deleteItemsAtIndices:`, `reloadItemsAtIndices:`,
  `reloadItemsAtIndices:payload:` with the data source's `listView:reconfigureCell:atIndex:payload:`,
  `moveItemAtIndex:toIndex:`, `performBatchUpdates:completion:` (UITableView's index rules, planned by the core's
  `planBatch`; a batch that does not add up reloads everything) and `applyChanges`, which diffs every key with the
  core's `diffKeys`, reloads rows whose `listView:contentVersionForItemAtIndex:` changed and returns a
  `ShadowListKitListChanges`. With `animatesChanges` all of them animate through `itemAnimator`, a `ShadowListKitItemAnimator`
  (`ShadowListKitDefaultItemAnimator` by default).
- Selection: `allowsSelection` (on), `allowsMultipleSelection`, `selectedIndices`, `selectItemAtIndex:animated:`,
  `deselectItemAtIndex:animated:`, `editing`. Delegate `shouldSelect`, `didSelect`, `didDeselect`,
  `shouldHighlight`. Cells get `setHighlighted:animated:`, `setSelected:animated:` and `setEditing:animated:`.
- Swipe actions: delegate `listView:leadingSwipeActionsForItemAtIndex:` and `...trailing...` return a
  `ShadowListKitSwipeActionsConfiguration` of `ShadowListKitSwipeAction`s. A full swipe performs the first action, like swipe to
  dismiss. `closeSwipeActionsAnimated:`. Off while editing.
- Context menus: delegate `listView:contextMenuForItemAtIndex:` returns a `UIMenu`, shown through
  `UIContextMenuInteraction`. A row that can also be reordered lifts on the hold, and letting it go in place shows
  the menu through `UIEditMenuInteraction`.
- Prefetching: `prefetchDataSource` hears `listView:prefetchItemsAtIndices:` for items the core's measured window
  brought in without a cell, and `listView:cancelPrefetchingForItemsAtIndices:` for ones that left it unseen.
- Pull to refresh: `refreshEnabled`, `refreshing`, delegate `listViewDidBeginRefreshing:`.
- Separators: `showsSeparators`, `separatorColor`, `separatorInsetStart`, `separatorInsetEnd`,
  `separatorThickness`, delegate `listView:showsSeparatorAfterItemAtIndex:`. Only between items of one section.
- Saved position: `anchorState` (`ShadowListKitAnchorState`, the key at the viewport start and the distance into it) and
  `restoreAnchorState:`, which lands now or once a reload brings the key. UIKit state restoration saves it.

Masonry is round robin: row `i` goes in column `i % numberOfColumns`. Full width rows in a grid and the
accessibility rotor are not supported.

## Install

iOS 16 or newer.

Swift Package Manager: add `https://github.com/azimgd/shadowlist` and the `ShadowListKit` product.

```swift
.package(url: "https://github.com/azimgd/shadowlist", from: "0.9.0")
```

CocoaPods:

```ruby
pod 'ShadowListKit', '~> 0.9'
```

Then `import ShadowListKit` in Swift, or `#import <ShadowListKit/ShadowListKit.h>` in Objective-C.

The core's `[SL]` debug log is off by default. The example's `build.sh` compiles it in when run with
`SHADOWLIST_DEBUG_LOG=1`. It prints on every pass.

## Run it

The example app lives in [`templates/shadowlist-uikit-example`](../../templates/shadowlist-uikit-example). From the repo root:

```sh
cd templates/shadowlist-uikit-example
./build.sh sim <simulator-udid>      # or: ./build.sh device any
xcrun simctl install <udid> build/dd-sim/Build/Products/Release-iphonesimulator/ShadowListKitExample.app
xcrun simctl launch <udid> shadowlist.uikit.example -SLRoute Chat -SLEngine sl
```

`xcodegen` generates `ShadowListKitExample.xcodeproj` from its `project.yml`.

`-SLRoute` takes `Feed`, `Masonry`, `Chat`, `SectionList`, `Reorder` and `Snap`, which run on any engine, and
`Sections` and `Inbox`, which run on ShadowListKitListView only.

| `-SLEngine`  | List                  | Row sizes                                          |
| ------------ | --------------------- | -------------------------------------------------- |
| `sl`         | ShadowListKitListView | precomputed layouts, off the main thread           |
| `sl-auto`    | ShadowListKitListView | each cell's `sizeThatFits:` on the main thread     |
| `table`      | UITableView           | `heightForRowAt` from the same precomputed layouts |
| `table-auto` | UITableView           | self-sizing cells with an estimated height         |

All engines show the same row views.
`-SLTextAsync 1` draws text on a background queue, and `-SLCount N` sets the list size.

## Benchmarks

`Bench/ShadowListKitBench.m` drives an app's main scroll view at a constant speed and logs one JSON line with
frame intervals, hitch time, main thread and process CPU, memory, and how much of the viewport no row covers.
The React Native example loads it through `DYLD_INSERT_LIBRARIES` on the simulator and an `OTHER_LDFLAGS`
override on a device.

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

- `prepend`, `append`: rest mid list, change the data, report how far visible rows moved.
- `jump`: scrollToIndex to a far row, report where it lands.
- `cost`: main thread time of one update plus its layout, with the layout prefetch finished
  and paused. `prependMs`/`appendMs` count the list's own work, the `*TotalMs` fields add the
  screen building its rows. `-SLCostRuns N` sets the updates of each kind.
- `animate` (Feed): with `animatesChanges`, remove a visible row and add one, then log the
  fade and the slide while they run and whether they settled.
- `a11y`: VoiceOver's view of the list: element count, the element of a far row and whether
  it is on screen after, and a page scroll.
- `-SLAutoDrag 1` on `Reorder` drags a row four rows down and logs the order.
- `sections` (route `Sections`): section lookups, self-sized Auto Layout rows, `scrollToSection:`, the pinned
  header, separators, the index, and a regroup that keeps the visible rows still.
- `features` (route `Inbox`): selection by key, a batch, `applyChanges`, a payload reload, a full and a partial
  swipe, the context menu, refresh, the saved position, prefetching and the item animator.
  `-SLSwipeDemo 1` opens a row's trailing actions for a screenshot.
