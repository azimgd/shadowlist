#import "ShadowListView.h"
#import "ShadowListView+Internal.h"

#import "ShadowListViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowListViewSpec/Props.h>
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"
#import <React/RCTConversions.h>

#include <cmath>

using namespace facebook::react;

// Host list view: lifecycle, child mounting, state/props, scroll delegate and commands.
// Sticky pinning lives in ShadowListView+Sticky, drag-to-reorder in ShadowListView+DragReorder.
@implementation ShadowListView

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
    // NSScrollView scrolls its documentView; the RCTUIScrollView shim derives contentOffset
    // and contentSize from it (UIScrollView instead scrolls plain subviews).
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
    const auto &templateProps = *std::static_pointer_cast<ShadowListTemplateViewProps const>(childComponentView.props);
    // Match template types explicitly: ShadowList can mount `header` and `empty` simultaneously,
    // and a catch-all else here would let `empty` silently overwrite the real sticky header.
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
  // The dragged (or just-dropped, still-settling) row's underlying data can be deleted
  // mid-drag, unmounting it here. Abort the drag/settle instead of leaving dangling
  // auto-scroll/state; teardownDrag does not dispatch a reorder commit.
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
  // A recycled view must not hand a leftover scroll position or the previous mount's state
  // to the next list. Reset _state BEFORE moving the offset: setContentOffset: fires
  // scrollViewDidScroll synchronously, which would otherwise publish the reset as a phantom
  // user scroll-to-top into the outgoing surface's state.
  _appliedOffset = CGPointZero;
  _hasAppliedOffset = NO;
  _armedToken = 0;
  _state.reset();
  [_scrollView setContentOffset:CGPointZero animated:NO];
  [super prepareForRecycle];
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &nextProps = *std::static_pointer_cast<ShadowListViewProps const>(props);
  // _props holds the previous commit's props until [super updateProps:] swaps it.
  const auto &prevProps = *std::static_pointer_cast<ShadowListViewProps const>(_props);
  _stickyHeader = nextProps.stickyHeader;
  _stickyFooter = nextProps.stickyFooter;
  _autoHideHeader = nextProps.autoHideHeader;
  _autoHideFooter = nextProps.autoHideFooter;
  _horizontal = nextProps.horizontal;
  _dragEnabled = nextProps.dragEnabled;
  _snapToItem = nextProps.snapToItem;
#if !TARGET_OS_OSX
  _dragRecognizer.enabled = _dragEnabled;
  // Sync mounted rows' VoiceOver custom actions only when dragEnabled actually changes:
  // updateProps runs on every prop commit, and rebuilding the actions per row each time
  // is allocation churn. Newly mounted rows are covered by mountChildComponentView.
  if (prevProps.dragEnabled != nextProps.dragEnabled) {
    for (UIView *sub in _contentView.subviews) {
      if ([sub conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
        [self applyDragAccessibilityActionsToView:sub];
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

// Keyboard avoidance: set the bottom contentInset and shift the offset by the same delta
// so rows behind the keyboard come into view. Clamped to range; skipped mid-drag and when horizontal.
- (void)applyContentInsetBottom:(CGFloat)inset
{
  if (inset < 0) inset = 0;
  if (inset == _contentInsetBottom) return;

  CGFloat delta = inset - _contentInsetBottom;
  _contentInsetBottom = inset;

  if (_horizontal) {
    // Inset is vertical-only; stored value is kept in sync above for a later axis flip.
    return;
  }

#if !TARGET_OS_OSX
  // Keyboard avoidance only applies on iOS (there is no software keyboard on macOS, so the
  // inset stays 0 and this method returns above before reaching here).
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
// Pull-to-refresh (UIRefreshControl) has no AppKit equivalent and is iOS only.
// Lazily create the pull-to-refresh control, tinted by the refreshColor prop.
- (UIRefreshControl *)ensureRefreshControl
{
  if (!_refreshControl) {
    _refreshControl = [[UIRefreshControl alloc] init];
    [_refreshControl addTarget:self
                        action:@selector(handleRefreshValueChanged)
              forControlEvents:UIControlEventValueChanged];
    if (_refreshColor) _refreshControl.tintColor = _refreshColor;
  }
  return _refreshControl;
}

// Install/remove the control with refreshEnabled, apply the tint, and begin/end it from
// the controlled `refreshing` prop. Only acts on a real change of the prop.
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
  if (_refreshControl && color) _refreshControl.tintColor = color;
  [self applyRefreshProgressOffset];

  if (refreshing == _refreshing) return;
  _refreshing = refreshing;

  if (!refreshing) {
    // Refresh ended: fire onRefreshSettle once the retract spring settles (see
    // scheduleRefreshSettle), so JS applies a held prepend on a free scroll view.
    _refreshAwaitingSettle = YES;
    [self scheduleRefreshSettle];
  }

  if (!_refreshControl) return;

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
  if (!_eventEmitter) return;
  std::static_pointer_cast<const ShadowListViewEventEmitter>(_eventEmitter)->onRefresh({});
}

// Tell JS the refresh spinner has fully retracted, so it can apply a held refresh-prepend.
- (void)emitRefreshSettle
{
  if (!_eventEmitter) return;
  std::static_pointer_cast<const ShadowListViewEventEmitter>(_eventEmitter)->onRefreshSettle({});
}

// Debounced settle: each call bumps the token and schedules a delayed check that fires only for
// the latest token, so it lands one window after the last retract-spring frame (spring at
// rest). Reschedules while a finger is down or the offset is still below the top.
- (void)scheduleRefreshSettle
{
  _refreshSettleToken += 1;
  NSInteger token = _refreshSettleToken;
  __weak ShadowListView *weakSelf = self;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.25 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
    ShadowListView *strongSelf = weakSelf;
    if (!strongSelf) return;
    if (token != strongSelf->_refreshSettleToken) return;        // a later frame rescheduled
    if (!strongSelf->_refreshAwaitingSettle || strongSelf->_refreshing) return;
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
  if (!_refreshControl) return;
  CGFloat offset = 0.0;
  if ((_stickyHeader || _autoHideHeader) && _stickyHeaderView) {
    offset = _stickyHeaderView.frame.size.height;
  }
  CGRect bounds = _refreshControl.bounds;
  if (bounds.origin.y == -offset) return;
  _refreshControl.bounds = CGRectMake(bounds.origin.x, -offset, bounds.size.width, bounds.size.height);
}
#endif // !TARGET_OS_OSX

#pragma mark - State

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  _state = std::static_pointer_cast<ShadowListViewShadowNode::ConcreteState const>(state);

  const auto &nextStateData = _state->getData();

  // Cache the published sticky section-header geometry for the per-scroll-tick pin.
  _stickyHeaderIndices.assign(nextStateData.stickyHeaderIndices_.begin(), nextStateData.stickyHeaderIndices_.end());
  _stickyHeaderOffsets.assign(nextStateData.stickyHeaderOffsets_.begin(), nextStateData.stickyHeaderOffsets_.end());
  _stickyHeaderSizes.assign(nextStateData.stickyHeaderSizes_.begin(), nextStateData.stickyHeaderSizes_.end());
  _snapOffsets.assign(nextStateData.snapOffsets_.begin(), nextStateData.snapOffsets_.end());

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

  // We own the offset while dragging/settling; ignore core offset corrections then.
  if (nextStateData.containerOffsetEnabled_ && !_dragging && !_dragDropPending) {
    CGPoint before = _scrollView.contentOffset;
    _appliedOffset = CGPointMake(
      nextStateData.containerOffsetX_,
      nextStateData.containerOffsetY_);
    // Arm the echo expectation BEFORE the write: a real move fires scrollViewDidScroll
    // synchronously, and it must see the armed flag to classify itself as our echo and
    // carry the commit token back.
    _hasAppliedOffset = YES;
    _armedToken = (uint64_t)nextStateData.commitToken_;
    _scrollView.contentOffset = _appliedOffset;
    // A no-op or fully-clamped write moves nothing, so no didScroll fires and the latch
    // would otherwise stay armed forever, swallowing the next genuine user scroll. Detect
    // "did not move" and disarm.
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
  // The retract spring bounces the offset around the top, firing this each frame. Push the
  // settle out per frame so it fires only after the spring stops (see scheduleRefreshSettle).
  if (_refreshAwaitingSettle) {
    [self scheduleRefreshSettle];
  }
#endif

  // During refresh or the overscroll gap above the top, skip the core update (it would
  // churn rows and write the offset back mid-settle); just keep the pins live.
  if (_refreshEnabled && (_refreshing || scrollView.contentOffset.y < 0)) {
    [self applyStickyTransforms:NO];
    return;
  }

  // Distinguish a genuine user scroll from the core's own echoed offset by CAUSALITY:
  // if we armed an echo (we just applied a core offset that moved the view), THIS report
  // is that echo regardless of where it landed (clamp/rubber-band safe), and we echo the
  // commit token back so the core matches its in-flight correction. Otherwise a human is
  // driving; the flag lets the core abandon an in-flight correction.
  BOOL userScrolled = YES;
  uint64_t echoToken = 0;
  if (_hasAppliedOffset) {
    userScrolled = NO;
    echoToken = _armedToken;
    _hasAppliedOffset = NO;
    _armedToken = 0;
  }

  SL_LOG("mm.scrollViewDidScroll: offset=(%.1f,%.1f) userScrolled=%d token=%llu",
    scrollView.contentOffset.x, scrollView.contentOffset.y, userScrolled ? 1 : 0,
    (unsigned long long)echoToken);
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetX_ = scrollView.contentOffset.x;
  nextStateData.containerOffsetY_ = scrollView.contentOffset.y;
  nextStateData.containerOffsetEnabled_ = false;
  nextStateData.commitToken_ = (double)echoToken;
  nextStateData.userScrolled_ = userScrolled;
  _state->updateState(std::move(nextStateData));

  // Advance the auto-hide only on genuine user scrolls (not our own echoed offset).
  [self applyStickyTransforms:userScrolled];
}

// Clear the user-scroll flag once the gesture and momentum end, so a later recommit
// is not mistaken for a user scroll and does not cancel a legitimate correction.
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

// The macOS RCTUIScrollViewDelegate exposes only scrollViewDidScroll:. The drag-end /
// deceleration / will-end-dragging callbacks below (and with them snap-to-item) are iOS only.
#if !TARGET_OS_OSX
- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
  // A human grabbed the list: drop any pending echo expectation so the drag is reported
  // as a user scroll instead of being mistaken for our own correction's echo.
  _hasAppliedOffset = NO;
  _armedToken = 0;
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

// Redirect the native fling so its deceleration lands on an element boundary. The
// core publishes the resting snap offsets; pick the one nearest the projected landing.
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
#endif // !TARGET_OS_OSX

#pragma mark - Element helpers

- (NSInteger)indexOfElementView:(RCTUIView *)view
{
  if (![view conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
    return NSNotFound;
  }
  auto props = std::static_pointer_cast<ShadowListElementViewProps const>(((RCTUIView<RCTComponentViewProtocol> *)view).props);
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
  auto props = std::static_pointer_cast<ShadowListElementViewProps const>(((RCTUIView<RCTComponentViewProtocol> *)view).props);
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

  // Bump the sequence so an unchanged index still triggers a fresh scroll.
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetIndex_ = index;
  nextStateData.containerOffsetIndexSequence_ = nextStateData.containerOffsetIndexSequence_ + 1;
  nextStateData.containerOffsetEnabled_ = true;
  _state->updateState(std::move(nextStateData));
}

- (void)scrollToOffset:(double)offset animated:(BOOL)animated
{
  if (!std::isfinite(offset)) {
    return;
  }

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

  // Use the SCROLL_TO_END_INDEX sentinel (-3) so the core converges on the true bottom
  // as off-screen rows are measured. The animated flag is unused but kept for API compatibility.
  (void)animated;
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetIndex_ = -3.0;
  nextStateData.containerOffsetIndexSequence_ = nextStateData.containerOffsetIndexSequence_ + 1;
  nextStateData.containerOffsetEnabled_ = true;
  _state->updateState(std::move(nextStateData));
}

Class<RCTComponentViewProtocol> ShadowListViewCls(void)
{
  return ShadowListView.class;
}

@end
