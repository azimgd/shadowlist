# ShadowListKit for AppKit

ShadowListKit for AppKit is the Mac counterpart of [ShadowListKit for UIKit](../shadowlist-uikit/README.md),
with the same class names and selectors on the shared C++ core. It is in progress: it has the text
layout and the model classes so far, and the list view comes next.

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
conversions (`ShadowListKitCoreBridge`), the platform class names (`ShadowListKitPlatform.h`) and
the model classes (`ShadowListKitListModels.mm`). Each compiles once in each kit.

## Tests

```sh
cd packages/shadowlist-appkit && swift test
```
