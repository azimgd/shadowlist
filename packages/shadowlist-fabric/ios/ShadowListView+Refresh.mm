#import "ShadowListView.h"
#import "ShadowListView+Private.h"

#import <react/renderer/components/ShadowListViewSpec/EventEmitters.h>

using namespace facebook::react;

#if !TARGET_OS_OSX
/*
 * How long the spinner must stay still before onRefreshSettle fires.
 */
static const NSTimeInterval SL_REFRESH_SETTLE_DELAY = 0.25;

/*
 * How far a refresh started from code scrolls when the control has no height yet.
 */
static const CGFloat SL_REFRESH_REVEAL_FALLBACK = 60.0;

/*
 * Pull to refresh. iOS only.
 */
@implementation ShadowListView (Refresh)

/*
 * Create the refresh control on first use, tinted with refreshColor. iOS only.
 */
- (UIRefreshControl *)ensureRefreshControl
{
  if (!_refreshControl) {
    _refreshControl = [UIRefreshControl new];
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
 * Add or remove the control with refreshEnabled, apply the tint, and start or stop it
 * from the refreshing prop. Only acts when the prop really changes.
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
  SLF_TRACE("ev=refresh-prop refreshing=%d off=%.1f", refreshing ? 1 : 0, _scrollView.contentOffset.y);
  _refreshing = refreshing;

  if (!refreshing) {
    /*
     * Refresh ended. Fire onRefreshSettle once the spinner has retracted. JS can then
     * apply a held prepend while nothing is moving. See scheduleRefreshSettle.
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
      // Scroll to show the spinner when refresh starts from code. A pull already shows it.
      if (!_dragging && !_dragDropPending && _scrollView.contentOffset.y >= 0) {
        CGFloat reveal = _refreshControl.frame.size.height > 0
          ? _refreshControl.frame.size.height : SL_REFRESH_REVEAL_FALLBACK;
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
  SLF_TRACE("ev=refresh-pull off=%.1f", _scrollView.contentOffset.y);
  if (!_eventEmitter) {
    return;
  }
  std::static_pointer_cast<const ShadowListViewEventEmitter>(_eventEmitter)->onRefresh({});
}

/*
 * Tell JS the spinner has fully retracted. It can then apply a held prepend.
 */
- (void)emitRefreshSettle
{
  SLF_TRACE("ev=refresh-settle off=%.1f", _scrollView.contentOffset.y);
  if (!_eventEmitter) {
    return;
  }
  std::static_pointer_cast<const ShadowListViewEventEmitter>(_eventEmitter)->onRefreshSettle({});
}

/*
 * Each call bumps the token and schedules a check, and only the latest one fires. It
 * lands a short while after the spinner stops moving. It waits again while a finger is
 * down or the offset is still past the top.
 */
- (void)scheduleRefreshSettle
{
  _refreshSettleToken += 1;
  NSInteger token = _refreshSettleToken;
  __weak ShadowListView *weakSelf = self;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(SL_REFRESH_SETTLE_DELAY * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
    ShadowListView *strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    // A newer call took over.
    if (token != strongSelf->_refreshSettleToken) {
      return;
    }
    if (!strongSelf->_refreshAwaitingSettle || strongSelf->_refreshing) {
      return;
    }
    // Still moving, or a finger is down. Keep waiting.
    if (strongSelf->_scrollView.isDragging || strongSelf->_scrollView.isTracking ||
        strongSelf->_scrollView.contentOffset.y < -1.0) {
      [strongSelf scheduleRefreshSettle];
      return;
    }
    strongSelf->_refreshAwaitingSettle = NO;
    [strongSelf emitRefreshSettle];
  });
}

/*
 * Move the spinner below a pinned header so the header does not cover it.
 */
- (void)applyRefreshProgressOffset
{
  if (!_refreshControl) {
    return;
  }
  CGFloat offset = _progressViewOffset;
  if ((_stickyHeader || _autoHideHeader) && _stickyHeaderView) {
    offset += _stickyHeaderView.frame.size.height;
  }
  CGRect bounds = _refreshControl.bounds;
  if (bounds.origin.y == -offset) {
    return;
  }
  _refreshControl.bounds = CGRectMake(bounds.origin.x, -offset, bounds.size.width, bounds.size.height);
}

@end
#endif
