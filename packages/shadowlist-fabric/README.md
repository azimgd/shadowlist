# ShadowList for React Native

ShadowList is the React Native Fabric package: the ShadowList, SectionList, TreeList and DraggableList components, and
ItemActionsView for a row outside a list.
It runs on iOS, Android and macOS, on the shared C++ core.

## Install

```sh
yarn add shadowlist
cd ios && pod install
```

### Requirements

| Dependency | Version |
| --- | --- |
| `react` | `>=19.2.0` |
| `react-native` | `>=0.83.0`, New Architecture |
| iOS | React Native's minimum |
| Android | API 24 or newer |
| macOS | 11 or newer, `react-native-macos` |

Autolinking picks up the pod on iOS and macOS and the CMake build on Android. The C++ core ships inside the package.
macOS has no pull to refresh or keyboard avoidance, and `snapToItem` moves to the nearest row edge without an animation
once the scroll ends.

Lists keep their keys in the `ShadowListStore` TurboModule the package registers. Without it, `__DEV__` builds warn
once, every list sends its keys as props, and taps change no selection.

## Usage

```tsx
const listRef = useRef<ShadowListCommands>(null);

<ShadowList
  ref={listRef}
  data={photos}
  renderItem={({ item }) => <Photo photo={item} />}
  numberOfColumns={3}
  reorderEnabled
  snapToItem
  snapAlignment="start"
/>;

listRef.current?.scrollToIndex({ index: 42, viewPosition: 0.5 });
```

## API

The tables summarize the API; [types.ts](src/types.ts) has full props and commands.

See [Public API words](../../docs/code-style.md#public-api-words) for naming across platforms.

### Props

| Prop | Default | What it does |
| --- | --- | --- |
| `renderItem({ item, index, separators, selected, select, deselect })` |  | Renders a row. |
| `keyExtractor` | `id`, then index | Matches items across updates. Use unique, stable keys: index keys follow positions and a prepend shifts the visible content. `__DEV__` builds warn about a duplicate key. |
| `getItemSizeSpec(item, index)` |  | Returns an `ItemSizeSpec`: a row's text with its font and insets, or a `fixedHeight`. Native then sizes rows near the screen before React renders them. |
| `measureLookaheadRows` | 48 | How many rows past the mounted ones get their size spec sent to native. |
| `initialScrollIndex` |  | The row the list opens at. Read on mount only. |
| `extraData` |  | Rebuilds the mounted rows when it changes. |
| `inverted` | false | Chat style list that opens at the end. Appended rows keep the visible content still. |
| `followAppends` | false | An inverted list resting at its end keeps it in view as rows are appended or grow in place, like a streaming reply. Scrolling away pauses it and coming back resumes it. |
| `onAtEndChange(atEnd)` |  | Called when an inverted list leaves its end because the user scrolled away, and when it is back. Show a jump to the latest row while `atEnd` is false. |
| `horizontal` | false | Scroll along the x axis. With `style={{ height: 'auto' }}` the list takes the height of its tallest mounted row and drops its default `flex: 1`. |
| `numberOfColumns`, `columnWrapperStyle` | 1 | Grid columns. Rows go round robin, row `i` in column `i % numberOfColumns`. `columnWrapperStyle` takes `gap`, `columnGap` and horizontal padding. Each column keeps the same width. |
| `ItemSeparatorComponent` |  | A component gets `highlighted`, `leadingItem` and `trailingItem`. A React element, or a function without parameters, renders once for every row. |
| `ListHeaderComponentStyle`, `ListFooterComponentStyle` |  | Style for the header and footer. |
| `contentContainerStyle`, `contentInset` |  | Numeric padding only. Padding along the scroll axis goes before the header and after the footer, across it into every row. Insets stay at zero otherwise: wrap the list in a safe area view. |
| `stickyIndices`, `renderStickyHeaderOverlay(index)` |  | Rows that pin to the top while their section scrolls, and an optional overlay drawn for the pinned row. |
| `stickyHeader`, `stickyFooter` | false | Pin `ListHeaderComponent` or `ListFooterComponent` to its edge. |
| `autoHideHeader`, `autoHideFooter` | false | A pinned header or footer slides out while the user scrolls further into the list and slides back when they scroll the other way, like a hiding toolbar. |
| `reorderEnabled` | false | Touch and hold to drag. `onMoveItem` reports a move; save its `data` to your state or the row snaps back. DraggableList enables it by default. |
| `dragBoardId` |  | DraggableLists that share a non-empty `dragBoardId` form one drag board, where a row dragged out of one list can drop in another. See [Drag boards](#drag-boards). |
| `dragListId` |  | Names this list on its drag board, unique among the board's mounted lists. A list without one is never a drop target. |
| `dragListLabel` |  | The name VoiceOver and TalkBack give this list in another list's Move to action. Defaults to `accessibilityLabel`, then `dragListId`. |
| `onMoveItemToList` |  | A drop in another list of the board, a `MoveItemToListInfo`: the moved `item`, its `sourceIndex`, `sourceListId`, this list's `data` without it, `destinationListId`, `destinationKey` and `destinationIndex`. Save `data`, and insert `item` in the destination list's state before the item keyed `destinationKey`, or at its end when the key is `''`. `destinationIndex` names the same place, for a key no longer in your state. |
| `snapToItem` | false | Rest on a row edge. |
| `snapAlignment` | `'start'` | Snap to `'start'`, `'center'` or `'end'`. |
| `overscan` | 1 | How far past the viewport native measures and places rows, in viewport sizes. |
| `mountOverscanRows`, `mountOverscanRowsLeading` | 4, 10 | Rows React keeps mounted on each side of the screen, and ahead of a fling. |
| `initialNumToRender` | 20 | Rows mounted before native first reports what is on screen. |
| `onStartReached`, `onEndReached` and their thresholds | 1 | Called near either end. Thresholds are in viewport sizes. |
| `persistentKeys` |  | Rows that stay mounted wherever the list scrolls, in their normal place. They are not pinned. |
| `nonAnchorKeys` |  | Rows never used to hold the visible content in place, like date pills or unread dividers whose keys come and go. |
| `trackItemSizes` | false | Keeps every mounted row's size for `getItemSize(key)` and `getItemSizes()` on the ref. |
| `viewabilityConfig`, `onViewableItemsChanged`, `viewabilityConfigCallbackPairs` |  | Viewable rows with `itemVisiblePercentThreshold`, `viewAreaCoveragePercentThreshold`, `minimumViewTime` and `waitForInteraction`. |
| `onScroll`, `onScrollBeginDrag`, `onScrollEndDrag`, `onMomentumScrollBegin`, `onMomentumScrollEnd`, `onContentSizeChange`, `scrollEventThrottle` |  | ScrollView's event shape: `contentOffset`, `contentSize`, `layoutMeasurement`, `contentInset`, `velocity` in points per millisecond. `onScroll` comes from the native scroll view, without a React commit per frame, and not on mount. An event held back by `scrollEventThrottle` is sent once the list rests. |
| `scrollEnabled`, `showsVerticalScrollIndicator`, `showsHorizontalScrollIndicator`, `bounces`, `decelerationRate`, `scrollsToTop`, `keyboardDismissMode`, `keyboardShouldPersistTaps`, `nestedScrollEnabled` |  | Passed to the native scroll view. `scrollsToTop` does nothing on Android. Android dismisses the keyboard from JS when a drag begins, for any `keyboardDismissMode` but `'none'`. `keyboardShouldPersistTaps` is handled in JS. Unset, taps always reach the rows. |
| `itemStyle` |  | Style for the view around each row. |
| `refreshing`, `onRefresh` |  | Pull to refresh. On a vertical list that is not inverted, data that changes while refreshing shows once the spinner has settled. |
| `refreshColor`, `progressViewOffset` |  | Tint and offset of the pull to refresh spinner. |

### Keyboard

| Export | What it does |
| --- | --- |
| `KeyboardView` | Dismisses the keyboard on a tap in an empty area. |
| `useKeyboardAnimation()` | Returns `{ height, progress }` as `Animated.Value`s that follow the keyboard frame by frame. On Android only when the Activity's `windowSoftInputMode` is `adjustResize`. |

### Ref commands

| Command | What it does |
| --- | --- |
| `scrollToIndex({ index, animated, viewOffset, viewPosition })` | Scrolls to a row by index. Animates by default. |
| `scrollToItem({ item, animated, viewOffset, viewPosition })` | Scrolls to a row by item. Animates by default. |
| `scrollToOffset({ offset, animated })` | Scrolls to an offset. Also takes positional arguments. |
| `scrollToEnd({ animated })` | Scrolls to the end. Also takes positional arguments. Animates by default. |
| `flashScrollIndicators()`, `recordInteraction()` | Same as in React Native. |
| `getNativeScrollRef()`, `getScrollResponder()`, `getScrollableNode()` | Return the native `ShadowListView` and its tag. |
| `setStartReachedEnabled(enabled)`, `setEndReachedEnabled(enabled)` | Pause the edge callbacks while a page loads. |
| `onScrollToIndexFailed` (prop) | Called for an index outside the data, or a `scrollToItem` item not in it with index -1, with `{ index, highestMeasuredFrameIndex, averageItemLength }`. Without it `__DEV__` builds warn. |

Scroll commands animate and land exactly, whatever the rows measure on the way. A finger on the list cancels them.
macOS lands right away without the animation.

### Selection, swipe actions and menus

| Prop or command | What it does |
| --- | --- |
| `allowsMultipleSelection`, `selectedKeys` (controlled), `onSelectionChange` | A row reads `selected` and calls `select()` or `deselect()` from `renderItem`. A selection follows its rows by key, and a removed row leaves it. |
| `shouldSelectItem({ item, index })` | Return false for rows that can't be selected, like headers. |
| `enableSelectionOnKeyPress`, `onActivateItem({ item, index })` | macOS: a click gives the list the keyboard. Up and Down select the row above or below and scroll it into view. Return or Space activates the selected row. |
| `selectItem(index)`, `deselectItem(index)`, `getSelectedIndices()` | Selection on the ref. |
| `leadingSwipeActionsForItem`, `trailingSwipeActionsForItem` | Return swipe actions and `performsFirstActionWithFullSwipe`; see [action types](src/types.ts). |
| `closeSwipeActions()` | Closes every row. |
| `contextMenuForItem` | Return a menu title and actions; see [menu types](src/types.ts). |
| `ItemActionsView` | A view outside a list with a row's `leadingSwipeActions`, `trailingSwipeActions` and `contextMenu`. Handlers get its `item` and index -1. |

Swipe actions work on vertical lists and on iOS and Android. An action's `backgroundColor` takes any color,
`PlatformColor` and `DynamicColorIOS` included. A full swipe performs the first action. An `onPress` that
returns a promise keeps the row revealed until the promise settles, then a surviving row slides back. Menus use `UIMenu`
on iOS, a long press popup on Android and a right click menu on macOS. With `reorderEnabled`, that long press starts
dragging and shows no menu.

### Drag to reorder

Touch and hold a row for 0.2 s to lift it. A finger that scrolls the list more than 8 points during the hold scrolls
instead, and any other drift up to 20 points still lifts. macOS lifts the row as soon as the mouse drags it, without a
hold. The list scrolls near its edges at the same speed per second on every display: within 90 points of the edge at up
to 960 points a second on iOS and macOS, within 60 dp at up to 720 dp a second on Android. The held row casts the
shadow of its card (the first view with a background down its single child chain, or its bounds), the drop spot ticks
a haptic as it moves, on macOS only with a Force Touch trackpad, and a cancelled gesture puts the rows back with the
same slide a drop uses. The drag math comes from the core on every platform. VoiceOver and TalkBack offer Move up and
Move down on each row while `reorderEnabled` is on.

`onMoveItem` gets a `MoveItemInfo` (`sourceIndex`, `destinationIndex` and the reordered `data`). The list keeps its
scroll offset when the reorder lands. The rows stay where the drag showed them.

### Drag boards

Boards are implemented on iOS, Android and macOS from the same core, and have been exercised on iOS. A long press
picks the row up on iOS and Android, a mouse drag on macOS.

- Board ids are global to the app. Two screens that use one id join one board. Give each screen's board its own id.
- Every list on a board needs its own `dragListId`. A list without one can't take a drop, and two lists with the same
  id can't be told apart. `__DEV__` builds warn about both.
- The finger picks the list it hovers through the window's hit test on iOS and macOS, so a list covered by another
  view isn't a target there. Android picks from the board's lists by their visible rect in the window, deepest first,
  and leaves out lists in another window, like a modal's.
- A drop sends one `onMoveItemToList` from the list the row came from. Update both lists in that handler, in one state
  change, within a second. The dragged row's picture waits that long for the row to show up in the destination's data,
  then gives up and the rows go back.
- SectionList and TreeList can't join a board: they ignore `dragBoardId` and `dragListId`.
- The scroll view around the board scrolls sideways once the finger heads for its edge, and comes to rest on its
  nearest page, `snapToInterval` or `snapToOffsets` when the drag ends. AppKit has no paging, so macOS snaps only to
  `snapToInterval` or `snapToOffsets`.
- A row carried back over its own list eases from where its picture was into its place there.
- VoiceOver and TalkBack add a Move to action on each row for every other list of the board that has drag on and shows
  in a window. It moves the row to that list's end through the same `onMoveItemToList` a drop sends. The action names
  the list by `dragListLabel`, then `accessibilityLabel`, then `dragListId`.

A scoped `<DragBoard>` provider is the planned next step: it would name its board with `useId`, which keeps
screens from sharing one by accident, take one `onMoveItem({ item, sourceListId, destinationListId, destinationIndex, destinationKey })`
for the whole board, and let a list refuse drops with `acceptsDrop`. Until then the string ids above are the API.
[ShadowListUtils' `Board.List`](../../templates/shadowlist-utils/README.md#board-drag-between-columns) already names
its board and lists itself and takes one `onMoveItem` for the whole board.

### Prefetching and saved position

| Prop or command | What it does |
| --- | --- |
| `prefetchDataSource={{ prefetchItems, cancelPrefetchingForItems }}` | Hears about rows up to `prefetchRows` (10) past the mounted ones, once each, and about prefetched rows that left before they mounted. Indices are positions in the data rendered now, after a data change too. |
| `await getAnchorState()` | Gives `{ key, offset }`, the row at the viewport start and how far into it the viewport starts. |
| `restoreAnchorState(state)` | Scrolls back there, now or once the data brings the key. |

### SectionList

SectionList and TreeList do not support reorder, drag boards, selection, swipe actions, menus, `prefetchDataSource` or
the viewability callbacks. `__DEV__` builds warn once about each such prop passed, and the list ignores it.

| Prop or command | What it does |
| --- | --- |
| `scrollToLocation({ sectionIndex, itemIndex, viewOffset, viewPosition, animated })` | Counts like React Native: `itemIndex` 0 is the section header. While headers pin, a row lands below its section's pinned header. |
| `scrollToSection(section, animated?)` | Lands on a section's first row. |
| `sectionIndexTitles`, `sectionForSectionIndexTitle(title, index)` | Shows an index along the trailing edge and maps a title to a section. |
| `ItemSeparatorComponent`, `SectionSeparatorComponent` | `ItemSeparatorComponent` gets `section` too. `SectionSeparatorComponent` gets `leadingSection` and `trailingSection`. |
