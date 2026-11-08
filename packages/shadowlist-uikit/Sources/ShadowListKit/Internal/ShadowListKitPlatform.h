#pragma once

#import <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

/*
 * The platform's color and image classes, for code both kits compile.
 */
#if TARGET_OS_OSX
typedef NSColor ShadowListKitColor;
typedef NSImage ShadowListKitImage;
#else
typedef UIColor ShadowListKitColor;
typedef UIImage ShadowListKitImage;
#endif

/*
 * The platform's view and edge insets, for code both kits compile.
 */
#if TARGET_OS_OSX
typedef NSView ShadowListKitView;
typedef NSEdgeInsets ShadowListKitEdgeInsets;
#else
typedef UIView ShadowListKitView;
typedef UIEdgeInsets ShadowListKitEdgeInsets;
#endif

/*
 * Lay a view out now if it waits for a layout pass.
 */
static inline void ShadowListKitLayoutNow(ShadowListKitView *view)
{
#if TARGET_OS_OSX
  [view layoutSubtreeIfNeeded];
#else
  [view layoutIfNeeded];
#endif
}

/*
 * Ask for a layout pass of a view.
 */
static inline void ShadowListKitSetNeedsLayout(ShadowListKitView *view)
{
#if TARGET_OS_OSX
  view.needsLayout = YES;
#else
  [view setNeedsLayout];
#endif
}
