#pragma once

#import <ShadowListKit/ShadowListKitListView.h>
#import <ShadowListKit/ShadowListKitScrollEngine.h>
#import <ShadowListKit/ShadowListKitScrollerView.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * Drive scrolling without a trackpad, for scripted checks. A wheel event carries delta the way
 * NSEvent's scrollingDeltaX and scrollingDeltaY do, in points with precise deltas, for any view's
 * scrollWheel:. Frames run at once instead of on the display link, the clock moving ahead with
 * them.
 */
FOUNDATION_EXPORT NSEvent *ShadowListKitScrollWheelEvent(CGPoint delta, NSEventPhase phase, NSEventPhase momentumPhase);

/*
 * The list's scrollers, a wheel event sent through scrollWheel: and laid out, and frames of its
 * engine with a layout each.
 */
@interface ShadowListKitListView (Testing)

@property (nonatomic, readonly) ShadowListKitScrollerView *shadowListKit_scrollerView;

- (void)shadowListKit_scrollWheelDelta:(CGPoint)delta phase:(NSEventPhase)phase momentumPhase:(NSEventPhase)momentumPhase;
- (void)shadowListKit_runScrollFramesFor:(NSTimeInterval)duration;

@end

@interface ShadowListKitScrollEngine (Testing)

- (void)shadowListKit_runFramesFor:(NSTimeInterval)duration;

@end

/*
 * The thumbs' frames while shown, CGRectNull while hidden, and the scroller style the system
 * setting would give.
 */
@interface ShadowListKitScrollerView (Testing)

@property (nonatomic, readonly) CGRect shadowListKit_verticalThumbFrame;
@property (nonatomic, readonly) CGRect shadowListKit_horizontalThumbFrame;

- (void)shadowListKit_setScrollerStyle:(NSScrollerStyle)style;
- (void)shadowListKit_hideScrollers;

@end

NS_ASSUME_NONNULL_END
