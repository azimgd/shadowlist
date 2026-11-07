#import <ShadowListKit/ShadowListKitScrollerView.h>
#import <ShadowListKit/ShadowListKitListView+Testing.h>

#import <QuartzCore/QuartzCore.h>

#include <shadowlist-core/host/ScrollPhysics.hpp>

using namespace azimgd::shadowlist;

/*
 * The thumbs: drawn this thick, thicker under the pointer, this far in from the edges and never
 * shorter than the least length. A pointer this far from the edge a shown thumb runs along takes
 * its track. They fade out this long after the offset rests.
 */
static const CGFloat SHADOWLIST_KIT_THUMB_BREADTH = 7;
static const CGFloat SHADOWLIST_KIT_THUMB_HOVER_BREADTH = 11;
static const CGFloat SHADOWLIST_KIT_THUMB_INSET = 2;
static const CGFloat SHADOWLIST_KIT_THUMB_MIN_LENGTH = 36;
static const CGFloat SHADOWLIST_KIT_THUMB_GRAB_DEPTH = 15;
static const NSTimeInterval SHADOWLIST_KIT_THUMB_HIDE_DELAY = 0.8;
static const NSTimeInterval SHADOWLIST_KIT_THUMB_FADE_DURATION = 0.3;

/*
 * Which thumb a point takes.
 */
typedef NS_ENUM(NSInteger, ShadowListKitThumbAxis) {
  ShadowListKitThumbAxisNone,
  ShadowListKitThumbAxisVertical,
  ShadowListKitThumbAxisHorizontal,
};

@implementation ShadowListKitScrollerView {
  CALayer *_verticalThumb;
  CALayer *_horizontalThumb;
  NSTrackingArea *_tracking;

  ShadowListKitThumbAxis _hover;
  ShadowListKitThumbAxis _grab;
  CGFloat _grabOffset;

  /*
   * The scroller style scripted checks set, or -1 for the system's.
   */
  NSInteger _scrollerStyle;
}

- (instancetype)initWithFrame:(NSRect)frame
{
  if (self = [super initWithFrame:frame]) {
    self.wantsLayer = YES;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawNever;
    _axes = ShadowListKitScrollAxesVertical;
    _scrollerStyle = -1;
    _verticalThumb = [self makeThumb];
    _horizontalThumb = [self makeThumb];
    [self applyColors];
    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(scrollerStyleDidChange:)
                                               name:NSPreferredScrollerStyleDidChangeNotification
                                             object:nil];
  }
  return self;
}

- (void)dealloc
{
  [NSNotificationCenter.defaultCenter removeObserver:self];
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
}

- (CALayer *)makeThumb
{
  CALayer *thumb = [CALayer layer];
  thumb.opacity = 0;
  thumb.hidden = YES;
  thumb.actions = @{@"bounds" : NSNull.null, @"position" : NSNull.null, @"hidden" : NSNull.null,
    @"cornerRadius" : NSNull.null};
  [self.layer addSublayer:thumb];
  return thumb;
}

- (BOOL)isFlipped
{
  return YES;
}

#pragma mark - Properties

- (void)setAxes:(ShadowListKitScrollAxes)axes
{
  _axes = axes;
  [self updateThumbs];
}

- (void)setContentSize:(CGSize)contentSize
{
  if (CGSizeEqualToSize(contentSize, _contentSize)) {
    return;
  }
  _contentSize = contentSize;
  [self updateThumbs];
}

- (void)setOffset:(CGPoint)offset
{
  if (CGPointEqualToPoint(offset, _offset)) {
    return;
  }
  _offset = offset;
  [self updateThumbs];
  [self flashScrollers];
}

- (void)setScrollerInsets:(NSEdgeInsets)scrollerInsets
{
  _scrollerInsets = scrollerInsets;
  [self updateThumbs];
}

- (void)setFrameSize:(NSSize)newSize
{
  [super setFrameSize:newSize];
  [self updateThumbs];
}

/*
 * Whether the system shows scrollers at all times (Show scroll bars: Always).
 */
- (BOOL)showsScrollersAlways
{
  NSScrollerStyle style = _scrollerStyle >= 0 ? (NSScrollerStyle)_scrollerStyle : NSScroller.preferredScrollerStyle;
  return style == NSScrollerStyleLegacy;
}

- (void)scrollerStyleDidChange:(NSNotification *)notification
{
  [self updateThumbs];
  [self flashScrollers];
}

- (void)viewDidMoveToWindow
{
  [super viewDidMoveToWindow];
  if (self.showsScrollersAlways) {
    [self flashScrollers];
  }
}

#pragma mark - Showing

- (void)flashScrollers
{
  [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideScrollers) object:nil];
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  _verticalThumb.opacity = 1;
  _horizontalThumb.opacity = 1;
  [CATransaction commit];
  if (!self.showsScrollersAlways) {
    [self performSelector:@selector(hideScrollers) withObject:nil afterDelay:SHADOWLIST_KIT_THUMB_HIDE_DELAY];
  }
}

- (void)hideScrollers
{
  if (_grab != ShadowListKitThumbAxisNone || _hover != ShadowListKitThumbAxisNone || self.showsScrollersAlways) {
    return;
  }
  [CATransaction begin];
  [CATransaction setAnimationDuration:SHADOWLIST_KIT_THUMB_FADE_DURATION];
  _verticalThumb.opacity = 0;
  _horizontalThumb.opacity = 0;
  [CATransaction commit];
}

#pragma mark - Thumbs

- (ScrollerThumb)verticalThumb
{
  if ((_axes & ShadowListKitScrollAxesVertical) == 0) {
    return ScrollerThumb();
  }
  return scrollerThumb(_contentSize.height, [self trackLength:NO], _offset.y, _scrollerInsets.top,
    SHADOWLIST_KIT_THUMB_INSET, SHADOWLIST_KIT_THUMB_MIN_LENGTH);
}

- (ScrollerThumb)horizontalThumb
{
  if ((_axes & ShadowListKitScrollAxesHorizontal) == 0) {
    return ScrollerThumb();
  }
  return scrollerThumb(_contentSize.width, [self trackLength:YES], _offset.x, _scrollerInsets.left,
    SHADOWLIST_KIT_THUMB_INSET, SHADOWLIST_KIT_THUMB_MIN_LENGTH);
}

- (CGFloat)trackLength:(BOOL)horizontal
{
  NSSize size = self.bounds.size;
  return horizontal ? size.width - _scrollerInsets.left - _scrollerInsets.right
                    : size.height - _scrollerInsets.top - _scrollerInsets.bottom;
}

- (CGFloat)breadthOf:(ShadowListKitThumbAxis)axis
{
  return _hover == axis || _grab == axis ? SHADOWLIST_KIT_THUMB_HOVER_BREADTH : SHADOWLIST_KIT_THUMB_BREADTH;
}

/*
 * Place the thumbs where the offset says. Content past an edge shortens them.
 */
- (void)updateThumbs
{
  NSSize size = self.bounds.size;
  CGFloat inset = SHADOWLIST_KIT_THUMB_INSET;
  ScrollerThumb vertical = [self verticalThumb];
  ScrollerThumb horizontal = [self horizontalThumb];
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  _verticalThumb.hidden = !vertical.visible;
  if (vertical.visible) {
    CGFloat breadth = [self breadthOf:ShadowListKitThumbAxisVertical];
    _verticalThumb.frame = CGRectMake(size.width - _scrollerInsets.right - breadth - inset, (CGFloat)vertical.start,
      breadth, (CGFloat)vertical.length);
    _verticalThumb.cornerRadius = breadth / 2;
  }
  _horizontalThumb.hidden = !horizontal.visible;
  if (horizontal.visible) {
    CGFloat breadth = [self breadthOf:ShadowListKitThumbAxisHorizontal];
    _horizontalThumb.frame = CGRectMake((CGFloat)horizontal.start,
      size.height - _scrollerInsets.bottom - breadth - inset, (CGFloat)horizontal.length, breadth);
    _horizontalThumb.cornerRadius = breadth / 2;
  }
  [CATransaction commit];
}

- (BOOL)isShown:(CALayer *)thumb
{
  return !thumb.hidden && (thumb.opacity > 0.01 || self.showsScrollersAlways);
}

/*
 * The shown thumb whose track is under a point, in the view's coordinates. A track takes the
 * pointer within SHADOWLIST_KIT_THUMB_GRAB_DEPTH of the edge it runs along.
 */
- (ShadowListKitThumbAxis)thumbAt:(NSPoint)point
{
  NSSize size = self.bounds.size;
  CGFloat depth = SHADOWLIST_KIT_THUMB_GRAB_DEPTH;
  CGFloat right = size.width - _scrollerInsets.right;
  CGFloat bottom = size.height - _scrollerInsets.bottom;
  if ([self isShown:_verticalThumb] && point.x >= right - depth && point.x <= right &&
      point.y >= _scrollerInsets.top && point.y <= bottom) {
    return ShadowListKitThumbAxisVertical;
  }
  if ([self isShown:_horizontalThumb] && point.y >= bottom - depth && point.y <= bottom &&
      point.x >= _scrollerInsets.left && point.x <= right) {
    return ShadowListKitThumbAxisHorizontal;
  }
  return ShadowListKitThumbAxisNone;
}

/*
 * Only the shown tracks take the mouse.
 */
- (NSView *)hitTest:(NSPoint)point
{
  NSPoint local = [self convertPoint:point fromView:self.superview];
  return [self thumbAt:local] != ShadowListKitThumbAxisNone ? self : nil;
}

- (void)viewDidChangeEffectiveAppearance
{
  [super viewDidChangeEffectiveAppearance];
  [self applyColors];
}

- (void)applyColors
{
  [self.effectiveAppearance performAsCurrentDrawingAppearance:^{
    CGColorRef color = [NSColor.labelColor colorWithAlphaComponent:0.45].CGColor;
    self->_verticalThumb.backgroundColor = color;
    self->_horizontalThumb.backgroundColor = color;
  }];
}

#pragma mark - Mouse

- (void)updateTrackingAreas
{
  [super updateTrackingAreas];
  if (_tracking) {
    [self removeTrackingArea:_tracking];
  }
  NSTrackingAreaOptions options = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited |
    NSTrackingActiveInActiveApp | NSTrackingInVisibleRect;
  _tracking = [[NSTrackingArea alloc] initWithRect:NSZeroRect options:options owner:self userInfo:nil];
  [self addTrackingArea:_tracking];
}

/*
 * A shown thumb under the pointer grows wider.
 */
- (void)mouseMoved:(NSEvent *)event
{
  [self hoverAt:[self convertPoint:event.locationInWindow fromView:nil]];
}

- (void)mouseExited:(NSEvent *)event
{
  [self hoverAt:NSMakePoint(-1, -1)];
}

- (void)hoverAt:(NSPoint)point
{
  ShadowListKitThumbAxis hover = [self thumbAt:point];
  if (hover == _hover) {
    return;
  }
  _hover = hover;
  [self updateThumbs];
  if (hover == ShadowListKitThumbAxisNone) {
    [self flashScrollers];
  }
}

/*
 * A press on a thumb holds it where it pressed. On the track off it the thumb jumps to center
 * on the pointer, then follows it.
 */
- (void)mouseDown:(NSEvent *)event
{
  NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  _grab = [self thumbAt:point];
  if (_grab == ShadowListKitThumbAxisNone) {
    return;
  }
  BOOL vertical = _grab == ShadowListKitThumbAxisVertical;
  ScrollerThumb thumb = vertical ? [self verticalThumb] : [self horizontalThumb];
  _grabOffset = (CGFloat)scrollerGrabOffset(thumb, vertical ? point.y : point.x);
  [self updateThumbs];
  [self dragTo:point];
}

- (void)mouseDragged:(NSEvent *)event
{
  [self dragTo:[self convertPoint:event.locationInWindow fromView:nil]];
}

- (void)mouseUp:(NSEvent *)event
{
  if (_grab == ShadowListKitThumbAxisNone) {
    return;
  }
  _grab = ShadowListKitThumbAxisNone;
  [self hoverAt:[self convertPoint:event.locationInWindow fromView:nil]];
  [self updateThumbs];
  [self flashScrollers];
}

/*
 * The content goes where the thumb's place along its track says.
 */
- (void)dragTo:(NSPoint)point
{
  CGPoint offset = _offset;
  if (_grab == ShadowListKitThumbAxisVertical) {
    CGFloat track = [self trackLength:NO];
    offset.y = (CGFloat)scrollerDragOffset(point.y, _grabOffset, _scrollerInsets.top, SHADOWLIST_KIT_THUMB_INSET,
      track, _verticalThumb.bounds.size.height, MAX(0, _contentSize.height - track));
  } else if (_grab == ShadowListKitThumbAxisHorizontal) {
    CGFloat track = [self trackLength:YES];
    offset.x = (CGFloat)scrollerDragOffset(point.x, _grabOffset, _scrollerInsets.left, SHADOWLIST_KIT_THUMB_INSET,
      track, _horizontalThumb.bounds.size.width, MAX(0, _contentSize.width - track));
  } else {
    return;
  }
  [self flashScrollers];
  if (!CGPointEqualToPoint(offset, _offset)) {
    [_delegate scrollerView:self didScrollToOffset:offset];
  }
}

@end

#pragma mark - Testing

@implementation ShadowListKitScrollerView (Testing)

- (CGRect)shadowListKit_verticalThumbFrame
{
  return [self isShown:_verticalThumb] ? _verticalThumb.frame : CGRectNull;
}

- (CGRect)shadowListKit_horizontalThumbFrame
{
  return [self isShown:_horizontalThumb] ? _horizontalThumb.frame : CGRectNull;
}

- (void)shadowListKit_setScrollerStyle:(NSScrollerStyle)style
{
  _scrollerStyle = style;
  [self scrollerStyleDidChange:nil];
}

- (void)shadowListKit_hideScrollers
{
  [self hideScrollers];
  [CATransaction flush];
}

@end
