#import "ShadowlistView.h"

#import "ShadowlistViewComponentDescriptor.h"
#import <react/renderer/components/ShadowlistViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowlistViewSpec/Props.h>
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"

#include <cmath>
#include <vector>

using namespace facebook::react;

/*
 * A scrollViewDidScroll offset within this many points of the offset the core
 * applied is its own echo, not a user scroll.
 */
static const CGFloat kScrollEchoTolerance = 2.0;

@interface ShadowlistView () <RCTShadowlistViewViewProtocol, UIScrollViewDelegate>

@end

@implementation ShadowlistView {
  ShadowlistViewShadowNode::ConcreteState::Shared _state;
  UIScrollView * _scrollView;
  UIView * _contentView;

  /*
   * Sticky header/footer are pinned natively here (not in the core layout) so they
   * track scrolling on the UI thread without the commit-cycle latency that made the
   * core-driven version choppy. The template views keep their resting position from
   * the shadow node; we layer a translation on top each scroll frame.
   */
  BOOL _stickyHeader;
  BOOL _stickyFooter;
  BOOL _horizontal;
  __weak UIView * _stickyHeaderView;
  __weak UIView * _stickyFooterView;

  /*
   * Sticky section headers (SectionList). Rather than pin a virtualized element
   * (which the JS layer would have to keep mounted, arriving several commits late
   * at each section boundary - the source of the scroll lag), the active section
   * header is rendered as an always-mounted overlay template (_sectionHeaderOverlay,
   * templateType "sectionHeader"). The core publishes the resting geometry (offset +
   * size along the scroll axis) of the in-flow section headers through state; we pin
   * the overlay to the viewport start every scroll tick (mirroring
   * Container::resolveStickyHeader), pushing it up as the next in-flow header
   * arrives. The overlay never unmounts, so its position never lags - only its
   * content swaps (in JS) at a boundary, masked by the real in-flow header behind it.
   */
  std::vector<int> _stickyHeaderIndices;
  std::vector<double> _stickyHeaderOffsets;
  std::vector<double> _stickyHeaderSizes;
  __weak UIView * _sectionHeaderOverlay;

  /*
   * The last offset the core applied itself. scrollViewDidScroll fires for both the
   * core's own setContentOffset and every user scroll, so we tell them apart by
   * comparing against this rather than isDragging/isDecelerating - those are only
   * set for touch gestures, so a trackpad / mouse-wheel scroll (e.g. the iOS
   * Simulator) would look programmatic and the core would never learn the user took
   * over, latching its inverted-pin / MVCP correction and blanking the list.
   */
  CGPoint _appliedOffset;
  BOOL _hasAppliedOffset;

  /*
   * Drag-to-reorder. The whole gesture runs on the UI thread - exactly like the
   * sticky pin - so it never waits on the commit cycle. A long press picks an
   * element up; while held we translate it under the finger every CADisplayLink
   * tick, auto-scroll when it nears a viewport edge, and detect when its centre
   * crosses into a neighbour's slot. Each crossing is relayed to JS (a live array
   * move) which re-commits the new order; the surviving rows flow to their new slots
   * via the normal layout, and the picked-up view re-glues to the finger against its
   * new resting frame. JS only ever sees onDragStart / onDragMove / onDragEnd.
   */
  BOOL _dragEnabled;
  UILongPressGestureRecognizer * _dragRecognizer;
  CADisplayLink * _dragDisplayLink;
  __weak UIView * _draggedView;
  BOOL _dragging;
  /*
   * The data order stays FIXED during the drag. _dragOriginIndex is where the row was
   * picked up; _dragInsertionIndex is where its centre currently sits (the gap), used
   * to shuffle the siblings between them. The single reorder is applied on drop.
   */
  NSInteger _dragOriginIndex;
  NSInteger _dragInsertionIndex;
  /* Size of the picked-up row along the scroll axis - the amount each shuffled
   * sibling is offset to open the gap. */
  CGFloat _draggedExtent;
  /*
   * Distance along the scroll axis from the picked-up cell's leading edge to the
   * touch point, so the cell stays under the same spot of the finger.
   */
  CGFloat _dragGrabOffset;
  /*
   * Latest touch location in this host's (viewport) coordinates - stable while the
   * content auto-scrolls under a still finger, unlike a content-space location.
   */
  CGPoint _dragTouchInViewport;
  /*
   * After a drop, hold the shuffle transforms until JS's single reorder commit lands
   * (the dropped row's index prop reaching _dropInsertionIndex), then clear them.
   */
  BOOL _dragDropPending;
  __weak UIView * _droppedView;
  NSInteger _dropInsertionIndex;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowlistViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowlistViewProps>();
    _props = defaultProps;

    _scrollView = [[UIScrollView alloc] init];
    _scrollView.delegate = self;
    _scrollView.showsVerticalScrollIndicator = YES;
    _scrollView.showsHorizontalScrollIndicator = YES;
    _scrollView.scrollEnabled = YES;
    _scrollView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    _scrollView.indicatorStyle = UIScrollViewIndicatorStyleWhite;

    _contentView = [[UIView alloc] init];
    [_scrollView addSubview:_contentView];

    self.contentView = _scrollView;

    /*
     * Long press to pick a row up. It lives on the scroll view so a quick swipe still
     * scrolls (the press has to settle for minimumPressDuration first); once it fires
     * we disable scrolling and drive the offset ourselves. Disabled until the
     * dragEnabled prop turns it on.
     */
    _dragRecognizer = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(handleDragGesture:)];
    _dragRecognizer.minimumPressDuration = 0.2;
    _dragRecognizer.enabled = NO;
    [_scrollView addGestureRecognizer:_dragRecognizer];
  }

  return self;
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if ([childComponentView conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
    const auto &childViewProps = *std::static_pointer_cast<ShadowlistElementViewProps const>(childComponentView.props);
    [_contentView insertSubview:childComponentView atIndex:childViewProps.index];
    // A newly mounted element can land above a pinned header/footer or section
    // header in z-order and cover it; re-pin so the active sticky views stay on top
    // (and a freshly mounted active section header gets its translation at once).
    [self applyStickyTransforms];
    // A row mounting mid-drag (auto-scroll) must not cover the picked-up row, and
    // needs its make-room shuffle offset applied at once so it appears in the right
    // place rather than flashing in at its resting slot first.
    if (_dragging && _draggedView) {
      [_contentView bringSubviewToFront:_draggedView];
      [self applyDragShuffle];
    }
    return;
  }

  if ([childComponentView conformsToProtocol:@protocol(RCTShadowlistTemplateViewViewProtocol)]) {
    const auto &templateProps = *std::static_pointer_cast<ShadowlistTemplateViewProps const>(childComponentView.props);
    if (templateProps.templateType == "footer") {
      _stickyFooterView = childComponentView;
    } else if (templateProps.templateType == "sectionHeader") {
      _sectionHeaderOverlay = childComponentView;
    } else {
      _stickyHeaderView = childComponentView;
    }
    [_contentView addSubview:childComponentView];
    [self applyStickyTransforms];
    return;
  }
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if (childComponentView == _stickyHeaderView) {
    _stickyHeaderView = nil;
  }
  if (childComponentView == _stickyFooterView) {
    _stickyFooterView = nil;
  }
  if (childComponentView == _sectionHeaderOverlay) {
    _sectionHeaderOverlay = nil;
  }
  [childComponentView removeFromSuperview];
}

- (void)prepareForRecycle
{
  _stickyHeaderView = nil;
  _stickyFooterView = nil;
  _stickyHeader = NO;
  _stickyFooter = NO;
  _horizontal = NO;
  _sectionHeaderOverlay = nil;
  _stickyHeaderIndices.clear();
  _stickyHeaderOffsets.clear();
  _stickyHeaderSizes.clear();
  [self teardownDrag];
  _dragEnabled = NO;
  _dragRecognizer.enabled = NO;
  [super prepareForRecycle];
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &nextProps = *std::static_pointer_cast<ShadowlistViewProps const>(props);
  _stickyHeader = nextProps.stickyHeader;
  _stickyFooter = nextProps.stickyFooter;
  _horizontal = nextProps.horizontal;
  _dragEnabled = nextProps.dragEnabled;
  _dragRecognizer.enabled = _dragEnabled;

  [super updateProps:props oldProps:oldProps];

  [self applyStickyTransforms];
}

/*
 * Pin the sticky header/footer to the viewport by translating them relative to
 * their resting frame. The header tracks the scroll offset (stays at the start);
 * the footer tracks offset + window - content so it stays at the viewport end and
 * lands exactly on its resting position at the scroll extreme. Runs on every scroll
 * tick on the UI thread, so it stays in lockstep with the finger.
 */
- (void)applyStickyTransforms
{
  CGPoint offset = _scrollView.contentOffset;
  CGSize window = _scrollView.bounds.size;
  CGSize content = _scrollView.contentSize;

  if (_stickyHeaderView) {
    CGFloat translation = _stickyHeader ? (_horizontal ? offset.x : offset.y) : 0.0;
    _stickyHeaderView.transform = _horizontal
      ? CGAffineTransformMakeTranslation(translation, 0.0)
      : CGAffineTransformMakeTranslation(0.0, translation);
  }

  if (_stickyFooterView) {
    CGFloat translation = 0.0;
    if (_stickyFooter) {
      translation = _horizontal
        ? offset.x + window.width - content.width
        : offset.y + window.height - content.height;
    }
    _stickyFooterView.transform = _horizontal
      ? CGAffineTransformMakeTranslation(translation, 0.0)
      : CGAffineTransformMakeTranslation(0.0, translation);
  }

  [self applyStickySectionHeaders];
  [self bringStickyViewsToFront];
}

/*
 * Pin the always-mounted section-header overlay to the viewport start, mirroring
 * Container::resolveStickyHeader. The overlay rests at the content origin and shows
 * the active section's header (its content is swapped in JS); here we only move it.
 * Walk the core-published in-flow header geometry for the active section (the last
 * header resting at/above the scroll offset) and the next one, pin the overlay to
 * the viewport start, and push it up as the next in-flow header scrolls in. When no
 * section is active (scrolled above the first header) the overlay is hidden, so the
 * regular list header shows through.
 */
- (void)applyStickySectionHeaders
{
  if (!_sectionHeaderOverlay) {
    return;
  }

  if (_stickyHeaderIndices.empty()) {
    _sectionHeaderOverlay.hidden = YES;
    return;
  }

  CGFloat axisOffset = _horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  if (axisOffset < 0.0) {
    axisOffset = 0.0;
  }

  /*
   * Headers are ascending by offset: the active one is the last resting at/above
   * the viewport start, and the first one past it is the "next" that pushes it up.
   */
  bool hasActive = false;
  double activeSize = 0.0;
  bool hasNext = false;
  double nextOffset = 0.0;
  for (size_t i = 0; i < _stickyHeaderOffsets.size(); i++) {
    double headerOffset = _stickyHeaderOffsets[i];
    if (headerOffset <= axisOffset) {
      hasActive = true;
      activeSize = _stickyHeaderSizes[i];
    } else {
      nextOffset = headerOffset;
      hasNext = true;
      break;
    }
  }

  if (!hasActive) {
    _sectionHeaderOverlay.hidden = YES;
    return;
  }

  /*
   * The overlay rests at the content origin, so its translation along the scroll
   * axis is simply its displayed top: pinned to the viewport start (axisOffset),
   * pushed up to nextOffset - activeSize as the next in-flow header arrives.
   */
  double translation = axisOffset;
  if (hasNext) {
    double pushedTop = nextOffset - activeSize;
    if (pushedTop < translation) {
      translation = pushedTop;
    }
  }

  _sectionHeaderOverlay.hidden = NO;
  _sectionHeaderOverlay.transform = _horizontal
    ? CGAffineTransformMakeTranslation(translation, 0.0)
    : CGAffineTransformMakeTranslation(0.0, translation);
  [_contentView bringSubviewToFront:_sectionHeaderOverlay];
}

/*
 * A pinned (sticky) header/footer must stay above the scrolling elements, which
 * mount continuously and would otherwise cover it.
 */
- (void)bringStickyViewsToFront
{
  if (_stickyHeader && _stickyHeaderView) {
    [_contentView bringSubviewToFront:_stickyHeaderView];
  }
  if (_stickyFooter && _stickyFooterView) {
    [_contentView bringSubviewToFront:_stickyFooterView];
  }
}

#pragma mark - State

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  _state = std::static_pointer_cast<ShadowlistViewShadowNode::ConcreteState const>(state);

  const auto &nextStateData = _state->getData();

  /*
   * Cache the sticky section-header geometry the core published this commit so the
   * per-scroll-tick pin (applyStickySectionHeaders) runs purely off cached values.
   */
  _stickyHeaderIndices.assign(nextStateData.stickyHeaderIndices_.begin(), nextStateData.stickyHeaderIndices_.end());
  _stickyHeaderOffsets.assign(nextStateData.stickyHeaderOffsets_.begin(), nextStateData.stickyHeaderOffsets_.end());
  _stickyHeaderSizes.assign(nextStateData.stickyHeaderSizes_.begin(), nextStateData.stickyHeaderSizes_.end());

  _scrollView.contentSize = CGSizeMake(
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);
  _contentView.frame = CGRectMake(
    0,
    0,
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);

  SL_LOG("mm.updateState: contentSize=(%.1f,%.1f) enabled=%d offset=(%.1f,%.1f) curOffset=(%.1f,%.1f)",
    nextStateData.totalContainerWidth_, nextStateData.totalContainerHeight_,
    nextStateData.containerOffsetEnabled_ ? 1 : 0,
    nextStateData.containerOffsetX_, nextStateData.containerOffsetY_,
    _scrollView.contentOffset.x, _scrollView.contentOffset.y);

  /*
   * While a drag is in flight (or settling after a drop) we own the scroll offset, so
   * a core offset correction must not yank the content out from under the finger or
   * jump the list as the reorder lands.
   */
  if (nextStateData.containerOffsetEnabled_ && !_dragging && !_dragDropPending) {
    _appliedOffset = CGPointMake(
      nextStateData.containerOffsetX_,
      nextStateData.containerOffsetY_);
    _hasAppliedOffset = YES;
    _scrollView.contentOffset = _appliedOffset;
  }

  /*
   * Re-pin after the content size / offset changed so a sticky footer (whose pin
   * depends on contentSize) stays put even when the list is not actively scrolling.
   */
  [self applyStickyTransforms];

  /*
   * Mid-drag commit (e.g. an auto-scroll re-virtualization mounted new rows): re-glue
   * the picked-up row to the finger and re-apply the make-room shuffle so freshly
   * mounted siblings are offset too.
   */
  if (_dragging) {
    [self updateDrag];
  }

  /*
   * After a drop, the reorder is applied by JS on a later commit. Hold the shuffle
   * transforms until that commit lands (detected by the picked-up row's index prop
   * reaching its dropped slot) so the rows never snap back to the pre-reorder layout;
   * the shuffled positions already equal the post-reorder resting positions, so
   * clearing the transforms then is seamless.
   */
  if (_dragDropPending && [self indexOfElementView:_droppedView] == _dropInsertionIndex) {
    [self clearDragTransforms];
    _dragDropPending = NO;
    _droppedView = nil;
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  if (!_state) {
    return;
  }

  /*
   * Distinguish a genuine user scroll from the offset the core applied itself by
   * comparing against the last applied offset (an "echo" of our own setContentOffset
   * lands within a pixel of it). This works for touch, trackpad and mouse-wheel
   * input alike, unlike isDragging/isDecelerating which only cover touch gestures.
   * Flagging the user scroll lets the core abandon an in-flight correction when the
   * user takes over (otherwise the correction latches and the visible window snaps
   * to a fixed anchor - a blank list on deep scroll).
   */
  CGPoint offset = scrollView.contentOffset;
  BOOL userScrolled = YES;
  if (_hasAppliedOffset &&
      fabs(offset.x - _appliedOffset.x) <= kScrollEchoTolerance &&
      fabs(offset.y - _appliedOffset.y) <= kScrollEchoTolerance) {
    userScrolled = NO;
    _hasAppliedOffset = NO;
  }

  SL_LOG("mm.scrollViewDidScroll: offset=(%.1f,%.1f) userScrolled=%d",
    scrollView.contentOffset.x, scrollView.contentOffset.y, userScrolled ? 1 : 0);
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetX_ = scrollView.contentOffset.x;
  nextStateData.containerOffsetY_ = scrollView.contentOffset.y;
  nextStateData.containerOffsetEnabled_ = false;
  nextStateData.userScrolled_ = userScrolled;
  _state->updateState(std::move(nextStateData));

  [self applyStickyTransforms];
}

/*
 * Clear the user-scroll flag once the gesture (and any momentum) ends, so a later
 * data/layout re-commit is not mistaken for a user scroll and does not cancel a
 * legitimate correction (e.g. a maintain-visible-content-position shift).
 */
- (void)clearUserScrolled
{
  if (!_state) {
    return;
  }

  auto nextStateData = _state->getData();
  if (!nextStateData.userScrolled_) {
    return;
  }
  nextStateData.userScrolled_ = false;
  _state->updateState(std::move(nextStateData));
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
  if (!decelerate) {
    [self clearUserScrolled];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
  [self clearUserScrolled];
}

#pragma mark - Drag to reorder

/*
 * A transform leaves center / bounds untouched (it is applied about the center), so
 * they give the element's resting frame even while it is translated under the finger.
 */
- (CGRect)restingFrameForView:(UIView *)view
{
  CGSize size = view.bounds.size;
  CGPoint center = view.center;
  return CGRectMake(center.x - size.width / 2.0, center.y - size.height / 2.0, size.width, size.height);
}

- (NSInteger)indexOfElementView:(UIView *)view
{
  if (![view conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
    return NSNotFound;
  }
  auto props = std::static_pointer_cast<ShadowlistElementViewProps const>(((UIView<RCTComponentViewProtocol> *)view).props);
  if (!props) {
    return NSNotFound;
  }
  return (NSInteger)props->index;
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

  // Clean slate: drop any leftover post-drop transforms from a previous drag.
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

  // We drive the offset ourselves from here (auto-scroll), so stop the scroll view
  // from also reacting to the finger.
  _scrollView.scrollEnabled = NO;

  // Lift feedback: raise the row and give it a shadow.
  [_contentView bringSubviewToFront:view];
  view.layer.shadowColor = [UIColor blackColor].CGColor;
  view.layer.shadowOpacity = 0.25;
  view.layer.shadowRadius = 8.0;
  view.layer.shadowOffset = CGSizeMake(0.0, 4.0);

  // The only event JS needs mid-drag: keep this row mounted while auto-scroll carries
  // it off-screen. No reorder happens until drop.
  [self dispatchDragEventType:1 from:index to:index];

  _dragDisplayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(dragTick)];
  [_dragDisplayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

  [self updateDrag];
}

/* Per-frame: auto-scroll at the edges, then re-glue the row and re-shuffle siblings. */
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
  // Let the resulting scrollViewDidScroll report this as a user scroll (do NOT echo-
  // suppress it): the drag owns the scroll position, so the core must virtualize at
  // this exact offset and abandon any maintain-visible-position correction. Echo-
  // suppressing here makes the core keep its correction and compute the visible window
  // at a different offset than the viewport, which blanks rows mid-drag.
  _scrollView.contentOffset = next;
}

/*
 * Glue the picked-up row under the finger, recompute where it would insert, and
 * shuffle the siblings to open the gap. Nothing is sent to JS - the data order is
 * fixed until drop - so this never triggers a re-render. Reads the finger in viewport
 * space and adds the scroll offset, so it tracks correctly through auto-scroll.
 */
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

  CGFloat translation = desiredLeading - restingLeading;
  view.transform = _horizontal
    ? CGAffineTransformMakeTranslation(translation, 0.0)
    : CGAffineTransformMakeTranslation(0.0, translation);

  _dragInsertionIndex = [self insertionIndexForCenter:(desiredLeading + extent / 2.0)];
  [self applyDragShuffle];
}

/*
 * The index the dragged row would insert at, by comparing its centre against each
 * neighbour's MIDPOINT relative to the (fixed) pickup index. Because the data order
 * never changes mid-drag, the midpoints are stable, giving a clean half-row dead zone
 * around each one - boundary jitter cannot flip the insertion back and forth. Returns
 * the farthest neighbour whose midpoint the centre has crossed.
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
 * Open a one-row gap at the insertion point by translating the siblings between the
 * pickup and the insertion toward the vacated pickup slot. The shift equals the
 * picked-up row's extent, so each shuffled sibling lands exactly on its post-reorder
 * resting position - which is why clearing the transforms after the drop commit is
 * seamless (the siblings do not move, only the dragged row settles into the gap).
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

/* Reset every element view's drag transform and lift shadow. */
- (void)clearDragTransforms
{
  for (UIView *sub in _contentView.subviews) {
    if (![sub conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
      continue;
    }
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
  // Keep the core's scroll corrections off for the duration of the drag (the drag owns
  // the offset); a reorder commit must virtualize at the live offset, not snap to an
  // anchor. Cleared on the end event so normal scrolling resumes afterwards.
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
  _dragging = NO;
  _draggedView = nil;

  // Emit the single reorder. Hold the shuffle transforms (the rows already sit at
  // their post-reorder positions) until JS's reorder commit lands, then
  // clearDragTransforms runs from updateState - so nothing snaps back in between.
  [self dispatchDragEventType:3 from:from to:to];

  if (from == to || !view) {
    // No reorder (dropped where it started): no commit will move the index, so settle
    // immediately instead of waiting for a landing that never changes anything.
    [self clearDragTransforms];
    _dragDropPending = NO;
    _droppedView = nil;
  } else {
    _dragDropPending = YES;
    _droppedView = view;
    _dropInsertionIndex = to;

    // Safety net: if the reorder never lands (e.g. a consumer that ignores onReorder),
    // clear the held transforms anyway so the gap cannot get stuck. A real reorder
    // clears earlier from updateState; a new drag clears at pickup.
    __weak ShadowlistView *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      ShadowlistView *strongSelf = weakSelf;
      if (strongSelf && strongSelf->_dragDropPending && !strongSelf->_dragging) {
        [strongSelf clearDragTransforms];
        strongSelf->_dragDropPending = NO;
        strongSelf->_droppedView = nil;
      }
    });
  }
}

/* Immediate teardown with no reorder (view recycle / drag disabled). */
- (void)teardownDrag
{
  [_dragDisplayLink invalidate];
  _dragDisplayLink = nil;
  _dragging = NO;
  _draggedView = nil;
  _dragDropPending = NO;
  _droppedView = nil;
  _scrollView.scrollEnabled = YES;
  [self clearDragTransforms];
}

#pragma mark - Commands

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowlistViewHandleCommand(self, commandName, args);
}

- (void)setStartReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  auto nextStateData = _state->getData();
  nextStateData.startReachedEnabled_ = enabled;
  _state->updateState(std::move(nextStateData));
}

- (void)setEndReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  auto nextStateData = _state->getData();
  nextStateData.endReachedEnabled_ = enabled;
  _state->updateState(std::move(nextStateData));
}

- (void)scrollToIndex:(NSInteger)index
{
  if (!_state) {
    return;
  }

  /*
   * Bump the nonce so the core treats this as a fresh request and re-scrolls even
   * when the index is unchanged from the previous call
   */
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetIndex_ = index;
  nextStateData.containerOffsetIndexNonce_ = nextStateData.containerOffsetIndexNonce_ + 1;
  nextStateData.containerOffsetEnabled_ = true;
  _state->updateState(std::move(nextStateData));
}

- (void)scrollToOffset:(double)offset animated:(BOOL)animated
{
  /*
   * Direct offset scroll along the scroll axis. The core picks up the new position
   * from the resulting scroll callback, so no state round-trip is needed here.
   */
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

  /*
   * Core-driven: ride the scrollToIndex command channel with the SCROLL_TO_END_INDEX
   * sentinel (-3, see shadowlist-core/Constants.hpp) so the core converges on the
   * true bottom as off-screen rows are measured, instead of a one-shot jump to the
   * current contentSize - a stale, estimate-based bottom that stops short on a
   * variable-height list. The animated flag no longer applies (the core steps to the
   * bottom); the argument is kept for API compatibility.
   */
  (void)animated;
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetIndex_ = -3.0;
  nextStateData.containerOffsetIndexNonce_ = nextStateData.containerOffsetIndexNonce_ + 1;
  nextStateData.containerOffsetEnabled_ = true;
  _state->updateState(std::move(nextStateData));
}

Class<RCTComponentViewProtocol> ShadowlistViewCls(void)
{
  return ShadowlistView.class;
}

@end
