#pragma once

#import <AppKit/AppKit.h>

#import <ShadowListKit/ShadowListKitListView.h>

NS_ASSUME_NONNULL_BEGIN

@class ShadowListKitScrollEngine;

/*
 * The axes a scroll engine or a scroller view moves along.
 */
typedef NS_OPTIONS(NSUInteger, ShadowListKitScrollAxes) {
  ShadowListKitScrollAxesHorizontal = 1 << 0,
  ShadowListKitScrollAxesVertical = 1 << 1,
};

NS_SWIFT_UI_ACTOR
@protocol ShadowListKitScrollEngineDelegate <NSObject>

/*
 * The engine moved the offset, from a wheel event, a frame or stopScrolling. Rubber banding
 * puts it past the content's edges.
 */
- (void)scrollEngine:(ShadowListKitScrollEngine *)engine didScrollToOffset:(CGPoint)offset NS_SWIFT_NAME(scrollEngine(_:didScrollTo:));

@optional

/*
 * The phase changed. A phase that starts is reported before the move that comes with it, and a
 * phase that ends after it.
 */
- (void)scrollEngine:(ShadowListKitScrollEngine *)engine didChangePhase:(ShadowListKitScrollPhase)phase;

/*
 * An animation started with animateToOffset: reached its target. One that a gesture,
 * stopScrolling or another animation cut short does not end here.
 */
- (void)scrollEngineDidEndAnimation:(ShadowListKitScrollEngine *)engine;

/*
 * Where a gesture coming to rest at offset rests instead, like the nearest item edge. The engine
 * eases there and stays settling until it arrives.
 */
- (CGPoint)scrollEngine:(ShadowListKitScrollEngine *)engine restingOffsetForOffset:(CGPoint)offset NS_SWIFT_NAME(scrollEngine(_:restingOffsetFor:));

@end

/*
 * Scrolls a view on one or two axes the way AppKit's scroll views do, for a view that owns its
 * scroll position. The host hands it scroll wheel events and the content and viewport sizes, and
 * places its content wherever the delegate's offset says. handleScrollWheel: returns whether the
 * engine took the event.
 *
 * - A trackpad gesture is dragging from its first movement and rubber bands past the edges. With
 *   two axes, a gesture that starts within the core's SCROLL_LOCK_DEGREES of an axis moves only
 *   that axis. With one axis, a gesture that starts more across it than along it is not handled,
 *   its momentum included, and the host passes it to the next responder.
 * - The momentum the system sends after a gesture is settling. Momentum that reaches an edge goes
 *   a little way past it and springs back.
 * - A mouse wheel moves lineScroll points a line and stops at the edges. A wheel that moves
 *   nothing is not handled.
 * - animateToOffset: eases to an offset in the idle phase.
 * - stopScrolling stops momentum, a spring back and an animation inside the content's edges. The
 *   rest of the current gesture's momentum is dropped. Fingers on the trackpad keep their
 *   gesture.
 *
 * Offsets run from 0 at the content start to the content size less the viewport size. offset
 * moves the content without a delegate call, like a correction, and a gesture, momentum or an
 * animation goes on from there. Frames come from the view's display link and run only while
 * something moves.
 */
NS_SWIFT_UI_ACTOR
@interface ShadowListKitScrollEngine : NSObject

@property (nonatomic, weak, nullable) id<ShadowListKitScrollEngineDelegate> delegate;
@property (nonatomic) ShadowListKitScrollAxes axes;
@property (nonatomic) CGPoint offset;
@property (nonatomic) CGSize contentSize;
@property (nonatomic) CGSize viewportSize;
@property (nonatomic, readonly) CGPoint maxOffset;
@property (nonatomic) CGFloat lineScroll;
@property (nonatomic, readonly) ShadowListKitScrollPhase phase;
@property (nonatomic, readonly, getter=isAnimating) BOOL animating;

- (instancetype)initWithView:(NSView *)view NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (BOOL)handleScrollWheel:(NSEvent *)event;

- (void)animateToOffset:(CGPoint)offset NS_SWIFT_NAME(animate(to:));
- (void)stopScrolling;

@end

NS_ASSUME_NONNULL_END
