#import <ShadowListKit/SLKListView.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * Drive a drag or a swipe without touches, for scripted checks. Points are in the list's
 * coordinates, the same as a gesture's locationInView: on the list. A swipe moves the item's row
 * distance points across the axis, positive toward the trailing side, and lets go with velocity.
 */
@interface SLKListView (Testing)

- (void)slk_beginDragAtPoint:(CGPoint)point;
- (void)slk_moveDragToPoint:(CGPoint)point;
- (void)slk_endDrag;
- (void)slk_swipeItemAtIndex:(NSInteger)index distance:(CGFloat)distance velocity:(CGFloat)velocity;

@end

NS_ASSUME_NONNULL_END
