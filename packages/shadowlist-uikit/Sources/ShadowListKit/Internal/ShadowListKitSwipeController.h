#pragma once

#import "ShadowListKitSwipeHost.h"

NS_ASSUME_NONNULL_BEGIN

/* Shared by list cells and standalone item actions. No item indices live in this controller. */
@interface ShadowListKitSwipeController : NSObject <UIGestureRecognizerDelegate>
- (instancetype)initWithHost:(id<ShadowListKitSwipeHost>)host;
@property (nonatomic, readonly) UIPanGestureRecognizer *pan;
@property (nonatomic, readonly, nullable) UIView *content;
@property (nonatomic, readonly, getter=isOpen) BOOL open;
- (BOOL)isSwipedOutContent:(UIView *)content;
- (BOOL)takesPan:(UIPanGestureRecognizer *)pan;
- (void)scriptSwipeOfContent:(UIView *)content distance:(CGFloat)distance velocity:(CGFloat)velocity;
- (void)closeAnimated:(BOOL)animated;
- (void)reset;
- (void)contentWillRecycle:(UIView *)content;
- (void)layout;
- (BOOL)ownsTapAtPoint:(CGPoint)point;
- (BOOL)hasButtonAtPoint:(CGPoint)point;
- (NSArray<UIAccessibilityCustomAction *> *)accessibilityActionsForContent:(UIView *)content;
@end

NS_ASSUME_NONNULL_END
