#include <TargetConditionals.h>
#import <QuartzCore/QuartzCore.h>
#import "ShadowListView.h"
#import "ShadowListView+Internal.h"
#import "ShadowListElementView.h"

#import "ShadowListViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

using namespace facebook::react;

/*
 * Long press and drag to reorder rows.
 */
/*
 * Give the lifted row's shadow an explicit shape. Without one, Core Animation renders the
 * row offscreen every frame to find its outline. Only rebuilt when the size changes.
 */
static void SLUpdateDragShadowPath(RCTUIView *view)
{
  CGRect bounds = view.bounds;
  CGPathRef current = view.layer.shadowPath;
  if (current && CGRectEqualToRect(CGPathGetBoundingBox(current), bounds)) {
    return;
  }
  CGPathRef path = CGPathCreateWithRoundedRect(bounds, view.layer.cornerRadius, view.layer.cornerRadius, NULL);
  view.layer.shadowPath = path;
  CGPathRelease(path);
}

/*
 * A drag key for JS, empty when the row has none.
 */
static NSString *SLDragKey(const std::string& key)
{
  return [NSString stringWithUTF8String:key.c_str()] ?: @"";
}

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
  NSString *key = [self keyOfElementView:view];
  return {
    (long)index,
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
- (RCTUIView *)elementViewAtContentPoint:(CGPoint)point
{
  RCTUIView *result = nil;
  for (RCTUIView *subview in _contentView.subviews) {
    if (![subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
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
      [self beginDragAtPoint:[gesture locationInView:self]];
      break;
    case SLGestureStateChanged:
      _dragTouchInViewport = [gesture locationInView:self];
      [self updateDrag];
      break;
    case SLGestureStateEnded:
      [self finishDrag];
      break;
    case SLGestureStateCancelled:
    case SLGestureStateFailed:
      // The system or dragEnabled turning off took the touch. Put the row back, no reorder.
      [self cancelDrag];
      break;
    default:
      break;
  }
}

/*
 * For scripted runs, like -SLAutoDrag in the example app. Drives the same path as the long
 * press with a point in this view: phase 1 picks up, 2 moves and 3 drops.
 */
- (void)debugDragPhase:(NSNumber *)phase point:(NSValue *)point
{
  if (!_dragEnabled) {
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
      _dragTouchInViewport = location;
      [self updateDrag];
      break;
    default:
      [self finishDrag];
      break;
  }
}

- (void)beginDragAtPoint:(CGPoint)location
{
  CGPoint contentPoint = [self convertPoint:location toView:_contentView];
  RCTUIView *view = [self elementViewAtContentPoint:contentPoint];
  NSInteger index = [self indexOfElementView:view];
  if (!view || index == NSNotFound) {
    return;
  }

  // Clear anything left over from the last drag.
  _dragDropPending = NO;
  _droppedView = nil;
  [self clearDragTransforms];

  _dragging = YES;
  _draggedView = view;
  azimgd::shadowlist::DragRow resting = [self dragRowForView:view index:index];
  if (_columns > 1) {
    _drag.beginCell(
      resting,
      _horizontal ? contentPoint.x : contentPoint.y,
      _horizontal ? contentPoint.y : contentPoint.x,
      (std::size_t)_columns);
  } else {
    _drag.begin(resting.index, resting.key, resting.leading, resting.extent, _horizontal ? contentPoint.x : contentPoint.y);
  }
  _dragTouchInViewport = location;

  // We scroll ourselves near the edges. Stop the scroll view from following the finger.
  _scrollView.scrollEnabled = NO;

  // Lift the row with a shadow so it looks picked up.
  SLRaiseSubview(_contentView, view, 4.0);
  // The row now sits above the sticky views. The next pin raises them again.
  _stickyOrderDirty = YES;
  view.layer.shadowColor = [RCTUIColor blackColor].CGColor;
  view.layer.shadowOpacity = 0.25;
  view.layer.shadowRadius = 8.0;
  view.layer.shadowOffset = CGSizeMake(0.0, 4.0);
  SLUpdateDragShadowPath(view);

  // Tell the core a drag started so this row stays mounted when it scrolls off screen.
  NSString *originKey = SLDragKey(_drag.originKey());
  [self commitDragEventType:azimgd::shadowlist::DRAG_EVENT_START fromKey:originKey toKey:originKey];

  _dragDisplayLink = [SLDisplayLink displayLinkWithTarget:self selector:@selector(dragTick)];
  [_dragDisplayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

  [self updateDrag];
}

/*
 * Every frame, scroll near the edges, then move the row and shift the others.
 */
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
  CGFloat newOffset = azimgd::shadowlist::dragAutoScrollOffset(
    azimgd::shadowlist::DRAG_AUTO_SCROLL_IOS, touch, window, offset, maxOffset);
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
  NSInteger currentIndex = [self indexOfElementView:view];
  NSString *currentKey = [self keyOfElementView:view];
  _drag.updateOrigin(
    currentIndex == NSNotFound ? -1 : (long)currentIndex, currentKey ? std::string(currentKey.UTF8String) : std::string());

  CGPoint touchContent = CGPointMake(
    _dragTouchInViewport.x + _scrollView.contentOffset.x, _dragTouchInViewport.y + _scrollView.contentOffset.y);
  azimgd::shadowlist::DragRow resting = [self dragRowForView:view index:_drag.originIndex()];
  azimgd::shadowlist::DragOffset translation;
  if (_drag.isGrid()) {
    translation = _drag.placeCell(
      _horizontal ? touchContent.x : touchContent.y,
      _horizontal ? touchContent.y : touchContent.x,
      resting,
      _horizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height,
      // The scroll view's content size is 0 across the scroll axis. Use the content view.
      _horizontal ? _contentView.bounds.size.height : _contentView.bounds.size.width);
  } else {
    translation.leading = _drag.place(
      _horizontal ? touchContent.x : touchContent.y,
      resting.leading,
      resting.extent,
      _horizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height);
  }

  // The row can change size while held.
  SLUpdateDragShadowPath(view);

  view.transform = [self dragTransformForOffset:translation];

  // Find where it would drop among the other mounted rows.
  std::vector<azimgd::shadowlist::DragRow> rows;
  rows.reserve(_contentView.subviews.count);
  for (RCTUIView *subview in _contentView.subviews) {
    if (subview == _draggedView) {
      continue;
    }
    NSInteger elementIndex = [self indexOfElementView:subview];
    if (elementIndex == NSNotFound) {
      continue;
    }
    rows.push_back([self dragRowForView:subview index:elementIndex]);
  }
  _drag.updateInsertion(rows);
  [self applyDragShuffle];
}

/*
 * Open a gap at the drop spot by shifting the rows in between by the dragged row's size.
 * In a grid each cell moves to its new resting place, which can be in another column.
 */
- (void)applyDragShuffle
{
  for (RCTUIView *subview in _contentView.subviews) {
    if (subview == _draggedView ||
        ![subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
      continue;
    }
    NSInteger elementIndex = [self indexOfElementView:subview];
    if (elementIndex == NSNotFound) {
      continue;
    }
    subview.transform = [self dragTransformForOffset:_drag.offsetFor((long)elementIndex)];
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
    if (subview == view || ![subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
      continue;
    }
    subview.transform = CGAffineTransformIdentity;
    subview.layer.shadowOpacity = 0.0;
    subview.layer.shadowPath = nil;
  }

  CGRect resting = [self restingFrameForView:view];
  azimgd::shadowlist::DragOffset start;
  start.leading = _dropReleaseLeading - (_horizontal ? resting.origin.x : resting.origin.y);
  if (_drag.isGrid()) {
    start.cross = _dropReleaseCross - (_horizontal ? resting.origin.y : resting.origin.x);
  }
  view.transform = [self dragTransformForOffset:start];

#if TARGET_OS_OSX
  CATransform3D released = view.layer.transform;
  view.transform = CGAffineTransformIdentity;
  CABasicAnimation *settle = [CABasicAnimation animationWithKeyPath:@"transform"];
  settle.fromValue = [NSValue valueWithCATransform3D:released];
  settle.toValue = [NSValue valueWithCATransform3D:CATransform3DIdentity];
  settle.duration = 0.18;
  settle.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
  __weak ShadowListView *weakSelf = self;
  __weak RCTUIView *weakView = view;
  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    ShadowListView *strongSelf = weakSelf;
    RCTUIView *settled = weakView;
    if (!strongSelf || !settled || (strongSelf->_dragging && strongSelf->_draggedView == settled)) {
      return;
    }
    settled.layer.shadowOpacity = 0.0;
    settled.layer.shadowPath = nil;
    settled.layer.zPosition = 0.0;
  }];
  [view.layer addAnimation:settle forKey:@"transform"];
  [CATransaction commit];
#else
  [UIView animateWithDuration:0.18
                        delay:0.0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     view.transform = CGAffineTransformIdentity;
                   }
                   completion:^(BOOL finished) {
                     view.layer.shadowOpacity = 0.0;
                     view.layer.shadowPath = nil;
                   }];
#endif
}

/*
 * Remove the drag offset and shadow from every row.
 */
- (void)clearDragTransforms
{
  for (RCTUIView *subview in _contentView.subviews) {
    if (![subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
      continue;
    }
    // Stop any drop animation still running before a new pickup.
    [subview.layer removeAllAnimations];
    subview.transform = CGAffineTransformIdentity;
    subview.layer.shadowOpacity = 0.0;
    subview.layer.shadowPath = nil;
#if TARGET_OS_OSX
    subview.layer.zPosition = 0.0;
#endif
  }
}

- (void)finishDrag
{
  if (!_dragging) {
    return;
  }

  [_dragDisplayLink invalidate];
  _dragDisplayLink = nil;
  _scrollView.scrollEnabled = YES;

  NSInteger from = _drag.originIndex();
  NSInteger to = _drag.insertionIndex();
  RCTUIView *view = _draggedView;
  _dropReleaseLeading = _drag.leading();
  _dropReleaseCross = _drag.crossLeading();
  _dragging = NO;
  _draggedView = nil;

  /*
   * Send the reorder by key and keep the rows shifted until the commit lands.
   * The indexes below still drive the settle animation.
   */
  [self commitDragEventType:azimgd::shadowlist::DRAG_EVENT_END
                    fromKey:SLDragKey(_drag.originKey())
                      toKey:SLDragKey(_drag.insertionKey())];

  if (from == to || !view) {
    // Dropped where it started. No commit will come. Settle now.
    [self clearDragTransforms];
    _dragDropPending = NO;
    _droppedView = nil;
  } else {
    _dragDropPending = YES;
    _droppedView = view;
    _dropInsertionIndex = to;

    /*
     * Check each frame for the landing. A reorder of same size rows may not publish
     * new state. Watch for the row's index to reach its new spot.
     */
    [_dropSettleLink invalidate];
    _dropSettleLink = [SLDisplayLink displayLinkWithTarget:self selector:@selector(dropSettleTick)];
    [_dropSettleLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

    /*
     * Fallback if the reorder never lands. The token stops an old timer from
     * clearing a newer drop.
     */
    NSInteger settleToken = ++_dropSettleToken;
    __weak ShadowListView *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      ShadowListView *strongSelf = weakSelf;
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
 * After a drop, wait each frame for the row to reach its new index, then animate it
 * into place and stop.
 */
- (void)dropSettleTick
{
  if (!_dragDropPending) {
    [_dropSettleLink invalidate];
    _dropSettleLink = nil;
    return;
  }
  if (_droppedView == nil) {
    // The dropped row went off screen and unmounted. There is nothing to animate.
    [self clearDragTransforms];
    _dragDropPending = NO;
    [_dropSettleLink invalidate];
    _dropSettleLink = nil;
    return;
  }
  if ([self indexOfElementView:_droppedView] == _dropInsertionIndex) {
    RCTUIView *view = _droppedView;
    _dragDropPending = NO;
    _droppedView = nil;
    [_dropSettleLink invalidate];
    _dropSettleLink = nil;
    [self settleDroppedView:view];
  }
}

/*
 * Abort a live drag without a reorder, when the gesture is cancelled, drag is turned off or
 * the held row is deleted. The end event goes out with the origin key on both sides. It
 * turns core corrections back on and clears the held key in JS without moving anything.
 * This can run inside a mount, where an immediate commit is not allowed. Send it async.
 */
- (void)cancelDrag
{
  if (_dragging && _state) {
    NSString *originKey = SLDragKey(_drag.originKey());
    BOOL wasInMountObserver = _inMountObserver;
    _inMountObserver = YES;
    [self commitDragEventType:azimgd::shadowlist::DRAG_EVENT_END fromKey:originKey toKey:originKey];
    _inMountObserver = wasInMountObserver;
  }
  [self teardownDrag];
}

/*
 * Stop everything without a reorder or an end event. Used directly only on recycle, where
 * the state belongs to the old list.
 */
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

#pragma mark - Accessibility

/*
 * For VoiceOver, give each row Move up and Move down actions while drag is on.
 * The handlers read the row's index and key when used. Data changes never leave them stale.
 */
- (void)applyDragAccessibilityActionsToView:(RCTUIView *)view
{
  if (![view isKindOfClass:[ShadowListElementView class]]) {
    return;
  }
  ShadowListElementView *elementView = (ShadowListElementView *)view;
  if (!_dragEnabled) {
    elementView.nativeAccessibilityActions = nil;
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
  elementView.nativeAccessibilityActions = @[ moveUp, moveDown ];
}

/*
 * Swap the row with the nearest mounted row above or below. It goes through the same
 * path as a real drop. onDragEnd and useDragReorder handle it as usual.

 */
- (BOOL)performAccessibilityMove:(RCTUIView *)view up:(BOOL)up
{
  NSInteger index = [self indexOfElementView:view];
  if (index == NSNotFound) {
    return NO;
  }
  NSString *key = [self keyOfElementView:view];
  if (!key) {
    return NO;
  }

  RCTUIView *neighbor = nil;
  NSInteger neighborIndex = up ? NSIntegerMin : NSIntegerMax;
  for (RCTUIView *subview in _contentView.subviews) {
    if (subview == view) {
      continue;
    }
    NSInteger subviewIndex = [self indexOfElementView:subview];
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
  NSString *neighborKey = [self keyOfElementView:neighbor];
  if (!neighborKey) {
    return NO;
  }

  [self commitDragEventType:azimgd::shadowlist::DRAG_EVENT_END fromKey:key toKey:neighborKey];
  return YES;
}

@end
