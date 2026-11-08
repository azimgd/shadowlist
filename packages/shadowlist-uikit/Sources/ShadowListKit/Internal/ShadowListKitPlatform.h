#pragma once

#import <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif
#import <QuartzCore/QuartzCore.h>

/*
 * The platform's color, image and font classes, for code both kits compile.
 */
#if TARGET_OS_OSX
typedef NSColor ShadowListKitColor;
typedef NSImage ShadowListKitImage;
typedef NSFont ShadowListKitFont;
#else
typedef UIColor ShadowListKitColor;
typedef UIImage ShadowListKitImage;
typedef UIFont ShadowListKitFont;
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

/*
 * A display link on the display that shows a view, calling a target every frame.
 */
static inline CADisplayLink *ShadowListKitDisplayLink(ShadowListKitView *view, id target, SEL selector)
{
#if TARGET_OS_OSX
  return [view displayLinkWithTarget:target selector:selector];
#else
  return [CADisplayLink displayLinkWithTarget:target selector:selector];
#endif
}
