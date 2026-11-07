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
 * engine with a layout each.
 */
@interface ShadowListKitListView (Testing)

@property (nonatomic, readonly) ShadowListKitScrollerView *shadowListKit_scrollerView;

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
 */
@interface ShadowListKitListView (ActionsTesting)

@property (nonatomic, readonly, nullable) CALayer *shadowListKit_menuOutline;

- (nullable NSMenu *)shadowListKit_menuForItemAtIndex:(NSInteger)index modifierFlags:(NSEventModifierFlags)flags;

@end

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
