# ShadowList for React Native

ShadowList is the React Native Fabric package: the ShadowList, SectionList, TreeList and DraggableList components.
It runs on iOS, Android and macOS, on the shared C++ core.

## Install

```sh
yarn add shadowlist
cd ios && pod install
```

### Requirements

| Dependency   | Version                         |
| ------------ | ------------------------------- |
| React        | 19.2 or newer                   |
| React Native | 0.83 or newer, New Architecture |
| iOS          | React Native's minimum          |
| Android      | API 24 or newer                 |
| macOS        | 11 or newer, react-native-macos |

Autolinking picks up the pod on iOS and macOS and the CMake build on Android. The C++ core ships inside the package.
macOS has no pull to refresh, `snapToItem` or keyboard avoidance.

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

Prop and command names follow the shared item, row, cell and key words.

See more: [Public API words](https://github.com/azimgd/shadowlist/blob/main/docs/code-style.md#public-api-words)

### Props

| Prop                                                                                                                                                                                                        | Default | What it does                                                                                                                                                                                                       |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `renderItem({ item, index, separators, selected, select, deselect })`                                                                                                                                       |         | Renders a row.                                                                                                                                                                                                     |
| `keyExtractor`                                                                                                                                                                                              | `id`    | Rows are matched across updates by key. The default reads an item's `id` (a string or a number) and falls back to its index. Index keys follow positions, not rows: rows prepended above the screen then shift it. |
| `getItemSizeSpec(item, index)`                                                                                                                                                                              |         | Returns an `ItemSizeSpec`: a row's text with its font and insets, or a `fixedHeight`. Native then sizes rows near the screen before React renders them.                                                            |
| `measureLookaheadRows`                                                                                                                                                                                      | 48      | How many rows past the mounted ones get their size spec sent to native.                                                                                                                                            |
| `initialScrollIndex`                                                                                                                                                                                        |         | The row the list opens at. Read on mount only.                                                                                                                                                                     |
| `extraData`                                                                                                                                                                                                 |         | Rebuilds the mounted rows when it changes.                                                                                                                                                                         |
| `inverted`                                                                                                                                                                                                  | false   | Chat style list that opens at the end. Appended rows keep the visible content still.                                                                                                                               |
| `followAppends`                                                                                                                                                                                             | false   | Scroll along with appended rows while the list rests at its end.                                                                                                                                                   |
| `horizontal`                                                                                                                                                                                                | false   | Scroll along the x axis.                                                                                                                                                                                           |
| `numberOfColumns`, `columnWrapperStyle`                                                                                                                                                                     | 1       | Grid columns. Rows go round robin, row `i` in column `i % numberOfColumns`. `columnWrapperStyle` takes `gap`, `columnGap` and horizontal padding. Each column keeps the same width.                                |
| `ItemSeparatorComponent`                                                                                                                                                                                    |         | A component gets `highlighted`, `leadingItem` and `trailingItem`. A React element, or a function without parameters, renders once for every row.                                                                   |
| `ListHeaderComponentStyle`, `ListFooterComponentStyle`                                                                                                                                                      |         | Style for the header and footer.                                                                                                                                                                                   |
| `contentContainerStyle`, `contentInset`                                                                                                                                                                     |         | Numeric padding only. Padding along the scroll axis goes before the header and after the footer, across it into every row. Insets stay at zero otherwise: wrap the list in a safe area view.                       |
| `stickyIndices`, `renderStickyHeaderOverlay(index)`                                                                                                                                                         |         | Rows that pin to the top while their section scrolls, and an optional overlay drawn for the pinned row.                                                                                                            |
| `stickyHeader`, `stickyFooter`                                                                                                                                                                              | false   | Pin `ListHeaderComponent` or `ListFooterComponent` to its edge.                                                                                                                                                    |
| `autoHideHeader`, `autoHideFooter`                                                                                                                                                                          | false   | A pinned header or footer slides out while the user scrolls further into the list and slides back when they scroll the other way, like a hiding toolbar.                                                           |
| `reorderEnabled`, `onMoveItem({ sourceIndex, destinationIndex, data })`                                                                                                                                     | false   | Touch and hold a row to drag it. Save `data` from `onMoveItem` to your state or the row snaps back. DraggableList turns it on by default.                                                                          |
| `snapToItem`, `snapAlignment`                                                                                                                                                                               | false   | Rest on a row edge: `'start'`, `'center'` or `'end'`.                                                                                                                                                              |
| `overscan`                                                                                                                                                                                                  | 1       | How far past the viewport native measures and places rows, in viewport sizes.                                                                                                                                      |
| `mountOverscanRows`, `mountOverscanRowsLeading`                                                                                                                                                             | 4, 10   | Rows React keeps mounted on each side of the screen, and ahead of a fling.                                                                                                                                         |
| `initialNumToRender`                                                                                                                                                                                        | 20      | Rows mounted before native first reports what is on screen.                                                                                                                                                        |
| `onStartReached`, `onEndReached` and their thresholds                                                                                                                                                       | 1       | Called near either end. Thresholds are in viewport sizes.                                                                                                                                                          |
| `persistentKeys`                                                                                                                                                                                            |         | Rows that stay mounted wherever the list scrolls, in their normal place. They are not pinned.                                                                                                                      |
| `nonAnchorKeys`                                                                                                                                                                                             |         | Rows never used to hold the visible content in place, like date pills or unread dividers whose keys come and go.                                                                                                   |
| `trackItemSizes`                                                                                                                                                                                            | false   | Keeps every mounted row's size for `getItemSize(key)` and `getItemSizes()` on the ref.                                                                                                                             |
| `viewabilityConfig`, `onViewableItemsChanged`, `viewabilityConfigCallbackPairs`                                                                                                                             |         | Viewable rows with `itemVisiblePercentThreshold`, `viewAreaCoveragePercentThreshold`, `minimumViewTime` and `waitForInteraction`.                                                                                  |
| `onScroll`, `onScrollBeginDrag`, `onScrollEndDrag`, `onMomentumScrollBegin`, `onMomentumScrollEnd`, `onContentSizeChange`, `scrollEventThrottle`                                                            |         | ScrollView's event shape: `contentOffset`, `contentSize`, `layoutMeasurement`, `velocity` in points per millisecond. An event held back by `scrollEventThrottle` is sent once the list rests.                      |
| `scrollEnabled`, `showsVerticalScrollIndicator`, `showsHorizontalScrollIndicator`, `bounces`, `decelerationRate`, `scrollsToTop`, `keyboardDismissMode`, `keyboardShouldPersistTaps`, `nestedScrollEnabled` |         | Passed to the native scroll view. `keyboardShouldPersistTaps` is handled in JS. Unset, taps always reach the rows.                                                                                                 |
| `itemStyle`                                                                                                                                                                                                 |         | Style for the view around each row.                                                                                                                                                                                |
| `refreshColor`, `progressViewOffset`                                                                                                                                                                        |         | Tint and offset of the pull to refresh spinner.                                                                                                                                                                    |

### Keyboard

| Export                   | What it does                                                                                 |
| ------------------------ | -------------------------------------------------------------------------------------------- |
| `KeyboardView`           | Dismisses the keyboard on a tap in an empty area.                                            |
| `useKeyboardAnimation()` | Returns `{ height, progress }` as `Animated.Value`s that follow the keyboard frame by frame. |

### Ref commands

| Command                                                               | What it does                                                                                          |
| --------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
| `scrollToIndex({ index, animated, viewOffset, viewPosition })`        | Scrolls to a row by index. Animates by default.                                                       |
| `scrollToItem({ item, animated, viewOffset, viewPosition })`          | Scrolls to a row by item. Animates by default.                                                        |
| `scrollToOffset({ offset, animated })`                                | Scrolls to an offset. Also takes positional arguments.                                                |
| `scrollToEnd({ animated })`                                           | Scrolls to the end. Also takes positional arguments. Animates by default.                             |
| `flashScrollIndicators()`, `recordInteraction()`                      | Same as in React Native.                                                                              |
| `getNativeScrollRef()`, `getScrollResponder()`, `getScrollableNode()` | Return the native `ShadowListView` and its tag.                                                       |
| `setStartReachedEnabled(enabled)`, `setEndReachedEnabled(enabled)`    | Pause the edge callbacks while a page loads.                                                          |
| `onScrollToIndexFailed` (prop)                                        | Called for an index outside the data, with `{ index, highestMeasuredFrameIndex, averageItemLength }`. |

Scroll commands animate and land exactly, whatever the rows measure on the way. A finger on the list cancels them.
macOS lands right away without the animation.

### Selection, swipe actions and menus

| Prop or command                                                             | What it does                                                                                                                                                                                                                                                                                                                                                        |
| --------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `allowsMultipleSelection`, `selectedKeys` (controlled), `onSelectionChange` | A row reads `selected` and calls `select()` or `deselect()` from `renderItem`. A selection follows its rows by key, and a removed row leaves it.                                                                                                                                                                                                                    |
| `selectItem(index)`, `deselectItem(index)`, `getSelectedIndices()`          | Selection on the ref.                                                                                                                                                                                                                                                                                                                                               |
| `leadingSwipeActionsForItem`, `trailingSwipeActionsForItem`                 | Return `{ actions: [{ title, style, backgroundColor, onPress }], performsFirstActionWithFullSwipe }`. The row swipes natively and a full swipe performs the first action. The row stays slid out while that action runs. An `onPress` that returns a promise keeps it out until the promise settles, then a row still in the data slides back. Vertical lists only. |
| `closeSwipeActions()`                                                       | Closes every row.                                                                                                                                                                                                                                                                                                                                                   |
| `contextMenuForItem`                                                        | Returns `{ title, actions: [{ title, style, disabled, systemImage, onPress }] }`, a `UIMenu` on iOS and a popup menu on a long press on Android. A list with `reorderEnabled` uses the long press for dragging and shows no menu.                                                                                                                                   |

### Prefetching and saved position

| Prop or command                                                     | What it does                                                                                                                          |
| ------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- |
| `prefetchDataSource={{ prefetchItems, cancelPrefetchingForItems }}` | Hears about rows up to `prefetchRows` (10) past the mounted ones, once each, and about prefetched rows that left before they mounted. |
| `await getAnchorState()`                                            | Gives `{ key, offset }`, the row at the viewport start and how far into it the viewport starts.                                       |
| `restoreAnchorState(state)`                                         | Scrolls back there, now or once the data brings the key.                                                                              |

### SectionList

| Prop or command                                                                     | What it does                                                                                                                     |
| ----------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| `scrollToLocation({ sectionIndex, itemIndex, viewOffset, viewPosition, animated })` | Counts like React Native: `itemIndex` 0 is the section header. While headers pin, a row lands below its section's pinned header. |
| `scrollToSection(section, animated?)`                                               | Lands on a section's first row.                                                                                                  |
| `sectionIndexTitles`, `sectionForSectionIndexTitle(title, index)`                   | Shows an index along the trailing edge and maps a title to a section.                                                            |
| `ItemSeparatorComponent`, `SectionSeparatorComponent`                               | `ItemSeparatorComponent` gets `section` too. `SectionSeparatorComponent` gets `leadingSection` and `trailingSection`.            |

### Accessibility

| Platform | Behavior                                                                                                                                 |
| -------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| iOS      | A VoiceOver three finger swipe moves one screen and says which rows show.                                                                |
| Android  | The list reports its row count as `CollectionInfo`, the shown rows with scroll events, and offers page scrolls and scrolling to any row. |

## Example app

The React Native example runs on iOS, Android and macOS.

See more: [Example apps](../../README.md#example-apps)

## Debug log

The C++ core can log every layout pass with an `[SL]` prefix. It is off by default, in debug builds too.

| Platform      | How to turn it on                                                                                                                                                                                                                                                          |
| ------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| iOS and macOS | Run `SHADOWLIST_DEBUG_LOG=1 pod install`, then rebuild. A plain `pod install` turns it off again. The lines show in the Xcode console.                                                                                                                                     |
| Android       | Pass `arguments "-DSHADOWLIST_DEBUG_LOG=1"` in the app's `android.defaultConfig.externalNativeBuild.cmake` block, then rebuild. Read it with `adb logcat -s SL`. Building with `-PshadowlistDebugLog` also turns on the library's Java `[SL]` trace through `BuildConfig`. |
