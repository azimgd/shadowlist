#include <TargetConditionals.h>

// Drag to reorder needs UIKit gestures, display links and animations, so it is iOS only.
#if !TARGET_OS_OSX

#import "ShadowListView.h"
#import "ShadowListView+Internal.h"

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
static void SLUpdateDragShadowPath(UIView *view)
{
  CGRect bounds = view.bounds;
  CGPathRef current = view.layer.shadowPath;
  if (current && CGRectEqualToRect(CGPathGetBoundingBox(current), bounds)) {
    return;
  }
  view.layer.shadowPath =
    [UIBezierPath bezierPathWithRoundedRect:bounds cornerRadius:view.layer.cornerRadius].CGPath;
}

static NSString *SLDragKey(const std::string& key)
{
  return [NSString stringWithUTF8String:key.c_str()] ?: @"";
}

@implementation ShadowListView (DragReorder)

#pragma mark - Drag gesture

/*
 * Where the view sits without any drag offset applied.
 */
- (CGRect)restingFrameForView:(UIView *)view
{
  CGSize size = view.bounds.size;
  CGPoint center = view.center;
  return CGRectMake(center.x - size.width / 2.0, center.y - size.height / 2.0, size.width, size.height);
}

/*
 * The topmost row under a point in the content.
 */
- (UIView *)elementViewAtContentPoint:(CGPoint)point
{
  UIView *result = nil;
  for (UIView *subview in _contentView.subviews) {
    if (![subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
      continue;
    }
    if (CGRectContainsPoint([self restingFrameForView:subview], point)) {
      result = subview;
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

  // Clear anything left over from the last drag.
  _dragDropPending = NO;
  _droppedView = nil;
  [self clearDragTransforms];

  _dragging = YES;
  _draggedView = view;
  NSString *key = [self keyOfElementView:view] ?: @"";
  CGRect resting = [self restingFrameForView:view];
  _drag.begin(
    (long)index,
    std::string(key.UTF8String),
    _horizontal ? resting.origin.x : resting.origin.y,
    _horizontal ? resting.size.width : resting.size.height,
    _horizontal ? contentPoint.x : contentPoint.y);
  _dragTouchInViewport = [gesture locationInView:self];

  // We scroll ourselves near the edges, so stop the scroll view from following the finger.
  _scrollView.scrollEnabled = NO;

  // Lift the row with a shadow so it looks picked up.
  [_contentView bringSubviewToFront:view];
  // The row now sits above the sticky views. The next pin raises them again.
  _stickyOrderDirty = YES;
  view.layer.shadowColor = [UIColor blackColor].CGColor;
  view.layer.shadowOpacity = 0.25;
  view.layer.shadowRadius = 8.0;
  view.layer.shadowOffset = CGSizeMake(0.0, 4.0);
  SLUpdateDragShadowPath(view);

  // Tell the core a drag started so this row stays mounted when it scrolls off screen.
  NSString *originKey = SLDragKey(_drag.originKey());
  [self dispatchDragEventType:1 fromKey:originKey toKey:originKey];

  _dragDisplayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(dragTick)];
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
  UIView *view = _draggedView;
  if (!_dragging || !view) {
    return;
  }

  /*
   * Read the dragged row's index and key again each time. A data change during the drag
   * can move them, and the drop math must use the current values, not the ones from pickup.
   */
  NSInteger currentIndex = [self indexOfElementView:view];
  NSString *currentKey = [self keyOfElementView:view];
  _drag.updateOrigin(currentIndex == NSNotFound ? -1 : (long)currentIndex, currentKey ? std::string(currentKey.UTF8String) : std::string());

  CGFloat offset = _horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  CGFloat touchViewport = _horizontal ? _dragTouchInViewport.x : _dragTouchInViewport.y;
  CGRect resting = [self restingFrameForView:view];
  CGFloat translation = _drag.place(
    touchViewport + offset,
    _horizontal ? resting.origin.x : resting.origin.y,
    _horizontal ? resting.size.width : resting.size.height,
    _horizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height);

  // The row can change size while held.
  SLUpdateDragShadowPath(view);

  view.transform = _horizontal
    ? CGAffineTransformMakeTranslation(translation, 0.0)
    : CGAffineTransformMakeTranslation(0.0, translation);

  // Find where it would drop among the other mounted rows.
  std::vector<azimgd::shadowlist::DragRow> rows;
  rows.reserve(_contentView.subviews.count);
  for (UIView *subview in _contentView.subviews) {
    if (subview == _draggedView) {
      continue;
    }
    NSInteger elementIndex = [self indexOfElementView:subview];
    if (elementIndex == NSNotFound) {
      continue;
    }
    CGRect restingFrame = [self restingFrameForView:subview];
    NSString *key = [self keyOfElementView:subview];
    rows.push_back({
      (long)elementIndex,
      key ? std::string(key.UTF8String) : std::string(),
      _horizontal ? restingFrame.origin.x : restingFrame.origin.y,
      _horizontal ? restingFrame.size.width : restingFrame.size.height});
  }
  _drag.updateInsertion(rows);
  [self applyDragShuffle];
}

/*
 * Open a gap at the drop spot by shifting the rows in between by the dragged row's size.
 */
- (void)applyDragShuffle
{
  for (UIView *subview in _contentView.subviews) {
    if (subview == _draggedView ||
        ![subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
      continue;
    }
    NSInteger elementIndex = [self indexOfElementView:subview];
    if (elementIndex == NSNotFound) {
      continue;
    }
    CGFloat shift = _drag.shiftFor((long)elementIndex);
    subview.transform = _horizontal
      ? CGAffineTransformMakeTranslation(shift, 0.0)
      : CGAffineTransformMakeTranslation(0.0, shift);
  }
}

/*
 * Once the reorder lands, reset the other rows right away and animate the dropped row
 * from where it was released into its place.
 */
- (void)settleDroppedView:(UIView *)view
{
  if (!view) {
    [self clearDragTransforms];
    return;
  }

  for (UIView *subview in _contentView.subviews) {
    if (subview == view || ![subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
      continue;
    }
    subview.transform = CGAffineTransformIdentity;
    subview.layer.shadowOpacity = 0.0;
    subview.layer.shadowPath = nil;
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
                     view.layer.shadowPath = nil;
                   }];
}

/*
 * Remove the drag offset and shadow from every row.
 */
- (void)clearDragTransforms
{
  for (UIView *subview in _contentView.subviews) {
    if (![subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
      continue;
    }
    // Stop any drop animation still running before a new pickup.
    [subview.layer removeAllAnimations];
    subview.transform = CGAffineTransformIdentity;
    subview.layer.shadowOpacity = 0.0;
    subview.layer.shadowPath = nil;
  }
}

- (void)dispatchDragEventType:(int)type fromKey:(NSString *)fromKey toKey:(NSString *)toKey
{
  [self commitDragEventType:type fromKey:fromKey toKey:toKey];
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
  UIView *view = _draggedView;
  _dropReleaseLeading = _drag.leading();
  _dragging = NO;
  _draggedView = nil;

  /*
   * Send the reorder by key and keep the rows shifted until the commit lands.
   * The indexes below still drive the settle animation.
   */
  [self dispatchDragEventType:3 fromKey:SLDragKey(_drag.originKey()) toKey:SLDragKey(_drag.insertionKey())];

  if (from == to || !view) {
    // Dropped where it started, so no commit will come. Settle now.
    [self clearDragTransforms];
    _dragDropPending = NO;
    _droppedView = nil;
  } else {
    _dragDropPending = YES;
    _droppedView = view;
    _dropInsertionIndex = to;

    /*
     * Check each frame for the landing. A reorder of same size rows may not publish
     * new state, so watch for the row's index to reach its new spot.
     */
    [_dropSettleLink invalidate];
    _dropSettleLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(dropSettleTick)];
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
    // The dropped row went off screen and unmounted, so there is nothing to animate.
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

/*
 * Stop everything without a reorder, when the view is recycled or drag is turned off.
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
 * The handlers read the row's index and key when used, so data changes never leave them stale.
 */
- (void)applyDragAccessibilityActionsToView:(UIView *)view
{
  if (!_dragEnabled) {
    view.accessibilityCustomActions = nil;
    return;
  }

  __weak ShadowListView *weakSelf = self;
  __weak UIView *weakView = view;
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
  view.accessibilityCustomActions = @[ moveUp, moveDown ];
}

/*
 * Swap the row with the nearest mounted row above or below. It goes through the same
 * path as a real drop, so onDragEnd and useDragReorder handle it as usual.
 */
- (BOOL)performAccessibilityMove:(UIView *)view up:(BOOL)up
{
  NSInteger index = [self indexOfElementView:view];
  if (index == NSNotFound) {
    return NO;
  }
  NSString *key = [self keyOfElementView:view];
  if (!key) {
    return NO;
  }

  UIView *neighbor = nil;
  NSInteger neighborIndex = up ? NSIntegerMin : NSIntegerMax;
  for (UIView *subview in _contentView.subviews) {
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

  [self dispatchDragEventType:3 fromKey:key toKey:neighborKey];
  return YES;
}

@end

#endif // !TARGET_OS_OSX
