#import "ShadowListView.h"
#import "ShadowListView+Private.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

using namespace facebook::react;

#if !TARGET_OS_OSX
/*
 * Scroll to top duration, close to UIKit's.
 */
static const CFTimeInterval SL_SCROLL_TO_TOP_DURATION = 0.45;

/*
 * How far the animation travels, in screens. The core keeps about a screen of rows mounted
 * past the visible area. This never outruns them. Longer trips jump closer first, like UIKit.
 */
static const CGFloat SL_SCROLL_TO_TOP_ANIMATED_VIEWPORTS = 1.0;

/*
 * Largest step per frame, in screens. The animation peaks near 0.1. This stops a correction
 * mid flight from stretching a step past the mounted rows.
 */
static const CGFloat SL_SCROLL_TO_TOP_MAX_STEP_VIEWPORTS = 0.35;

/*
 * How much of the screen at the jump target must be covered by rows before the jump lands.
 */
static const CGFloat SL_SCROLL_TO_TOP_JUMP_COVERAGE = 0.9;

/*
 * Longest wait for the rows at the jump target, in seconds. If they never arrive, say the
 * JS thread is busy, land anyway so the tap is not ignored.
 */
static const CFTimeInterval SL_SCROLL_TO_TOP_JUMP_MAX_WAIT = 0.5;

/*
 * Status bar tap scrolls to the top, jumping first on a long trip. iOS only.
 */
@implementation ShadowListView (ScrollToTop)

/*
 * Status bar tap. Return NO so UIKit does not animate and run ours instead.
 * A horizontal list has nothing to scroll up. Let UIKit handle it.
 */
- (BOOL)scrollViewShouldScrollToTop:(UIScrollView *)scrollView
{
  if (!_scrollsToTop) {
    return NO;
  }
  if (!_state || _horizontal) {
    return YES;
  }
  if (!_dragging && !_dragDropPending) {
    [self startScrollToTop];
  }
  return NO;
}

- (void)startScrollToTop
{
  CGFloat top = -_scrollView.contentInset.top;
  if (_scrollingToTop || _scrollView.contentOffset.y <= top + 0.5) {
    return;
  }
  // Stop any momentum first, like UIKit does.
  [_scrollView setContentOffset:_scrollView.contentOffset animated:NO];

  SLF_TRACE("ev=stt-start off=%.1f cs=%.1f", _scrollView.contentOffset.y, _scrollView.contentSize.height);
  _scrollingToTop = YES;
  _scrollToTopProgress = 0.0;
  _scrollToTopStartTime = CACurrentMediaTime();
  _scrollToTopLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(scrollToTopTick)];
  [_scrollToTopLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

  /*
   * A long trip jumps first. Jumping right away would show a blank screen until rows mount.
   * Send the target to the core and stay put. The core renders rows there, and the jump
   * lands in the mount that brings them in. See mountingTransactionDidMount.
   */
  CGFloat viewport = _scrollView.bounds.size.height;
  CGFloat jumpY = top + viewport * SL_SCROLL_TO_TOP_ANIMATED_VIEWPORTS;
  if (_state && viewport > 0.0 && _scrollView.contentOffset.y > jumpY + viewport) {
    _scrollToTopJumpPending = YES;
    _scrollToTopJumpY = jumpY;
    _scrollToTopJumpToken = 0;
    /*
     * The live report says the view is at the target too. A commit for another reason
     * renders the same rows. It keeps the mounted ack, since the jump reports when it lands.
     */
    ShadowListLiveScroll::Report report;
    report.offsetX = _scrollView.contentOffset.x;
    report.offsetY = jumpY;
    report.userScrolled = true;
    report.scrollPhase = SCROLL_PHASE_SETTLING;
    report.concealGenerationAck = _state->getData().concealGenerationAck_;
    [self commitStatePatch:_scrollSync.reportPatch(report)];
  }
}

/*
 * Whether mounted rows fill the screen starting at y. A jump there then shows content right away.
 * Row frames already match the core's layout, including rows just added around the target.
 */
- (BOOL)mountedRowsCoverViewportAt:(CGFloat)y
{
  CGFloat end = MIN(y + _scrollView.bounds.size.height, _scrollView.contentSize.height);
  if (end <= y) {
    return YES;
  }

  std::vector<std::pair<CGFloat, CGFloat>> spans;
  for (UIView *subview in _contentView.subviews) {
    if (subview.hidden ||
        !([subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)] ||
          [subview conformsToProtocol:@protocol(RCTShadowListTemplateViewViewProtocol)])) {
      continue;
    }
    CGFloat low = MAX(CGRectGetMinY(subview.frame), y);
    CGFloat high = MIN(CGRectGetMaxY(subview.frame), end);
    if (high > low) {
      spans.emplace_back(low, high);
    }
  }
  std::sort(spans.begin(), spans.end());

  CGFloat covered = 0.0;
  CGFloat reached = y;
  for (const auto& span : spans) {
    CGFloat low = MAX(span.first, reached);
    if (span.second > low) {
      covered += span.second - low;
      reached = span.second;
    }
  }
  return covered >= (end - y) * SL_SCROLL_TO_TOP_JUMP_COVERAGE;
}

/*
 * Jump to the target once its rows are mounted or the wait runs out, then animate the rest.
 */
- (void)landScrollToTopJumpIfReady
{
  if (!_scrollToTopJumpPending) {
    return;
  }
  BOOL waitedTooLong = CACurrentMediaTime() - _scrollToTopStartTime > SL_SCROLL_TO_TOP_JUMP_MAX_WAIT;
  if (!waitedTooLong && ![self mountedRowsCoverViewportAt:_scrollToTopJumpY]) {
    return;
  }

  _scrollToTopJumpPending = NO;
  CGFloat top = -_scrollView.contentInset.top;
  CGFloat maxY = MAX(top, _scrollView.contentSize.height - _scrollView.bounds.size.height
    + _scrollView.contentInset.bottom);
  CGPoint before = _scrollView.contentOffset;
  CGPoint target = CGPointMake(before.x, MIN(MAX(_scrollToTopJumpY, top), maxY));
  /*
   * A core correction moved the jump target. Send its token with the landing report so the
   * core knows its correction arrived and does not shift again.
   */
  if (_scrollToTopJumpToken != 0) {
    _scrollSync.arm(target.x, target.y, false, _scrollToTopJumpToken);
  }
  SLF_TRACE("ev=stt-land %.1f->%.1f waitedTooLong=%d", before.y, target.y, waitedTooLong ? 1 : 0);
  _scrollView.contentOffset = target;
  if (fabs(_scrollView.contentOffset.y - before.y) < 0.01) {
    _scrollSync.disarm();
  }
  _scrollToTopJumpToken = 0;
  _scrollToTopProgress = 0.0;
  _scrollToTopStartTime = CACurrentMediaTime();
}

/*
 * One frame of the ease toward the top. The distance left comes from the live offset. A
 * core correction since the previous frame just makes the rest of the trip longer or shorter.
 * Steps are capped, and a capped trip keeps going past the normal duration if needed.
 */
- (void)scrollToTopTick
{
  if (!_scrollingToTop) {
    return;
  }

  // Still waiting for rows at the jump target. The mount observer usually lands it first.
  if (_scrollToTopJumpPending) {
    [self landScrollToTopJumpIfReady];
    return;
  }

  // The previous frame reached the top. Finish.
  if (_scrollToTopProgress >= 1.0) {
    [self finishScrollToTop];
    return;
  }

  CFTimeInterval now = CACurrentMediaTime();
  CGFloat top = -_scrollView.contentInset.top;
  CGFloat time = MIN(1.0, (now - _scrollToTopStartTime) / SL_SCROLL_TO_TOP_DURATION);
  CGFloat eased = 1.0 - pow(1.0 - time, 3.0);
  CGFloat remainingFraction = 1.0 - _scrollToTopProgress;
  CGFloat currentY = _scrollView.contentOffset.y;
  CGFloat nextY = (time >= 1.0 || remainingFraction <= 0.0)
    ? top
    : top + (currentY - top) * (1.0 - eased) / remainingFraction;
  CGFloat progress = time >= 1.0 ? 1.0 : eased;

  CGFloat maxStep = MAX(1.0, _scrollView.bounds.size.height * SL_SCROLL_TO_TOP_MAX_STEP_VIEWPORTS);
  if (currentY - nextY > maxStep) {
    nextY = currentY - maxStep;
    // Count progress by how far the capped step really went.
    progress = 1.0 - remainingFraction * (nextY - top) / (currentY - top);
  }
  _scrollToTopProgress = progress;

  if (fabs(nextY - currentY) >= 0.01) {
    SLF_TRACE("ev=stt-tick %.1f->%.1f progress=%.3f", currentY, nextY, progress);
    _scrollView.contentOffset = CGPointMake(_scrollView.contentOffset.x, nextY);
  }
}

/*
 * Stop the animation. Returns whether one was running.
 */
- (BOOL)cancelScrollToTop
{
  if (!_scrollingToTop) {
    return NO;
  }
  [_scrollToTopLink invalidate];
  _scrollToTopLink = nil;
  _scrollingToTop = NO;
  _scrollToTopJumpPending = NO;
  _scrollToTopJumpToken = 0;
  return YES;
}

/*
 * Stop the animation and tell the core the motion is over. Send the view's real offset,
 * since the mounted one may be older and would make the core render rows we already left.
 */
- (void)finishScrollToTop
{
  SLF_TRACE("ev=stt-finish off=%.1f", _scrollView.contentOffset.y);
  [self cancelScrollToTop];
  if (!_state) {
    return;
  }
  ShadowListLiveScroll::Report report;
  report.offsetX = _scrollView.contentOffset.x;
  report.offsetY = _scrollView.contentOffset.y;
  report.concealGenerationAck = _state->getData().concealGenerationAck_;
  [self commitStatePatch:_scrollSync.reportPatch(report)];
}

@end
#endif
