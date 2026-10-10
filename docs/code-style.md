# Code style

One rule set for `shadowlist-core`, `shadowlist-fabric`, `shadowlist-uikit`, `shadowlist-android` and the apps in `templates/`. Where a language has its own idiom the table says so. Everything else is the same in every language.

## Naming

| What                          | C++                                | Objective-C                         | Swift                     | Kotlin                       | Java                            |
| ----------------------------- | ---------------------------------- | ----------------------------------- | ------------------------- | ---------------------------- | ------------------------------- |
| Types                         | `PascalCase`                       | `Prefix` + `PascalCase`             | `PascalCase`              | `Prefix` + `PascalCase`      | `Prefix` + `PascalCase`         |
| Functions, locals, parameters | `camelCase`                        | `camelCase`                         | `camelCase`               | `camelCase`                  | `camelCase`                     |
| Private members               | `member_`                          | `_member` ivar                      | `member`                  | `member`                     | `mMember`                       |
| Public struct fields          | `member`                           | —                                   | —                         | —                            | —                               |
| Constants and macros          | `UPPER_SNAKE_CASE`                 | `PREFIX_UPPER_SNAKE_CASE`           | `static let camelCase`    | `const val UPPER_SNAKE_CASE` | `static final UPPER_SNAKE_CASE` |
| Enums                         | `enum class`, `PascalCase` values  | `NS_ENUM`, type name as case prefix | `enum`, `camelCase` cases | `enum class`                 | `static final int` slots        |
| Accessors                     | `getX()`, bools `isX()` / `hasX()` | property `x`, bool getter `isX`     | property                  | property                     | `getX()`, `isX()`               |
| Test names                    | `snake_case_sentence`              | —                                   | —                         | —                            | —                               |

Class prefixes say which package a type lives in. Objective-C has one global namespace and the prefixes must never collide.

| Package                                  | Prefix                                                                                                          |
| ---------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| `shadowlist-fabric`                      | `ShadowList` for classes, `SL` for file-local helpers, macros and constants (`SL_SCROLL_TO_TOP_DURATION`)       |
| `shadowlist-uikit`, `shadowlist-android` | `ShadowListKit` for everything, including the Bench apps and constants (`SHADOWLIST_KIT_LIFT_DURATION`)         |
| `shadowlist-core`                        | namespace `azimgd::shadowlist`, no prefix. File-local helpers go in an anonymous namespace                      |
| Runtime flags                            | `SL` + `PascalCase` (`-SLRoute`, `-SLEngine`, `-SLBench`, `SLCount`). One name on every platform. Never renamed |

One word per concept in every language: `previous` (not `prev`, `old`, `last`), `current`, `index`, `count`, `offset`, `low` / `high` for an inclusive index range, `delete` (not `remove`), `anchor` for what MVCP holds in place, and `xDp` / `xPx` for units. The plural is always `indices`, never `indexes`. Framework parameter names (`oldProps`, `oldScrollX`) and trace log keys stay as they are. A leading `_` marks an unused JS binding, and names local to a macro take a prefix such as `slt_a`. Visible means any pixel on screen, viewable means past the threshold, and measured means the core's measured range (visible plus overscan). `window` is the viewport size only, never a range of rows.

## Public API words

Four nouns, the same in every layer:

- `item`: one entry of the user's data. The only data word in public API, and its index is the item index.
- `row`: one placed slot in the layout, an item or a section header or footer row. Core, host and internals only. In a plain list the row index equals the item index.
- `cell`: the view that shows a row (`ShadowListKitListCell`, Fabric `ShadowListCellView`).
- `key`: stable identity of an item or row. Never `id`.

Every platform uses the same word for each concept and only changes the shape to fit its language. Fabric keeps React Native's name where FlatList, SectionList or ScrollView define one (`renderItem`, `initialNumToRender`, `scrollToIndex`, `ListHeaderComponent`).

| Concept                | Word                       | Objective-C                                                                                       | Kotlin                                                                                  | Fabric (JS)                                                                                                      |
| ---------------------- | -------------------------- | ------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| Drag reorder switch    | `reorderEnabled`           | `reorderEnabled`                                                                                  | `reorderEnabled`                                                                        | `reorderEnabled`                                                                                                 |
| Grid columns           | `numberOfColumns`          | `numberOfColumns`                                                                                 | `numberOfColumns`                                                                       | `numberOfColumns`                                                                                                |
| Snap alignment         | `snapAlignment`            | `snapAlignment` (`ShadowListKitSnapAlignment`)                                                    | `snapAlignment` (`ShadowListKitSnapAlignment`)                                          | `snapAlignment` (`'start' \| 'center' \| 'end'`)                                                                 |
| Sticky items           | `stickyIndices`            | `stickyIndices` (`NSIndexSet`)                                                                    | `stickyIndices` (`IntArray`)                                                            | `stickyIndices`                                                                                                  |
| Scroll to an item      | `scrollToItem`             | `scrollToItemAtIndex:viewPosition:animated:`                                                      | `scrollToItem(index, viewPosition, animated)`                                           | `scrollToIndex({ index, animated, viewOffset, viewPosition })`, `scrollToItem({ item, ... })`                    |
| Section headers pinned | `stickySectionHeaders`     | `stickySectionHeaders`                                                                            | `stickySectionHeaders`                                                                  | SectionList `stickySectionHeadersEnabled` (React Native's name)                                                  |
| Sections               | `numberOfSections`         | data source `numberOfSectionsInListView:`                                                         | `Sections.numberOfSections`                                                             | —                                                                                                                |
| Scroll to a section    | `scrollToSection`          | `scrollToSection:animated:`                                                                       | `scrollToSection(section, animated)`                                                    | SectionList `scrollToSection(section, animated?)`, `scrollToLocation(params)`                                    |
| Section index          | `sectionIndexTitles`       | data source `sectionIndexTitlesForListView:`                                                      | `Sections.sectionIndexTitles`                                                           | SectionList `sectionIndexTitles`, `sectionForSectionIndexTitle`                                                  |
| Batch of changes       | `performBatchUpdates`      | `performBatchUpdates:completion:`                                                                 | `performBatchUpdates(updates, completion)`                                              | —                                                                                                                |
| Diff the data          | `applyChanges`             | `applyChanges` → `ShadowListKitListChanges`                                                       | `applyChanges()` → `ShadowListKitListChanges`                                           | —                                                                                                                |
| Move an item           | `moveItem`                 | `moveItemAtIndex:toIndex:` (`sourceIndex`, `destinationIndex`)                                    | `moveItem(sourceIndex, destinationIndex)`                                               | `onMoveItem({ sourceIndex, destinationIndex, data })`                                                            |
| Partial reload         | `reconfigureCell`          | `reloadItemsAtIndices:payload:`, data source `listView:reconfigureCell:atIndex:payload:`          | `reloadItems(indices, payload)`, `DataSource.reconfigureCell`                           | —                                                                                                                |
| Selected items         | `selectedIndices`          | `selectedIndices` (`NSIndexSet`)                                                                  | `selectedIndices` (`IntArray`)                                                          | `selectedKeys`, `onSelectionChange`, `getSelectedIndices()`, `selectItem(index)`, `deselectItem(index)`          |
| Multiple selection     | `allowsMultipleSelection`  | `allowsMultipleSelection`                                                                         | `allowsMultipleSelection`                                                               | `allowsMultipleSelection`                                                                                        |
| Swipe actions          | `ShadowListKitSwipeAction` | delegate `leadingSwipeActionsForItemAtIndex:`                                                     | `Delegate.leadingSwipeActionsForItem`                                                   | `leadingSwipeActionsForItem`, `trailingSwipeActionsForItem` (`SwipeActionsConfiguration`), `closeSwipeActions()` |
| Item menu              | `contextMenuForItem`       | delegate `contextMenuForItemAtIndex:` (`UIMenu`)                                                  | `Delegate.contextMenuForItem` (`Menu`)                                                  | `contextMenuForItem` (`ContextMenu`)                                                                             |
| Prefetching            | `prefetchDataSource`       | `ShadowListKitListViewPrefetchDataSource`                                                         | `PrefetchDataSource`                                                                    | `prefetchDataSource` (`prefetchItems`, `cancelPrefetchingForItems`), `prefetchRows`                              |
| Pull to refresh        | `refreshEnabled`           | `refreshEnabled`, `refreshing`                                                                    | `refreshEnabled`, `refreshing`                                                          | `onRefresh` (turns it on), `refreshing`, `progressViewOffset`                                                    |
| Separators             | `showsSeparators`          | `showsSeparators`, `separatorInsetStart`                                                          | `showsSeparators`, `separatorInsetStart`                                                | `ItemSeparatorComponent` (`highlighted`, `leadingItem`, `trailingItem`, `separators`)                            |
| Change animations      | `itemAnimator`             | `itemAnimator` (`animateInsert`, `animateDelete`, `animateMove`)                                  | `itemAnimator` (`animateInsert`, `animateDelete`, `animateMove`)                        | —                                                                                                                |
| Render an item         | `cellForItem`              | data source `listView:cellForItemAtIndex:`                                                        | `DataSource.cellForItem`                                                                | `renderItem({ item, index })`, `keyExtractor(item, index)`                                                       |
| Item frame             | `rectForItem`              | `rectForItemAtIndex:`, `rectForHeaderInSection:`, `rectForFooterInSection:` (content coordinates) | `rectForItem`, `rectForHeaderInSection`, `rectForFooterInSection` (content coordinates) | —                                                                                                                |
| Item size estimate     | `estimatedItemSize`        | `estimatedItemSize`                                                                               | `estimatedItemSize`                                                                     | `getItemSizeSpec` (`ItemSizeSpec`), `getItemSize(key)`                                                           |
| Saved position         | `anchorState`              | `anchorState`, `restoreAnchorState:`                                                              | `anchorState`, `restoreAnchorState(state)`                                              | `getAnchorState()` (a `Promise`), `restoreAnchorState(state)`                                                    |
| Opening position       | `initialScrollIndex`       | —                                                                                                 | —                                                                                       | `initialScrollIndex`                                                                                             |
| Viewable items         | `viewabilityConfig`        | —                                                                                                 | —                                                                                       | `viewabilityConfig`, `viewabilityConfigCallbackPairs`, `recordInteraction()`                                     |

- A new public concept gets one row here before it ships on any platform.
- A rename is a hard break: no alias and a minor version bump.

## C++

- `#pragma once` in every header, including the Objective-C++ headers in `shadowlist-fabric`.
- Indices are `std::size_t`, and `UNDEFINED_INDEX` means "no index". Don't use `long` or `-1`. Convert to a signed type only inside the arithmetic that needs it.
- Platform edges convert the missing index: `UNDEFINED_INDEX` ↔ `-1` in JNI, `UNDEFINED_INDEX` ↔ `NSNotFound` in Objective-C.
- Commit tokens are `std::uint64_t` in C++. A bridge that carries one in a `double` slot converts at the JNI or Objective-C boundary.
- Write `this->` only when a name is shadowed.
- Pass a non-null `Container` as `Container&`. A raw pointer always means non-owning and nullable.
- Mark classes that aren't designed for subclassing `final`. Plain data stays a `struct` with public fields.
- Constants are namespace-scope `constexpr` unless only one class reads them.
- Write references as `const T& name`, in core and in Objective-C++ alike.
- Includes come in this order: the file's own header, a blank line, `<shadowlist-core/...>` sorted, a blank line, then std headers sorted. Tests put their quoted local headers first.
- Each parameter goes on its own line once a signature passes 120 columns.
- Leave unused parameters unnamed (`JNIEnv*, jclass`, `const std::string&`).
- Callbacks stored in a member are `std::function` named `onXxxCallback`. Helpers in headers take template callables.

## Core tests

- Every change in `packages/shadowlist-core` comes with a test in `packages/shadowlist-core-tests`.
- Test names are sentences in `snake_case` that state the behavior: `TEST(a_prediction_for_a_live_row_reflows_the_rows_after_it)`.
- Put the test in the `tests_*.cpp` file of its area. Host layer tests go in `tests_host_*.cpp`.

## Objective-C and Swift

- Headers sit inside `NS_ASSUME_NONNULL_BEGIN/END` and mark nullable values with `nullable`.
- Private headers are `Class+Private.h`.
- Allocate with `[X new]` when there are no arguments. Check for nil with `!_x` / `_x`. A pointer turned into a `BOOL` keeps `_x != nil`.
- Inside the class, read ivars directly (`_horizontal`), not through `self.`.
- C++ engine objects are value ivars. Lambdas that call back into Objective-C capture `weakSelf`.
- Constants are `static const` with the package prefix: `SL_X` in Fabric, `SHADOWLIST_KIT_X` in the kit. Durations are in seconds.
- Split files into sections with `#pragma mark - Name`.

## Kotlin and Java

- Kotlin for new Android code. Fabric's Java stays Java until a file is rewritten.
- Methods declared `external` / `native` are named `native*` and are `private`. A public or internal wrapper calls them with the handle.
- Durations are `Long` constants in milliseconds with an `_MS` suffix (`LIFT_DURATION_MS`). Dp values take `_DP`.
- Mark sections with `// region Name` and close them with `// endregion`. Use `@Nullable` in Java and `T?` in Kotlin. Library code never uses `!!`.

## JNI

- The C++ object behind a handle is `struct Peer`, found with `peerOf(jlong handle)`.
- The field holding it is `handle` in Kotlin and `mHandle` in Java. `destroy()` frees the peer and sets the handle to 0.
- The owner calls `destroy()` when its view detaches or is dropped. The next use creates a new peer lazily.
- Entry points sit in one `extern "C"` block and use one macro per class (`SHADOWLIST_KIT_JNI(name)`, `SL_SCROLL_SYNC_JNI(name)`).
- Includes: `<jni.h>` first, then std headers, then core headers.
- Slot arrays mirror a named enum on both sides (`PASS_*`, `STICKY_*`). Never index them with bare numbers.

## Kits

`shadowlist-uikit` and `shadowlist-android` are one design in two languages. A change in one kit lands in the other in the same change with the same names, or the change notes why not.

- `ShadowListKitListView` sections come in this order on both: Axis, Properties, Cells, Data, Sections, Layout, Mounting, Prefetching, Measurement, Separators, Events, Selection, Scroll events, Queries, Saved position, Accessibility, Scroll commands.
- iOS puts Data, Sections, Selection, Saved position with Scroll commands, Accessibility, Drag, and swipe actions with menus in category files (`ShadowListKitListView+Data.mm`, `+Sections`, `+Selection`, `+Commands`, `+Accessibility`, `+Drag`, `+Actions`). Android keeps them as regions of `ShadowListKitListView.kt` or in its controllers (`ShadowListKitDragController`, `ShadowListKitSwipeController`).
- Platform-only sections stay where they are: List, Delegate and Sticky on iOS. Drawing order, Edges, Nested scrolling and Drag on Android.
- Drag files use Pick up, Follow, Drop. Text files use Layout, View.
- Lifecycle teardown sits next to init: `dealloc` on iOS, `onDetachedFromWindow` on Android.
- Helpers share names: `mountRow`, `recycleStaleCells`, `mountPlan`, `mountedCell`, `unpinCell`, `stopScrolling`, `runCommandNow`, `hasHeldRow`, and the `previousOffset` field.
- A value both kits use lives once in `packages/shadowlist-core/host` as a `constexpr` in milliseconds (`DRAG_DROP_DURATION_MS`). iOS converts it to seconds (`SHADOWLIST_KIT_DROP_DURATION = DRAG_DROP_DURATION_MS / 1000.0`), and Android reads it through JNI (`ShadowListKitCore.DROP_DURATION_MS`).
- Other constants share names: iOS `SHADOWLIST_KIT_X` in seconds, Android `X_MS` in milliseconds.
- Scroll commands call `stopScrolling` (stop momentum and cancel the landing) before they send the command to the core.
- The same logic carries the same comment wording in both kits.

## Comments

Comments are short narration of what happens, not explanations of how. A comment longer than one line, or any comment above a declaration, is a `/* */` block with one `*` per line. A one-line comment inside a body stays `//`. Interface and type blocks carry no comments: TypeScript `interface` and `type` bodies and Objective-C `@interface` blocks list members only. The class doc goes in one block above the `@interface`. Contracts the user implements, Objective-C `@protocol` and Kotlin `interface` bodies such as the data source and delegate, keep one block above each method. Delete a comment when the code already says it. Avoid the "X, so Y" pattern in comments and docs: split it into two sentences, write "Y because X", or drop the clause.

## Structure

New code follows the file and siblings it lives next to: include and import order, initializer style, file layout and export style. Shared logic goes in `packages/shadowlist-core/host`, and the iOS, Android and Fabric adapters call it rather than copying it. Unused code is deleted, not commented out.

Use named constants, never bare numbers for event types or slots. Enums mirrored across C++, Objective-C, Kotlin and Java, such as out slots and state keys, keep the same names and numbers. Removing a feature removes its docs, package entries, trace macros, includes and test helpers too.

## Repo

- Edit `packages/shadowlist-core` only, never its mirror in `shadowlist-fabric`.
- Example apps, demo screens and `shadowlist-utils` live in `templates/`.
- Commits are a single conventional line such as `fix:` or `refactor:`.

## Checklist before you finish

- Core changed: a new test, and `ctest` passes in `packages/shadowlist-core-tests`.
- Kit changed: the other kit has the same change, and both kit example apps build.
- Fabric or JS changed: `yarn typecheck`, `yarn test` and `yarn lint` pass.
- A new or renamed public word has its row in the Public API words table.
- Comments follow the block rules and no `@interface` or TS `interface` / `type` body has one.
- No "X, so Y" in new comments, docs or commit messages.
