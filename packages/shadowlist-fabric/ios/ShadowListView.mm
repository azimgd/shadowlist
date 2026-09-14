#import "ShadowListView.h"
#import "ShadowListView+Internal.h"

#import "ShadowListViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowListViewSpec/Props.h>
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"
#import <React/RCTConversions.h>
#if !TARGET_OS_OSX
#import <React/RCTMountingTransactionObserving.h>
#endif

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#if !TARGET_OS_OSX
// Lands a pending scroll-to-top jump in the mount transaction that brings its rows in.
@interface ShadowListView () <RCTMountingTransactionObserving>
@end
#endif

using namespace facebook::react;

/*
 * Host list view: lifecycle, child mounting, state/props, scroll delegate and commands.
 * Sticky pinning lives in ShadowListView+Sticky, drag-to-reorder in ShadowListView+DragReorder.
 */
@implementation ShadowListView

#pragma mark - Lifecycle

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowListViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowListViewProps>();
    _props = defaultProps;

    _scrollView = [[RCTUIScrollView alloc] init];
    _scrollView.delegate = self;
    _scrollView.showsVerticalScrollIndicator = YES;
    _scrollView.showsHorizontalScrollIndicator = YES;
    _scrollView.scrollEnabled = YES;
#if !TARGET_OS_OSX
    _scrollView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    _scrollView.indicatorStyle = UIScrollViewIndicatorStyleWhite;
#endif

    _contentView = [[RCTUIView alloc] init];
#if TARGET_OS_OSX
    /*
     * NSScrollView scrolls its documentView; the RCTUIScrollView shim derives contentOffset
     * and contentSize from it (UIScrollView instead scrolls plain subviews).
     */
    _scrollView.documentView = _contentView;
#else
    [_scrollView addSubview:_contentView];
#endif

    self.contentView = _scrollView;

#if !TARGET_OS_OSX
    // Long press to pick a row up; enabled by the dragEnabled prop. Drag-to-reorder is iOS only.
    _dragRecognizer = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(handleDragGesture:)];
    _dragRecognizer.minimumPressDuration = 0.2;
    _dragRecognizer.enabled = NO;
    [_scrollView addGestureRecognizer:_dragRecognizer];
#endif
  }

  return self;
}

#pragma mark - Mounting

- (void)mountChildComponentView:(RCTUIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if ([childComponentView conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
    [_contentView insertSubview:childComponentView atIndex:index];
    // Re-pin so sticky views stay on top of the newly mounted element.
    [self applyStickyTransforms:NO];
#if !TARGET_OS_OSX
    // A row mounting mid-drag must stay below the picked-up row and pick up the shuffle offset.
    if (_dragging && _draggedView) {
      [_contentView bringSubviewToFront:_draggedView];
      [self applyDragShuffle];
    }
    // VoiceOver alternative to the long-press drag gesture (no-op unless dragEnabled).
    [self applyDragAccessibilityActionsToView:childComponentView];
#endif
    return;
  }

  if ([childComponentView conformsToProtocol:@protocol(RCTShadowListTemplateViewViewProtocol)]) {
    const auto& templateProps = *std::static_pointer_cast<const ShadowListTemplateViewProps>(childComponentView.props);
    /*
     * Match template types explicitly: ShadowList can mount `header` and `empty` simultaneously,
     * and a catch-all else here would let `empty` silently overwrite the real sticky header.
     */
    if (templateProps.templateType == "footer") {
      _stickyFooterView = childComponentView;
    } else if (templateProps.templateType == "sectionHeader") {
      _sectionHeaderOverlay = childComponentView;
    } else if (templateProps.templateType == "header") {
      _stickyHeaderView = childComponentView;
    }
    [_contentView addSubview:childComponentView];
    [self applyStickyTransforms:NO];
    return;
  }
}

- (void)unmountChildComponentView:(RCTUIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
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
#if !TARGET_OS_OSX
  /*
   * The dragged (or just-dropped, still-settling) row's underlying data can be deleted
   * mid-drag, unmounting it here. Abort the drag/settle instead of leaving dangling
   * auto-scroll/state; teardownDrag does not dispatch a reorder commit.
   */
  if ((UIView *)childComponentView == _draggedView || (UIView *)childComponentView == _droppedView) {
    [self teardownDrag];
  }
#endif
  [childComponentView removeFromSuperview];
}

- (void)prepareForRecycle
{
  _stickyHeaderView = nil;
  _stickyFooterView = nil;
  _stickyHeader = NO;
  _stickyFooter = NO;
  _autoHideHeader = NO;
  _autoHideFooter = NO;
  _headerHidden = 0.0;
  _footerHidden = 0.0;
  _lastAutoHideOffset = 0.0;
  _horizontal = NO;
  _snapToItem = NO;
  _snapOffsets.clear();
  _contentInsetBottom = 0.0;
  _scrollView.contentInset = UIEdgeInsetsZero;
#if TARGET_OS_OSX
  _scrollView.scrollIndicatorInsets = UIEdgeInsetsZero;
#else
  _scrollView.verticalScrollIndicatorInsets = UIEdgeInsetsZero;
#endif
  _refreshing = NO;
  _refreshEnabled = NO;
  _refreshAwaitingSettle = NO;
#if !TARGET_OS_OSX
  _refreshColor = nil;
  if (_refreshControl) {
    [_refreshControl endRefreshing];
    _scrollView.refreshControl = nil;
    _refreshControl = nil;
  }
#endif
  _sectionHeaderOverlay = nil;
  _stickyHeaderIndices.clear();
  _stickyHeaderOffsets.clear();
  _stickyHeaderSizes.clear();
  _dragEnabled = NO;
#if !TARGET_OS_OSX
  [self teardownDrag];
  _dragRecognizer.enabled = NO;
#endif
  /*
   * A recycled view must not hand a leftover scroll position or the previous mount's state
   * to the next list. Reset _state BEFORE moving the offset: setContentOffset: fires
   * scrollViewDidScroll synchronously, which would otherwise publish the reset as a phantom
   * user scroll-to-top into the outgoing surface's state.
   */
  _appliedOffset = CGPointZero;
  _hasAppliedOffset = NO;
  _armedToken = 0;
  _echoedToken = 0;
  _publishedGesture = NO;
  _shiftedToken = 0;
  _shiftedTokenDelta = 0.0;
  _commandIndex = 0.0;
  _commandSequence = 0.0;
#if !TARGET_OS_OSX
  [self cancelScrollToTop];
#endif
  _state.reset();
  [_scrollView setContentOffset:CGPointZero animated:NO];
  /*
   * The previous list's content size would otherwise stand until the new list's first state
   * lands, and applyStickyTransforms (called from mountChildComponentView, i.e. before that)
   * derives every footer/overlay pin from it. Clear it with the offset.
   */
  _scrollView.contentSize = CGSizeZero;
  _contentView.frame = CGRectZero;
  [super prepareForRecycle];
}

#pragma mark - Props

- (void)updateProps:(const Props::Shared&)props oldProps:(const Props::Shared&)oldProps
{
  const auto& nextProps = *std::static_pointer_cast<const ShadowListViewProps>(props);
  // _props holds the previous commit's props until [super updateProps:] swaps it.
  const auto& prevProps = *std::static_pointer_cast<const ShadowListViewProps>(_props);
  _stickyHeader = nextProps.stickyHeader;
  _stickyFooter = nextProps.stickyFooter;
  _autoHideHeader = nextProps.autoHideHeader;
  _autoHideFooter = nextProps.autoHideFooter;
  _horizontal = nextProps.horizontal;
  _dragEnabled = nextProps.dragEnabled;
  _snapToItem = nextProps.snapToItem;
#if !TARGET_OS_OSX
  _dragRecognizer.enabled = _dragEnabled;
  /*
   * Sync mounted rows' VoiceOver custom actions only when dragEnabled actually changes:
   * updateProps runs on every prop commit, and rebuilding the actions per row each time
   * is allocation churn. Newly mounted rows are covered by mountChildComponentView.
   */
  if (prevProps.dragEnabled != nextProps.dragEnabled) {
    for (UIView *subview in _contentView.subviews) {
      if ([subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
        [self applyDragAccessibilityActionsToView:subview];
      }
    }
  }
  // decelerationRate (for snap-to-item) and pull-to-refresh have no AppKit equivalent.
  _scrollView.decelerationRate = _snapToItem ? UIScrollViewDecelerationRateFast : UIScrollViewDecelerationRateNormal;
#endif

  [self applyContentInsetBottom:nextProps.contentInsetBottom];
#if !TARGET_OS_OSX
  [self applyRefreshState:nextProps.refreshEnabled
                refreshing:nextProps.refreshing
                     color:RCTUIColorFromSharedColor(nextProps.refreshColor)];
#endif

  [super updateProps:props oldProps:oldProps];

  [self applyStickyTransforms:NO];
}

/*
 * Keyboard avoidance: set the bottom contentInset and shift the offset by the same delta
 * so rows behind the keyboard come into view. Clamped to range; skipped mid-drag and when horizontal.
 */
- (void)applyContentInsetBottom:(CGFloat)inset
{
  if (inset < 0) {
    inset = 0;
  }
  if (inset == _contentInsetBottom) {
    return;
  }

  CGFloat delta = inset - _contentInsetBottom;
  _contentInsetBottom = inset;

  if (_horizontal) {
    // Inset is vertical-only; stored value is kept in sync above for a later axis flip.
    return;
  }

#if !TARGET_OS_OSX
  /*
   * Keyboard avoidance only applies on iOS (there is no software keyboard on macOS, so the
   * inset stays 0 and this method returns above before reaching here).
   */
  UIEdgeInsets contentInset = _scrollView.contentInset;
  contentInset.bottom = inset;
  UIEdgeInsets indicatorInset = _scrollView.verticalScrollIndicatorInsets;
  indicatorInset.bottom = inset;

  // Shift the offset by the inset delta, clamped to range; skipped mid-drag.
  CGPoint offset = _scrollView.contentOffset;
  CGFloat maxOffset = MAX(-inset, _scrollView.contentSize.height - _scrollView.bounds.size.height + inset);
  CGFloat followedY = (_dragging || _dragDropPending)
    ? offset.y
    : MIN(MAX(offset.y + delta, -contentInset.top), maxOffset);

  [UIView animateWithDuration:0.25
                        delay:0
                      options:UIViewAnimationOptionCurveEaseOut | UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
    self->_scrollView.contentInset = contentInset;
    self->_scrollView.verticalScrollIndicatorInsets = indicatorInset;
    self->_scrollView.contentOffset = CGPointMake(offset.x, followedY);
  }
                   completion:nil];
#else
  (void)delta;
#endif
}

#if !TARGET_OS_OSX
#pragma mark - Pull to refresh

/*
 * Pull-to-refresh (UIRefreshControl) has no AppKit equivalent and is iOS only.
 * Lazily create the pull-to-refresh control, tinted by the refreshColor prop.
 */
- (UIRefreshControl *)ensureRefreshControl
{
  if (!_refreshControl) {
    _refreshControl = [[UIRefreshControl alloc] init];
    [_refreshControl addTarget:self
                        action:@selector(handleRefreshValueChanged)
              forControlEvents:UIControlEventValueChanged];
    if (_refreshColor) {
      _refreshControl.tintColor = _refreshColor;
    }
  }
  return _refreshControl;
}

/*
 * Install/remove the control with refreshEnabled, apply the tint, and begin/end it from
 * the controlled `refreshing` prop. Only acts on a real change of the prop.
 */
- (void)applyRefreshState:(BOOL)enabled refreshing:(BOOL)refreshing color:(UIColor *)color
{
  _refreshColor = color;

  if (enabled && !_scrollView.refreshControl) {
    _scrollView.refreshControl = [self ensureRefreshControl];
  } else if (!enabled && _scrollView.refreshControl) {
    [_refreshControl endRefreshing];
    _scrollView.refreshControl = nil;
  }
  _refreshEnabled = enabled;
  if (_refreshControl && color) {
    _refreshControl.tintColor = color;
  }
  [self applyRefreshProgressOffset];

  if (refreshing == _refreshing) {
    return;
  }
  _refreshing = refreshing;

  if (!refreshing) {
    /*
     * Refresh ended: fire onRefreshSettle once the retract spring settles (see
     * scheduleRefreshSettle), so JS applies a held prepend on a free scroll view.
     */
    _refreshAwaitingSettle = YES;
    [self scheduleRefreshSettle];
  }

  if (!_refreshControl) {
    return;
  }

  if (refreshing) {
    if (!_refreshControl.isRefreshing) {
      [_refreshControl beginRefreshing];
      // Scroll to reveal the spinner on a programmatic refresh (a pull already revealed it).
      if (!_dragging && !_dragDropPending && _scrollView.contentOffset.y >= 0) {
        CGFloat reveal = _refreshControl.frame.size.height > 0
          ? _refreshControl.frame.size.height : 60.0;
        [_scrollView setContentOffset:CGPointMake(_scrollView.contentOffset.x,
                                                  _scrollView.contentOffset.y - reveal)
                             animated:YES];
      }
    }
  } else {
    [_refreshControl endRefreshing];
  }
}

- (void)handleRefreshValueChanged
{
  if (!_eventEmitter) {
    return;
  }
  std::static_pointer_cast<const ShadowListViewEventEmitter>(_eventEmitter)->onRefresh({});
}

// Tell JS the refresh spinner has fully retracted, so it can apply a held refresh-prepend.
- (void)emitRefreshSettle
{
  if (!_eventEmitter) {
    return;
  }
  std::static_pointer_cast<const ShadowListViewEventEmitter>(_eventEmitter)->onRefreshSettle({});
}

/*
 * Debounced settle: each call bumps the token and schedules a delayed check that fires only for
 * the latest token, so it lands one window after the last retract-spring frame (spring at
 * rest). Reschedules while a finger is down or the offset is still below the top.
 */
- (void)scheduleRefreshSettle
{
  _refreshSettleToken += 1;
  NSInteger token = _refreshSettleToken;
  __weak ShadowListView *weakSelf = self;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.25 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
    ShadowListView *strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    // A later frame rescheduled.
    if (token != strongSelf->_refreshSettleToken) {
      return;
    }
    if (!strongSelf->_refreshAwaitingSettle || strongSelf->_refreshing) {
      return;
    }
    // Not settled yet (finger still down, or band not fully retracted): keep waiting.
    if (strongSelf->_scrollView.isDragging || strongSelf->_scrollView.isTracking ||
        strongSelf->_scrollView.contentOffset.y < -1.0) {
      [strongSelf scheduleRefreshSettle];
      return;
    }
    strongSelf->_refreshAwaitingSettle = NO;
    [strongSelf emitRefreshSettle];
  });
}

// Push the refresh spinner below a pinned (sticky/auto-hide) header so it is not covered.
- (void)applyRefreshProgressOffset
{
  if (!_refreshControl) {
    return;
  }
  CGFloat offset = 0.0;
  if ((_stickyHeader || _autoHideHeader) && _stickyHeaderView) {
    offset = _stickyHeaderView.frame.size.height;
  }
  CGRect bounds = _refreshControl.bounds;
  if (bounds.origin.y == -offset) {
    return;
  }
  _refreshControl.bounds = CGRectMake(bounds.origin.x, -offset, bounds.size.width, bounds.size.height);
}
#endif // !TARGET_OS_OSX

#pragma mark - State

- (void)updateState:(const State::Shared&)state oldState:(const State::Shared&)oldState
{
  _state = std::static_pointer_cast<const ShadowListViewShadowNode::ConcreteState>(state);

  const auto& nextStateData = _state->getData();

  /*
   * Cache the published sticky section-header geometry for the per-scroll-tick pin. These
   * arrive as shared pointers and a null one means empty (see ShadowListViewState), so go
   * through a helper rather than dereferencing.
   */
  auto copyPublished = [](auto& destination, const auto& published) {
    if (published) {
      destination.assign(published->begin(), published->end());
    } else {
      destination.clear();
    }
  };
  copyPublished(_stickyHeaderIndices, nextStateData.stickyHeaderIndices_);
  copyPublished(_stickyHeaderOffsets, nextStateData.stickyHeaderOffsets_);
  copyPublished(_stickyHeaderSizes, nextStateData.stickyHeaderSizes_);
  copyPublished(_snapOffsets, nextStateData.snapOffsets_);

  // A clamp these writes cause is reported as a non-user scroll; see _applyingContentSize.
  _applyingContentSize = YES;
  _scrollView.contentSize = CGSizeMake(
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);
  _contentView.frame = CGRectMake(
    0,
    0,
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);
  _applyingContentSize = NO;

  SL_LOG("mm.updateState: contentSize=(%.1f,%.1f) enabled=%d offset=(%.1f,%.1f) curOffset=(%.1f,%.1f)",
    nextStateData.totalContainerWidth_, nextStateData.totalContainerHeight_,
    nextStateData.containerOffsetEnabled_ ? 1 : 0,
    nextStateData.containerOffsetX_, nextStateData.containerOffsetY_,
    _scrollView.contentOffset.x, _scrollView.contentOffset.y);

  /*
   * A correction the core computed against a pending scroll-to-top jump starts from the
   * offset that jump published, not from where the view still sits. It moves the jump
   * target; the view follows when the jump lands.
   */
  BOOL retargetsScrollToTopJump = _scrollToTopJumpPending && nextStateData.containerOffsetEnabled_ &&
    fabs(nextStateData.containerOffsetBaseY_ - _scrollToTopJumpY) < 0.5;
  if (retargetsScrollToTopJump) {
    _scrollToTopJumpY = nextStateData.containerOffsetY_;
    _scrollToTopJumpToken = (uint64_t)nextStateData.commitToken_;
  } else if (nextStateData.containerOffsetEnabled_ && !_dragging && !_dragDropPending) {
    // We own the offset while dragging/settling; ignore core offset corrections then.
    CGPoint before = _scrollView.contentOffset;
    _appliedOffset = CGPointMake(
      nextStateData.containerOffsetX_,
      nextStateData.containerOffsetY_);
#if !TARGET_OS_OSX
    /*
     * Mid scroll-to-top the view travels hundreds of points a frame, so it has moved on
     * from the offset this correction was computed against by the time the commit mounts.
     * Writing the absolute offset would undo that travel; shift the live offset by the
     * correction instead, clamped to the scrollable range. A finger or a fling moves the
     * view the same way (an MVCP correction for a prepend that lands mid-momentum mounts
     * several frames after the report it was computed from), so shift it there too.
     *
     * A correction computed from a gesture report stays a shift once the motion has stopped,
     * and so does a retarget of a correction this view already shifted: the view has travelled
     * on from that report (the rest of a fling or a bounce) or sits where the shift put it, and
     * writing the absolute offset would throw that travel away. Scroll commands carry the live
     * offset, so shifting their corrections lands on the same place as writing them.
     */
    uint64_t token = (uint64_t)nextStateData.commitToken_;
    BOOL continuesShiftedCorrection = token != 0 && token == _shiftedToken;
    BOOL computedDuringGesture = token != 0 &&
      (nextStateData.userScrolled_ || nextStateData.scrollPhase_ != SCROLL_PHASE_IDLE);
    if (_scrollingToTop || _scrollView.isDragging || _scrollView.isDecelerating || continuesShiftedCorrection ||
        computedDuringGesture) {
      CGFloat top = -_scrollView.contentInset.top;
      CGFloat maxY = MAX(top, _scrollView.contentSize.height - _scrollView.bounds.size.height
        + _scrollView.contentInset.bottom);
      CGFloat shift = nextStateData.containerOffsetY_ - nextStateData.containerOffsetBaseY_;
      /*
       * The core retargets an operation's correction against every newer report until the
       * host echoes its token, and each retarget carries the whole correction again. Shift
       * only by what this token has not moved yet, or a prepend landing mid-fling shifts the
       * view once per retarget.
       */
      if (token != 0) {
        CGFloat unapplied = token == _shiftedToken ? shift - _shiftedTokenDelta : shift;
        _shiftedToken = token;
        _shiftedTokenDelta = shift;
        shift = unapplied;
      }
      _appliedOffset = CGPointMake(before.x, MIN(MAX(before.y + shift, top), maxY));
    }
#endif
    /*
     * Arm the echo expectation BEFORE the write: a real move fires scrollViewDidScroll
     * synchronously, and it must see the armed flag to classify itself as our echo and
     * carry the commit token back.
     */
    _hasAppliedOffset = YES;
    _armedToken = (uint64_t)nextStateData.commitToken_;
    _scrollView.contentOffset = _appliedOffset;
    /*
     * A no-op or fully-clamped write moves nothing, so no didScroll fires and the latch
     * would otherwise stay armed forever, swallowing the next genuine user scroll. Detect
     * "did not move" and disarm.
     */
    CGPoint after = _scrollView.contentOffset;
    if (fabs(after.x - before.x) < 0.01 && fabs(after.y - before.y) < 0.01) {
      _hasAppliedOffset = NO;
      _armedToken = 0;
    }
  }

  // Re-pin after content size/offset changed so a sticky footer stays put.
  [self applyStickyTransforms:NO];

#if !TARGET_OS_OSX
  // Header may have remeasured; push the refresh indicator below it.
  [self applyRefreshProgressOffset];

  // Mid-drag commit: reglue the picked-up row to the finger and reapply the shuffle.
  if (_dragging) {
    [self updateDrag];
  }
#endif
}

- (void)finalizeUpdates:(RNComponentViewUpdateMask)updateMask
{
  [super finalizeUpdates:updateMask];
  [self applyStickyTransforms:NO];
}

#pragma mark - RCTUIScrollViewDelegate

- (void)scrollViewDidScroll:(RCTUIScrollView *)scrollView
{
  if (!_state) {
    return;
  }

#if !TARGET_OS_OSX
  /*
   * The retract spring bounces the offset around the top, firing this each frame. Push the
   * settle out per frame so it fires only after the spring stops (see scheduleRefreshSettle).
   */
  if (_refreshAwaitingSettle) {
    [self scheduleRefreshSettle];
  }
#endif

  /*
   * During refresh or the overscroll gap above the top, skip the core update (it would
   * churn rows and write the offset back mid-settle); just keep the pins live.
   */
  if (_refreshEnabled && (_refreshing || scrollView.contentOffset.y < 0)) {
    [self applyStickyTransforms:NO];
    return;
  }

  /*
   * Distinguish a genuine user scroll from the core's own echoed offset by CAUSALITY:
   * if we armed an echo (we just applied a core offset that moved the view), THIS report
   * is that echo regardless of where it landed (clamp/rubber-band safe), and we echo the
   * commit token back so the core matches its in-flight correction. Otherwise a human is
   * driving; the flag lets the core abandon an in-flight correction.
   */
  BOOL userScrolled = !_applyingContentSize;
  if (_hasAppliedOffset) {
    userScrolled = NO;
    _echoedToken = _armedToken;
    _hasAppliedOffset = NO;
    _armedToken = 0;
  }
  /*
   * Keep echoing the last applied token on the reports after its echo. State updates
   * coalesce, so the echo report itself can be replaced by the next momentum frame before
   * the core lays it out; the core would then read the correction as gesture travel and
   * apply it again. Operation ids are never reused, so a token the core already released
   * matches nothing.
   */
  uint64_t echoToken = _echoedToken;

  SL_LOG("mm.scrollViewDidScroll: offset=(%.1f,%.1f) userScrolled=%d token=%llu",
    scrollView.contentOffset.x, scrollView.contentOffset.y, userScrolled ? 1 : 0,
    (unsigned long long)echoToken);
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetX_ = scrollView.contentOffset.x;
  nextStateData.containerOffsetY_ = scrollView.contentOffset.y;
  nextStateData.containerOffsetEnabled_ = false;
  nextStateData.commitToken_ = (double)echoToken;
  nextStateData.userScrolled_ = userScrolled;
  /*
   * The live gesture phase (finger down, momentum, idle). It persists across the commits
   * between scroll frames, so the core keeps the inverted bottom pin off while a finger
   * rests on the list (see Container::gestureActive). macOS exposes no drag state.
   */
#if !TARGET_OS_OSX
  nextStateData.scrollPhase_ = [self currentScrollPhase];
#endif
  _publishedGesture = userScrolled || nextStateData.scrollPhase_ != SCROLL_PHASE_IDLE;
  [self carryScrollCommandInto:nextStateData];
  _state->updateState(std::move(nextStateData));

  // Advance the auto-hide only on genuine user scrolls (not our own echoed offset).
  [self applyStickyTransforms:userScrolled];
}

/*
 * Clear the user-scroll flag once the gesture and momentum end, so a later recommit
 * is not mistaken for a user scroll and does not cancel a legitimate correction.
 */
- (void)clearUserScrolled
{
  if (!_state) {
    return;
  }

  auto nextStateData = _state->getData();
  /*
   * Skip only when neither the mounted state nor the last published update describes a
   * gesture. The mounted state alone is not enough: a pull past the top reports nothing while
   * the offset is negative, so the bounce back publishes a single settling report at 0 that has
   * not mounted yet when the deceleration ends. Skipping then would leave that report as the
   * list's last state, and every later layout, including the core's own MVCP correction, would
   * read as a gesture taking over and drop the correction.
   */
  if (!_publishedGesture && !nextStateData.userScrolled_ && nextStateData.scrollPhase_ == SCROLL_PHASE_IDLE) {
    return;
  }
  _publishedGesture = NO;
  nextStateData.userScrolled_ = false;
  nextStateData.scrollPhase_ = SCROLL_PHASE_IDLE;
  [self carryLiveOffsetInto:nextStateData];
  [self carryScrollCommandInto:nextStateData];
  _state->updateState(std::move(nextStateData));
}

#if !TARGET_OS_OSX
/*
 * The gesture phase reported with every scroll frame. A finger on the list is read from
 * isTracking alone: UIScrollView keeps isDragging set after the finger lifts, for as long as
 * the fling decelerates, and reporting that momentum as a drag would let it cancel a scroll
 * command issued mid-fling. The scroll-to-top animation counts as momentum: none of these
 * flags is set while it runs, and an idle phase would let the inverted bottom pin re-engage
 * on the commits between its frames and snap the view back down.
 */
- (double)currentScrollPhase
{
  if (_scrollView.isTracking) {
    return SCROLL_PHASE_DRAGGING;
  }
  if (_scrollView.isDecelerating || _scrollView.isDragging || _scrollingToTop) {
    return SCROLL_PHASE_SETTLING;
  }
  return SCROLL_PHASE_IDLE;
}
#endif

/*
 * The macOS RCTUIScrollViewDelegate exposes only scrollViewDidScroll:. The drag-end /
 * deceleration / will-end-dragging callbacks below (and with them snap-to-item) are iOS only.
 */
#if !TARGET_OS_OSX
- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
  /*
   * A human grabbed the list: drop any pending echo expectation so the drag is reported
   * as a user scroll instead of being mistaken for our own correction's echo.
   */
  _hasAppliedOffset = NO;
  _armedToken = 0;
  // A finger takes over from a running scroll-to-top; the drag reports its own phase.
  [self cancelScrollToTop];
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

/*
 * Redirect the native fling so its deceleration lands on an element boundary. The
 * core publishes the resting snap offsets; pick the one nearest the projected landing.
 */
- (void)scrollViewWillEndDragging:(UIScrollView *)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint *)targetContentOffset
{
  if (!_snapToItem || _snapOffsets.empty()) {
    return;
  }

  CGFloat projected = _horizontal ? targetContentOffset->x : targetContentOffset->y;
  CGFloat best = (CGFloat)_snapOffsets.front();
  CGFloat bestDistance = fabs(best - projected);
  for (double snapOffset : _snapOffsets) {
    CGFloat distance = fabs((CGFloat)snapOffset - projected);
    if (distance < bestDistance) {
      bestDistance = distance;
      best = (CGFloat)snapOffset;
    }
  }

  if (_horizontal) {
    targetContentOffset->x = best;
  } else {
    targetContentOffset->y = best;
  }
}

#pragma mark - Scroll to top

/*
 * Status-bar tap. Returning NO keeps UIKit's own animation out of it (see _scrollingToTop)
 * and runs ours instead. A horizontal list has no vertical travel, so UIKit's no-op stands.
 */
- (BOOL)scrollViewShouldScrollToTop:(UIScrollView *)scrollView
{
  if (!_state || _horizontal) {
    return YES;
  }
  if (!_dragging && !_dragDropPending) {
    [self startScrollToTop];
  }
  return NO;
}

// Duration of the scroll-to-top animation, close to UIKit's own.
static const CFTimeInterval SCROLL_TO_TOP_DURATION = 0.45;

/*
 * How much of the trip the animation covers, in viewports. The core keeps about a viewport of
 * rows mounted beyond the visible window, so an animation over this distance never outruns
 * them. A longer trip jumps to this distance from the top first, as UIKit's own does.
 */
static const CGFloat SCROLL_TO_TOP_ANIMATED_VIEWPORTS = 1.0;

/*
 * Largest step one frame may take, in viewports. The eased animation over the distance above
 * peaks near 0.1; this bounds the steps a correction made mid-flight (rows above measured
 * taller than estimated) would otherwise stretch into a jump past the mounted rows.
 */
static const CGFloat SCROLL_TO_TOP_MAX_STEP_VIEWPORTS = 0.35;

// Share of the jump target's viewport the mounted rows must cover before the jump lands.
static const CGFloat SCROLL_TO_TOP_JUMP_COVERAGE = 0.9;

/*
 * Longest wait for the jump target's rows, in seconds. A list whose rows never cover the
 * target (every row hidden by the materialization band, a JS thread stalled elsewhere)
 * lands anyway rather than leaving the status-bar tap unanswered.
 */
static const CFTimeInterval SCROLL_TO_TOP_JUMP_MAX_WAIT = 0.5;

- (void)startScrollToTop
{
  CGFloat top = -_scrollView.contentInset.top;
  if (_scrollingToTop || _scrollView.contentOffset.y <= top + 0.5) {
    return;
  }
  // Stop any momentum first, as UIKit does before its own scroll-to-top.
  [_scrollView setContentOffset:_scrollView.contentOffset animated:NO];

  _scrollingToTop = YES;
  _scrollToTopProgress = 0.0;
  _scrollToTopStartTime = CACurrentMediaTime();
  _scrollToTopLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(scrollToTopTick)];
  [_scrollToTopLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

  /*
   * A long trip jumps first. Moving the view there straight away would show a blank viewport
   * until the rows for it are rendered and mounted, so publish the target to the core instead
   * and keep the view where it is. The core virtualizes around the target, and the jump lands
   * in the mount transaction that brings those rows in (mountingTransactionDidMount:).
   */
  CGFloat viewport = _scrollView.bounds.size.height;
  CGFloat jumpY = top + viewport * SCROLL_TO_TOP_ANIMATED_VIEWPORTS;
  if (_state && viewport > 0.0 && _scrollView.contentOffset.y > jumpY + viewport) {
    _scrollToTopJumpPending = YES;
    _scrollToTopJumpY = jumpY;
    _scrollToTopJumpToken = 0;
    auto nextStateData = _state->getData();
    nextStateData.containerOffsetX_ = _scrollView.contentOffset.x;
    nextStateData.containerOffsetY_ = jumpY;
    nextStateData.containerOffsetEnabled_ = false;
    nextStateData.commitToken_ = 0.0;
    nextStateData.userScrolled_ = true;
    nextStateData.scrollPhase_ = SCROLL_PHASE_SETTLING;
    _publishedGesture = YES;
    [self carryScrollCommandInto:nextStateData];
    _state->updateState(std::move(nextStateData));
  }
}

/*
 * Whether mounted rows cover the viewport that would start at `y`, so a jump there shows
 * content on its first frame. Frames of mounted rows are already the core's positions for
 * the committed state, including the rows the core just virtualized around the target.
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
  return covered >= (end - y) * SCROLL_TO_TOP_JUMP_COVERAGE;
}

/*
 * Move the view to a pending jump target once its rows are mounted (or the wait ran out),
 * then start the animation over the remaining distance from there.
 */
- (void)landScrollToTopJumpIfReady
{
  if (!_scrollToTopJumpPending) {
    return;
  }
  BOOL waitedTooLong = CACurrentMediaTime() - _scrollToTopStartTime > SCROLL_TO_TOP_JUMP_MAX_WAIT;
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
   * A core correction retargeted the jump: echo its token with the landing report, so the
   * core sees its own write arrive and releases the correction instead of shifting it again.
   */
  if (_scrollToTopJumpToken != 0) {
    _hasAppliedOffset = YES;
    _armedToken = _scrollToTopJumpToken;
  }
  _scrollView.contentOffset = target;
  if (fabs(_scrollView.contentOffset.y - before.y) < 0.01) {
    _hasAppliedOffset = NO;
    _armedToken = 0;
  }
  _scrollToTopJumpToken = 0;
  _scrollToTopProgress = 0.0;
  _scrollToTopStartTime = CACurrentMediaTime();
}

/*
 * Runs after every mount transaction on the surface, so it returns at once unless a
 * scroll-to-top jump is waiting for its rows. Landing here, before the transaction renders,
 * is what keeps the jump from showing a frame without them.
 */
- (void)mountingTransactionDidMount:(const MountingTransaction&)transaction
               withSurfaceTelemetry:(const SurfaceTelemetry&)surfaceTelemetry
{
  if (_scrollToTopJumpPending) {
    [self landScrollToTopJumpIfReady];
  }
}

/*
 * One step: ease out toward the top, scaling whatever distance REMAINS by the progress
 * still to go. The remaining distance is read from the live offset, so a core correction
 * applied since the last step (rows above measured taller than estimated, a header
 * resize) simply lengthens or shortens the rest of the trip instead of being overwritten.
 * A step never exceeds SCROLL_TO_TOP_MAX_STEP_VIEWPORTS; a capped step keeps going on the
 * following frames, past the nominal duration if it has to.
 */
- (void)scrollToTopTick
{
  if (!_scrollingToTop) {
    return;
  }

  // Waiting for the jump target's rows; the mount observer normally lands it first.
  if (_scrollToTopJumpPending) {
    [self landScrollToTopJumpIfReady];
    return;
  }

  // The last step wrote the top on the previous frame: finish.
  if (_scrollToTopProgress >= 1.0) {
    [self finishScrollToTop];
    return;
  }

  CFTimeInterval now = CACurrentMediaTime();
  CGFloat top = -_scrollView.contentInset.top;
  CGFloat time = MIN(1.0, (now - _scrollToTopStartTime) / SCROLL_TO_TOP_DURATION);
  CGFloat eased = 1.0 - pow(1.0 - time, 3.0);
  CGFloat remainingFraction = 1.0 - _scrollToTopProgress;
  CGFloat currentY = _scrollView.contentOffset.y;
  CGFloat nextY = (time >= 1.0 || remainingFraction <= 0.0)
    ? top
    : top + (currentY - top) * (1.0 - eased) / remainingFraction;
  CGFloat progress = time >= 1.0 ? 1.0 : eased;

  CGFloat maxStep = MAX(1.0, _scrollView.bounds.size.height * SCROLL_TO_TOP_MAX_STEP_VIEWPORTS);
  if (currentY - nextY > maxStep) {
    nextY = currentY - maxStep;
    // Progress is what the capped step actually covered of the distance left.
    progress = 1.0 - remainingFraction * (nextY - top) / (currentY - top);
  }
  _scrollToTopProgress = progress;

  if (fabs(nextY - currentY) >= 0.01) {
    _scrollView.contentOffset = CGPointMake(_scrollView.contentOffset.x, nextY);
  }
}

// Stops the animation; returns whether one was running.
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
 * Stop the animation and tell the core the momentum is over. The state carries the view's
 * ACTUAL offset rather than the mounted state's: the last step's report may not have
 * committed yet, and copying an older offset back would send the core mid-list and
 * virtualize rows for a place the view has already left.
 */
- (void)finishScrollToTop
{
  [self cancelScrollToTop];
  if (!_state) {
    return;
  }
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetX_ = _scrollView.contentOffset.x;
  nextStateData.containerOffsetY_ = _scrollView.contentOffset.y;
  nextStateData.containerOffsetEnabled_ = false;
  nextStateData.commitToken_ = 0.0;
  nextStateData.userScrolled_ = false;
  nextStateData.scrollPhase_ = SCROLL_PHASE_IDLE;
  _publishedGesture = NO;
  [self carryScrollCommandInto:nextStateData];
  _state->updateState(std::move(nextStateData));
}
#endif // !TARGET_OS_OSX

#pragma mark - Element helpers

- (NSInteger)indexOfElementView:(RCTUIView *)view
{
  if (![view conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
    return NSNotFound;
  }
  auto props = std::static_pointer_cast<const ShadowListElementViewProps>(((RCTUIView<RCTComponentViewProtocol> *)view).props);
  if (!props) {
    return NSNotFound;
  }
  return (NSInteger)props->index;
}

- (NSString *)keyOfElementView:(RCTUIView *)view
{
  if (![view conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
    return nil;
  }
  auto props = std::static_pointer_cast<const ShadowListElementViewProps>(((RCTUIView<RCTComponentViewProtocol> *)view).props);
  if (!props) {
    return nil;
  }
  return [NSString stringWithUTF8String:props->elementKey.c_str()];
}

#pragma mark - Commands

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowListViewHandleCommand(self, commandName, args);
}

/*
 * A scroll command supersedes any momentum still running: a scroll-to-top animation or a
 * deceleration. Stop it, so its next frame cannot move the view off the offset the core is
 * about to apply, and keep its settling phase out of the command's state. A finger still on
 * the list keeps its dragging phase, so the core lets the drag cancel the command.
 */
- (void)yieldMomentumInto:(ShadowListViewShadowNode::ConcreteState::Data&)stateData
{
#if !TARGET_OS_OSX
  BOOL yielded = [self cancelScrollToTop];
  // isDragging stays set while a released fling decelerates; only isTracking means a finger.
  if (_scrollView.isDecelerating && !_scrollView.isTracking) {
    // Writing the current offset stops the deceleration, as startScrollToTop does.
    [_scrollView setContentOffset:_scrollView.contentOffset animated:NO];
    yielded = YES;
  }
  if (yielded) {
    stateData.userScrolled_ = false;
    stateData.scrollPhase_ = SCROLL_PHASE_IDLE;
    _publishedGesture = NO;
  }
#else
  (void)stateData;
#endif
}

// Writes the last scroll command into a state update this view builds (see _commandSequence).
- (void)carryScrollCommandInto:(ShadowListViewShadowNode::ConcreteState::Data&)stateData
{
  if (_commandSequence > 0) {
    stateData.containerOffsetIndex_ = _commandIndex;
    stateData.containerOffsetIndexSequence_ = _commandSequence;
  }
}

/*
 * A state update copied from the mounted state holds that state's offset, which can be many
 * frames old while the view decelerates. Write the live offset into it, or the update hands
 * that old offset back to the core, which then virtualizes rows for a place the view has
 * already left. The update applies no correction, so it disables the offset write; it
 * carries the last echoed token like a scroll report (see scrollViewDidScroll:).
 */
- (void)carryLiveOffsetInto:(ShadowListViewShadowNode::ConcreteState::Data&)stateData
{
  stateData.containerOffsetX_ = _scrollView.contentOffset.x;
  stateData.containerOffsetY_ = _scrollView.contentOffset.y;
  stateData.containerOffsetEnabled_ = false;
  stateData.commitToken_ = (double)_echoedToken;
}

- (void)setStartReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  auto nextStateData = _state->getData();
  nextStateData.startReachedEnabled_ = enabled;
  [self carryLiveOffsetInto:nextStateData];
  [self carryScrollCommandInto:nextStateData];
  _state->updateState(std::move(nextStateData));
}

- (void)setEndReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  auto nextStateData = _state->getData();
  nextStateData.endReachedEnabled_ = enabled;
  [self carryLiveOffsetInto:nextStateData];
  [self carryScrollCommandInto:nextStateData];
  _state->updateState(std::move(nextStateData));
}

- (void)scrollToIndex:(NSInteger)index
{
  if (!_state) {
    return;
  }

  // Bump the sequence so an unchanged index still triggers a fresh scroll.
  auto nextStateData = _state->getData();
  [self yieldMomentumInto:nextStateData];
  _commandIndex = index;
  _commandSequence = MAX(nextStateData.containerOffsetIndexSequence_, _commandSequence) + 1;
  [self carryLiveOffsetInto:nextStateData];
  [self carryScrollCommandInto:nextStateData];
  nextStateData.containerOffsetEnabled_ = true;
  _state->updateState(std::move(nextStateData));
}

- (void)scrollToOffset:(double)offset animated:(BOOL)animated
{
  if (!std::isfinite(offset)) {
    return;
  }

#if !TARGET_OS_OSX
  [self cancelScrollToTop];
#endif
  // Direct offset scroll along the scroll axis; the core picks up the new position from the callback.
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
   * Use the SCROLL_TO_END_INDEX sentinel (-3) so the core converges on the true bottom
   * as off-screen rows are measured. The animated flag is unused but kept for API compatibility.
   */
  (void)animated;
  auto nextStateData = _state->getData();
  [self yieldMomentumInto:nextStateData];
  _commandIndex = -3.0;
  _commandSequence = MAX(nextStateData.containerOffsetIndexSequence_, _commandSequence) + 1;
  [self carryLiveOffsetInto:nextStateData];
  [self carryScrollCommandInto:nextStateData];
  nextStateData.containerOffsetEnabled_ = true;
  _state->updateState(std::move(nextStateData));
}

Class<RCTComponentViewProtocol> ShadowListViewCls(void)
{
  return ShadowListView.class;
}

@end
