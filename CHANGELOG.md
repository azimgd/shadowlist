# Changelog

## Unreleased

### ShadowList (React Native)

- `followAppends` keeps the end of an inverted list in view as rows grow in place, like a streaming reply, not only as rows are appended. A row inserted at the end is followed too.
- `onAtEndChange(atEnd)` reports a reader who scrolled an inverted list away from its end and back.
- `ItemActionsView` gives a view outside a list the swipe actions and menu of a list row.
- macOS: `contextMenuForItem` opens on a right click, and `enableSelectionOnKeyPress` with `onActivateItem` selects and activates rows from the keyboard.
- `shouldSelectItem` keeps rows like headers out of the selection.
- A horizontal list styled `height: 'auto'` takes the height of its tallest mounted row.
- Native decides which rows mount, paces the overscan on display frames, and works out viewable items, the pinned section header and prefetching in the shared C++ core. JS only applies the result. The props and callbacks of the lists are the same. The raw `ShadowListView` native component sends `onMountedRangeChange`, `onViewabilityChange` and `onPrefetchChange` instead of `onMeasuredRangeChange` and `onViewableIndicesChange`.
- Row keys, section and tree rows and row size specs no longer travel as props. Each list writes them by version into a native key store through the new `ShadowListStore` C++ TurboModule, only what changed, and native builds the row keys of sections and trees, the sticky section headers, and the key lookups. SectionList and TreeList only build the rows that mount, and a tree expand or collapse splices only the toggled node's rows. The props and callbacks of the lists are the same. The raw `ShadowListView` takes `keysStore`, `keysVersion` and `sizeSpecsVersion`, and keeps `rowKeys` (now optional), `rowSizeSpecs` and `stickyIndices` only for when the module is missing. `onMountedRangeChange` also carries `lowIndex` and `highIndex`. SectionList row keys follow the core's scheme (`\x1fh:`, `\x1fi:`, `\x1ff:` with the section key) instead of `sh:`, `si:` and `sf:`.
- The selection is the core's `ListSelection`, kept with the list's native store and changed through `ShadowListStore`, the same rules UIKit and the Android kit use. `onSelectionChange` reports keys in the order they were selected. A removed row leaves the selection with native looking up only the selected keys. Without the module, rows show `selectedKeys` and taps change nothing.
- `onScroll` comes from the native scroll view (`scrollViewDidScroll` on iOS and macOS, `updateScrollState` on Android) instead of a commit of the list on every scroll frame. Its name, payload and `scrollEventThrottle` behavior are the same, and the event held back by the throttle is still sent at rest. No `onScroll` at offset 0 fires on mount. `contentInset` in `onScroll` and the drag and momentum events carries the list's inset instead of 0.
- The raw `ShadowListView`'s `onViewabilityChange` carries only `addedKeys` and `removedKeys` per config, without `viewableKeys`, and `onPrefetchChange` carries `prefetchKeys` and `cancelKeys` beside the indices. `prefetchItems` and `cancelPrefetchingForItems` get indices in the data rendered now, also after a data change.
- `SwipeAction.backgroundColor` reaches native as a color: `PlatformColor` and `DynamicColorIOS` work instead of falling back to gray or red.
- The raw `ShadowListCellView` has no `index` prop. Native finds a row's index by its key, and rows above an edit in a reorderable list no longer render again.
- SectionList and TreeList drop the props they do not support (reorder, drag boards, selection, swipe actions, menus, `prefetchDataSource` and the viewability callbacks) with one `__DEV__` warning per prop. A reorder in a list without a data array warns in `__DEV__` instead of calling `onMoveItem` with an empty `data`.
- `__DEV__` builds warn when the `ShadowListStore` module is missing, about duplicate keys, and about a `templateType` the native template view does not know. `scrollToItem` with an item not in the data calls `onScrollToIndexFailed` with index -1, or warns in `__DEV__` without it.
- Number props native reads as whole numbers, like `numberOfColumns` and `mountOverscanRows`, are cut to whole numbers in JS too.

### ShadowListKit (iOS and Android)

- `followAppends` also keeps items growing at the end in view.
- Android keeps the selection, the batch `performBatchUpdates` collects and the content versions in the core's `ListSelection`, `BatchCollector` and `ContentVersions` through the JNI, beside the core and outliving it. UIKit takes its batches from `BatchCollector` too.
- The default change animation duration is the core's `CHANGE_DURATION_MS` in both kits. On Android `ShadowListKitDefaultItemAnimator.CHANGE_DURATION_MS` is a `val` read from the core instead of a `const val`, still 250 ms.
- Android and the web count a scroll as the user's from the core's `OFFSET_MOVED_THRESHOLD` (0.5) instead of a bare 1, like iOS. Android offsets are whole pixels and behave the same. The web now notices moves from half a pixel.
- Android: rows moved by an edge scroll during a drag animate to their new place.
- `performBatchUpdates`' completion is documented as running once the change is laid out and, with the default item animator only, its animation ended. A custom item animator's animations are not waited for.
- Android and the web hold the row keys natively across cores: one `commitKeys` call sends only the keys between the unchanged ends and native does the splice, the diff, the content versions and the selection in one step, and a new core is seeded from the held keys. `retainSelection` and the per-change key repacking are gone.
- Items are prefetched before their cells are made: a pass measures the rows about to show, reports prefetching and cancels, and only then builds the remaining cells. Without `prefetchDataSource` nothing changes.
- Android TalkBack's "Moved to" announcement waits for the row to land in the target list, like VoiceOver, and stays silent after the landing timeout.

### Core (C++)

- Scroll command and prop indices are `std::size_t` with `UNDEFINED_INDEX` for none: `ScrollCommand::index`, `ScrollPatch::commandIndex`, `Container::requestScrollToRow` and `commandTargetOffset`. `SCROLL_TO_END_INDEX` and `SCROLL_TO_OFFSET_INDEX` are `std::size_t` sentinels. The platforms still carry -3 and -4 in a double and convert with `scrollCommandIndexFromDouble` and `scrollCommandIndexToDouble`.
- `prefetchWindow` is `prefetchRange`, and the debug log macro `SL_LOG` is `SHADOWLIST_LOG`.
- `KeyEdit` has a `Splice` kind with `start`, `deleted` and `inserted`, which every `ListDriver` edit hands to the core through `keyEditOf`. `endEdit` is gone. `ListDriver::reloadKeys` takes the splice.
- `Row::index` is removed.
- `stickyIndicesFromProps` sorts and deduplicates the indices, and skips an index too large for an `int`.
- Row keys are stored once and shared across versions: `KeyList` is a read-only view over a version's keys and `SharedKeys` holds them as pointers into a pool shared down a line of versions. An append or prepend writes only the new pointers, a delete at either end shares the buffer, and a middle splice copies pointers, never strings. Appending one key to 100k rows costs 0.17 µs against 1.5 ms before.

### ShadowListUtils

- `TemplateList` passes selection, keyboard selection, swipe actions, menus and `contentContainerStyle` through to the list.
- `Grouped.Actions`, and swipe actions and a menu on `Grouped.Row`.
- `Assistant.List` dismisses the keyboard on drag.
- `Chat.Bubble` takes `showsAuthor`, `renderText` and `children`, and `getChatMessageSizeSpec` takes `showsAuthor`.
- `Assistant.Composer` takes a controlled `value` with `onChangeText`, and an `accessory` in its toolbar.
- `Board.List` draws a board of draggable columns on one drag board with ids it names itself, `Board.Card` a card for it, and `useDragBoardState` with the pure `moveBoardItem` hold the columns and apply each move within or between them.
- `Board.List` takes `renderColumnFooter` for a view under each column's cards that never drags, `renderColumnHeader` and `columnStyle`, and `columnHeight="content"` with `minColumnHeight`, `maxColumnHeight` and `estimatedItemHeight` for a board that sizes itself to its tallest column. An empty column of a board whose cards cannot move says "No cards" (`emptyColumnReadOnly`), and a column's list no longer draws a background of its own over the column's.
- `Board.List`'s `columnWidth` sets every column's width in points, which the snap to each column follows, and one that is not a positive number falls back to 78% of the board, at most 360. `sidePadding` sets the inset before the first column and after the last in points, in place of the 24 points or the centring, and one that is negative or not finite is ignored.
- `useDragBoardState` calls `onMoveItem` only for a move that changes the board. A move to an unknown column or to a card's own place changes nothing and stays silent.

### ShadowListKit for the web

- New `shadowlist-wasm` package: `ShadowListKitListView`, the custom element `<shadowlist-kit-list-view>`, on the
  core compiled to WebAssembly with Emscripten. Keys, measurement through cells or `sizeForItem`, reloads, inserts,
  deletes, batches, `applyChanges`, inverted lists, grids, horizontal lists, sticky rows, header and footer views,
  prefetching, snapping, saved positions and scroll commands.
- `templates/shadowlist-web-example` shows Feed, Chat, Masonry, Horizontal and Sticky screens.

### Benchmarks

- The core bench's FLING scenarios start each batch from a fresh list with only the first screen measured, and the warm update scenarios are named scroll frame and move 8 points per frame. Snap offsets are timed twice: built cold after a geometry change, and reused from the cache.
- The kit benches' blank metric counts undrawn content, and each platform has a self-test engine that must report blank. The RecyclerView engine uses `DefaultItemAnimator` only when the bench's animate setting is on, and the UITableView engine appends with `insertRows` and keeps its self-sized heights. The table engines on a grid are labelled as `UICollectionView`, which they run on there.
- The React Native example turns on React Native feature flags that are off in a stock app. Every bench script records their state in its results, and `ENGINE_FLAGS` sets or checks it.
- `compare.py` and `perf.py` call a change faster or slower only with at least 3 runs per side, a change of 5% or more and run medians that do not overlap.

### shadowhash

- Fidelity is scored on a held-out corpus of 233 hand-written posts in 13 groups instead of the corpus `LETTERS` was calibrated on. The README gives the held-out numbers beside the chars-only baseline, and says every number comes from CoreText with the macOS system font.

## 0.9.0

### ShadowListKit (iOS and Android)

- iOS: Swift package at the repo root (product `ShadowListKit`) and the `ShadowListKit` CocoaPods spec.
- Android: published as `io.github.azimgd:shadowlist-kit` on Maven Central and through JitPack.
- Android: the native library links the C++ runtime statically and is aligned for 16 KB page sizes.
