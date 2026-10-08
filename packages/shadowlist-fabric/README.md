# shadowlist

React Native Fabric list components on the shared C++ core: `ShadowList`, `SectionList`, `TreeList` and `DraggableList`.

Prop and command names follow the shared words in [docs/code-style.md](../../docs/code-style.md#public-api-words). The same concept has the same name on Fabric, UIKit and Android.

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

`ShadowList` takes FlatList's props and ref methods where they mean something for a list whose rows the native core places. Swap the import and most screens keep working.

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

### Scroll commands

`scrollToItem`, `scrollToIndex`, `scrollToEnd` and an animated `scrollToOffset` animate the way the native lists do. The view animates to where the core estimates the row is, then the same command runs without the animation and lands exactly, whatever the rows measure on the way. A finger on the list cancels it. macOS lands right away without the animation. The positional `scrollToItem(index)` does not animate unless asked, as before. `scrollToEnd()` and every object form animate by default, like FlatList.

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
