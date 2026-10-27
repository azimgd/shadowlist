#import "ShadowListView.h"
#import "ShadowListView+Private.h"
#import "ShadowListCellView.h"

#import "ShadowListViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

#import <QuartzCore/QuartzCore.h>
#include <TargetConditionals.h>

using namespace facebook::react;

/*
 * The core's index for a view index, UNDEFINED_INDEX for NSNotFound.
 */
static std::size_t SLCoreIndex(NSInteger index)
{
  return index == NSNotFound || index < 0 ? azimgd::shadowlist::UNDEFINED_INDEX : (std::size_t)index;
}

/*
 * A view index for the core's index, NSNotFound for UNDEFINED_INDEX.
 */
static NSInteger SLViewIndex(std::size_t index)
{
  return index == azimgd::shadowlist::UNDEFINED_INDEX ? NSNotFound : (NSInteger)index;
}

/*
 * Animate a row's transform to target over duration, then run completion with whether it
 * finished. macOS animates the layer because AppKit has no view animation for a transform.
 */
static void SLAnimateDragTransform(
  RCTUIView *view,
  CGAffineTransform target,
  NSTimeInterval duration,
  void (^_Nullable completion)(BOOL finished))
{
#if TARGET_OS_OSX
  CATransform3D from = view.layer.transform;
  view.transform = target;
  CABasicAnimation *slide = [CABasicAnimation animationWithKeyPath:@"transform"];
  slide.fromValue = [NSValue valueWithCATransform3D:from];
  slide.toValue = [NSValue valueWithCATransform3D:view.layer.transform];
  slide.duration = duration;
  slide.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    if (completion) {
      completion(YES);
    }
  }];
  [view.layer addAnimation:slide forKey:@"transform"];
  [CATransaction commit];
#else
  [UIView animateWithDuration:duration
                        delay:0.0
                      options:UIViewAnimationOptionBeginFromCurrentState | UIViewAnimationOptionCurveEaseOut |
                              UIViewAnimationOptionAllowUserInteraction
                   animations:^{
                     view.transform = target;
                   }
                   completion:completion];
#endif
}

#if !TARGET_OS_OSX
/*
 * How fast the drag link asks the display to run, in frames per second. A ProMotion screen
 * shows the edge scroll and the shifting rows at its full rate.
 */
static const float SL_DRAG_FRAME_RATE_MIN = 60.0f;
static const float SL_DRAG_FRAME_RATE_MAX = 120.0f;

static UIImpactFeedbackGenerator *SLDragLiftGenerator(void)
{
  static UIImpactFeedbackGenerator *generator;
  if (!generator) {
    generator = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
  }
  return generator;
}

static UISelectionFeedbackGenerator *SLDragSelectionGenerator(void)
{
  static UISelectionFeedbackGenerator *generator;
  if (!generator) {
    generator = [UISelectionFeedbackGenerator new];
  }
  return generator;
}

void SLPrepareDragFeedback(void)
{
  [SLDragLiftGenerator() prepare];
}

void SLDragLiftFeedback(void)
{
  [SLDragLiftGenerator() impactOccurred];
  // The engine stays awake for the ticks of this drag.
  [SLDragSelectionGenerator() prepare];
}

void SLDragSelectionFeedback(void)
{
  [SLDragSelectionGenerator() selectionChanged];
  [SLDragSelectionGenerator() prepare];
}
#else
void SLPrepareDragFeedback(void)
{
}

void SLDragLiftFeedback(void)
{
  [[NSHapticFeedbackManager defaultPerformer] performFeedbackPattern:NSHapticFeedbackPatternGeneric
                                                     performanceTime:NSHapticFeedbackPerformanceTimeNow];
}

void SLDragSelectionFeedback(void)
{
  [[NSHapticFeedbackManager defaultPerformer] performFeedbackPattern:NSHapticFeedbackPatternAlignment
                                                     performanceTime:NSHapticFeedbackPerformanceTimeNow];
}
#endif

/*
 * Long press and drag to reorder rows.
 */
@implementation ShadowListView (DragReorder)

#pragma mark - Drag gesture

/*
 * Where the view sits without any drag offset applied.
 */
- (CGRect)restingFrameForView:(RCTUIView *)view
{
  CGSize size = view.bounds.size;
#if TARGET_OS_OSX
  CGPoint center = CGPointMake(CGRectGetMidX(view.frame), CGRectGetMidY(view.frame));
#else
  CGPoint center = view.center;
#endif
  return CGRectMake(center.x - size.width / 2.0, center.y - size.height / 2.0, size.width, size.height);
}

/*
 * A row's resting frame for the drag math, along and across the scroll axis.
 */
- (azimgd::shadowlist::DragRow)dragRowForView:(RCTUIView *)view index:(NSInteger)index
{
  CGRect resting = [self restingFrameForView:view];
  NSString *key = [self keyOfCellView:view];
  return {
    SLCoreIndex(index),
    key ? std::string(key.UTF8String) : std::string(),
    _horizontal ? resting.origin.x : resting.origin.y,
    _horizontal ? resting.size.width : resting.size.height,
    _horizontal ? resting.origin.y : resting.origin.x,
    _horizontal ? resting.size.height : resting.size.width};
}

- (CGAffineTransform)dragTransformForOffset:(azimgd::shadowlist::DragOffset)offset
{
  return _horizontal
    ? CGAffineTransformMakeTranslation(offset.leading, offset.cross)
    : CGAffineTransformMakeTranslation(offset.cross, offset.leading);
}

/*
 * The topmost row under a point in the content.
 */
- (RCTUIView *)cellViewAtContentPoint:(CGPoint)point
{
  RCTUIView *result = nil;
  for (RCTUIView *subview in _contentView.subviews) {
    if (![subview conformsToProtocol:@protocol(RCTShadowListCellViewViewProtocol)]) {
      continue;
    }
    if (CGRectContainsPoint([self restingFrameForView:subview], point)) {
      result = subview;
    }
  }
  return result;
}

- (void)handleDragGesture:(SLDragGestureRecognizer *)gesture
{
  switch (gesture.state) {
    case SLGestureStateBegan:
#if !TARGET_OS_OSX
      if (![self dragPressHeldStill]) {
        // The finger scrolled the list during the hold. Fail the press, the scroll goes on.
        gesture.enabled = NO;
        gesture.enabled = YES;
        break;
      }
#endif
      [self beginDragAtPoint:[gesture locationInView:self]];
      break;
    case SLGestureStateChanged:
      [self setDragTouch:[gesture locationInView:self]];
      [self updateDrag];
      break;
    case SLGestureStateEnded:
      [self finishDrag];
      break;
    case SLGestureStateCancelled:
    case SLGestureStateFailed:
      // The system or reorderEnabled turning off took the touch. Put the row back, no reorder.
      [self cancelDrag];
      break;
    default:
      break;
  }
}

#if !TARGET_OS_OSX
/*
 * The press runs together with the list's pan. A finger that drifts still lifts the row.
 * A finger that scrolled the list farther than the core's DRAG_PRESS_SCROLL_SLOP along the scroll axis
 * meant to scroll, and the press fails.
 */
- (BOOL)dragPressHeldStill
{
  UIPanGestureRecognizer *pan = _scrollView.panGestureRecognizer;
  if (pan.state != UIGestureRecognizerStateBegan && pan.state != UIGestureRecognizerStateChanged) {
    return YES;
  }
  CGPoint moved = [pan translationInView:_scrollView];
  return azimgd::shadowlist::dragPressHolds(_horizontal ? moved.x : moved.y, _horizontal ? moved.y : moved.x);
}
#endif

/*
 * For scripted runs, like -SLAutoDrag in the example app. Drives the same path as the long
 * press with a point in this view: phase 1 picks up, 2 moves and 3 drops.
 */
- (void)debugDragPhase:(NSNumber *)phase point:(NSValue *)point
{
  if (!_reorderEnabled) {
    return;
  }
#if TARGET_OS_OSX
  CGPoint location = point.pointValue;
#else
  CGPoint location = point.CGPointValue;
#endif
  switch (phase.intValue) {
    case 1:
      [self beginDragAtPoint:location];
      break;
    case 2:
      [self setDragTouch:location];
      [self updateDrag];
      break;
    default:
      [self finishDrag];
      break;
  }
}

/*
 * The finger moved to location in this view.
 */
- (void)setDragTouch:(CGPoint)location
{
  _dragTouchInViewport = location;
  _dragTouchInWindow = [self convertPoint:location toView:nil];
}

/*
 * On a drag board the view around this list may have scrolled under a finger that held still.
 * Find the finger in this view again from where it is in the window.
 */
- (void)refreshDragTouchFromWindow
{
  if (_dragBoard && self.window) {
    _dragTouchInViewport = [self convertPoint:_dragTouchInWindow fromView:nil];
  }
}

- (void)beginDragAtPoint:(CGPoint)location
{
  CGPoint contentPoint = [self convertPoint:location toView:_contentView];
  RCTUIView *view = [self cellViewAtContentPoint:contentPoint];
  NSInteger index = [self indexOfCellView:view];
  if (!view || index == NSNotFound) {
    return;
  }

  // Clear anything left over from the previous drag.
  _dragDropPending = NO;
  _droppedView = nil;
  _dropSettle.reset();
  [_dropSettleLink invalidate];
  _dropSettleLink = nil;
  [self clearDragTransforms];

  _dragging = YES;
  _draggedView = view;
  azimgd::shadowlist::DragRow resting = [self dragRowForView:view index:index];
  _drag.begin(
    resting,
    _horizontal ? contentPoint.x : contentPoint.y,
    _horizontal ? contentPoint.y : contentPoint.x,
    (std::size_t)MAX(_numberOfColumns, 1));
  [self setDragTouch:location];
  _dragFeedback.reset(_drag.getOriginIndex());
  _dragLastTickTime = 0.0;
  SLDragLiftFeedback();

  // A drop still landing on this list's drag board ends before the next pickup.
  [self dragBoardDidPickUpView:view key:[self keyOfCellView:view]];

  // We scroll ourselves near the edges. Stop the scroll view from following the finger.
  _scrollView.scrollEnabled = NO;

  // Lift the row with a shadow so it looks picked up.
  SLRaiseSubview(_contentView, view, 4.0);
  // The row now sits above the sticky views. The next pin raises them again.
  _stickyOrderDirty = YES;
  SLApplyDragLiftShadow(view, view.layer);

  // Tell the core a drag started so this row stays mounted when it scrolls off screen.
  NSString *originKey = SLDragKey(_drag.getOriginKey());
  [self commitDragEventType:azimgd::shadowlist::DRAG_EVENT_START sourceKey:originKey destinationKey:originKey];

  // A scripted pickup can arrive while a drag runs. Its previous link would keep this view alive.
  [_dragDisplayLink invalidate];
  _dragDisplayLink = [SLDisplayLink displayLinkWithTarget:self selector:@selector(dragTick)];
#if !TARGET_OS_OSX
  _dragDisplayLink.preferredFrameRateRange =
    CAFrameRateRangeMake(SL_DRAG_FRAME_RATE_MIN, SL_DRAG_FRAME_RATE_MAX, SL_DRAG_FRAME_RATE_MAX);
#endif
  [_dragDisplayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

  [self updateDrag];
}

/*
 * Every frame, scroll near the edges, then move the row and shift the others. The edge
 * speeds are per second. A ProMotion frame scrolls less and the speed stays the same.
 */
- (void)dragTick
{
  if (!_dragging) {
    return;
  }
  CFTimeInterval now = CACurrentMediaTime();
  double dtSeconds = _dragLastTickTime > 0.0 ? now - _dragLastTickTime : 1.0 / 60.0;
  _dragLastTickTime = now;
  [self refreshDragTouchFromWindow];
  // A row floating over another list of the drag board scrolls that list instead.
  if (![self applyDragBoardAutoScrollWithDt:dtSeconds]) {
    [self applyDragAutoScrollForTouch:_dragTouchInViewport dtSeconds:dtSeconds];
  }
  // The board's sideways scroll just moved this list under the finger.
  [self refreshDragTouchFromWindow];
  [self updateDrag];
}

/*
 * Scroll near the edges for a finger at touch in this view.
 */
- (void)applyDragAutoScrollForTouch:(CGPoint)touchPoint dtSeconds:(double)dtSeconds
{
  CGFloat window = _horizontal ? _scrollView.bounds.size.width : _scrollView.bounds.size.height;
  CGFloat content = _horizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height;
  CGFloat maxOffset = MAX(0.0, content - window);
  CGFloat offset = _horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  CGFloat touch = _horizontal ? touchPoint.x : touchPoint.y;
  CGFloat newOffset = azimgd::shadowlist::dragAutoScrollOffset(
    azimgd::shadowlist::DRAG_AUTO_SCROLL_IOS, touch, window, offset, maxOffset, dtSeconds);
  if (newOffset == offset) {
    return;
  }

  CGPoint next = _horizontal
    ? CGPointMake(newOffset, _scrollView.contentOffset.y)
    : CGPointMake(_scrollView.contentOffset.x, newOffset);
  /*
   * This must count as a user scroll so the core renders rows at this offset.
   * Treating it as our own move leaves blank rows during the drag.
   */
  _scrollView.contentOffset = next;
}

/*
 * Put the row under the finger, find where it would drop, and shift the others.
 */
- (void)updateDrag
{
  RCTUIView *view = _draggedView;
  if (!_dragging || !view) {
    return;
  }

  /*
   * Read the dragged row's index and key again each time. A data change during the drag
   * can move them, and the drop math must use the current values, not the ones from pickup.
   */
  NSInteger currentIndex = [self indexOfCellView:view];
  NSString *currentKey = [self keyOfCellView:view];
  _drag.updateOrigin(SLCoreIndex(currentIndex), currentKey ? std::string(currentKey.UTF8String) : std::string());

  // Over another list of the drag board the row floats above the window. The board moves it.
  if ([self updateDragBoard]) {
    return;
  }
  if (!_dragging) {
    // The board cancelled the drag on this frame.
    return;
  }

  CGPoint touchContent = CGPointMake(
    _dragTouchInViewport.x + _scrollView.contentOffset.x, _dragTouchInViewport.y + _scrollView.contentOffset.y);
  azimgd::shadowlist::DragRow resting = [self dragRowForView:view index:SLViewIndex(_drag.getOriginIndex())];
  azimgd::shadowlist::DragOffset translation = _drag.placeRow(
    _horizontal ? touchContent.x : touchContent.y,
    _horizontal ? touchContent.y : touchContent.x,
    resting,
    _horizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height,
    // The scroll view's content size is 0 across the scroll axis. Use the content view.
    _horizontal ? _contentView.bounds.size.height : _contentView.bounds.size.width);

  view.transform = [self dragTransformForOffset:translation];
  [self updateDragInsertion];
  // A row that came home on this frame's board update slid its neighbors back once.
  _dragBoardAnimateShifts = NO;
}

/*
 * Find where the held row would drop among the other mounted rows and shift them. The drag
 * board also calls this while the row is away or comes from another list.
 */
- (void)updateDragInsertion
{
  std::vector<azimgd::shadowlist::DragRow> rows;
  rows.reserve(_contentView.subviews.count);
  for (RCTUIView *subview in _contentView.subviews) {
    if (subview == _draggedView) {
      continue;
    }
    NSInteger rowIndex = [self indexOfCellView:subview];
    if (rowIndex == NSNotFound) {
      continue;
    }
    rows.push_back([self dragRowForView:subview index:rowIndex]);
  }
  _drag.updateInsertion(rows);
  [self noteDragInsertionForFeedback];
  [self applyDragShuffle];
}

/*
 * A tick when the drop spot moves, in the list the row is over: the home list while the row
 * is in it, or the list it hovers on a drag board. The home list skips the spots it goes
 * through while the row is away. The core's DragFeedback spaces the ticks out.
 */
- (void)noteDragInsertionForFeedback
{
  if (!_drag.isForeign() && (!_dragging || _drag.isHeldAway())) {
    return;
  }
  if (_dragFeedback.shouldTick(_drag.getInsertionIndex(), SLDragNowMs())) {
    SLDragSelectionFeedback();
  }
}

/*
 * Open a gap at the drop spot by shifting the rows in between by the dragged row's size.
 * In a grid each cell moves to its new resting place, which can be in another column.
 */
- (void)applyDragShuffle
{
  BOOL animated = _dragBoard && (_drag.isForeign() || _drag.isHeldAway() || _dragBoardAnimateShifts);
  for (RCTUIView *subview in _contentView.subviews) {
    if (subview == _draggedView ||
        ![subview conformsToProtocol:@protocol(RCTShadowListCellViewViewProtocol)]) {
      continue;
    }
    NSInteger rowIndex = [self indexOfCellView:subview];
    if (rowIndex == NSNotFound) {
      continue;
    }
    CGAffineTransform shift = [self dragTransformForOffset:_drag.offsetFor(SLCoreIndex(rowIndex))];
    /*
     * On a drag board a row leaves or enters a list as a whole gap at once. The rows slide
     * there instead of jumping, while the row is away from its home list, hovers this list
     * from another one, or just came back or left. A reorder within one list moves its rows
     * like any list.
     */
    if (animated && !CGAffineTransformEqualToTransform(subview.transform, shift)) {
      SLAnimateDragTransform(subview, shift, SL_DRAG_SHIFT_DURATION, nil);
      continue;
    }
    subview.transform = shift;
  }
}

/*
 * Once the reorder lands, reset the other rows right away and animate the dropped row
 * from where it was released into its place.
 */
- (void)settleDroppedView:(RCTUIView *)view
{
  if (!view) {
    [self clearDragTransforms];
    return;
  }

  for (RCTUIView *subview in _contentView.subviews) {
    if (subview == view || ![subview conformsToProtocol:@protocol(RCTShadowListCellViewViewProtocol)]) {
      continue;
    }
    // A drag board slides the rows. Stop a slide still running, or it ends on the old shift.
    [subview.layer removeAllAnimations];
    subview.transform = CGAffineTransformIdentity;
    SLClearDragLiftShadow(subview.layer);
  }

  CGRect resting = [self restingFrameForView:view];
  azimgd::shadowlist::DragOffset start;
  start.leading = _dropReleaseLeading - (_horizontal ? resting.origin.x : resting.origin.y);
  if (_drag.isGrid()) {
    start.cross = _dropReleaseCross - (_horizontal ? resting.origin.y : resting.origin.x);
  }
  view.transform = [self dragTransformForOffset:start];
  [self slideDraggedViewHome:view duration:SL_DROP_SETTLE_DURATION];
}

/*
 * Animate a held or dropped row to its resting place and drop its lift there. A new pickup of
 * the same row cancels this animation. The completion then runs after the lift and must not
 * clear the new shadow.
 */
- (void)slideDraggedViewHome:(RCTUIView *)view duration:(NSTimeInterval)duration
{
  __weak ShadowListView *weakSelf = self;
  __weak RCTUIView *weakView = view;
  SLAnimateDragTransform(view, CGAffineTransformIdentity, duration, ^(BOOL finished) {
    ShadowListView *strongSelf = weakSelf;
    RCTUIView *settled = weakView;
    if (!settled || (strongSelf && strongSelf->_dragging && strongSelf->_draggedView == settled)) {
      return;
    }
    SLClearDragLiftShadow(settled.layer);
#if TARGET_OS_OSX
    settled.layer.zPosition = 0.0;
#endif
  });
}

/*
 * Remove the drag offset and shadow from every row.
 */
- (void)clearDragTransforms
{
  for (RCTUIView *subview in _contentView.subviews) {
    if (![subview conformsToProtocol:@protocol(RCTShadowListCellViewViewProtocol)]) {
      continue;
    }
    // Stop any drop animation still running before a new pickup.
    [subview.layer removeAllAnimations];
    subview.transform = CGAffineTransformIdentity;
    SLClearDragLiftShadow(subview.layer);
#if TARGET_OS_OSX
    subview.layer.zPosition = 0.0;
#endif
  }
}

/*
 * A cancel puts every row back the way a drop settles: the held row flies home over the drop's
 * duration and the others slide back like they shifted. Rows off the window snap.
 */
- (void)slideDragTransformsHome
{
  RCTUIView *held = _draggedView;
  for (RCTUIView *subview in _contentView.subviews) {
    if (![subview conformsToProtocol:@protocol(RCTShadowListCellViewViewProtocol)]) {
      continue;
    }
    if (!subview.window) {
      [subview.layer removeAllAnimations];
      subview.transform = CGAffineTransformIdentity;
      SLClearDragLiftShadow(subview.layer);
      continue;
    }
    if (subview == held) {
      [self slideDraggedViewHome:subview duration:SL_DROP_SETTLE_DURATION];
    } else if (!CGAffineTransformIsIdentity(subview.transform)) {
      SLAnimateDragTransform(subview, CGAffineTransformIdentity, SL_DRAG_SHIFT_DURATION, nil);
    }
  }
}

- (void)finishDrag
{
  if (!_dragging) {
    return;
  }
  // A row let go over another list of the drag board drops there.
  if ([self finishDragBoard]) {
    return;
  }

  [_dragDisplayLink invalidate];
  _dragDisplayLink = nil;
  _scrollView.scrollEnabled = _scrollEnabled;

  NSInteger from = SLViewIndex(_drag.getOriginIndex());
  NSInteger to = SLViewIndex(_drag.getInsertionIndex());
  RCTUIView *view = _draggedView;
  _dropReleaseLeading = _drag.getLeading();
  _dropReleaseCross = _drag.getCrossLeading();
  _dragging = NO;
  _draggedView = nil;

  /*
   * Send the reorder by key and keep the rows shifted until the commit lands.
   * The indices below still drive the settle animation.
   */
  [self commitDragEventType:azimgd::shadowlist::DRAG_EVENT_END
                    sourceKey:SLDragKey(_drag.getOriginKey())
                      destinationKey:SLDragKey(_drag.getInsertionKey())];

  if (from == to || !view) {
    // Dropped where it started. No commit will come. Settle now.
    _dragDropPending = NO;
    _droppedView = nil;
    if (view) {
      [self settleDroppedView:view];
    } else {
      [self clearDragTransforms];
    }
    return;
  }
  _dragDropPending = YES;
  _droppedView = view;

  /*
   * Check each frame for the landing. A reorder of same size rows may not publish new state.
   * The core's DropSettle watches the row's index reach its new spot, and gives up after
   * DRAG_DROP_SETTLE_TIMEOUT_MS if the reorder never lands.
   */
  _dropSettle.begin(SLCoreIndex(to), SLDragNowMs());
  [_dropSettleLink invalidate];
  _dropSettleLink = [SLDisplayLink displayLinkWithTarget:self selector:@selector(dropSettleTick)];
  [_dropSettleLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

/*
 * After a drop, wait each frame for the row to reach its new index, then animate it
 * into place and stop.
 */
- (void)dropSettleTick
{
  RCTUIView *view = _droppedView;
  using Status = azimgd::shadowlist::DropSettle::Status;
  Status status = _dragDropPending
    ? _dropSettle.tick(view ? SLCoreIndex([self indexOfCellView:view]) : azimgd::shadowlist::UNDEFINED_INDEX,
        SLDragNowMs())
    : Status::Idle;
  if (status == Status::Waiting) {
    return;
  }
  [_dropSettleLink invalidate];
  _dropSettleLink = nil;
  if (!_dragDropPending) {
    return;
  }
  _dragDropPending = NO;
  _droppedView = nil;
  if (status == Status::Settle) {
    [self settleDroppedView:view];
  } else if (!_dragging) {
    // The reorder never landed, or the row unmounted. Put every row back at once.
    [self clearDragTransforms];
  }
}

/*
 * Abort a live drag without a reorder, when the gesture is cancelled, drag is turned off or
 * the held row is deleted. The end event goes out with the origin key on both sides. It
 * turns core corrections back on and clears the held key in JS without moving anything.
 * This can run inside a mount, where an immediate commit is not allowed. Send it async.
 * The rows slide back to their places.
 */
- (void)cancelDrag
{
  BOOL wasDragging = _dragging;
  if (_dragging && _state) {
    NSString *originKey = SLDragKey(_drag.getOriginKey());
    BOOL wasInMountObserver = _inMountObserver;
    _inMountObserver = YES;
    [self commitDragEventType:azimgd::shadowlist::DRAG_EVENT_END sourceKey:originKey destinationKey:originKey];
    _inMountObserver = wasInMountObserver;
  }
  // A row floating over another list of the drag board flies back to its place.
  [self cancelDragBoard];
  [self tearDownDragAnimated:wasDragging && self.window != nil];
}

/*
 * Stop everything without a reorder or an end event. Used directly only on recycle, where
 * the state belongs to the previous list.
 */
- (void)tearDownDrag
{
  [self tearDownDragAnimated:NO];
}

- (void)tearDownDragAnimated:(BOOL)animated
{
  [self tearDownDragBoard];
  [_dragDisplayLink invalidate];
  _dragDisplayLink = nil;
  [_dropSettleLink invalidate];
  _dropSettleLink = nil;
  _dropSettle.reset();
  [_droppedView.layer removeAllAnimations];
  if (animated) {
    [self slideDragTransformsHome];
  }
  _dragging = NO;
  _draggedView = nil;
  _dragDropPending = NO;
  _droppedView = nil;
  _scrollView.scrollEnabled = _scrollEnabled;
  if (!animated) {
    [self clearDragTransforms];
  }
}

#pragma mark - Accessibility

/*
 * For VoiceOver, give each row Move up and Move down actions while drag is on.
 * The handlers read the row's index and key when used. Data changes never leave them stale.
 */
- (void)applyDragAccessibilityActionsToView:(RCTUIView *)view
{
  if (![view isKindOfClass:[ShadowListCellView class]]) {
    return;
  }
  ShadowListCellView *cellView = (ShadowListCellView *)view;
  if (!_reorderEnabled) {
    cellView.nativeAccessibilityActions = nil;
    return;
  }

  __weak ShadowListView *weakSelf = self;
  __weak RCTUIView *weakView = view;
#if TARGET_OS_OSX
  NSAccessibilityCustomAction *moveUp = [[NSAccessibilityCustomAction alloc]
    initWithName:NSLocalizedString(@"Move up", nil) handler:^BOOL {
      return [weakSelf performAccessibilityMove:weakView up:YES];
    }];
  NSAccessibilityCustomAction *moveDown = [[NSAccessibilityCustomAction alloc]
    initWithName:NSLocalizedString(@"Move down", nil) handler:^BOOL {
      return [weakSelf performAccessibilityMove:weakView up:NO];
    }];
#else
  UIAccessibilityCustomAction *moveUp = [[UIAccessibilityCustomAction alloc]
      initWithName:NSLocalizedString(@"Move up", nil)
     actionHandler:^BOOL(UIAccessibilityCustomAction *action) {
       (void)action;
       return [weakSelf performAccessibilityMove:weakView up:YES];
     }];
  UIAccessibilityCustomAction *moveDown = [[UIAccessibilityCustomAction alloc]
      initWithName:NSLocalizedString(@"Move down", nil)
     actionHandler:^BOOL(UIAccessibilityCustomAction *action) {
       (void)action;
       return [weakSelf performAccessibilityMove:weakView up:NO];
     }];
#endif
  cellView.nativeAccessibilityActions = @[ moveUp, moveDown ];
}

/*
 * Swap the row with the nearest mounted row above or below. It goes through the same
 * path as a real drop. onDragEnd and useDragReorder handle it as usual.
 */
- (BOOL)performAccessibilityMove:(RCTUIView *)view up:(BOOL)up
{
  NSInteger index = [self indexOfCellView:view];
  if (index == NSNotFound) {
    return NO;
  }
  NSString *key = [self keyOfCellView:view];
  if (!key) {
    return NO;
  }

  RCTUIView *neighbor = nil;
  NSInteger neighborIndex = up ? NSIntegerMin : NSIntegerMax;
  for (RCTUIView *subview in _contentView.subviews) {
    if (subview == view) {
      continue;
    }
    NSInteger subviewIndex = [self indexOfCellView:subview];
    if (subviewIndex == NSNotFound) {
      continue;
    }
    if (up ? (subviewIndex < index && subviewIndex > neighborIndex)
           : (subviewIndex > index && subviewIndex < neighborIndex)) {
      neighborIndex = subviewIndex;
      neighbor = subview;
    }
  }
  if (!neighbor) {
    return NO;
  }
  NSString *neighborKey = [self keyOfCellView:neighbor];
  if (!neighborKey) {
    return NO;
  }

  [self commitDragEventType:azimgd::shadowlist::DRAG_EVENT_END sourceKey:key destinationKey:neighborKey];
  return YES;
}

@end
