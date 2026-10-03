#pragma once
#import "ShadowListCompat.h"

#if TARGET_OS_OSX
@protocol ShadowListMacScrollDelegate <RCTUIScrollViewDelegate>
- (void)shadowListScrollWillBegin;
- (void)shadowListScrollDidEnd;
- (void)shadowListRefresh;
@end

typedef NS_ENUM(NSInteger, ShadowListMacScrollPhase) {
  ShadowListMacScrollPhaseIdle,
  ShadowListMacScrollPhaseTracking,
  ShadowListMacScrollPhaseMomentum,
};

// AppKit's scroll lifecycle is not exposed by RCTUIScrollViewDelegate.
@interface ShadowListMacScrollView : RCTUIScrollView
@property (nonatomic, weak) id<ShadowListMacScrollDelegate> delegate;
@property (nonatomic, readonly) ShadowListMacScrollPhase phase;
/*
 * The scroll axis. A wheel gesture mostly along the other axis goes to the enclosing scroll
 * view, which is how a horizontal shelf inside a vertical list lets the page scroll.
 */
@property (nonatomic) BOOL horizontal;
- (BOOL)stopMomentum;
- (void)resetScroll;
@end
#endif
