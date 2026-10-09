#pragma once

#import "ShadowListCompat.h"

#if TARGET_OS_OSX
NS_ASSUME_NONNULL_BEGIN

@protocol ShadowListMacScrollDelegate <RCTUIScrollViewDelegate>
- (void)shadowListScrollWillBegin;
- (void)shadowListDragDidEnd;
- (void)shadowListMomentumWillBegin;
- (void)shadowListScrollDidEnd;
@end

typedef NS_ENUM(NSInteger, ShadowListMacScrollPhase) {
  ShadowListMacScrollPhaseIdle,
  ShadowListMacScrollPhaseTracking,
  ShadowListMacScrollPhaseMomentum,
};

/*
 * AppKit's scroll lifecycle is not exposed by RCTUIScrollViewDelegate. horizontal is the
 * scroll axis. A wheel gesture mostly along the other axis goes to the enclosing scroll view,
 * which is how a horizontal shelf inside a vertical list lets the page scroll.
 */
@interface ShadowListMacScrollView : RCTUIScrollView
@property (nonatomic, weak, nullable) id<ShadowListMacScrollDelegate> delegate;
@property (nonatomic, readonly) ShadowListMacScrollPhase phase;
@property (nonatomic) BOOL horizontal;
- (BOOL)stopMomentum;
- (void)resetScroll;
@end

NS_ASSUME_NONNULL_END
#endif
