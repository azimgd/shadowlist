#import <ShadowListKit/ShadowListKitListView.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * The buttons behind a swiped row, for both sides. It sits under the row's cell with the
 * cell's resting frame. The side being revealed shows its buttons stretched over the gap the
 * row leaves. Past the full swipe point the first button fills all of it.
 */
@interface ShadowListKitSwipeActionsView : UIView
@property (nonatomic, strong, nullable) ShadowListKitSwipeActionsConfiguration *leading;
@property (nonatomic, strong, nullable) ShadowListKitSwipeActionsConfiguration *trailing;
@property (nonatomic) BOOL horizontal;
@property (nonatomic, copy, nullable) void (^onAction)(ShadowListKitSwipeAction *action);

- (instancetype)initWithLeading:(nullable ShadowListKitSwipeActionsConfiguration *)leading
                       trailing:(nullable ShadowListKitSwipeActionsConfiguration *)trailing
                     horizontal:(BOOL)horizontal;
- (CGFloat)leadingWidth;
- (CGFloat)trailingWidth;
- (void)layoutForOffset:(CGFloat)offset full:(BOOL)full;
- (BOOL)hasButtonAtPoint:(CGPoint)point;

@end

NS_ASSUME_NONNULL_END
