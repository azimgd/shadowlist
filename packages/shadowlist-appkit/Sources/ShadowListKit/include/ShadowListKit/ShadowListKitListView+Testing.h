#pragma once

#import <ShadowListKit/ShadowListKitListView.h>
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

/*
 * The list's scrollers, a wheel event sent through scrollWheel: and laid out, and frames of its
 * engine with a layout each. The counts are the layout passes, the runs of the core and the
 * mount passes since the list was made. viewsBehind says a driven list moved its content or
 * cells by their layers and their views have not caught up yet.
 */
@interface ShadowListKitListView (Testing)

@property (nonatomic, readonly) ShadowListKitScrollerView *shadowListKit_scrollerView;
@property (nonatomic, readonly) NSUInteger shadowListKit_layoutPassCount;
@property (nonatomic, readonly) NSUInteger shadowListKit_corePassCount;
@property (nonatomic, readonly) NSUInteger shadowListKit_mountPassCount;
@property (nonatomic, readonly) BOOL shadowListKit_viewsBehind;

- (void)shadowListKit_scrollWheelDelta:(CGPoint)delta phase:(NSEventPhase)phase momentumPhase:(NSEventPhase)momentumPhase;
- (void)shadowListKit_runScrollFramesFor:(NSTimeInterval)duration;

@end

/*
 * Mouse and key events sent to the list, laid out after each. A key goes through keyDown: and
 * the key bindings. A click is a mouse down and up at the middle of a shown item, a move puts
 * the pointer at a point in the list's coordinates and an exit takes it off the list.
 */
@interface ShadowListKitListView (SelectionTesting)

- (void)shadowListKit_pressKey:(NSString *)characters keyCode:(unsigned short)keyCode modifierFlags:(NSEventModifierFlags)flags;
- (void)shadowListKit_clickItemAtIndex:(NSInteger)index modifierFlags:(NSEventModifierFlags)flags clickCount:(NSInteger)clickCount;
- (void)shadowListKit_moveMouseToPoint:(CGPoint)point;
- (void)shadowListKit_mouseExited;

@end

/*
 * The menu a right click on a shown item gets, or a Control-click with the Control flag, asked
 * through the item's cell. The outline drawn over the menu target while a menu is open, or nil.
 * A swipe moves a shown item's row distance points across the axis and lets go with velocity,
 * the way UIKit's kit does. A swipe wheel event goes through scrollWheel: with the pointer at a
 * point in the list's coordinates, laid out after. Settle frames run at once instead of on the
 * display link. swipedIndex is the item whose actions are open, or NSNotFound, swipeOffset how
 * far its row shows moved and swipeActionsView the view of its buttons.
 */
@interface ShadowListKitListView (ActionsTesting)

@property (nonatomic, readonly, nullable) CALayer *shadowListKit_menuOutline;
@property (nonatomic, readonly) NSInteger shadowListKit_swipedIndex;
@property (nonatomic, readonly) CGFloat shadowListKit_swipeOffset;
@property (nonatomic, readonly, nullable) NSView *shadowListKit_swipeActionsView;

- (nullable NSMenu *)shadowListKit_menuForItemAtIndex:(NSInteger)index modifierFlags:(NSEventModifierFlags)flags;
- (void)shadowListKit_swipeItemAtIndex:(NSInteger)index distance:(CGFloat)distance velocity:(CGFloat)velocity;
- (void)shadowListKit_swipeWheelDelta:(CGPoint)delta
                                phase:(NSEventPhase)phase
                        momentumPhase:(NSEventPhase)momentumPhase
                              atPoint:(CGPoint)point;
- (void)shadowListKit_runSwipeFramesFor:(NSTimeInterval)duration;

@end

/*
 * Drive a drag without a mouse, for scripted checks. Points are in the list's coordinates. The
 * first three pick a row up at once, move it and drop it, the way UIKit's kit does. The mouse
 * calls send a press, a drag and a release through mouseDown:, mouseDragged: and mouseUp:, laid
 * out after each, which lift the row only past the drag slop. Auto scroll runs frames of the
 * display link at once. heldIndex is the held item and dropIndex the item it would drop at, or
 * NSNotFound without a held row.
 */
@interface ShadowListKitListView (DragTesting)

@property (nonatomic, readonly) NSInteger shadowListKit_heldIndex;
@property (nonatomic, readonly) NSInteger shadowListKit_dropIndex;

- (void)shadowListKit_beginDragAtPoint:(CGPoint)point;
- (void)shadowListKit_moveDragToPoint:(CGPoint)point;
- (void)shadowListKit_endDrag;
- (void)shadowListKit_mouseDownAtPoint:(CGPoint)point;
- (void)shadowListKit_mouseDragToPoint:(CGPoint)point;
- (void)shadowListKit_mouseUpAtPoint:(CGPoint)point;
- (void)shadowListKit_runDragFramesFor:(NSTimeInterval)duration;

@end

/*
 * Hears every accessibility notification the list posts, with its user info, as it is posted.
 * A selection change is posted once the turn it was made in ends.
 */
@interface ShadowListKitListView (AccessibilityTesting)

@property (nonatomic, copy, nullable) void (^shadowListKit_accessibilityNotificationHandler)(NSAccessibilityNotificationName name, NSDictionary *_Nullable userInfo);

@end

/*
 * Frames of the engine run at once instead of on the display link.
 */
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
