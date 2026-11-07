#pragma once

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
