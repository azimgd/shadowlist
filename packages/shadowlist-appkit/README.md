# ShadowListKit for AppKit

ShadowListKit for AppKit is the Mac counterpart of [ShadowListKit for UIKit](../shadowlist-uikit/README.md),
with the same class names and selectors on the shared C++ core. It is in progress: it has the list
view with its data changes, sections, scroll commands, saved position and separators, the text
layout and the model classes. Momentum, scrollers, selection, menus, drag reorder, sticky rows,
change animations and accessibility come next.

`ShadowListKitListView` is a flipped, layer backed `NSView` that owns its scroll position, with no
`NSScrollView`. Its cells sit in a content view moved by `contentOffset`. With `scrollDriven` a host
moves it instead: the host sets `contentOffset` and `scrollPhase`, and the list reports the offsets
it wants, corrections that keep the visible items still and scroll command targets, to the
delegate's `listView:didCorrectOffset:`.

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
