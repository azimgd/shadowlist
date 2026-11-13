#pragma once

#import <UIKit/UIKit.h>
#import <ShadowListKit/ShadowListKitListModels.h>

NS_ASSUME_NONNULL_BEGIN

/* The host resolves current content and policy; the controller owns the reveal session. */
@protocol ShadowListKitSwipeHost <NSObject>
@property (nonatomic, readonly) UIView *swipeRootView;
@property (nonatomic, readonly, getter=isHorizontal) BOOL horizontal;
- (nullable UIView *)swipeContentAtPoint:(CGPoint)point;
- (BOOL)isSwipeContentValid:(UIView *)content;
- (nullable ShadowListKitSwipeActionsConfiguration *)leadingActionsForContent:(UIView *)content;
- (nullable ShadowListKitSwipeActionsConfiguration *)trailingActionsForContent:(UIView *)content;
- (void)unhighlightSwipeContent:(UIView *)content;
@end

NS_ASSUME_NONNULL_END
