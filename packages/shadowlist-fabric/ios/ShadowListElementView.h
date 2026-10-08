#pragma once

#import <React/RCTViewComponentView.h>
#import "ShadowListCompat.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * nativeAccessibilityActions are native VoiceOver actions, like the drag Move up and Move down,
 * added after any from props. RCTViewComponentView builds accessibilityCustomActions from props
 * and ignores the setter. closeSwipeActionsAnimated: slides a swiped row back. swipeOpen is a row
 * showing its swipe actions. swipedOut is a row slid all the way out by a full swipe, waiting for
 * its removal.
 */
@interface ShadowListElementView : RCTViewComponentView
@property (nonatomic, copy, nullable) NSArray<SLAccessibilityCustomAction *> *nativeAccessibilityActions;
@property (nonatomic, readonly, getter=isSwipeOpen) BOOL swipeOpen;
@property (nonatomic, readonly, getter=isSwipedOut) BOOL swipedOut;
- (void)closeSwipeActionsAnimated:(BOOL)animated;
@end

NS_ASSUME_NONNULL_END
