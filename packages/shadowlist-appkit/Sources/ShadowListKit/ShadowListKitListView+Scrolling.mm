#import "Internal/ShadowListKitListView+Private.h"

#import <ShadowListKit/ShadowListKitListView+Testing.h>

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewScrollingLink = 0;

// The class interface in ShadowListKitListView.h declares the public members implemented here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"

/*
 * The wheel, the scrollers and animations move a list that is not driven. The engine reports
 * offsets along the axis from the content start, the same offsets the core runs on.
 */
@implementation ShadowListKitListView (Scrolling)

#pragma mark - Engine

- (void)setUpScrolling
{
  _engine = [[ShadowListKitScrollEngine alloc] initWithView:self];
  _engine.delegate = self;
  _scrollers = [[ShadowListKitScrollerView alloc] initWithFrame:self.bounds];
  _scrollers.delegate = self;
  [self addSubview:_scrollers];
}

- (CGPoint)enginePointAt:(double)offset
{
  return _horizontal ? CGPointMake((CGFloat)offset, 0) : CGPointMake(0, (CGFloat)offset);
}

/*
 * Hand the engine and the scrollers the content, the viewport and the offset. A driven list
 * uses neither and hides the scrollers.
 */
- (void)scrollGeometryChanged
{
  BOOL driven = self.scrollDriven;
  if (_scrollers.hidden != driven) {
    _scrollers.hidden = driven;
  }
  if (driven) {
    return;
  }
  CGPoint offset = [self enginePointAt:[self offset]];
  CGSize content = self.contentSize;
  _engine.viewportSize = _horizontal ? CGSizeMake(_windowAlong, _windowCross) : CGSizeMake(_windowCross, _windowAlong);
  _engine.contentSize = content;
  _engine.offset = offset;
  NSRect bounds = self.bounds;
  if (!NSEqualRects(_scrollers.frame, bounds)) {
    _scrollers.frame = bounds;
  }
  _scrollers.scrollerInsets = self.adjustedContentInset;
  _scrollers.contentSize = content;
  _scrollers.offset = offset;
}

- (void)animateToOffset:(double)offset landing:(ScrollLanding)landing
{
  [_engine stopScrolling];
  _driver.setLanding(landing);
  [_engine animateToOffset:[self enginePointAt:offset]];
}

- (void)scrollEngine:(ShadowListKitScrollEngine *)engine didScrollToOffset:(CGPoint)offset
{
  [self writeOffset:[self along:offset] byUser:self.scrollPhase != ShadowListKitScrollPhaseIdle];
}

- (void)scrollEngine:(ShadowListKitScrollEngine *)engine didChangePhase:(ShadowListKitScrollPhase)phase
{
  self.scrollPhase = phase;
}

/*
 * An animation arrived. The core lands a command exactly on its target.
 */
- (void)scrollEngineDidEndAnimation:(ShadowListKitScrollEngine *)engine
{
  if (_driver.land()) {
    [self runCommandNow];
  }
  if ([_userDelegate respondsToSelector:@selector(listViewDidEndScrollingAnimation:)]) {
    [_userDelegate listViewDidEndScrollingAnimation:self];
  }
  [self invalidateRestorableStateIfSettled];
}

- (CGPoint)scrollEngine:(ShadowListKitScrollEngine *)engine restingOffsetForOffset:(CGPoint)offset
{
  if (!self.snapToItem) {
    return offset;
  }
  return [self enginePointAt:_driver.nearestSnapOffset([self along:offset])];
}

#pragma mark - Scrollers

/*
 * A thumb held by the pointer drags the list like a gesture: the phase is Dragging until it lets
 * go, which tells the delegate, closes an open row and keeps the core from clamping mid drag.
 */
- (void)scrollerViewWillBeginDragging:(ShadowListKitScrollerView *)scrollerView
{
  [self stopScrolling];
  self.scrollPhase = ShadowListKitScrollPhaseDragging;
}

- (void)scrollerView:(ShadowListKitScrollerView *)scrollerView didScrollToOffset:(CGPoint)offset
{
  if (self.scrollPhase != ShadowListKitScrollPhaseDragging) {
    [self stopScrolling];
  }
  [self writeOffset:[self along:offset] byUser:YES];
}

- (void)scrollerViewDidEndDragging:(ShadowListKitScrollerView *)scrollerView
{
  self.scrollPhase = ShadowListKitScrollPhaseIdle;
}

#pragma mark - Wheel

/*
 * The engine takes the wheel. What it passes on goes to the next responder unless the list
 * claims it. A driven list leaves the wheel to the next responder.
 */
- (void)scrollWheel:(NSEvent *)event
{
  if (self.scrollDriven || !self.scrollEnabled) {
    [super scrollWheel:event];
    return;
  }
  if (![_engine handleScrollWheel:event] && ![self claimsScrollWheel:event]) {
    [super scrollWheel:event];
  }
}

/*
 * A trackpad gesture across the axis may swipe an item.
 */
- (BOOL)claimsScrollWheel:(NSEvent *)event
{
  return [self swipeWithScrollWheel:event];
}

@end

#pragma mark - Testing

@implementation ShadowListKitListView (Testing)

- (ShadowListKitScrollerView *)shadowListKit_scrollerView
{
  return _scrollers;
}

- (NSUInteger)shadowListKit_layoutPassCount
{
  return _layoutPasses;
}

- (NSUInteger)shadowListKit_corePassCount
{
  return _corePasses;
}

- (NSUInteger)shadowListKit_mountPassCount
{
  return _mountPasses;
}

- (BOOL)shadowListKit_viewsBehind
{
  return _viewsBehind;
}

- (void)shadowListKit_scrollWheelDelta:(CGPoint)delta phase:(NSEventPhase)phase momentumPhase:(NSEventPhase)momentumPhase
{
  [self scrollWheel:ShadowListKitScrollWheelEvent(delta, phase, momentumPhase)];
  [self layoutSubtreeIfNeeded];
}

/*
 * One layout per frame, the way the display link's frames each get one.
 */
- (void)shadowListKit_runScrollFramesFor:(NSTimeInterval)duration
{
  NSTimeInterval frame = 1.0 / 120;
  for (NSTimeInterval run = 0; run < duration; run += frame) {
    [_engine shadowListKit_runFramesFor:frame];
    [self layoutSubtreeIfNeeded];
  }
}

@end
#pragma clang diagnostic pop
