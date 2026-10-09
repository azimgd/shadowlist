# ShadowListKit (prototype)

A virtualized list for UIKit, written in Objective-C++ on the shadowlist core. No React Native.
`SLKListView` is a `UIScrollView` subclass with a UITableView-like data source, and it adds
what the shadowlist core does well: the visible content stays still while rows are measured,
inserted above or removed. Rows have stable keys.

## How it works

One `layoutSubviews` pass on the main thread does what Fabric spreads over commits. The core
side of it is `ListDriver` in `packages/shadowlist-core/host`, shared with the Android list:

1. `ListDriver::runPasses` calls `Virtualizer::update` with the current offset. Keys go in by reference.
2. Every row in the core's window that has no size gets measured. Sizes come from the data
   source, which is a cache lookup when layouts are precomputed, or from the cell's `sizeThatFits:`.
   Then one `commitElementSizes` reflow runs.
3. Any offset correction the core asks for is applied inside the same call, and the core gets
   more passes to confirm it. The list then sets the content size and the offset. Corrections
   land before the frame is drawn. There is no commit token round trip and no hidden row.
   A correction that still waits for a report gets one more layout on the next display frame.
4. Cells within `mountOverscan` of the viewport are mounted by key and the rest are recycled.
   `ListDriver::planMount` picks the rows, including the pinned section header.

UIScrollView only rests on device pixels. An offset the core asks for is written rounded to
the pixel grid, and while the view stays there the core is told the exact offset back. Rows are
drawn shifted by the difference, with their edges on device pixels, which keeps an anchored row
exactly where it was: a prepend moves visible rows by 0 pt, not a fraction of a pixel.

`reloadData` compares the new keys with the old ones and hands the core only the changed
middle. An edit at one end, like a prepend or a trimmed tail, then reaches the core as one edit
the core applies without comparing every key.

A scroll frame inside the core's `computeOffsetBand` skips steps 1 to 3. Most frames only do
the mount pass, which is a short scan of the measured window. Sticky headers are pinned by the
host, which keeps the band usable for section lists.

| File                                                                             | Role                                                                                                                                     |
| -------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `packages/shadowlist-core/host/ListDriver.{hpp,cpp}`                             | The core driven for a native list: layout passes, measurement, keys, mount plan, sticky math, scroll landing, drag. Shared with Android. |
| `SLKListView.mm`                                                                 | Public API, layout pass, mounting, sticky placement, scroll commands, delegate proxy.                                                    |
| `packages/shadowlist-core/host/ListSections.{hpp,cpp}`                           | Sections over the rows: item and row indices, header and footer rows, row keys, drops inside a section. Shared with Android.             |
| `packages/shadowlist-core/host/KeyDiff.{hpp,cpp}`                                | `diffKeys` for `applyChanges` and `planBatch` for `performBatchUpdates:completion:`. Shared with Android.                                |
| `packages/shadowlist-core/host/SwipeReveal.{hpp,cpp}`, `ListSelection.{hpp,cpp}` | Swipe action offsets and rests, selection by key.                                                                                        |
| `SLKListView+Drag.mm`                                                            | Touch and hold to reorder, and the testing hooks in `SLKListView+Testing.h`.                                                             |
| `SLKListView+Actions.mm`                                                         | Swipe actions and context menus.                                                                                                         |
| `SLKListCell.mm`                                                                 | Row base view: highlight, selection and editing states, Auto Layout fitting.                                                             |
| `SLKListModels.mm`                                                               | `SLKSwipeAction`, `SLKAnchorState`, `SLKListChanges`, `SLKDefaultItemAnimator`.                                                          |
| `SLKChangeAnimator.mm`                                                           | Works out what `animatesChanges` animates and hands it to the `itemAnimator`.                                                            |
| `SLKSectionIndexView.mm`                                                         | The section index along the trailing edge.                                                                                               |
| `Internal/*.h`                                                                   | State the list shares with its drag category and the change animator. Not public headers.                                                |
| `SLKText.{h,mm}`                                                                 | Precomputed text layout and drawing.                                                                                                     |

`SLKTextLayout` and `SLKTextView` move text off the main thread. Line breaking happens when a
row's layout is computed, on any thread. The view either draws the ready lines, or with
`displaysAsynchronously` gets a bitmap drawn on a background queue.

## Features

|                                                           | SLKListView                                                                   | UITableView                                 | shadowlist (RN) |
| --------------------------------------------------------- | ----------------------------------------------------------------------------- | ------------------------------------------- | --------------- |
| Variable row sizes without jumps                          | yes, measured or precomputed                                                  | estimates jump, or every height up front    | yes             |
| Keep visible rows still on insert, remove and regroup     | yes, by key                                                                   | manual offset fix, lost on reload           | yes             |
| Chat style list that opens at the end                     | `inverted`, `followAppends`                                                   | flip transform or manual scrolling          | yes             |
| Sections with headers, footers and an index               | data source sections, `stickySectionHeaders`                                  | yes                                         | yes             |
| Sticky rows                                               | `stickyIndices`                                                               | sections                                    | yes             |
| Columns                                                   | round robin `numberOfColumns`                                                 | needs UICollectionView                      | yes             |
| Header and footer views                                   | yes                                                                           | yes                                         | yes             |
| Start and end reached callbacks                           | yes                                                                           | by hand in scrollViewDidScroll              | yes             |
| scrollToIndex and scrollToEnd that land through estimates | yes                                                                           | yes                                         | yes             |
| Touch and hold to reorder, lists and grids                | yes                                                                           | drag and drop or edit mode                  | yes             |
| Snap to row edges                                         | `snapToItem`                                                                  | by hand                                     | yes             |
| Horizontal                                                | `horizontal`                                                                  | no                                          | yes             |
| Insert and delete animations                              | `animatesChanges` (fade in, fade out, slide)                                  | yes                                         | no              |
| VoiceOver past the mounted rows                           | every row is an accessibility element, page scrolls                           | yes                                         | partly          |
| Pull to refresh                                           | `refreshEnabled`                                                              | `refreshControl`                            | yes             |
| Incremental inserts and deletes                           | `insertItemsAtIndices:`                                                       | `insertRows`                                | n/a             |
| Moves, batches and diffing                                | `moveItemAtIndex:toIndex:`, `performBatchUpdates:completion:`, `applyChanges` | `performBatchUpdates`, diffable data source | n/a             |
| Partial reloads                                           | `reloadItemsAtIndices:payload:`                                               | `reconfigureRows`                           | n/a             |
| Selection, highlight, editing                             | `allowsMultipleSelection`, `selectedIndices`, `editing`                       | yes                                         | n/a             |
| Swipe actions and swipe to delete                         | `SLKSwipeActionsConfiguration`                                                | yes                                         | no              |
| Context menus                                             | `UIMenu` per row, also on reorderable rows                                    | yes                                         | no              |
| Prefetching                                               | `prefetchDataSource`                                                          | yes                                         | n/a             |
| Separators                                                | `showsSeparators`                                                             | yes                                         | no              |
| Self-sizing Auto Layout cells                             | default `sizeThatFits:`                                                       | yes                                         | n/a             |
| Custom change animations                                  | `itemAnimator`                                                                | no                                          | no              |
| Saved scroll position                                     | `anchorState`, state restoration                                              | offset only                                 | no              |
| Not in the prototype                                      | full width rows in grids, shortest column masonry, accessibility rotor work   |                                             |                 |

## API

The header, `SLKListView.h`, documents every member. The same names exist in the Android list.

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
  core's `diffKeys`, reloads rows whose `listView:contentVersionForItemAtIndex:` changed and returns an
  `SLKListChanges`. With `animatesChanges` all of them animate through `itemAnimator`, an `SLKItemAnimator`
  (`SLKDefaultItemAnimator` by default).
- Selection: `allowsSelection` (on), `allowsMultipleSelection`, `selectedIndices`, `selectItemAtIndex:animated:`,
  `deselectItemAtIndex:animated:`, `editing`. Delegate `shouldSelect`, `didSelect`, `didDeselect`,
  `shouldHighlight`. Cells get `setHighlighted:animated:`, `setSelected:animated:` and `setEditing:animated:`.
- Swipe actions: delegate `listView:leadingSwipeActionsForItemAtIndex:` and `...trailing...` return an
  `SLKSwipeActionsConfiguration` of `SLKSwipeAction`s. A full swipe performs the first action, like swipe to
  dismiss. `closeSwipeActionsAnimated:`. Off while editing.
- Context menus: delegate `listView:contextMenuForItemAtIndex:` returns a `UIMenu`, shown through
  `UIContextMenuInteraction`. A row that can also be reordered lifts on the hold, and letting it go in place shows
  the menu through `UIEditMenuInteraction`.
- Prefetching: `prefetchDataSource` hears `listView:prefetchItemsAtIndices:` for items the core's measured window
  brought in without a cell, and `listView:cancelPrefetchingForItemsAtIndices:` for ones that left it unseen.
- Pull to refresh: `refreshEnabled`, `refreshing`, delegate `listViewDidBeginRefreshing:`.
- Separators: `showsSeparators`, `separatorColor`, `separatorInsetStart`, `separatorInsetEnd`,
  `separatorThickness`, delegate `listView:showsSeparatorAfterItemAtIndex:`. Only between items of one section.
- Saved position: `anchorState` (`SLKAnchorState`, the key at the viewport start and the distance into it) and
  `restoreAnchorState:`, which lands now or once a reload brings the key. UIKit state restoration saves it.

Masonry stays round robin. A shortest column layout would move rows between columns whenever an earlier row is
measured, which breaks keeping the visible rows still. Full width rows in a grid are not supported: the core places
row `i` in column `i % numberOfColumns`, and its visible row search, reflow, anchoring and drag all rely on that.

## Add it to an app

There is no CocoaPods spec or Swift package yet. Build a static framework target the way the example's
`project.yml` does:

- Compile `Sources/ShadowListKit/*.mm` and `packages/shadowlist-core/*.cpp` and `host/*.cpp` into it.
- Make the headers in `Sources/ShadowListKit` public and keep `Internal/` project only.
- Add `packages` to `HEADER_SEARCH_PATHS`, because the core includes itself as `<shadowlist-core/...>`.
- Use C++20 (`CLANG_CXX_LANGUAGE_STANDARD = c++20`) and ARC, and set `DEFINES_MODULE` for Swift.
- The core's `[SL]` debug log is off by default. To read it while debugging the kit, add `SHADOWLIST_DEBUG_LOG=1`
  to `GCC_PREPROCESSOR_DEFINITIONS`. It prints on every pass. The example's `build.sh` does this when run with
  `SHADOWLIST_DEBUG_LOG=1` in its environment.
- Link the app with `-ObjC -lc++`. iOS 16 or newer.

## Run it

The example app lives in `templates/shadowlist-uikit-example`. From the repo root:

```sh
cd templates/shadowlist-uikit-example
./build.sh sim <simulator-udid>      # or: ./build.sh device any
xcrun simctl install <udid> build/dd-sim/Build/Products/Release-iphonesimulator/ShadowListKitExample.app
xcrun simctl launch <udid> shadowlist.uikit.example -SLRoute Chat -SLEngine sl
```

`xcodegen` generates `ShadowListKitExample.xcodeproj` from its `project.yml`. The framework
compiles `Sources/ShadowListKit` and the canonical core in `packages/shadowlist-core` directly.

The example has the same screens and looks as the React Native example: Feed, Gallery, Chat,
Directory, Boarding Order and Destinations. The same data comes from the same fixtures. Every
screen runs on any engine. Two more screens run on SLKListView only: Sections (route `Sections`, section headers
and footers, the index and Auto Layout cells without sizes) and Inbox (route `Inbox`, swipe actions, menus,
selection and editing, refresh, batches, `applyChanges` and the saved position).

| `-SLEngine`  | List        | Row sizes                                          |
| ------------ | ----------- | -------------------------------------------------- |
| `sl`         | SLKListView | precomputed layouts, off the main thread           |
| `sl-auto`    | SLKListView | each cell's `sizeThatFits:` on the main thread     |
| `table`      | UITableView | `heightForRowAt` from the same precomputed layouts |
| `table-auto` | UITableView | self-sizing cells with an estimated height         |

All engines show the same row views. The comparison isolates the list itself.
`-SLTextAsync 1` draws text on a background queue, and `-SLCount N` sets the list size.

## Benchmarks

`Bench/SLKBench.m` drives any app's main scroll view at a constant speed with a display link. It
logs one JSON line with frame intervals, hitch time, main thread and process CPU, memory, and
how much of the viewport no row covers. The UIKit example links it in. The React Native example
gets it injected without any project change. On the simulator that is `DYLD_INSERT_LIBRARIES`,
and on a device it is an `OTHER_LDFLAGS` override on the xcodebuild command line.

```sh
Bench/bench.sh device <devicectl-udid> my-label      # ENGINES, SCREENS, COUNTS, RUNS, SPEED, AXES
Bench/summarize.py results/my-label/runs.jsonl
```

`-SLBenchAxes y,x,xy` (`AXES` in `bench.sh`) runs one leg pair per axis and logs one line each with an
`axis` key. y drives the vertical scroll view with the most content, x the horizontal one, xy both at
once. An axis without a scroll view logs a line with `skipped`. A view that draws its content after
its rows show, like tiles, can adopt `SLKBenchProbe` and return the share of the viewport not drawn
yet from `slk_benchBlankFraction`. The bench asks the driven views and their superviews, and the
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
- `-SLAutoDrag 1` on Boarding Order drags a row four rows down and logs the order.
- `sections` (route `Sections`): section lookups, self-sized Auto Layout rows, `scrollToSection:`, the pinned
  header, separators, the index, and a regroup that keeps the visible rows still.
- `features` (route `Inbox`): selection by key, a batch, `applyChanges`, a payload reload, a full and a partial
  swipe, the context menu, refresh, the saved position, prefetching and the item animator.
  `-SLSwipeDemo 1` opens a row's trailing actions for a screenshot.

## Results

iPhone 12 Pro, iOS 18.7, 60 Hz, Release builds, 1000 rows, the list driven at 6000 pt/s for
6 s each way, 3 interleaved runs, medians. Cells are main thread / whole process CPU in ms per
second of scrolling. Lower is better. Raw runs are in `results/device-matrix`.

| Screen    | SLK, async text | SLK       | SLK self-sizing | UITableView | UITableView self-sizing | shadowlist RN |
| --------- | --------------- | --------- | --------------- | ----------- | ----------------------- | ------------- |
| Feed      | 83 / 192        | 102 / 141 | 145 / 179       | 122 / 161   | 139 / 178               | 158 / 287     |
| Chat      | 85 / 241        | 115 / 140 | 166 / 191       | 138 / 159   | 224 / 239               | 158 / 267     |
| Directory | 123 / 291       | 140 / 148 | 206 / 215       | 188 / 192   | 245 / 249               | 149 / 469     |
| Gallery   | 113 / 178       | 122 / 135 | 171 / 185       | 152 / 168   | 171 / 186               | 174 / 363     |

- No engine dropped a frame or showed a blank area in these runs, not even at 16000 pt/s.
  On this device the list engine decides how much CPU headroom is left, not whether frames drop.
- With the same cells, SLKListView uses 16 to 26 percent less main thread time than
  UITableView, and 27 to 49 percent less than self-sizing UITableView.
- SLK with async text has the lowest main thread time on every screen, 26 to 38 percent
  below UITableView. Its text bitmaps mostly arrive before the row is on screen. When one arrives
  after its row is on screen, it is less than a frame late (simulator: 0 late in Feed, 13 percent
  in Chat with a 1.4 ms average wait).
- shadowlist RN keeps its main thread lean through Fabric, but the whole process uses 2 to 3
  times the CPU of SLK. Its peak memory is lower in image screens and much higher in Directory.
- Before text moved off the main thread, UILabel re-typesetting dominated every native engine
  and RN beat them at high speeds. `SLKTextLayout` fixed that for all native engines.

Data updates, `-SLScenario cost`, 10k rows, simulator (iPhone 16 Pro, iOS 26), resting mid
list, the list's own main thread ms for one update plus its layout, 3 rounds of 30 updates,
medians. Before is the list before the key and reconcile work of 2026-10-07 (the core rehashed
its whole key map on every small insert, and reloadData sent every key).

| 10k rows                        | SLK before | SLK  | UITableView |
| ------------------------------- | ---------- | ---- | ----------- |
| Chat, prepend 50                | 0.34       | 0.14 | 6.0         |
| Chat, append 10                 | 0.20       | 0.04 | 3.4         |
| Feed, prepend 10                | 0.29       | 0.13 | 4.4         |
| Feed, append 20                 | 0.20       | 0.04 | 4.4         |
| Directory, regroup after 10 new | 2.29       | 1.91 | 3.9         |

Building the rows in the example costs more than the list update on every engine (Directory
regroups and sorts 10k contacts: about 32 ms, the same for every engine).

Keeping the place, `-SLScenario prepend`: adding travellers to the Directory regroups the
sections. Visible rows move 0 pt with SLK and 67 pt with UITableView, which loses the
position on reloadData. Feed and Chat prepends hold still on every engine, because UITableView
gets the usual manual offset fix there.
