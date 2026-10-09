#import <ShadowListKit/ShadowListKitListView.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * Drive a drag or a swipe without touches, for scripted checks. Points are in the list's
 * coordinates, the same as a gesture's locationInView: on the list. A swipe moves the item's row
 * distance points across the axis, positive toward the trailing side, and lets go with velocity.
 */
@interface ShadowListKitListView (Testing)

- (void)shadowListKit_beginDragAtPoint:(CGPoint)point;
- (void)shadowListKit_moveDragToPoint:(CGPoint)point;
- (void)shadowListKit_endDrag;
- (void)shadowListKit_swipeItemAtIndex:(NSInteger)index distance:(CGFloat)distance velocity:(CGFloat)velocity;

@end

NS_ASSUME_NONNULL_END
