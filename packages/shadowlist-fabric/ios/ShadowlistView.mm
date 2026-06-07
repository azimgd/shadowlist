#import "ShadowlistView.h"
#import "ShadowlistView+Internal.h"

#import "ShadowlistViewComponentDescriptor.h"
#import <react/renderer/components/ShadowlistViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowlistViewSpec/Props.h>
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"
#import <React/RCTConversions.h>

#include <cmath>

using namespace facebook::react;

// A scroll offset within this tolerance of the last applied offset is an echo, not a user scroll.
static const CGFloat kScrollEchoTolerance = 2.0;

// Host list view: lifecycle, child mounting, state/props, scroll delegate and commands.
// Sticky pinning lives in ShadowlistView+Sticky, drag-to-reorder in ShadowlistView+DragReorder.
@implementation ShadowlistView

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

    // Long press to pick a row up; enabled by the dragEnabled prop.
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
    [_contentView insertSubview:childComponentView atIndex:index];
    // Re-pin so sticky views stay on top of the newly mounted element.
    [self applyStickyTransforms:NO];
    // A row mounting mid-drag must stay below the picked-up row and pick up the shuffle offset.
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
    [self applyStickyTransforms:NO];
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
  _scrollView.verticalScrollIndicatorInsets = UIEdgeInsetsZero;
  _refreshing = NO;
  _refreshEnabled = NO;
  _refreshAwaitingSettle = NO;
  _refreshColor = nil;
  if (_refreshControl) {
    [_refreshControl endRefreshing];
    _scrollView.refreshControl = nil;
    _refreshControl = nil;
  }
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
  _autoHideHeader = nextProps.autoHideHeader;
  _autoHideFooter = nextProps.autoHideFooter;
  _horizontal = nextProps.horizontal;
  _dragEnabled = nextProps.dragEnabled;
  _dragRecognizer.enabled = _dragEnabled;
  _snapToItem = nextProps.snapToItem;
  _scrollView.decelerationRate = _snapToItem ? UIScrollViewDecelerationRateFast : UIScrollViewDecelerationRateNormal;

  [self applyContentInsetBottom:nextProps.contentInsetBottom];
  [self applyRefreshState:nextProps.refreshEnabled
                refreshing:nextProps.refreshing
                     color:RCTUIColorFromSharedColor(nextProps.refreshColor)];

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
}

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
  std::static_pointer_cast<const ShadowlistViewEventEmitter>(_eventEmitter)->onRefresh({});
}

// Tell JS the refresh spinner has fully retracted, so it can apply a held refresh-prepend.
- (void)emitRefreshSettle
{
  if (!_eventEmitter) return;
  std::static_pointer_cast<const ShadowlistViewEventEmitter>(_eventEmitter)->onRefreshSettle({});
}

// Debounced settle: each call bumps the token and arms a delayed check that fires only for
// the latest token, so it lands one window after the last retract-spring frame (spring at
// rest). Re-arms while a finger is down or the offset is still below the top.
- (void)scheduleRefreshSettle
{
  _refreshSettleToken += 1;
  NSInteger token = _refreshSettleToken;
  __weak ShadowlistView *weakSelf = self;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.25 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
    ShadowlistView *strongSelf = weakSelf;
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

#pragma mark - State

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  _state = std::static_pointer_cast<ShadowlistViewShadowNode::ConcreteState const>(state);

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
    _appliedOffset = CGPointMake(
      nextStateData.containerOffsetX_,
      nextStateData.containerOffsetY_);
    _hasAppliedOffset = YES;
    _scrollView.contentOffset = _appliedOffset;
  }

  // Re-pin after content size/offset changed so a sticky footer stays put.
  [self applyStickyTransforms:NO];

  // Header may have re-measured; push the refresh indicator below it.
  [self applyRefreshProgressOffset];

  // Mid-drag commit: re-glue the picked-up row to the finger and re-apply the shuffle.
  if (_dragging) {
    [self updateDrag];
  }
}

- (void)finalizeUpdates:(RNComponentViewUpdateMask)updateMask
{
  [super finalizeUpdates:updateMask];
  [self applyStickyTransforms:NO];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  if (!_state) {
    return;
  }

  // The retract spring bounces the offset around the top, firing this each frame. Push the
  // settle out per frame so it fires only after the spring stops (see scheduleRefreshSettle).
  if (_refreshAwaitingSettle) {
    [self scheduleRefreshSettle];
  }

  // During refresh or the over-scroll gap above the top, skip the core update (it would
  // churn rows and write the offset back mid-settle); just keep the pins live.
  if (_refreshEnabled && (_refreshing || scrollView.contentOffset.y < 0)) {
    [self applyStickyTransforms:NO];
    return;
  }

  // Distinguish a genuine user scroll from the core's own echoed offset by comparing
  // against the last applied offset. The flag lets the core abandon an in-flight correction.
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

  // Advance the auto-hide only on genuine user scrolls (not our own echoed offset).
  [self applyStickyTransforms:userScrolled];
}

// Clear the user-scroll flag once the gesture and momentum end, so a later re-commit
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

#pragma mark - Element helpers

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

  // Bump the nonce so an unchanged index still triggers a fresh scroll.
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetIndex_ = index;
  nextStateData.containerOffsetIndexNonce_ = nextStateData.containerOffsetIndexNonce_ + 1;
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
  nextStateData.containerOffsetIndexNonce_ = nextStateData.containerOffsetIndexNonce_ + 1;
  nextStateData.containerOffsetEnabled_ = true;
  _state->updateState(std::move(nextStateData));
}

Class<RCTComponentViewProtocol> ShadowlistViewCls(void)
{
  return ShadowlistView.class;
}

@end
