# ShadowListKit for AppKit

ShadowListKit for AppKit is the Mac package: ShadowListKitListView, a flipped, layer backed NSView with a data
source like NSTableView's. It is written in Objective-C++ on the shared C++ core, with the class names and
selectors of [ShadowListKit for UIKit](../shadowlist-uikit/README.md). Code that imports `<ShadowListKit/...>`
builds on both.

The list owns its scroll position, with no `NSScrollView`. Its cells sit in a content view moved by
`contentOffset`, a scroll engine scrolls it with the trackpad and the mouse wheel, and overlay scrollers show
over it. With `scrollDriven` a host such as a two axis grid moves it instead.

## Install

Swift Package Manager: add `https://github.com/azimgd/shadowlist` and the ShadowListKitAppKit product. The package's
ShadowListKit product is the UIKit kit and builds for iOS only, and `swift build` on a Mac takes
`--target ShadowListKitAppKit`.

```swift
.package(url: "https://github.com/azimgd/shadowlist", from: "0.9.0")
```

CocoaPods: the ShadowListKit pod builds this kit on macOS.

```ruby
platform :osx, '14.0'
pod 'ShadowListKit', '~> 0.9'
```

Then `import ShadowListKitAppKit` in Swift with Swift Package Manager, `import ShadowListKit` with CocoaPods, or
`#import <ShadowListKit/ShadowListKit.h>` in Objective-C with either.

### Requirements

| Dependency | Version |
| --- | --- |
| macOS | 14 or newer |

## Usage

```swift
import ShadowListKitAppKit

final class FeedController: NSViewController, ShadowListKitListViewDataSource, ShadowListKitListViewDelegate {
  private let list = ShadowListKitListView()
  private var posts: [Post] = []

  override func loadView() {
    view = NSView()
    list.autoresizingMask = [.width, .height]
    list.dataSource = self
    list.delegate = self
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

  func listView(_ listView: ShadowListKitListView, didActivateItemAt index: Int) {
    open(posts[index])
  }
}
```

The same in Objective-C:

```objc
#import <ShadowListKit/ShadowListKit.h>

@interface FeedController : NSViewController <ShadowListKitListViewDataSource>
@end

@implementation FeedController {
  ShadowListKitListView *_list;
  NSArray<Post *> *_posts;
}

- (void)loadView
{
  self.view = [NSView new];
  _list = [ShadowListKitListView new];
  _list.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  _list.dataSource = self;
  [_list registerClass:[PostCell class] forCellReuseIdentifier:@"post"];
  [self.view addSubview:_list];
  [_list reloadData];
}

- (NSInteger)numberOfItemsInListView:(ShadowListKitListView *)listView
{
  return _posts.count;
}

- (NSString *)listView:(ShadowListKitListView *)listView keyForItemAtIndex:(NSInteger)index
{
  return _posts[index].identifier;
}

- (ShadowListKitListCell *)listView:(ShadowListKitListView *)listView cellForItemAtIndex:(NSInteger)index
{
  PostCell *cell = [listView dequeueReusableCellWithIdentifier:@"post"];
  [cell configure:_posts[index]];
  return cell;
}

@end
```

`PostCell` subclasses `ShadowListKitListCell` and configures a `Post`. A cell lays out its subviews in `layout`
and reports its height in `sizeThatFits:`, or leaves `sizeThatFits:` alone and fits its Auto Layout constraints.

## API

The table summarizes the API; [ShadowListKitListView.h](Sources/ShadowListKit/include/ShadowListKit/ShadowListKitListView.h)
has full signatures and defaults. Rows marked Mac only have no UIKit counterpart.

| Area | API | Notes |
| --- | --- | --- |
| Data source | `numberOfItemsInListView:`, `listView:keyForItemAtIndex:`, `listView:cellForItemAtIndex:`, `listView:reconfigureCell:atIndex:payload:` | `listView:reconfigureCell:atIndex:payload:` updates a shown cell for a payload reload. Return `NO` to reload the item in full. |
| Sizing | `listView:sizeForItemAtIndex:crossSize:` | Optional. Without it every item is measured once through its cell's `sizeThatFits:`, which fits Auto Layout constraints. |
| Sections | `numberOfSectionsInListView:`, `listView:numberOfItemsInSection:`, `listView:keyForSection:`, `listView:titleForHeaderInSection:`, `listView:titleForFooterInSection:`, `listView:cellForHeaderInSection:`, `listView:cellForFooterInSection:`, `listView:sizeForHeaderInSection:crossSize:`, `listView:sizeForFooterInSection:crossSize:` | A title, an empty one too, gives a section a header or footer, shown in a plain cell unless a cell is given. The section key defaults to the key of its first item. |
| Content versions | `listView:contentVersionForItemAtIndex:` | A revision or hash. `applyChanges` reloads items whose value changed. |
| Delegate | `listView:willDisplayCell:atIndex:`, `listView:didEndDisplayingCell:atIndex:`, `listView:didSelectItemAtIndex:`, `listView:didDeselectItemAtIndex:`, `listView:shouldSelectItemAtIndex:`, `listView:shouldHighlightItemAtIndex:`, `listView:canMoveItemAtIndex:`, `listView:moveItemAtIndex:toIndex:`, `listViewDidReachStart:`, `listViewDidReachEnd:` | Every method is optional. |
| Mac delegate | `listView:didActivateItemAtIndex:`, `listView:typeSelectStringForItemAtIndex:`, `listViewDidLayOut:`, `listView:didCorrectOffset:` | Mac only. A double-click, Return or Space activates. Typing selects by the string. `listViewDidLayOut:` follows every layout pass. `listView:didCorrectOffset:` is the driven mode's offset. |
| Scroll events | `listViewDidScroll:`, `listViewWillBeginDragging:`, `listViewDidEndDragging:willDecelerate:`, `listViewDidEndDecelerating:`, `listViewDidEndScrollingAnimation:` | `UIScrollViewDelegate`'s calls, on the list's own delegate. They follow `contentOffset` and `scrollPhase`. |
| Cells | `registerClass:forCellReuseIdentifier:`, `dequeueReusableCellWithIdentifier:` | `prepareForReuse` runs before a cell is handed out again. |
| Cell states | `highlighted`, `selected`, `editing`, `hovered`, `emphasized`, `menuTarget` | Override the `set...:animated:` setters to show them. `hovered`, `emphasized` and `menuTarget` are Mac only. |
| Layout | `inverted`, `followAppends`, `horizontal`, `numberOfColumns`, `estimatedItemSize`, `overscan`, `mountOverscan`, `startReachedThreshold`, `endReachedThreshold`, `snapToItem`, `snapAlignment`, `reorderEnabled`, `stickyIndices`, `stickySectionHeaders`, `headerView`, `footerView` | `followAppends` is off by default, which keeps the visible items in place. Set `needsLayout` on the list after `headerView` or `footerView` changes size. |
| Changes | `reloadData`, `insertItemsAtIndices:`, `deleteItemsAtIndices:`, `reloadItemsAtIndices:`, `reloadItemsAtIndices:payload:`, `moveItemAtIndex:toIndex:`, `performBatchUpdates:completion:`, `applyChanges` | An insert past the end goes at the end, a delete past the end is dropped. `applyChanges` diffs every key and returns a `ShadowListKitListChanges`. |
| Animations | `animatesChanges`, `itemAnimator` | Off by default. Inserted items fade in, deleted items fade out and items that stay slide. `ShadowListKitDefaultItemAnimator` unless replaced. |
| Selection | `allowsSelection`, `allowsMultipleSelection`, `selectedIndices`, `clickedIndex`, `selectItemAtIndex:animated:`, `deselectItemAtIndex:animated:` | Selection follows keys. The select and deselect calls skip the delegate. See [Selection and keys](#selection-and-keys). |
| Swipe actions | Delegate `listView:leadingSwipeActionsForItemAtIndex:`, `listView:trailingSwipeActionsForItemAtIndex:`, `closeSwipeActionsAnimated:` | A `ShadowListKitSwipeActionsConfiguration` per side. Trackpad only. See [Swipe actions](#swipe-actions). |
| Context menus | Delegate `listView:contextMenuForItemAtIndex:` | An `NSMenu` for a right click, a Control-click or the menu key. An item without one shows its swipe actions as a menu. |
| Prefetching | `prefetchDataSource`: `listView:prefetchItemsAtIndices:`, `listView:cancelPrefetchingForItemsAtIndices:` | Items in the measured range past the mounted ones, prefetched by the layout pass that brings them in, after it measures the items it will show and before it measures these. Without `listView:sizeForItemAtIndex:crossSize:` that is before `listView:cellForItemAtIndex:` makes their cells. Cancelled when one leaves the range unseen. |
| Separators | `showsSeparators`, `separatorColor`, `separatorInsetStart`, `separatorInsetEnd`, `separatorThickness`, delegate `listView:showsSeparatorAfterItemAtIndex:` | Only between items of one section. One device pixel of the window's backing scale thick by default, in a color that follows the appearance. |
| Queries | `cellForItemAtIndex:`, `visibleCells`, `visibleRange`, `rectForItemAtIndex:`, `numberOfSections`, `sectionForItemAtIndex:`, `firstItemIndexInSection:`, `rectForHeaderInSection:`, `rectForFooterInSection:` | `visibleRange` runs low to high. |
| Scroll position | `contentOffset`, `setContentOffset:animated:`, `contentSize`, `contentInset`, `adjustedContentInset`, `scrollEnabled`, `scrollPhase`, `tracking`, `dragging`, `decelerating` | `UIScrollView`'s names. `scrollEnabled` off passes wheel events to the next responder. |
| Driven mode | `scrollDriven` (Swift `isScrollDriven`), `contentOffset`, `scrollPhase`, delegate `listView:didCorrectOffset:` | The Android kit has `scrollDriven` too. See [Driven mode](#driven-mode). |
| Scrolling | `scrollToItemAtIndex:viewPosition:animated:`, `scrollToSection:animated:`, `scrollToStartAnimated:`, `scrollToEndAnimated:` | `viewPosition` goes from 0 at the viewport start to 1 at the end. The core keeps correcting until the item lands. |
| Saved position | `anchorState`, `restoreAnchorState:` | The key at the viewport start and the distance into it. Lands now or once a reload brings the key. Saved with the window's state restoration when the list has an `identifier`. |
| Accessibility | A table of one column, every item a row | See [Accessibility](#accessibility). |
| Scroll engine | `ShadowListKitScrollEngine`: `axes`, `offset`, `contentSize`, `viewportSize`, `maxOffset`, `lineScroll`, `phase`, `animating`, `handleScrollWheel:`, `animateToOffset:`, `stopScrolling` | Mac only. See [Scrolling](#scrolling). |
| Scroll engine delegate | `scrollEngine:didScrollToOffset:`, `scrollEngine:didChangePhase:`, `scrollEngineDidEndAnimation:`, `scrollEngine:restingOffsetForOffset:` | Mac only. The first is required. `scrollEngine:restingOffsetForOffset:` moves where a gesture rests, like an item edge. |
| Scrollers | `ShadowListKitScrollerView`: `axes`, `contentSize`, `offset`, `scrollerInsets`, `showsScrollersAlways`, `flashScrollers` | Mac only. Overlay scrollers for a view that owns its scroll position. |
| Scrollers delegate | `scrollerView:didScrollToOffset:`, `scrollerViewDidScrollForAccessibility:` | Mac only. The first is required. The host moves its content to the offset. |
| Text | `ShadowListKitTextLayout`, `ShadowListKitTextView` | Text typeset off the main thread with an `NSFont` and shown as a bitmap at the window's backing scale, the same CoreText code as the UIKit kit. |

Masonry is round robin by row: row `r`, counting section header and footer rows, goes in column
`r % numberOfColumns`, and each column stacks its own rows. Full width rows in a grid are not supported.

These combinations do nothing:

- `showsSeparators` in a grid.
- `followAppends` without `inverted`.
- `snapToItem` and `scrollEnabled` with `scrollDriven`. The host scrolls a driven list.
- Trackpad swipes with `scrollDriven` or without `scrollEnabled`, where the wheel goes to the next responder.
  The swipe actions still show as the menu and as accessibility actions.

## Scrolling

`ShadowListKitScrollEngine` scrolls a view that owns its scroll position the way a scroll view does, on one axis
or two, with the physics in the core's `host/ScrollPhysics`:

- A trackpad gesture drags from its first movement and rubber bands past the edges. On two axes a gesture that
  starts within the core's `SCROLL_LOCK_DEGREES` of an axis moves that axis only. On one axis a gesture that
  starts across it is passed on whole, momentum included.
- The system's momentum settles it, and momentum that reaches an edge goes at most 12% of the viewport past it
  before it springs back.
- A mouse wheel scrolls by `lineScroll` points a line and stops at the edges. One that moves nothing is passed on.
- `animateToOffset:` eases to an offset. A gesture coming to rest can snap through the delegate's
  `scrollEngine:restingOffsetForOffset:`.
- `stopScrolling` stops momentum, a spring back and an animation inside the content's edges.

`handleScrollWheel:` returns whether the engine took the event. A host passes the rest to `super`.

`ShadowListKitScrollerView` draws overlay scrollers over such a view: shown while it scrolls and at all times
when Show scroll bars is Always, wider under the pointer, with a draggable thumb and a track that moves the thumb
to a click. Only the tracks take the mouse, and `scrollerInsets` keeps them off the view's edges.

The list uses both unless `scrollDriven` is set. Its animated commands ease toward their estimated target and the
core lands them exactly, then `listViewDidEndScrollingAnimation:` follows. `snapToItem` rests a gesture or a
mouse wheel on the item edge `snapAlignment` names. A gesture across the list's axis that swipes no item goes to the
next responder whole, with its momentum, and so does a mouse wheel at an edge.

## Driven mode

A view that owns the scrolling, like a spreadsheet grid that pans on two axes, puts the list inside itself and
moves it:

- `scrollDriven = YES`. The list never moves itself, shows no scrollers and passes wheel events to the next
  responder.
- The host runs its own `ShadowListKitScrollEngine` with both axes and a `ShadowListKitScrollerView`, sets the
  list's `contentOffset` on the list's axis from the engine's offset and `scrollPhase` from
  `scrollEngine:didChangePhase:`, and shifts its cells across the other axis itself.
- Every offset the list wants comes to the delegate's `listView:didCorrectOffset:`: a correction that keeps the
  visible items still while rows are measured, inserted or deleted, the target of a scroll command and the edge
  scroll of a held row. The host sets `contentOffset` to it inside the call and moves its engine's offset to
  match. The list lays out at the offset it has when the call returns.
- `listViewDidLayOut:` follows every layout pass, when `contentSize` is current for the engine and the scrollers.
- `allowsSelection = NO` leaves the mouse, the keyboard and the hover to the host, which selects by itself.
- `accessibilityElement = NO` hides the list and its rows when the host gives its own elements.

A scroll command on a driven list moves at once, without an animation. Change animations, sticky rows and drag
reorder work the same as in a list that scrolls by itself. Trackpad swipes do not reach a driven list's items.

## Selection and keys

The list selects the way `NSTableView` does while `allowsSelection` is set, the default:

- A click selects when the mouse goes down, Command-click adds or removes an item and Shift-click with
  `allowsMultipleSelection` selects from the anchor, the last item clicked. A click on no item deselects all.
- A double-click, Return and Space call the delegate's `listView:didActivateItemAtIndex:`.
- The list takes the first responder on a click. The arrow keys move the selection, across the axis between a
  grid's columns. Shift extends it, Command-arrows go to the ends and Option-Page keys move a page. Home, End and
  the page keys scroll without selecting. Command-A selects all and Escape deselects. The selected item scrolls
  into view at the nearest edge.
- Typed characters within a second of each other build a prefix matched against the delegate's
  `listView:typeSelectStringForItemAtIndex:`, for items shown or not.
- Cells get `highlighted` during a press, `hovered` under a resting pointer, `emphasized` while the list is the
  key window's first responder and `menuTarget` while their menu is open. The focus ring follows `focusRingType`.

The anchor and the range are the core's `host/ListSelection`. Without `allowsSelection` the list takes no first
responder, tracks no pointer and passes mouse and key events on.

## Menus

A right click, a Control-click or the menu key shows the delegate's `listView:contextMenuForItemAtIndex:` menu,
with or without `allowsSelection`. While it is open `clickedIndex` is the item, its cell is the `menuTarget` and
the list draws an outline around it. The chosen menu item's action still reads `clickedIndex`, like `clickedRow` in
an NSTableView menu. `clickedIndex` is also the item a mouse press is on until the mouse goes up.

An item the delegate gives no menu, but swipe actions, shows its trailing then its leading actions as a menu, a
destructive one in red. A mouse without a trackpad reaches the actions that way.

## Swipe actions

A two finger swipe across the scroll axis on a trackpad moves an item with actions from the delegate and shows
their buttons behind it, the way `NSTableView` shows row actions. A mouse wheel never swipes.

- The core's `host/SwipeReveal` decides how far the row follows and where it rests: open past half its buttons
  or when flung with the speed of the last wheel events, closed otherwise.
- A full swipe performs the side's first action when `performsFirstActionWithFullSwipe` is set.
- A click on a button performs its action with the handler and completion of `ShadowListKitSwipeAction`, the
  same as the UIKit kit's tap.
- A click anywhere else, scrolling, Escape, a right click, picking up a row, a data change and
  `closeSwipeActionsAnimated:` close the item. A recycled cell comes back closed.
- A swipe on an item without actions on that side goes to the next responder.

The swipe actions also show as the item's menu when the delegate gives it none, and run as the row's custom
actions in accessibility.

## Sections

Sections come from the same data source calls as the UIKit kit. A section with only a title shows it in a
built-in cell: the headline font over the window background for a header, the small system font for a footer,
both in the secondary label color and redrawn when the appearance changes. `stickySectionHeaders` pins the
header of the section at the top, `stickyIndices` pins items, and the next pinned row pushes the current one up,
with the core's `ListDriver` placing them. Pinned rows sit over the other rows and under a held one, follow a
driven list's offset, go with the content while it rubber bands past the start and stay pinned past the end.
`headerView` and `footerView` sit before the first row and after the last, sized by their frame,
`sizeThatFits:` or `fittingSize`, and the item rects and scroll commands count them.

## Drag reorder

With `reorderEnabled` an item lifts as soon as the mouse drags it 3 points, without the hold a finger needs on
iOS. A shorter press stays a click. The held row follows the mouse a little bigger and with a shadow, the other
rows slide aside to open the gap, and the list scrolls within 90 points of an edge at up to 960 points a second.
A Force Touch trackpad ticks as the drop spot moves. Escape puts the row back. The drop calls the delegate's
`listView:moveItemAtIndex:toIndex:`, after `listView:canMoveItemAtIndex:` allowed the pickup, the same calls as
the UIKit kit. It works in grids and without `allowsSelection`, and in a list with sections an item stays in its
own section. A driven list scrolls near the edges through `listView:didCorrectOffset:`. The drag math is the
core's `host/DragReorder`.

## Change animations

With `animatesChanges` inserted items fade in, deleted items fade out where they were and items that stay slide
from where they showed, the same changes the UIKit kit animates with the core's `host/ChangeAnimation`.
`ShadowListKitDefaultItemAnimator` adds explicit Core Animation animations of a cell layer's opacity and
position, additive for a slide, which leaves the frame the list gives a cell and the transform a drag shifts it
by to the list. A deleted item's cell goes back to the pool once its fade ends. `itemAnimator` takes another
animator with the same calls as the UIKit kit, and `performBatchUpdates:completion:` finishes after the default
animator's animation. A driven list animates the same way and reports its corrections as before. Off, the
default, the list has no animator state and adds no work to a layout pass.

## Accessibility

The list is a table of one column to VoiceOver, the way `NSTableView` shows itself:

- Every row is a row element, mounted or not, which makes the row count and reading order cover the whole list.
  Section header and footer rows are rows in their place, between the header view and the footer view.
- A mounted row holds its cell, which takes the cell role, and frames where the cell shows. A row not mounted
  frames where the core places it. Scrolling a row to visible scrolls it into view at the nearest edge.
- Setting the selected rows selects the way a click does, with the delegate's calls, and posts
  `NSAccessibilitySelectedRowsChangedNotification` once a turn. A data change that changes the row count posts
  `NSAccessibilityRowCountChangedNotification`.
- Each scroller track that can scroll is a scroll bar whose value runs from 0 to 1. Setting it scrolls there and
  incrementing it scrolls a page, after which the list announces the rows shown.
- An item's swipe actions are its row's custom actions. With `reorderEnabled` an item the delegate lets move has
  Move up and Move down actions, and Move left and Move right across a grid's columns, which move it the way a
  drop a step away would, inside its section.

A host that gives its own elements, like a grid over a driven list, sets `accessibilityElement` to `NO` on the
list. That hides the list, its rows and its notifications.

## Differences from the UIKit kit

Left out on the Mac:

| UIKit API | Why |
| --- | --- |
| `refreshEnabled`, `refreshing`, `listViewDidBeginRefreshing:` | Pull to refresh is a touch gesture. Mac apps refresh from a menu item or a button. |
| `sectionIndexTitlesForListView:`, `listView:sectionForSectionIndexTitle:atIndex:` | AppKit has no section index. Type select reaches items by their text instead. |
| `editing`, `setEditing:animated:` on the list | AppKit lists have no editing mode. The cell keeps its `editing` state, which the list does not set. |
| Keyboard insets | A Mac window has no on-screen keyboard to inset the list for. |
| Scroll to top on a status bar tap | A Mac window has no status bar. `scrollToStartAnimated:` remains. |
| `UIScrollViewDelegate` | The list is not a scroll view. Its delegate has the scroll calls under `listView` names. |

Only on the Mac so far, not in the UIKit or Android kits:

- The swipe actions as an item's menu when the delegate gives it none.
- The swipe actions as accessibility custom actions.
- Move up, down, left and right accessibility actions with `reorderEnabled`.
- Keyboard selection, type select, activation, hover, emphasis and `clickedIndex`.
- `listView:didCorrectOffset:` and `listViewDidLayOut:` for a driven list. The Android kit has `scrollDriven` too,
  and its list moves itself with `adjustOffsetBy` and calls a subclass's `didLayOut` instead.
- `ShadowListKitScrollEngine` and `ShadowListKitScrollerView`.

## Layout

The files the two kits share live in the UIKit kit and are linked here: the key and index conversions
(`ShadowListKitCoreBridge`), the platform names and helpers (`ShadowListKitPlatform.h`), the list's and the cell's
private headers, the change animator's header, the model classes (`ShadowListKitListModels.h` and `.mm`) and the
`+Data` and `+Sections` categories. Each compiles once in each kit, and what differs sits behind `TARGET_OS_OSX`. The
other categories are the UIKit kit's (`+Selection`, `+Commands`, `+Accessibility`, `+Drag`, `+Actions`) on AppKit,
plus `+Scrolling` for the engine and the scrollers.

## Tests

```sh
cd packages/shadowlist-appkit && swift test
```

`swift test` runs the kit's XCTest suites on this Mac: mounting, data changes, sections, driven mode, scrolling,
the scroll engine and scrollers, selection, hover, menus, drag reorder, change animations, accessibility, swipe
actions, the text layout and the models. The kit has no Bench app yet. The UIKit and Android kits measure theirs
there.

[ShadowListKitListView+Testing.h](Sources/ShadowListKit/include/ShadowListKit/ShadowListKitListView+Testing.h)
drives the list without a trackpad, a mouse or a keyboard, for tests and scripted checks. Frames run at once
instead of on the display link.

| Area | API | Notes |
| --- | --- | --- |
| Scrolling | `ShadowListKitScrollWheelEvent(delta, phase, momentumPhase)`, `shadowListKit_scrollWheelDelta:phase:momentumPhase:`, `shadowListKit_runScrollFramesFor:`, `shadowListKit_scrollerView` | A wheel event with precise deltas in points for any view's `scrollWheel:`. |
| Selection and keys | `shadowListKit_pressKey:keyCode:modifierFlags:`, `shadowListKit_clickItemAtIndex:modifierFlags:clickCount:`, `shadowListKit_moveMouseToPoint:`, `shadowListKit_mouseExited` | A key goes through `keyDown:` and the key bindings. A click lands at the middle of a shown item. |
| Menus | `shadowListKit_menuForItemAtIndex:modifierFlags:`, `shadowListKit_menuOutline` | A right click, or a Control-click with the Control flag. |
| Swipe actions | `shadowListKit_swipeItemAtIndex:distance:velocity:`, `shadowListKit_swipeWheelDelta:phase:momentumPhase:atPoint:`, `shadowListKit_runSwipeFramesFor:`, `shadowListKit_swipedIndex`, `shadowListKit_swipeOffset`, `shadowListKit_swipeActionsView` | The first is the UIKit kit's selector. `swipedIndex` is `NSNotFound` with no item open. |
| Drag reorder | `shadowListKit_beginDragAtPoint:`, `shadowListKit_moveDragToPoint:`, `shadowListKit_endDrag`, `shadowListKit_mouseDownAtPoint:`, `shadowListKit_mouseDragToPoint:`, `shadowListKit_mouseUpAtPoint:`, `shadowListKit_runDragFramesFor:`, `shadowListKit_heldIndex`, `shadowListKit_dropIndex` | The first three are the UIKit kit's selectors and lift at once. The mouse calls lift past the drag slop. |
| Accessibility | `shadowListKit_accessibilityNotificationHandler` | Hears every notification the list posts, with its user info. |
| Scroll engine | `shadowListKit_runFramesFor:` | On `ShadowListKitScrollEngine`. |
| Scrollers | `shadowListKit_verticalThumbFrame`, `shadowListKit_horizontalThumbFrame`, `shadowListKit_setScrollerStyle:`, `shadowListKit_hideScrollers` | On `ShadowListKitScrollerView`. A thumb's frame is `CGRectNull` while hidden. |

## Example app

[templates/shadowlist-appkit-example](../../templates/shadowlist-appkit-example) runs the kit in a Mac app.

```sh
cd templates/shadowlist-appkit-example && ./build.sh mac
```

`build.sh mac [Release|Debug]` generates the project with xcodegen and builds it. With
`SHADOWLIST_DEBUG_LOG=1` it compiles in the core's `[SL]` debug log. The app reads these launch arguments:

| Argument | Meaning |
| --- | --- |
| `-SLRoute` | `Feed`, the default: posts with separators, selection, menus, reorder and swipe actions. `Driven`: a host that moves a list on two axes with a scroll engine and scrollers the way a grid does. `Sections`: travellers under sticky section headers between a header and a footer view. |
| `-SLScenario` | A scripted check that logs one `[SLSCENARIO]` JSON line. See below. |
| `-SLBenchExit 1` | Quits after the scenario logs. |
| `-SLShot <path>` | Writes a PNG of the window's layers and quits. |
| `-SLCount` | The number of rows, 1000 by default. |
| `-SLTheme` | `light` or `dark`. |

| Scenario | Route | Checks |
| --- | --- | --- |
| `prepend` | `Feed` | Rows added at the start keep the visible rows still. |
| `append` | `Feed` | The same with rows added at the end. |
| `jump` | `Feed` | `scrollToItemAtIndex:viewPosition:animated:` to a far item lands at the viewport start. |
| `driven` | `Driven` | Drags on both axes, rows prepended mid drag, a settle, a rest and a jump. |
| `wheel` | `Feed` | Trackpad gestures with momentum, a bounce off each end, a mouse wheel, a gesture across the axis and animated commands. |
| `keys` | `Feed` | Clicks with modifiers, the arrows, Command arrows, Option page keys, Return, type select, Escape, emphasis and hover. |
| `menu` | `Feed` | A right click and a Control-click: the menu, `clickedIndex`, the menu target and its outline. |
| `reorder` | `Feed` | A press short of the slop and past it, the drop spot, a drop, the edge scroll and Escape. |
| `sections` | `Sections` | The section queries, self-sized rows, a scroll to a section, a pinned header pushed by the next one, overscroll and a regroup. |
| `animate` | `Feed` | A delete and an insert with `animatesChanges`: the fade and the slide. |
| `a11y` | `Feed` | The row count, a far row, scrolling it to visible, a page through the scroll bar, the selected rows and a post's actions. |
| `swipe` | `Feed` | Trackpad swipes: resting open and closed, a fling, both sides, clicks, Escape, a scroll, a mouse wheel, a full swipe, the menu fallback and the accessibility actions. |

```sh
open -W build/dd-mac/Build/Products/Release/ShadowListKitAppKitExample.app --args -SLRoute Feed -SLScenario swipe -SLBenchExit 1
```

With `keys`, `menu`, `reorder`, `sections` or `swipe`, `-SLShot` shoots the scenario's end state.
