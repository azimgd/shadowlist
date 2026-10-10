#import "ShadowListView.h"
#import "ShadowListView+Private.h"
#import "ShadowListElementView.h"
#import "ShadowListMacScrollView.h"

#include <cmath>

using namespace facebook::react;

/*
 * Commands from JS: scrolling, reached flags, swipe actions and the anchor request.
 */
@implementation ShadowListView (Commands)

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowListViewHandleCommand(self, commandName, args);
}

#pragma mark - Momentum

/*
 * A scroll command replaces any running momentum, from scroll to top or a fling. Stop it so
 * its next frame cannot move the view off the core's offset. A finger on the list keeps its
 * drag phase. The core lets the drag cancel the command.
 * Returns whether momentum stopped, which makes the command's report idle.
 */
- (BOOL)yieldMomentum
{
#if !TARGET_OS_OSX
  return [self stopMomentum];
#else
  return [(ShadowListMacScrollView *)_scrollView stopMomentum];
#endif
}

#if !TARGET_OS_OSX
/*
 * Stop scroll to top or a fling. Returns YES if one was running.
 */
- (BOOL)stopMomentum
{
  BOOL yielded = [self cancelScrollToTop];
  // isDragging stays set during a fling. Only isTracking means a finger is down.
  if (_scrollView.isDecelerating && !_scrollView.isTracking) {
    // Writing the current offset stops the fling, like in startScrollToTop.
    [_scrollView setContentOffset:_scrollView.contentOffset animated:NO];
    yielded = YES;
  }
  return yielded;
}
#endif

#pragma mark - State reports

/*
 * Acknowledge a state that hides rows when no scroll report will, because its correction
 * moved nothing or we did not apply it. Otherwise the rows stay hidden until the next scroll.
 * Skip while a scroll to top jump waits, since it reports when it lands. See concealGenerationAck_.
 */
- (void)reportConcealedRowsMounted
{
  if (!_state || _scrollToTopJumpPending) {
    return;
  }
  [self commitStatePatch:[self livePatch]];
}

- (void)setStartReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  auto patch = [self livePatch];
  patch.hasStartReachedEnabled = true;
  patch.startReachedEnabled = enabled;
  [self commitStatePatch:patch];
}

- (void)setEndReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  auto patch = [self livePatch];
  patch.hasEndReachedEnabled = true;
  patch.endReachedEnabled = enabled;
  [self commitStatePatch:patch];
}

#pragma mark - Scroll commands

/*
 * Send a scroll command. The sequence always goes past the previous one. The same index
 * still scrolls again, and the offset is marked as ours until the core applies it.
 * An animated command first gets the core's estimate, see animateCommandTo.
 */
- (void)commitScrollCommand:(const azimgd::shadowlist::ScrollCommand&)command
{
  BOOL yielded = [self yieldMomentum];
  if (yielded) {
    _scrollSync.momentumStopped();
  }
  [self commitStatePatch:_scrollSync.issueCommand(
    command, _scrollView.contentOffset.x, _scrollView.contentOffset.y, yielded)];
}

- (void)scrollToItem:(NSInteger)index viewPosition:(double)viewPosition viewOffset:(double)viewOffset animated:(BOOL)animated
{
  if (!_state) {
    return;
  }

  SLF_TRACE("ev=cmd-scroll-to-index index=%ld viewPosition=%.2f animated=%d", (long)index, viewPosition, animated ? 1 : 0);
  azimgd::shadowlist::ScrollCommand command;
  command.index = (double)index;
  command.viewPosition = viewPosition;
  // The core moves the resting offset by rowOffset. viewOffset moves the other way.
  command.rowOffset = std::isfinite(viewOffset) ? -viewOffset : 0.0;
  command.animated = [self animatesCommands] && animated;
  [self commitScrollCommand:command];
}

- (void)scrollToOffset:(double)offset animated:(BOOL)animated
{
  if (!std::isfinite(offset)) {
    return;
  }

#if !TARGET_OS_OSX
  [self cancelScrollToTop];
#endif
  /*
   * An animated scroll goes through the core like scrollToItem. UIKit's own animation would
   * stop at the first correction a row measured on the way sends. Where commands land at once
   * the core still lands it, in a commit after the rows JS mounted for it.
   */
  if (animated && _state) {
    SLF_TRACE("ev=cmd-scroll-to-offset offset=%.1f", offset);
    azimgd::shadowlist::ScrollCommand command;
    command.index = azimgd::shadowlist::SCROLL_TO_OFFSET_INDEX;
    command.rowOffset = offset;
    command.animated = [self animatesCommands];
    [self commitScrollCommand:command];
    return;
  }
  // Scroll straight to the offset. The core learns the new position from the scroll report.
  CGPoint contentOffset = _horizontal
    ? CGPointMake(offset, _scrollView.contentOffset.y)
    : CGPointMake(_scrollView.contentOffset.x, offset);
  [_scrollView setContentOffset:contentOffset animated:animated];
}

- (void)scrollToEnd:(BOOL)animated
{
  if (!_state) {
    return;
  }

  // SCROLL_TO_END_INDEX keeps the core aiming at the real bottom as rows get measured.
  SLF_TRACE("ev=cmd-scroll-to-end off=%.1f,%.1f animated=%d", _scrollView.contentOffset.x, _scrollView.contentOffset.y,
    animated ? 1 : 0);
  azimgd::shadowlist::ScrollCommand command;
  command.index = azimgd::shadowlist::SCROLL_TO_END_INDEX;
  command.animated = [self animatesCommands] && animated;
  [self commitScrollCommand:command];
}

/*
 * AppKit's scroll view has no end of animation callback. macOS commands land right away.
 */
- (BOOL)animatesCommands
{
#if TARGET_OS_OSX
  return NO;
#else
  return YES;
#endif
}

/*
 * Animate to where the core estimates an animated command lands. The frames are ours, and
 * the end lands the command exactly.
 */
- (void)animateCommandTo:(CGPoint)target
{
#if !TARGET_OS_OSX
  CGPoint current = _scrollView.contentOffset;
  CGFloat top = -_scrollView.contentInset.top;
  CGFloat maxAlong = MAX(top, _horizontal
    ? _scrollView.contentSize.width - _scrollView.bounds.size.width + _scrollView.contentInset.right
    : _scrollView.contentSize.height - _scrollView.bounds.size.height + _scrollView.contentInset.bottom);
  if (_horizontal) {
    target.x = MIN(MAX(target.x, top), maxAlong);
  } else {
    target.y = MIN(MAX(target.y, top), maxAlong);
  }
  SLF_TRACE("ev=cmd-animate %.1f->%.1f", _horizontal ? current.x : current.y, _horizontal ? target.x : target.y);
  if (fabs(target.x - current.x) < 0.5 && fabs(target.y - current.y) < 0.5) {
    [self landAnimatedCommand];
    return;
  }
  _scrollSync.arm(target.x, target.y, true);
  [_scrollView setContentOffset:target animated:YES];
#else
  [self landAnimatedCommand];
#endif
}

/*
 * Send an animated command again without the animation, which lands it exactly.
 */
- (void)landAnimatedCommand
{
  if (!_state) {
    return;
  }
  if (auto patch = _scrollSync.land(_scrollView.contentOffset.x, _scrollView.contentOffset.y)) {
    SLF_TRACE("ev=cmd-land off=%.1f,%.1f", _scrollView.contentOffset.x, _scrollView.contentOffset.y);
    _landCommandSequence = patch->commandSequence;
    [self commitStatePatch:*patch];
  }
}

#pragma mark - Swipe actions

/*
 * Slide every swiped row back.
 */
- (void)closeSwipeActions
{
  for (RCTUIView *subview in _contentView.subviews) {
    if ([subview isKindOfClass:[ShadowListElementView class]]) {
      [(ShadowListElementView *)subview closeSwipeActionsAnimated:YES];
    }
  }
}

#if !TARGET_OS_OSX
/*
 * Close the open rows a touch in view is not on. Returns whether any closed.
 */
- (BOOL)closeSwipeActionsForTouchInView:(UIView *)view
{
  BOOL closed = NO;
  for (RCTUIView *subview in _contentView.subviews) {
    if (![subview isKindOfClass:[ShadowListElementView class]]) {
      continue;
    }
    ShadowListElementView *elementView = (ShadowListElementView *)subview;
    if (elementView.isSwipeOpen && !elementView.isSwipedOut && ![view isDescendantOfView:elementView]) {
      [elementView closeSwipeActionsAnimated:YES];
      closed = YES;
    }
  }
  return closed;
}
#endif

/*
 * Only one row stays open. A row swiped all the way stays out while its removal runs.
 */
- (void)closeSwipeActionsExcept:(RCTUIView *)view
{
  for (RCTUIView *subview in _contentView.subviews) {
    if (subview != view && [subview isKindOfClass:[ShadowListElementView class]] &&
        ![(ShadowListElementView *)subview isSwipedOut]) {
      [(ShadowListElementView *)subview closeSwipeActionsAnimated:YES];
    }
  }
}

#pragma mark - Indicators and anchor

- (void)flashScrollIndicators
{
#if TARGET_OS_OSX
  [_scrollView flashScrollers];
#else
  [_scrollView flashScrollIndicators];
#endif
}

/*
 * Ask the core for the row at the viewport start. The answer comes back as onAnchorState.
 */
- (void)requestAnchorState
{
  if (!_state) {
    return;
  }
  [self commitStatePatch:_scrollSync.requestAnchor(_scrollView.contentOffset.x, _scrollView.contentOffset.y)];
}

@end
