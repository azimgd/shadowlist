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

### ShadowListKit (iOS and Android)

- `followAppends` also keeps items growing at the end in view.
- Android keeps the selection, the batch `performBatchUpdates` collects and the content versions in the core's `ListSelection`, `BatchCollector` and `ContentVersions` through the JNI, beside the core and outliving it. UIKit takes its batches from `BatchCollector` too.
- The default change animation duration is the core's `CHANGE_DURATION_MS` in both kits. On Android `ShadowListKitDefaultItemAnimator.CHANGE_DURATION_MS` is a `val` read from the core instead of a `const val`, still 250 ms.
- Android and the web count a scroll as the user's from the core's `OFFSET_MOVED_THRESHOLD` (0.5) instead of a bare 1, like iOS. Android offsets are whole pixels and behave the same. The web now notices moves from half a pixel.

### Core (C++)

- Scroll command and prop indices are `std::size_t` with `UNDEFINED_INDEX` for none: `ScrollCommand::index`, `ScrollPatch::commandIndex`, `Container::requestScrollToRow` and `commandTargetOffset`. `SCROLL_TO_END_INDEX` and `SCROLL_TO_OFFSET_INDEX` are `std::size_t` sentinels. The platforms still carry -3 and -4 in a double and convert with `scrollCommandIndexFromDouble` and `scrollCommandIndexToDouble`.
- `prefetchWindow` is `prefetchRange`, and the debug log macro `SL_LOG` is `SHADOWLIST_LOG`.

### ShadowListUtils

- `TemplateList` passes selection, keyboard selection, swipe actions, menus and `contentContainerStyle` through to the list.
- `Grouped.Actions`, and swipe actions and a menu on `Grouped.Row`.
- `Assistant.List` dismisses the keyboard on drag.
- `Chat.Bubble` takes `showsAuthor`, `renderText` and `children`, and `getChatMessageSizeSpec` takes `showsAuthor`.
- `Assistant.Composer` takes a controlled `value` with `onChangeText`, and an `accessory` in its toolbar.
- `Board.List` draws a board of draggable columns on one drag board with ids it names itself, `Board.Card` a card for it, and `useDragBoardState` with the pure `moveBoardItem` hold the columns and apply each move within or between them.

### ShadowListKit for the web

- New `shadowlist-wasm` package: `ShadowListKitListView`, the custom element `<shadowlist-kit-list-view>`, on the
  core compiled to WebAssembly with Emscripten. Keys, measurement through cells or `sizeForItem`, reloads, inserts,
  deletes, batches, `applyChanges`, inverted lists, grids, horizontal lists, sticky rows, header and footer views,
  prefetching, snapping, saved positions and scroll commands.
- `templates/shadowlist-web-example` shows Feed, Chat, Masonry, Horizontal and Sticky screens.

## 0.9.0

### ShadowListKit (iOS and Android)

- iOS: Swift package at the repo root (product `ShadowListKit`) and the `ShadowListKit` CocoaPods spec.
- Android: published as `io.github.azimgd:shadowlist-kit` on Maven Central and through JitPack.
- Android: the native library links the C++ runtime statically and is aligned for 16 KB page sizes.
