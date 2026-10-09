# shadowlist

React Native Fabric list components on the shared C++ core: `ShadowList`, `SectionList`, `TreeList` and `DraggableList`.

Prop and command names follow the shared words in [docs/code-style.md](https://github.com/azimgd/shadowlist/blob/main/docs/code-style.md#public-api-words).

## Install

```sh
yarn add shadowlist
cd ios && pod install
```

Requirements:

- React Native 0.83 or newer with the New Architecture (Fabric). The legacy architecture is not supported.
- iOS at React Native's minimum version, Android API 24 or newer, and macOS 11 or newer through `react-native-macos`.
- Autolinking picks up the pod on iOS and macOS and the CMake build on Android. The C++ core ships inside the package.

On macOS, pull to refresh, `snapToItem` and keyboard avoidance are not available.

## Usage

```tsx
const listRef = useRef<ShadowListCommands>(null);

<ShadowList
  ref={listRef}
  data={photos}
  renderElement={({ element }) => <Photo photo={element} />}
  numberOfColumns={3}
  reorderEnabled
  snapToItem
  snapAlignment="start"
/>;

listRef.current?.scrollToItem(42, 0.5);
```

## FlatList compatibility

`ShadowList` takes FlatList's props and ref methods where they mean something for a list whose rows the native core places.

| FlatList                                                                                                                                                                                                    | ShadowList                                                                                                                                                                                                                                      |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `renderItem({ item, index, separators })`                                                                                                                                                                   | `renderElement({ element, index, separators, selected, select, deselect })`                                                                                                                                                                     |
| `initialScrollIndex`                                                                                                                                                                                        | `initialScrollIndex`, read on mount only. `containerOffsetIndex` still works and still scrolls on every change, but is deprecated.                                                                                                              |
| `scrollToIndex({ index, animated, viewOffset, viewPosition })`                                                                                                                                              | same. It lands exactly through estimated sizes and never needs `getItemLayout`.                                                                                                                                                                 |
| `scrollToItem({ item, ... })`                                                                                                                                                                               | same, plus the positional `scrollToItem(index, viewPosition?, animated?)`                                                                                                                                                                       |
| `scrollToOffset({ offset, animated })`, `scrollToEnd({ animated })`                                                                                                                                         | same, plus the positional forms                                                                                                                                                                                                                 |
| `onScrollToIndexFailed`                                                                                                                                                                                     | called for an index outside the data, with FlatList's `{ index, highestMeasuredFrameIndex, averageItemLength }`                                                                                                                                 |
| `flashScrollIndicators()`, `recordInteraction()`                                                                                                                                                            | same                                                                                                                                                                                                                                            |
| `getNativeScrollRef()`, `getScrollResponder()`, `getScrollableNode()`                                                                                                                                       | the native `ShadowListView` and its tag. Its scroll view has no React instance of its own under Fabric. There is no ScrollResponder mixin.                                                                                                      |
| `extraData`                                                                                                                                                                                                 | rebuilds the mounted rows when it changes                                                                                                                                                                                                       |
| `ItemSeparatorComponent`                                                                                                                                                                                    | a component gets `highlighted`, `leadingItem` and `trailingItem`. An element, or a function without parameters, renders once for every row.                                                                                                     |
| `ListHeaderComponentStyle`, `ListFooterComponentStyle`                                                                                                                                                      | same                                                                                                                                                                                                                                            |
| `columnWrapperStyle`                                                                                                                                                                                        | with `numberOfColumns` above 1: `gap`, `columnGap` and horizontal padding only. Each column keeps the same width.                                                                                                                               |
| `contentContainerStyle`, `contentInset`                                                                                                                                                                     | numeric padding only. Padding along the scroll axis goes before the header and after the footer, across it into every row.                                                                                                                      |
| `progressViewOffset`                                                                                                                                                                                        | same                                                                                                                                                                                                                                            |
| `viewabilityConfig`, `onViewableItemsChanged`, `viewabilityConfigCallbackPairs`                                                                                                                             | `itemVisiblePercentThreshold`, `viewAreaCoveragePercentThreshold`, `minimumViewTime` and `waitForInteraction`                                                                                                                                   |
| `onScroll`, `onScrollBeginDrag`, `onScrollEndDrag`, `onMomentumScrollBegin`, `onMomentumScrollEnd`, `onContentSizeChange`, `scrollEventThrottle`                                                            | same event shape as ScrollView: `contentOffset`, `contentSize`, `layoutMeasurement`, `velocity` in points per millisecond. `contentOffsetX` and `contentOffsetY` stay. An event held back by `scrollEventThrottle` is sent once the list rests. |
| `scrollEnabled`, `showsVerticalScrollIndicator`, `showsHorizontalScrollIndicator`, `bounces`, `decelerationRate`, `scrollsToTop`, `keyboardDismissMode`, `keyboardShouldPersistTaps`, `nestedScrollEnabled` | passed to the native scroll view. `keyboardShouldPersistTaps` is handled in JS like ScrollView. Unset, taps always reach the rows.                                                                                                              |

Not supported: `getItemLayout` (use `getElementSizeSpec`), `contentInsetAdjustmentBehavior` and `automaticallyAdjustContentInsets` (the list keeps its insets at zero, wrap it in a safe area view), `zoomScale`, `inverted` flipping `contentContainerStyle` padding, `removeClippedSubviews`, `windowSize`, `maxToRenderPerBatch` and `initialNumToRender` (see `overscanRows` and `initialElementsSize`).

## Props beyond FlatList

| Prop                                                | Default | What it does                                                                                                                                                                                                                         |
| --------------------------------------------------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `keyExtractor`                                      | `id`    | Rows are matched across updates by key. Like FlatList, the default reads an element's `id` (a string or a number) and falls back to its index. Index keys follow positions, not rows: rows prepended above the screen then shift it. |
| `getElementSizeSpec(element, index)`                |         | Returns an `ElementSizeSpec`: a row's text with its font and insets, or a `fixedHeight`. Native then sizes rows near the screen before React renders them. Use it in place of `getItemLayout`.                                       |
| `measureLookaheadRows`                              | 48      | How many rows past the mounted ones get their size spec sent to native.                                                                                                                                                              |
| `inverted`                                          | false   | Chat style list that opens at the end. Appended rows keep the visible content still.                                                                                                                                                 |
| `followAppends`                                     | false   | Scroll along with appended rows while the list rests at its end.                                                                                                                                                                     |
| `horizontal`                                        | false   | Scroll along the x axis.                                                                                                                                                                                                             |
| `numberOfColumns`                                   | 1       | Grid columns. Rows go round robin, row `i` in column `i % numberOfColumns`.                                                                                                                                                          |
| `stickyIndices`, `renderStickyHeaderOverlay(index)` |         | Rows that pin to the top while their section scrolls, and an optional overlay drawn for the pinned row.                                                                                                                              |
| `stickyHeader`, `stickyFooter`                      | false   | Pin `ListHeaderComponent` or `ListFooterComponent` to its edge.                                                                                                                                                                      |
| `autoHideHeader`, `autoHideFooter`                  | false   | A pinned header or footer slides out while the user scrolls further into the list and slides back when they scroll the other way, like a hiding toolbar.                                                                             |
| `reorderEnabled`, `onReorder({ from, to, data })`   | false   | Touch and hold a row to drag it. Save `data` from `onReorder` to your state or the row snaps back. `DraggableList` turns it on by default.                                                                                           |
| `snapToItem`, `snapAlignment`                       | false   | Rest on a row edge: `'start'`, `'center'` or `'end'`.                                                                                                                                                                                |
| `overscan`                                          | 1       | How far past the viewport native measures and places rows, in viewport sizes.                                                                                                                                                        |
| `overscanRows`, `overscanRowsLeading`               | 4, 10   | Rows React keeps mounted on each side of the screen, and ahead of a fling.                                                                                                                                                           |
| `initialElementsSize`                               | 20      | Rows mounted before native first reports what is on screen.                                                                                                                                                                          |
| `onStartReached`, `onStartReachedThreshold`         | 1       | Like `onEndReached` at the start. Thresholds are in viewport sizes.                                                                                                                                                                  |
| `persistentKeys`                                    |         | Rows that stay mounted wherever the list scrolls, in their normal place. They are not pinned.                                                                                                                                        |
| `nonAnchorKeys`                                     |         | Rows never used to hold the visible content in place, like date pills or unread dividers whose keys come and go.                                                                                                                     |
| `trackElementSizes`                                 | false   | Keeps every mounted row's size for `getElementSize(key)` and `getElementSizes()` on the ref.                                                                                                                                         |
| `elementStyle`                                      |         | Style for the view around each row.                                                                                                                                                                                                  |
| `refreshColor`                                      |         | Tint of the pull to refresh spinner.                                                                                                                                                                                                 |

The ref also has `setStartReachedEnabled(enabled)` and `setEndReachedEnabled(enabled)`, which pause the edge callbacks while a page loads.

`KeyboardView` dismisses the keyboard on a tap in an empty area. `useKeyboardAnimation()` returns `{ height, progress }` as `Animated.Value`s that follow the keyboard frame by frame.

### Scroll commands

`scrollToItem`, `scrollToIndex`, `scrollToEnd` and an animated `scrollToOffset` animate and land exactly, whatever the rows measure on the way. A finger on the list cancels it. macOS lands right away without the animation. The positional `scrollToItem(index)` does not animate unless asked. `scrollToEnd()` and every object form animate by default, like FlatList.

### Selection, swipe actions and menus

- `allowsMultipleSelection`, `selectedKeys` (controlled) and `onSelectionChange`. A row reads `selected` and calls `select()` or `deselect()` from `renderElement`. The ref has `selectItem(index)`, `deselectItem(index)` and `getSelectedIndices()`. A selection follows its rows by key, and a removed row leaves it.
- `leadingSwipeActionsForItem` and `trailingSwipeActionsForItem` return `{ actions: [{ title, style, backgroundColor, onPress }], performsFirstActionWithFullSwipe }`. The row swipes natively and a full swipe performs the first action. The row stays slid out while that action runs. An `onPress` that returns a promise keeps it out until the promise settles, then a row still in the data slides back. `closeSwipeActions()` closes every row. Vertical lists only.
- `contextMenuForItem` returns `{ title, actions: [{ title, style, disabled, systemImage, onPress }] }`, a `UIMenu` on iOS and a popup menu on a long press on Android. A list with `reorderEnabled` uses the long press for dragging and shows no menu.

### Prefetching and saved position

- `prefetchDataSource={{ prefetchItems, cancelPrefetchingForItems }}` hears about rows up to `prefetchRows` (10) past the mounted ones, once each, and about prefetched rows that left before they mounted.
- `await ref.getAnchorState()` gives `{ key, offset }`, the row at the viewport start and how far into it the viewport starts. `restoreAnchorState(state)` scrolls back there, now or once the data brings the key.

### SectionList

`scrollToLocation({ sectionIndex, itemIndex, viewOffset, viewPosition, animated })` counts like React Native: `itemIndex` 0 is the section header. While headers pin, a row lands below its section's pinned header, like in React Native. `scrollToSection(section, animated?)` lands on a section's first row. `sectionIndexTitles` shows an index along the trailing edge, with `sectionForSectionIndexTitle(title, index)` to map a title to a section. `ItemSeparatorComponent` gets `section` too, and `SectionSeparatorComponent` gets `leadingSection` and `trailingSection`.

### Accessibility

On iOS a VoiceOver three finger swipe moves one screen and says which rows show. On Android the list reports its row count as `CollectionInfo`, the shown rows with scroll events, and offers page scrolls and scrolling to any row.

### Debug log

The C++ core can log every layout pass with an `[SL]` prefix. It is off by default, in debug builds too. To turn it on:

- iOS and macOS: run `SHADOWLIST_DEBUG_LOG=1 pod install`, then rebuild. A plain `pod install` turns it off again. The lines show in the Xcode console.
- Android: pass `arguments "-DSHADOWLIST_DEBUG_LOG=1"` in the app's `android.defaultConfig.externalNativeBuild.cmake` block, then rebuild. Read it with `adb logcat -s SL`.

## Migration to 0.9

0.9 renames five public names. The old names are removed with no aliases.

| 0.8                                      | 0.9                                     |
| ---------------------------------------- | --------------------------------------- |
| `dragEnabled`                            | `reorderEnabled`                        |
| `columns`                                | `numberOfColumns`                       |
| `snapToAlignment`                        | `snapAlignment`                         |
| `stickyHeaderIndices`                    | `stickyIndices`                         |
| `ref.scrollToIndex(index, viewPosition)` | `ref.scrollToItem(index, viewPosition)` |

The arguments and values are unchanged. `ShadowListForwardedProps` omits `stickyIndices` and `reorderEnabled` in place of the old names.

On iOS and macOS the native view's command method is now `scrollToItem:viewPosition:`.
