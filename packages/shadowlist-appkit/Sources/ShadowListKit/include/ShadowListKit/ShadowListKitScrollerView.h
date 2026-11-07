#pragma once

#import <AppKit/AppKit.h>

#import <ShadowListKit/ShadowListKitScrollEngine.h>

NS_ASSUME_NONNULL_BEGIN

@class ShadowListKitScrollerView;

NS_SWIFT_UI_ACTOR
@protocol ShadowListKitScrollerViewDelegate <NSObject>

/*
 * The pointer dragged a thumb or clicked a track. The host moves its content to offset.
 */
- (void)scrollerView:(ShadowListKitScrollerView *)scrollerView didScrollToOffset:(CGPoint)offset NS_SWIFT_NAME(scrollerView(_:didScrollTo:));

@optional

/*
 * An assistive app moved a scroller by a page or to a value, after
 * scrollerView:didScrollToOffset:.
 */
- (void)scrollerViewDidScrollForAccessibility:(ShadowListKitScrollerView *)scrollerView;

@end

/*
 * Overlay scrollers for a view that owns its scroll position, drawn as thumbs over the content
 * the way AppKit's overlay scrollers are. The host lays it over the scrolling area, sets the
 * content size and the offset on every move, and moves its content when the delegate says.
 *
 * - The thumbs show while the offset changes and fade out after it rests, unless the system's
 *   Show scroll bars setting is Always, which keeps them shown.
 * - A thumb under the pointer grows wider. Dragging it scrolls, and a click on its track moves
 *   the thumb's middle there and follows the pointer from there.
 * - Only the tracks take the mouse. Everything else goes to the views below.
 * - scrollerInsets keeps the tracks off the view's edges, like under a header. The track along
 *   each axis is the view's size less the insets, and the content shows that much at a time.
 * - Each track that can scroll is a scroll bar to accessibility, with the offset as a value from
 *   0 to 1. An assistive app sets the value or steps it a page at a time.
 */
NS_SWIFT_UI_ACTOR
@interface ShadowListKitScrollerView : NSView

@property (nonatomic, weak, nullable) id<ShadowListKitScrollerViewDelegate> delegate;
@property (nonatomic) ShadowListKitScrollAxes axes;
@property (nonatomic) CGSize contentSize;
@property (nonatomic) CGPoint offset;
@property (nonatomic) NSEdgeInsets scrollerInsets;
@property (nonatomic, readonly) BOOL showsScrollersAlways;

- (void)flashScrollers;

@end

NS_ASSUME_NONNULL_END
