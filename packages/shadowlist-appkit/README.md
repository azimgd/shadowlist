# ShadowListKit for AppKit

ShadowListKit for AppKit is the Mac counterpart of [ShadowListKit for UIKit](../shadowlist-uikit/README.md),
with the same class names and selectors on the shared C++ core. It is in progress: it has the list
view with its data changes, sections with sticky headers, header and footer views, scroll
commands, saved position and separators, its scrolling, selection with the mouse and the keyboard,
context menus, drag reorder, the text layout and the model classes. Change animations and
accessibility come next.

`ShadowListKitListView` is a flipped, layer backed `NSView` that owns its scroll position, with no
`NSScrollView`. Its cells sit in a content view moved by `contentOffset`. With `scrollDriven` a host
moves it instead: the host sets `contentOffset` and `scrollPhase`, and the list reports the offsets
it wants, corrections that keep the visible items still and scroll command targets, to the
delegate's `listView:didCorrectOffset:`.

## Scrolling

`ShadowListKitScrollEngine` scrolls a view that owns its scroll position the way a scroll view does,
on one axis or two, with the physics in the core's `host/ScrollPhysics`:

- A trackpad gesture drags from its first movement and rubber bands past the edges. On two axes a
  gesture that starts within 30 degrees of an axis moves that axis only. On one axis a gesture that
  starts across it is passed on whole, momentum included.
- The system's momentum settles it, and momentum that reaches an edge goes at most 12% of the
  viewport past it before it springs back.
- A mouse wheel scrolls by lines and stops at the edges. One that moves nothing is passed on.
- `animateToOffset:` eases to an offset. A gesture coming to rest can snap through the delegate.

`ShadowListKitScrollerView` draws overlay scrollers over such a view: shown while it scrolls and at
all times when Show scroll bars is Always, wider under the pointer, with a draggable thumb and a
track that moves the thumb to a click.

The list uses both unless `scrollDriven` is set. Its animated commands ease toward their estimated
target and the core lands them exactly, and `snapToItem` rests it on an item edge. A host that
drives a list, like a two axis grid, runs its own engine and scrollers and hands the list its
offset and phase.

## Selection and menus

The list selects the way `NSTableView` does while `allowsSelection` is set:

- A click selects when the mouse goes down, Command-click adds or removes an item and Shift-click
  with `allowsMultipleSelection` selects from the anchor, the last item clicked. A double-click,
  Return and Space call the delegate's `listView:didActivateItemAtIndex:`.
- The list takes the first responder on a click. The arrow keys move the selection, with Shift
  they extend it, Command-arrows go to the ends and Option-Page keys a page. Home, End and the
  page keys scroll. Command-A selects all and Escape deselects. Typing selects by the delegate's
  `listView:typeSelectStringForItemAtIndex:`, for items shown or not.
- Cells get `highlighted` during a press, `hovered` under a resting pointer, `emphasized` while
  the list is the key window's first responder and `menuTarget` while their menu is open.

A right click, a Control-click or the menu key shows the delegate's
`listView:contextMenuForItemAtIndex:` menu. While it is open `clickedIndex` is the item and the
list draws an outline around it. A host that selects by itself, like a grid over a driven list,
turns `allowsSelection` off: the list then takes no first responder, tracks no pointer and passes
mouse and key events on.

## Sections

Sections come from the same data source calls as the UIKit kit. A section with only a title shows
it in a built-in cell: the headline font over the window background for a header, the small system
font for a footer, both in the secondary label color and redrawn when the appearance changes.
`stickySectionHeaders` pins the header of the section at the top, `stickyIndices` pins items, and
the next pinned row pushes the current one up, with the core's `ListDriver` placing them. Pinned
rows sit over the other rows and under a held one, follow a driven list's offset, go with the
content while it rubber bands past the start and stay pinned past the end. `headerView` and
`footerView` sit before the first row and after the last, and the item rects and scroll commands
count them. There is no section index on the Mac.

## Drag reorder

With `reorderEnabled` an item lifts as soon as the mouse drags it 3 points, without the hold a
finger needs on iOS. A shorter press stays a click. The held row follows the mouse a little bigger
and with a shadow, the other rows slide aside to open the gap, and the list scrolls within 90
points of an edge at up to 960 points a second. The trackpad ticks as the drop spot moves, with a
Force Touch trackpad. Escape puts the row back. The drop calls the delegate's
`listView:moveItemAtIndex:toIndex:`, after `listView:canMoveItemAtIndex:` allowed the pickup, the
same calls as the UIKit kit. It works in grids and without `allowsSelection`, and in a list with
sections an item stays in its own section. A driven list scrolls near the edges through
`listView:didCorrectOffset:` like its other offsets. The drag math is the core's `host/DragReorder`.

## Install

Swift Package Manager: add `https://github.com/azimgd/shadowlist` and the ShadowListKitAppKit product.
Then `import ShadowListKitAppKit` in Swift, or `#import <ShadowListKit/ShadowListKit.h>` in Objective-C.

CocoaPods: the ShadowListKit pod builds this kit on macOS.

### Requirements

| Dependency | Version |
| --- | --- |
| macOS | 14 or newer |

## Layout

The files the two kits share live in the UIKit kit and are linked here: the key and index
conversions (`ShadowListKitCoreBridge`), the platform class names (`ShadowListKitPlatform.h`), the
cell's private header and the model classes (`ShadowListKitListModels.mm`). Each compiles once in
each kit.

## Tests

```sh
cd packages/shadowlist-appkit && swift test
```

The demo app in [templates/shadowlist-appkit-example](../../templates/shadowlist-appkit-example)
runs scripted checks with `-SLScenario`: `prepend`, `append`, `jump`, `wheel`, `keys`, `menu` and
`reorder` on the feed, and `driven` with `-SLRoute Driven`, a host that moves a list on two axes with a
scroll engine and scrollers the way a grid does.
