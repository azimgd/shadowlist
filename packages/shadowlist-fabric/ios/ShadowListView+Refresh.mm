#import "ShadowListView.h"
#import "ShadowListView+Private.h"

#import <react/renderer/components/ShadowListViewSpec/EventEmitters.h>

using namespace facebook::react;

#if !TARGET_OS_OSX
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

  if (refreshing == [self refreshInProgress]) {
    return;
  }
  SLF_TRACE("ev=refresh-prop refreshing=%d off=%.1f", refreshing ? 1 : 0, _scrollView.contentOffset.y);
  [self transitionRefresh:refreshing ? SLRefreshEvent::Begin : SLRefreshEvent::End];

  if (!_refreshControl) {
    [self checkRefreshSettle];
    return;
  }

  if (refreshing) {
    if (!_refreshControl.isRefreshing) {
      [_refreshControl beginRefreshing];
      // Scroll to show the spinner when refresh starts from code. A pull already shows it.
      if (!_dragging && !_dragDropPending && _scrollView.contentOffset.y >= 0) {
        CGFloat reveal =
          _refreshControl.frame.size.height > 0 ? _refreshControl.frame.size.height : SL_REFRESH_REVEAL_FALLBACK;
        [_scrollView setContentOffset:CGPointMake(_scrollView.contentOffset.x, _scrollView.contentOffset.y - reveal)
                             animated:YES];
      }
    }
  } else {
    /*
     * The spinner retracts in an animation of this transaction. Its completion is the moment
     * the spinner is gone, see checkRefreshSettle.
     */
    UIRefreshControl *control = _refreshControl;
    __weak ShadowListView *weakSelf = self;
    [CATransaction begin];
    [CATransaction setCompletionBlock:^{
      ShadowListView *strongSelf = weakSelf;
      if (strongSelf && strongSelf->_refreshControl == control) {
        [strongSelf transitionRefresh:SLRefreshEvent::Retracted];
        [strongSelf checkRefreshSettle];
      }
    }];
    [_refreshControl endRefreshing];
    [CATransaction commit];
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
 * The phase owns the refreshing prop, pending request and retract completion. The control
 * can refresh before JS receives the pull, so its isRefreshing cannot replace the prop.
 */
- (BOOL)refreshInProgress
{
  return _refreshPhase == SLRefreshPhase::Refreshing || _refreshPhase == SLRefreshPhase::RefreshingAwaiting;
}

- (BOOL)refreshSettleRequested
{
  return _refreshPhase == SLRefreshPhase::RefreshingAwaiting || _refreshPhase == SLRefreshPhase::RetractingAwaiting ||
         _refreshPhase == SLRefreshPhase::Awaiting;
}

- (void)transitionRefresh:(SLRefreshEvent)event
{
  switch (event) {
    case SLRefreshEvent::Begin:
      _refreshPhase = [self refreshSettleRequested] ? SLRefreshPhase::RefreshingAwaiting : SLRefreshPhase::Refreshing;
      break;
    case SLRefreshEvent::End:
      if (_refreshControl) {
        _refreshPhase = [self refreshSettleRequested] ? SLRefreshPhase::RetractingAwaiting : SLRefreshPhase::Retracting;
      } else {
        _refreshPhase = [self refreshSettleRequested] ? SLRefreshPhase::Awaiting : SLRefreshPhase::Rest;
      }
      break;
    case SLRefreshEvent::Await:
      if ([self refreshInProgress]) {
        _refreshPhase = SLRefreshPhase::RefreshingAwaiting;
      } else if (_refreshPhase == SLRefreshPhase::Retracting) {
        _refreshPhase = SLRefreshPhase::RetractingAwaiting;
      } else if (_refreshPhase != SLRefreshPhase::RetractingAwaiting) {
        _refreshPhase = SLRefreshPhase::Awaiting;
      }
      break;
    case SLRefreshEvent::Retracted:
      if (_refreshPhase == SLRefreshPhase::RetractingAwaiting) {
        _refreshPhase = SLRefreshPhase::Awaiting;
      } else if (_refreshPhase == SLRefreshPhase::Retracting) {
        _refreshPhase = SLRefreshPhase::Rest;
      }
      break;
    case SLRefreshEvent::Check:
      if (
        _refreshPhase == SLRefreshPhase::Awaiting && !_scrollSync.isLanding() && !_scrollSync.isArmedAnimated() &&
        _landCommandSequence == 0 && !_scrollingToTop && !_scrollToTopJumpPending && !_scrollView.isDragging &&
        !_scrollView.isTracking && !_scrollView.isDecelerating && _scrollView.contentOffset.y >= -1.0) {
        _refreshPhase = SLRefreshPhase::Sent;
        [self emitRefreshSettle];
      }
      break;
    case SLRefreshEvent::Reset:
      _refreshPhase = SLRefreshPhase::Rest;
      break;
  }
}

/*
 * Scroll callbacks also check: UIKit can return from a pull after the retract transaction
 * without another drag or scroll animation end. Gesture flags do not cover programmatic
 * motion, so the transition also waits for the existing command and scroll-to-top state.
 */
- (void)checkRefreshSettle
{
  [self transitionRefresh:SLRefreshEvent::Check];
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
