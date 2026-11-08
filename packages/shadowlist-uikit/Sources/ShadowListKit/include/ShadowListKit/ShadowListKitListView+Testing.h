#pragma once

#import <ShadowListKit/ShadowListKitListView.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * A swipe moves a shown item's row distance points across the axis, positive toward the trailing
 * side, and lets go with velocity, the way the AppKit kit does.
 */
@interface ShadowListKitListView (ActionsTesting)

- (void)shadowListKit_swipeItemAtIndex:(NSInteger)index distance:(CGFloat)distance velocity:(CGFloat)velocity;

@end

/*
 * Drive a drag without touches, for scripted checks. Points are in the list's coordinates, the
 * same as a gesture's locationInView: on the list. The calls pick a row up at once, move it and
 * drop it, the way the AppKit kit does.
 */
@interface ShadowListKitListView (DragTesting)

- (void)shadowListKit_beginDragAtPoint:(CGPoint)point;
- (void)shadowListKit_moveDragToPoint:(CGPoint)point;
- (void)shadowListKit_endDrag;

@end

NS_ASSUME_NONNULL_END
