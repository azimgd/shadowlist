#import "ShadowlistView.h"
#import "ShadowlistView+Internal.h"

#import "ShadowlistViewComponentDescriptor.h"
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

using namespace facebook::react;

/* Long-press drag-to-reorder. */
@implementation ShadowlistView (DragReorder)

/* The view's resting frame, ignoring any active drag translation. */
- (CGRect)restingFrameForView:(UIView *)view
{
  CGSize size = view.bounds.size;
  CGPoint center = view.center;
  return CGRectMake(center.x - size.width / 2.0, center.y - size.height / 2.0, size.width, size.height);
}

/* Topmost element view whose resting frame contains the content-space point. */
- (UIView *)elementViewAtContentPoint:(CGPoint)point
{
  UIView *result = nil;
  for (UIView *sub in _contentView.subviews) {
    if (![sub conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
      continue;
    }
    if (CGRectContainsPoint([self restingFrameForView:sub], point)) {
      result = sub;
    }
  }
  return result;
}

- (void)handleDragGesture:(UILongPressGestureRecognizer *)gesture
{
  switch (gesture.state) {
    case UIGestureRecognizerStateBegan:
      [self beginDrag:gesture];
      break;
    case UIGestureRecognizerStateChanged:
      _dragTouchInViewport = [gesture locationInView:self];
      [self updateDrag];
      break;
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed:
      [self finishDrag];
      break;
    default:
      break;
  }
}

- (void)beginDrag:(UILongPressGestureRecognizer *)gesture
{
  CGPoint contentPoint = [gesture locationInView:_contentView];
  UIView *view = [self elementViewAtContentPoint:contentPoint];
  NSInteger index = [self indexOfElementView:view];
  if (!view || index == NSNotFound) {
    return;
  }

  // Drop any leftover transforms from a previous drag.
  _dragDropPending = NO;
  _droppedView = nil;
  [self clearDragTransforms];

  _dragging = YES;
  _draggedView = view;
  _dragOriginIndex = index;
  _dragInsertionIndex = index;

  CGRect resting = [self restingFrameForView:view];
  CGFloat restingLeading = _horizontal ? resting.origin.x : resting.origin.y;
  _draggedExtent = _horizontal ? resting.size.width : resting.size.height;
  CGFloat touchAxis = _horizontal ? contentPoint.x : contentPoint.y;
  _dragGrabOffset = touchAxis - restingLeading;
  _dragTouchInViewport = [gesture locationInView:self];

  // Auto-scroll drives the offset; disable the scroll view's own finger tracking.
  _scrollView.scrollEnabled = NO;

  // Lift feedback: raise the row and give it a shadow.
  [_contentView bringSubviewToFront:view];
  view.layer.shadowColor = [UIColor blackColor].CGColor;
  view.layer.shadowOpacity = 0.25;
  view.layer.shadowRadius = 8.0;
  view.layer.shadowOffset = CGSizeMake(0.0, 4.0);

  // Drag start: keeps this row mounted while auto-scroll carries it off-screen.
  [self dispatchDragEventType:1 from:index to:index];

  _dragDisplayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(dragTick)];
  [_dragDisplayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

  [self updateDrag];
}

/* Per-frame: auto-scroll at the edges, then re-place the row and shuffle siblings. */
- (void)dragTick
{
  if (!_dragging) {
    return;
  }
  [self applyDragAutoScroll];
  [self updateDrag];
}

- (void)applyDragAutoScroll
{
  CGFloat window = _horizontal ? _scrollView.bounds.size.width : _scrollView.bounds.size.height;
  CGFloat content = _horizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height;
  CGFloat maxOffset = MAX(0.0, content - window);
  CGFloat offset = _horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  CGFloat touch = _horizontal ? _dragTouchInViewport.x : _dragTouchInViewport.y;

  static const CGFloat kEdge = 90.0;
  static const CGFloat kMaxSpeed = 16.0;
  CGFloat delta = 0.0;
  if (touch < kEdge) {
    delta = -kMaxSpeed * (1.0 - touch / kEdge);
  } else if (touch > window - kEdge) {
    delta = kMaxSpeed * (1.0 - (window - touch) / kEdge);
  }
  if (delta == 0.0) {
    return;
  }

  CGFloat newOffset = MIN(MAX(offset + delta, 0.0), maxOffset);
  if (newOffset == offset) {
    return;
  }

  CGPoint next = _horizontal
    ? CGPointMake(newOffset, _scrollView.contentOffset.y)
    : CGPointMake(_scrollView.contentOffset.x, newOffset);
  // Must report as a user scroll so the core virtualizes at this exact offset;
  // echo-suppressing here blanks rows mid-drag.
  _scrollView.contentOffset = next;
}

/* Place the row under the finger, recompute the insertion point, shuffle siblings. */
- (void)updateDrag
{
  UIView *view = _draggedView;
  if (!_dragging || !view) {
    return;
  }

  CGFloat offset = _horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  CGFloat touchViewport = _horizontal ? _dragTouchInViewport.x : _dragTouchInViewport.y;
  CGFloat touchContent = touchViewport + offset;

  CGRect resting = [self restingFrameForView:view];
  CGFloat restingLeading = _horizontal ? resting.origin.x : resting.origin.y;
  CGFloat extent = _horizontal ? resting.size.width : resting.size.height;
  CGFloat contentExtent = _horizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height;

  CGFloat desiredLeading = touchContent - _dragGrabOffset;
  desiredLeading = MAX(0.0, MIN(desiredLeading, MAX(0.0, contentExtent - extent)));
  _dragLeading = desiredLeading;

  CGFloat translation = desiredLeading - restingLeading;
  view.transform = _horizontal
    ? CGAffineTransformMakeTranslation(translation, 0.0)
    : CGAffineTransformMakeTranslation(0.0, translation);

  _dragInsertionIndex = [self insertionIndexForCenter:(desiredLeading + extent / 2.0)];
  [self applyDragShuffle];
}

/*
 * Insertion index for the dragged row's centre: the farthest neighbour whose midpoint
 * the centre has crossed, measured against the fixed pickup index.
 */
- (NSInteger)insertionIndexForCenter:(CGFloat)center
{
  NSInteger insertion = _dragOriginIndex;
  for (UIView *sub in _contentView.subviews) {
    if (sub == _draggedView) {
      continue;
    }
    NSInteger idx = [self indexOfElementView:sub];
    if (idx == NSNotFound) {
      continue;
    }
    CGRect r = [self restingFrameForView:sub];
    CGFloat lead = _horizontal ? r.origin.x : r.origin.y;
    CGFloat extent = _horizontal ? r.size.width : r.size.height;
    CGFloat mid = lead + extent / 2.0;
    if (idx > _dragOriginIndex && center > mid) {
      insertion = MAX(insertion, idx);
    } else if (idx < _dragOriginIndex && center < mid) {
      insertion = MIN(insertion, idx);
    }
  }
  return insertion;
}

/*
 * Open a one-row gap at the insertion point by shifting the siblings between pickup
 * and insertion by the picked-up row's extent, onto their post-reorder positions.
 */
- (void)applyDragShuffle
{
  for (UIView *sub in _contentView.subviews) {
    if (sub == _draggedView ||
        ![sub conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
      continue;
    }
    NSInteger idx = [self indexOfElementView:sub];
    if (idx == NSNotFound) {
      continue;
    }
    CGFloat shift = 0.0;
    if (_dragOriginIndex < _dragInsertionIndex && idx > _dragOriginIndex && idx <= _dragInsertionIndex) {
      shift = -_draggedExtent;
    } else if (_dragInsertionIndex < _dragOriginIndex && idx >= _dragInsertionIndex && idx < _dragOriginIndex) {
      shift = _draggedExtent;
    }
    sub.transform = _horizontal
      ? CGAffineTransformMakeTranslation(shift, 0.0)
      : CGAffineTransformMakeTranslation(0.0, shift);
  }
}

/*
 * After the reorder lands: clear the siblings instantly and animate the dropped row
 * from its release point (_dropReleaseLeading, content space) into its slot.
 */
- (void)settleDroppedView:(UIView *)view
{
  if (!view) {
    [self clearDragTransforms];
    return;
  }

  for (UIView *sub in _contentView.subviews) {
    if (sub == view || ![sub conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
      continue;
    }
    sub.transform = CGAffineTransformIdentity;
    sub.layer.shadowOpacity = 0.0;
  }

  CGRect resting = [self restingFrameForView:view];
  CGFloat newResting = _horizontal ? resting.origin.x : resting.origin.y;
  CGFloat startTranslation = _dropReleaseLeading - newResting;
  view.transform = _horizontal
    ? CGAffineTransformMakeTranslation(startTranslation, 0.0)
    : CGAffineTransformMakeTranslation(0.0, startTranslation);

  [UIView animateWithDuration:0.18
                        delay:0.0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     view.transform = CGAffineTransformIdentity;
                   }
                   completion:^(BOOL finished) {
                     view.layer.shadowOpacity = 0.0;
                   }];
}

/* Reset every element view's drag transform and lift shadow. */
- (void)clearDragTransforms
{
  for (UIView *sub in _contentView.subviews) {
    if (![sub conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
      continue;
    }
    // Cancel any in-flight drop-settle animation before a fresh pickup.
    [sub.layer removeAllAnimations];
    sub.transform = CGAffineTransformIdentity;
    sub.layer.shadowOpacity = 0.0;
  }
}

- (void)dispatchDragEventType:(int)type from:(NSInteger)from to:(NSInteger)to
{
  if (!_state) {
    return;
  }
  auto data = _state->getData();
  data.dragEventNonce_ = data.dragEventNonce_ + 1;
  data.dragEventType_ = (double)type;
  data.dragFromIndex_ = (double)from;
  data.dragToIndex_ = (double)to;
  // Disable scroll corrections during the drag; cleared on the end event (type 3).
  data.userScrolled_ = (type != 3);
  _state->updateState(std::move(data));
}

- (void)finishDrag
{
  if (!_dragging) {
    return;
  }

  [_dragDisplayLink invalidate];
  _dragDisplayLink = nil;
  _scrollView.scrollEnabled = YES;

  NSInteger from = _dragOriginIndex;
  NSInteger to = _dragInsertionIndex;
  UIView *view = _draggedView;
  _dropReleaseLeading = _dragLeading;
  _dragging = NO;
  _draggedView = nil;

  // Emit the reorder; hold the shuffle transforms until the commit lands.
  [self dispatchDragEventType:3 from:from to:to];

  if (from == to || !view) {
    // Dropped where it started: settle immediately, no commit will move the index.
    [self clearDragTransforms];
    _dragDropPending = NO;
    _droppedView = nil;
  } else {
    _dragDropPending = YES;
    _droppedView = view;
    _dropInsertionIndex = to;

    // Poll for the landing: a same-size reorder may publish no new state, so detect
    // the dropped row's index reaching its slot directly.
    [_dropSettleLink invalidate];
    _dropSettleLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(dropSettleTick)];
    [_dropSettleLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

    // Safety net: clear the held transforms if the reorder never lands. The token
    // invalidates this block if a newer drop supersedes it, so a stale timer can't tear
    // down a fresh drag's settle.
    NSInteger settleToken = ++_dropSettleToken;
    __weak ShadowlistView *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      ShadowlistView *strongSelf = weakSelf;
      if (strongSelf && settleToken == strongSelf->_dropSettleToken &&
          strongSelf->_dragDropPending && !strongSelf->_dragging) {
        [strongSelf->_dropSettleLink invalidate];
        strongSelf->_dropSettleLink = nil;
        [strongSelf clearDragTransforms];
        strongSelf->_dragDropPending = NO;
        strongSelf->_droppedView = nil;
      }
    });
  }
}

/*
 * Per-frame after a drop: wait for the dropped row's index to reach its slot, then
 * animate it into place. Self-stops once handled.
 */
- (void)dropSettleTick
{
  if (!_dragDropPending) {
    [_dropSettleLink invalidate];
    _dropSettleLink = nil;
    return;
  }
  if (_droppedView == nil) {
    // Dropped row unmounted off-screen: nothing to settle visually.
    [self clearDragTransforms];
    _dragDropPending = NO;
    [_dropSettleLink invalidate];
    _dropSettleLink = nil;
    return;
  }
  if ([self indexOfElementView:_droppedView] == _dropInsertionIndex) {
    UIView *view = _droppedView;
    _dragDropPending = NO;
    _droppedView = nil;
    [_dropSettleLink invalidate];
    _dropSettleLink = nil;
    [self settleDroppedView:view];
  }
}

/* Immediate teardown with no reorder (view recycle / drag disabled). */
- (void)teardownDrag
{
  [_dragDisplayLink invalidate];
  _dragDisplayLink = nil;
  [_dropSettleLink invalidate];
  _dropSettleLink = nil;
  [_droppedView.layer removeAllAnimations];
  _dragging = NO;
  _draggedView = nil;
  _dragDropPending = NO;
  _droppedView = nil;
  _scrollView.scrollEnabled = YES;
  [self clearDragTransforms];
}

@end
