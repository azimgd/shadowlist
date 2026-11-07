# ShadowListKit for AppKit

ShadowListKit for AppKit is the Mac counterpart of [ShadowListKit for UIKit](../shadowlist-uikit/README.md),
with the same class names and selectors on the shared C++ core. It is in progress: it has the list
view with its data changes, sections, scroll commands, saved position and separators, its
scrolling, the text layout and the model classes. Selection, menus, drag reorder, sticky rows,
change animations and accessibility come next.

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
runs scripted checks with `-SLScenario`: `prepend`, `append`, `jump` and `wheel` on the feed, and
`driven` with `-SLRoute Driven`, a host that moves a list on two axes with a scroll engine and
scrollers the way a grid does.
