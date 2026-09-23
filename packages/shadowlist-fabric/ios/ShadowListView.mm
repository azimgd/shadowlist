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
/*
 * Lets a waiting scroll to top jump land in the same mount that brings its rows in.
 */
@interface ShadowListView () <RCTMountingTransactionObserving>
@end
#endif

#if !TARGET_OS_OSX
#import <UIKit/UIGestureRecognizerSubclass.h>

/*
 * Cancel the React Native touch under this view. RN only cancels a press when its own
 * ScrollView takes over, so without this a row pressed at the start of a swipe fires on release.
 * Toggling the touch recognizer is how RN cancels touches itself, and the scroll keeps going.
 */
static void CancelReactTouches(UIView *view)
{
  static Class touchHandlerClass;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    touchHandlerClass = NSClassFromString(@"RCTSurfaceTouchHandler");
  });
  if (touchHandlerClass == nil) {
    return;
  }
  for (UIView *ancestor = view; ancestor != nil; ancestor = ancestor.superview) {
    for (UIGestureRecognizer *recognizer in ancestor.gestureRecognizers) {
      if ([recognizer isKindOfClass:touchHandlerClass]) {
        recognizer.enabled = NO;
        recognizer.enabled = YES;
        return;
      }
    }
  }
}

/*
 * A tap while the list is still coasting after a flick should stop the scroll, not press a row.
 * RN's ScrollView does the same. We check this list and every scroll view around it at touch time.
 * The recognizer only watches and never recognizes, so it takes nothing from the list or rows.
 */
@interface ShadowListStopTapRecognizer : UIGestureRecognizer
@end

@implementation ShadowListStopTapRecognizer
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  BOOL coasting = NO;
  for (UIView *ancestor = self.view; ancestor != nil; ancestor = ancestor.superview) {
    if ([ancestor isKindOfClass:[UIScrollView class]] && ((UIScrollView *)ancestor).isDecelerating) {
      coasting = YES;
      break;
    }
  }
  if (coasting) {
    // Wait until RN has seen the touch start, so the cancel reaches that press.
    __weak UIView *weakView = self.view;
    dispatch_async(dispatch_get_main_queue(), ^{
      UIView *strong = weakView;
      if (strong != nil) {
        CancelReactTouches(strong);
      }
    });
  }
  self.state = UIGestureRecognizerStateFailed;
}
@end
#endif

using namespace facebook::react;

namespace {

using ShadowListStateData = ShadowListViewShadowNode::ConcreteState::Data;

/*
 * The host owned fields one state update changes. The update patches them onto the newest
 * committed state instead of copying the mounted one, so a core change that is committed
 * but not mounted yet, like new geometry or a drag event, is never undone by a scroll report.
 * Every update carries a full live report: offset, token, ack, user scroll flag and phase.
 * The rest is only set by the updates that own it.
 */
struct ShadowListStatePatch {
  ShadowListLiveScroll::Report report;
  bool containerOffsetEnabled = false;
  bool hasCommand = false;
  double commandIndex = 0.0;
  double commandSequence = 0.0;
  double commandViewPosition = 0.0;
  bool hasStartReachedEnabled = false;
  bool startReachedEnabled = true;
  bool hasEndReachedEnabled = false;
  bool endReachedEnabled = true;

  void applyTo(ShadowListStateData& data) const {
    data.containerOffsetX_ = report.offsetX;
    data.containerOffsetY_ = report.offsetY;
    data.containerOffsetEnabled_ = containerOffsetEnabled;
    data.commitToken_ = report.commitToken;
    data.concealGenerationAck_ = report.concealGenerationAck;
    data.userScrolled_ = report.userScrolled;
    data.scrollPhase_ = report.scrollPhase;
    data.hostSequence_ = static_cast<double>(report.sequence);
    if (hasCommand) {
      data.containerOffsetIndex_ = commandIndex;
      data.containerOffsetIndexSequence_ = commandSequence;
      data.containerOffsetIndexViewPosition_ = commandViewPosition;
    }
    if (hasStartReachedEnabled) {
      data.startReachedEnabled_ = startReachedEnabled;
    }
    if (hasEndReachedEnabled) {
      data.endReachedEnabled_ = endReachedEnabled;
    }
  }

  /*
   * Whether the committed data already holds all of this, so the update can be dropped.
   * Only when nothing waits on the host: a state with an offset to apply or hidden rows
   * needs its report even if it repeats, since each commit moves those along.
   */
  bool isNoOpOn(const ShadowListStateData& data) const {
    if (data.containerOffsetEnabled_ || containerOffsetEnabled || data.concealGeneration_ != 0.0) {
      return false;
    }
    return data.containerOffsetX_ == report.offsetX && data.containerOffsetY_ == report.offsetY &&
      data.commitToken_ == report.commitToken && data.concealGenerationAck_ == report.concealGenerationAck &&
      data.userScrolled_ == report.userScrolled && data.scrollPhase_ == report.scrollPhase &&
      (!hasCommand ||
       (data.containerOffsetIndex_ == commandIndex && data.containerOffsetIndexSequence_ == commandSequence &&
        data.containerOffsetIndexViewPosition_ == commandViewPosition)) &&
      (!hasStartReachedEnabled || data.startReachedEnabled_ == startReachedEnabled) &&
      (!hasEndReachedEnabled || data.endReachedEnabled_ == endReachedEnabled);
  }
};

}

#if SHADOWLIST_FRAME_TRACE_COMPILED && !TARGET_OS_OSX
/*
 * Frame trace for debugging scroll jumps. Off unless the app launches with
 * SHADOWLIST_FRAME_TRACE=1, on the simulator via SIMCTL_CHILD_SHADOWLIST_FRAME_TRACE=1.
 * Event lines mark each place we move the view. Frame lines show what each committed frame
 * put on screen, so a correction that lands a frame late shows up as rows jumping and back.
 */
static BOOL SLFrameTraceEnabled(void)
{
  static BOOL enabled = NO;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    const char *value = getenv("SHADOWLIST_FRAME_TRACE");
    enabled = value != NULL && strcmp(value, "1") == 0;
  });
  return enabled;
}

#define SLF_TRACE(fmt, ...)                                                    \
  do {                                                                         \
    if (SLFrameTraceEnabled()) {                                               \
      printf("[SLF] t=%.4f id=%ld " fmt "\n", CACurrentMediaTime(),            \
        (long)self.tag, ##__VA_ARGS__);                                        \
    }                                                                          \
  } while (0)

@interface ShadowListView () {
  CFRunLoopObserverRef _frameTraceObserver;
  NSString *_frameTraceLast;
}
- (void)traceFrame;
@end

// Run right after Core Animation commits, which uses order 2000000, to see the final frame.
static const CFIndex SLF_TRACE_OBSERVER_ORDER = 2000001;

static void SLFrameTraceCallback(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info)
{
  [(__bridge ShadowListView *)info traceFrame];
}
#else
#define SLF_TRACE(...) ((void)0)
#endif

/*
 * The native list view. Sticky pinning is in ShadowListView+Sticky and drag to reorder
 * is in ShadowListView+DragReorder.
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
#if !TARGET_OS_OSX
    ShadowListStopTapRecognizer *stopTap = [ShadowListStopTapRecognizer new];
    stopTap.cancelsTouchesInView = NO;
    stopTap.delaysTouchesBegan = NO;
    stopTap.delaysTouchesEnded = NO;
    [_scrollView addGestureRecognizer:stopTap];
#endif
    _scrollView.showsVerticalScrollIndicator = YES;
    _scrollView.showsHorizontalScrollIndicator = YES;
    _scrollView.scrollEnabled = YES;
#if !TARGET_OS_OSX
    _scrollView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    _scrollView.indicatorStyle = UIScrollViewIndicatorStyleWhite;
#endif

    _contentView = [[RCTUIView alloc] init];
#if TARGET_OS_OSX
    // On macOS the scroll view scrolls its documentView, and offset and size come from it.
    _scrollView.documentView = _contentView;
#else
    [_scrollView addSubview:_contentView];
#endif

    self.contentView = _scrollView;

#if !TARGET_OS_OSX
    // Long press picks a row up. The dragEnabled prop turns it on. iOS only.
    _dragRecognizer = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(handleDragGesture:)];
    _dragRecognizer.minimumPressDuration = 0.2;
    _dragRecognizer.enabled = NO;
    [_scrollView addGestureRecognizer:_dragRecognizer];
#endif
#if SHADOWLIST_FRAME_TRACE_COMPILED && !TARGET_OS_OSX
    if (SLFrameTraceEnabled()) {
      CFRunLoopObserverContext context = {0, (__bridge void *)self, NULL, NULL, NULL};
      _frameTraceObserver = CFRunLoopObserverCreate(
        kCFAllocatorDefault, kCFRunLoopBeforeWaiting | kCFRunLoopExit, true, SLF_TRACE_OBSERVER_ORDER,
        SLFrameTraceCallback, &context);
      CFRunLoopAddObserver(CFRunLoopGetMain(), _frameTraceObserver, kCFRunLoopCommonModes);
    }
#endif
  }

  return self;
}

#if SHADOWLIST_FRAME_TRACE_COMPILED && !TARGET_OS_OSX
- (void)dealloc
{
  if (_frameTraceObserver) {
    CFRunLoopObserverInvalidate(_frameTraceObserver);
    CFRelease(_frameTraceObserver);
  }
}
#endif

#pragma mark - Mounting

- (void)mountChildComponentView:(RCTUIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if ([childComponentView conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
    [_contentView insertSubview:childComponentView atIndex:index];
    /*
     * The new row may sit above the sticky views, and during a drag above the dragged row
     * and unshifted. Fix both once when the transaction finishes, before the frame renders.
     */
    _stickyOrderDirty = YES;
    _mountNeedsSticky = YES;
#if !TARGET_OS_OSX
    if (_dragging && _draggedView) {
      _mountNeedsDragShuffle = YES;
    }
    // Add the VoiceOver move actions. Does nothing unless dragEnabled.
    [self applyDragAccessibilityActionsToView:childComponentView];
#endif
    return;
  }

  if ([childComponentView conformsToProtocol:@protocol(RCTShadowListTemplateViewViewProtocol)]) {
    const auto& templateProps = *std::static_pointer_cast<const ShadowListTemplateViewProps>(childComponentView.props);
    /*
     * Check each type by name. Header and empty can be mounted together, and a plain else
     * would let empty replace the real sticky header.
     */
    if (templateProps.templateType == "footer") {
      _stickyFooterView = childComponentView;
    } else if (templateProps.templateType == "sectionHeader") {
      _sectionHeaderOverlay = childComponentView;
    } else if (templateProps.templateType == "header") {
      _stickyHeaderView = childComponentView;
    }
    [_contentView addSubview:childComponentView];
    _stickyOrderDirty = YES;
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
   * The dragged or dropping row can be deleted mid drag and unmount here. Stop the drag
   * so nothing is left running. teardownDrag sends no reorder.
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
  _stickyOrderDirty = YES;
  _overlayOrderDirty = YES;
  _mountNeedsSticky = NO;
  _mountNeedsDragShuffle = NO;
  _stickyHeaderIndices.clear();
  _stickyHeaderOffsets.clear();
  _stickyHeaderSizes.clear();
  _copiedStickyHeaderIndices.reset();
  _copiedStickyHeaderOffsets.reset();
  _copiedStickyHeaderSizes.reset();
  _copiedSnapOffsets.reset();
  _dragEnabled = NO;
#if !TARGET_OS_OSX
  [self teardownDrag];
  _dragRecognizer.enabled = NO;
#endif
  /*
   * A recycled view must not pass its old scroll position or state to the next list.
   * Reset _state before moving the offset. setContentOffset reports a scroll right away,
   * which would otherwise reach the old list as a fake user scroll to the top.
   */
  _appliedOffset = CGPointZero;
  _hasAppliedOffset = NO;
  _armedToken = 0;
  _echoedToken = 0;
  _publishedGesture = NO;
  _shiftedToken = 0;
  _shiftedTokenDelta = 0.0;
  _yieldedToken = 0;
  _reportedDuringStateUpdate = NO;
  _commandIndex = 0.0;
  _commandViewPosition = 0.0;
  _commandSequence = 0.0;
#if !TARGET_OS_OSX
  [self cancelScrollToTop];
#endif
  _state.reset();
  // The live report belongs to the old list, so let it go with the state.
  _liveScroll.reset();
  _lastLiveReport = {};
  _lastPushedReport = {};
  _hasPushedReport = NO;
  [_scrollView setContentOffset:CGPointZero animated:NO];
  /*
   * Clear the old content size too. Sticky pinning runs on mount, before the new list's
   * first state lands, and would use the stale size.
   */
  _scrollView.contentSize = CGSizeZero;
  _contentView.frame = CGRectZero;
  [super prepareForRecycle];
}

#pragma mark - Props

- (void)updateProps:(const Props::Shared&)props oldProps:(const Props::Shared&)oldProps
{
  const auto& nextProps = *std::static_pointer_cast<const ShadowListViewProps>(props);
  // _props still has the old props until super updateProps swaps them.
  const auto& previousProps = *std::static_pointer_cast<const ShadowListViewProps>(_props);
  // Turning pinning on must raise the sticky views on the next pin.
  if (_stickyHeader != nextProps.stickyHeader || _stickyFooter != nextProps.stickyFooter ||
      _autoHideHeader != nextProps.autoHideHeader || _autoHideFooter != nextProps.autoHideFooter) {
    _stickyOrderDirty = YES;
  }
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
   * Update the VoiceOver actions only when dragEnabled changes, since this runs on every
   * props commit. New rows get them in mountChildComponentView.
   */
  if (previousProps.dragEnabled != nextProps.dragEnabled) {
    for (UIView *subview in _contentView.subviews) {
      if ([subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
        [self applyDragAccessibilityActionsToView:subview];
      }
    }
  }
  // Snap deceleration and pull to refresh have no macOS version.
  _scrollView.decelerationRate = _snapToItem ? UIScrollViewDecelerationRateFast : UIScrollViewDecelerationRateNormal;
  [self applyRefreshState:nextProps.refreshEnabled
                refreshing:nextProps.refreshing
                     color:RCTUIColorFromSharedColor(nextProps.refreshColor)];
#endif

  [super updateProps:props oldProps:oldProps];

  [self applyStickyTransforms:NO];
}

#if !TARGET_OS_OSX
#pragma mark - Pull to refresh

/*
 * Create the refresh control on first use, tinted with refreshColor. iOS only.
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
     * Refresh ended. Fire onRefreshSettle once the spinner has retracted, so JS can
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
  SLF_TRACE("ev=refresh-pull off=%.1f", _scrollView.contentOffset.y);
  if (!_eventEmitter) {
    return;
  }
  std::static_pointer_cast<const ShadowListViewEventEmitter>(_eventEmitter)->onRefresh({});
}

/*
 * Tell JS the spinner has fully retracted, so it can apply a held prepend.
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
 * Each call bumps the token and schedules a check, and only the latest one fires, so it
 * lands a short while after the spinner stops moving. It waits again while a finger is
 * down or the offset is still past the top.
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
    // A newer call took over.
    if (token != strongSelf->_refreshSettleToken) {
      return;
    }
    if (!strongSelf->_refreshAwaitingSettle || strongSelf->_refreshing) {
      return;
    }
    // Still moving, or a finger is down, so keep waiting.
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
  _reportedDuringStateUpdate = NO;
  // A state update sent from inside this mount waits for the next beat, see stateUpdateMode.
  _inStateUpdate = YES;

  const auto& nextStateData = _state->getData();
  _liveScroll = nextStateData.liveScroll_;

  /*
   * Copy the section header positions for pinning on each scroll. A null pointer means
   * empty, see ShadowListViewState. The core only publishes a new pointer when the values
   * changed, so copy only then. Copying on every mount cost a full snap list per frame.
   */
  BOOL stickyGeometryChanged = NO;
  auto copyPublished = [&stickyGeometryChanged](auto& destination, auto& copiedFrom, const auto& published) {
    if (copiedFrom == published && (published || destination.empty())) {
      return;
    }
    copiedFrom = published;
    stickyGeometryChanged = YES;
    if (published) {
      destination.assign(published->begin(), published->end());
    } else {
      destination.clear();
    }
  };
  copyPublished(_stickyHeaderIndices, _copiedStickyHeaderIndices, nextStateData.stickyHeaderIndices_);
  copyPublished(_stickyHeaderOffsets, _copiedStickyHeaderOffsets, nextStateData.stickyHeaderOffsets_);
  copyPublished(_stickyHeaderSizes, _copiedStickyHeaderSizes, nextStateData.stickyHeaderSizes_);
  copyPublished(_snapOffsets, _copiedSnapOffsets, nextStateData.snapOffsets_);

  __unused CGFloat traceBeforeY = _horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  __unused CGFloat traceBeforeHeight = _horizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height;
  /*
   * Give a scroll range only along the scroll axis. The other axis can lag a size change,
   * and a too wide vertical list would scroll sideways under the finger.
   * Write only on a change. Most mounts keep the size, and each write costs UIKit a layout.
   */
  CGSize contentSize = _horizontal
    ? CGSizeMake(nextStateData.totalContainerWidth_, 0)
    : CGSizeMake(0, nextStateData.totalContainerHeight_);
  CGRect contentFrame = CGRectMake(0, 0, nextStateData.totalContainerWidth_, nextStateData.totalContainerHeight_);
  BOOL contentSizeChanged = !CGSizeEqualToSize(_scrollView.contentSize, contentSize) ||
    !CGRectEqualToRect(_contentView.frame, contentFrame);
  if (contentSizeChanged) {
    // If these writes clamp the offset, it is not reported as a user scroll. See _applyingContentSize.
    _applyingContentSize = YES;
    _scrollView.contentSize = contentSize;
    _contentView.frame = contentFrame;
    _applyingContentSize = NO;
  }
  CGPoint offsetBeforeCorrection = _scrollView.contentOffset;

  SL_LOG("mm.updateState: contentSize=(%.1f,%.1f) enabled=%d offset=(%.1f,%.1f) curOffset=(%.1f,%.1f)",
    nextStateData.totalContainerWidth_, nextStateData.totalContainerHeight_,
    nextStateData.containerOffsetEnabled_ ? 1 : 0,
    nextStateData.containerOffsetX_, nextStateData.containerOffsetY_,
    _scrollView.contentOffset.x, _scrollView.contentOffset.y);

  /*
   * A correction made against a waiting scroll to top jump moves the jump target instead
   * of the view. The view follows when the jump lands.
   */
  BOOL retargetsScrollToTopJump = _scrollToTopJumpPending && nextStateData.containerOffsetEnabled_ &&
    fabs(nextStateData.containerOffsetBaseY_ - _scrollToTopJumpY) < 0.5;
  if (retargetsScrollToTopJump) {
    _scrollToTopJumpY = nextStateData.containerOffsetY_;
    _scrollToTopJumpToken = (uint64_t)nextStateData.commitToken_;
#if !TARGET_OS_OSX
    /*
     * The jump applies this whole correction when it lands. The core may send the same
     * token again with the full amount, so remember what we applied and only add the rest.
     */
    if (_scrollToTopJumpToken != 0) {
      _shiftedToken = _scrollToTopJumpToken;
      _shiftedTokenDelta = nextStateData.containerOffsetY_ - nextStateData.containerOffsetBaseY_;
    }
#endif
  } else if (nextStateData.containerOffsetEnabled_ && !_dragging && !_dragDropPending) {
    // While a row is dragged or dropping we own the offset and skip core corrections.
#if !TARGET_OS_OSX
    /*
     * A ShadowListNative scroll command reaches the core in a commit, not through this view.
     * Stop momentum when it mounts, like scrollToIndex does, and write the offset.
     * A finger on the list wins, and the core lets the drag cancel the command.
     */
    uint64_t yieldToken = (uint64_t)nextStateData.momentumYieldToken_;
    BOOL scrollCommand = yieldToken != 0 && (uint64_t)nextStateData.commitToken_ == yieldToken &&
      !_scrollView.isTracking;
    if (scrollCommand && yieldToken != _yieldedToken) {
      _yieldedToken = yieldToken;
      [self stopMomentum];
    }
#else
    BOOL scrollCommand = NO;
#endif
    CGPoint before = _scrollView.contentOffset;
    _appliedOffset = CGPointMake(
      nextStateData.containerOffsetX_,
      nextStateData.containerOffsetY_);
#if !TARGET_OS_OSX
    /*
     * While the view is moving, by scroll to top, a finger or a fling, it has moved on by the
     * time a correction mounts. Writing the fixed offset would undo that, so shift the live
     * offset by the correction instead, clamped to the scroll range.
     * Keep shifting for corrections made during a gesture, or ones we already shifted, even
     * after the motion stops. Scroll commands carry the live offset, so shifting is the same.
     */
    uint64_t token = (uint64_t)nextStateData.commitToken_;
    BOOL continuesShiftedCorrection = token != 0 && token == _shiftedToken;
    BOOL computedDuringGesture = token != 0 &&
      (nextStateData.userScrolled_ || nextStateData.scrollPhase_ != SCROLL_PHASE_IDLE);
    if (!scrollCommand && (_scrollingToTop || _scrollView.isDragging || _scrollView.isDecelerating ||
        continuesShiftedCorrection || computedDuringGesture)) {
      // Work along the scroll axis. A horizontal list corrects x.
      BOOL horizontal = _horizontal;
      CGFloat top = horizontal ? -_scrollView.contentInset.left : -_scrollView.contentInset.top;
      CGFloat maxY = horizontal
        ? MAX(top, _scrollView.contentSize.width - _scrollView.bounds.size.width + _scrollView.contentInset.right)
        : MAX(top, _scrollView.contentSize.height - _scrollView.bounds.size.height + _scrollView.contentInset.bottom);
      CGFloat shift = horizontal
        ? nextStateData.containerOffsetX_ - nextStateData.containerOffsetBaseX_
        : nextStateData.containerOffsetY_ - nextStateData.containerOffsetBaseY_;
      /*
       * The core resends the full correction with each new report until we echo its token.
       * Shift only by what this token has not moved yet, or a prepend during a fling
       * would shift the view again each time.
       */
      if (token != 0) {
        CGFloat unapplied = token == _shiftedToken ? shift - _shiftedTokenDelta : shift;
        _shiftedToken = token;
        _shiftedTokenDelta = shift;
        shift = unapplied;
      }
      _appliedOffset = horizontal
        ? CGPointMake(MIN(MAX(before.x + shift, top), maxY), before.y)
        : CGPointMake(before.x, MIN(MAX(before.y + shift, top), maxY));
    }
#endif
    /*
     * Arm this before writing. A real move calls scrollViewDidScroll right away, and it
     * needs the flag to know the move was ours and send the token back.
     */
    _hasAppliedOffset = YES;
    _armedToken = (uint64_t)nextStateData.commitToken_;
    _scrollView.contentOffset = _appliedOffset;
    /*
     * If the write did not move the view, no scroll report fires and the flag would stay
     * set, swallowing the next real user scroll. Clear it.
     */
    CGPoint after = _scrollView.contentOffset;
    if (fabs(after.x - before.x) < 0.01 && fabs(after.y - before.y) < 0.01) {
      _hasAppliedOffset = NO;
      _armedToken = 0;
    }
  }

  /*
   * A state that hides rows waits for a report built on it. The write above usually sends
   * one. If it did not, send it here.
   */
  if (nextStateData.concealGeneration_ != 0.0 && nextStateData.containerOffsetEnabled_ && !_reportedDuringStateUpdate) {
    [self reportConcealedRowsMounted];
  }

  SLF_TRACE("ev=state cs=%.1f->%.1f off=%.1f->%.1f enabled=%d core=%.1f base=%.1f token=%llu user=%d phase=%.0f stt=%d jumpPending=%d retarget=%d conceal=%.0f",
    traceBeforeHeight, _horizontal ? nextStateData.totalContainerWidth_ : nextStateData.totalContainerHeight_,
    traceBeforeY, _horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y,
    nextStateData.containerOffsetEnabled_ ? 1 : 0,
    _horizontal ? nextStateData.containerOffsetX_ : nextStateData.containerOffsetY_,
    _horizontal ? nextStateData.containerOffsetBaseX_ : nextStateData.containerOffsetBaseY_,
    (unsigned long long)nextStateData.commitToken_,
    nextStateData.userScrolled_ ? 1 : 0, nextStateData.scrollPhase_, _scrollingToTop ? 1 : 0,
    _scrollToTopJumpPending ? 1 : 0, retargetsScrollToTopJump ? 1 : 0, nextStateData.concealGeneration_);

  /*
   * Pin again after the size, offset or section positions changed so a sticky footer stays
   * put. A mount that changed none of them leaves the pins as they are. Scroll frames and
   * finalizeUpdates pin on their own.
   */
  CGPoint offsetAfterCorrection = _scrollView.contentOffset;
  if (contentSizeChanged || stickyGeometryChanged ||
      !CGPointEqualToPoint(offsetBeforeCorrection, offsetAfterCorrection)) {
    [self applyStickyTransforms:NO];
  }

#if !TARGET_OS_OSX
  // The header may have a new size, so move the spinner below it.
  [self applyRefreshProgressOffset];

  // A commit during a drag. Put the row back under the finger and shift the others again.
  if (_dragging) {
    [self updateDrag];
  }
#endif
  _inStateUpdate = NO;
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
   * The spinner retracting fires this every frame. Push the settle back each time so it
   * fires only after it stops. See scheduleRefreshSettle.
   */
  if (_refreshAwaitingSettle) {
    [self scheduleRefreshSettle];
  }
#endif

  /*
   * Pull to refresh offsets are reported like any other. Rows prepended while the spinner
   * shows are placed against this offset, so the core must know it, or the row the user
   * reads would move up by the spinner's height.
   */

  /*
   * Tell a user scroll from our own move by who caused it. If we just applied a core
   * offset, this report is ours wherever it landed, and we send the token back so the core
   * can match its correction. Otherwise the user is scrolling and the core can drop it.
   */
  BOOL userScrolled = !_applyingContentSize;
  BOOL ourMove = NO;
  if (_hasAppliedOffset) {
    userScrolled = NO;
    ourMove = YES;
    _echoedToken = _armedToken;
    _hasAppliedOffset = NO;
    _armedToken = 0;
  }
  /*
   * Keep sending the last token on later reports too. Updates merge, so the next frame
   * can replace our report before the core sees it, and the core would apply the
   * correction again. Tokens are never reused, so an old one matches nothing.
   */
  uint64_t echoToken = _echoedToken;

  /*
   * The gesture phase, finger down, momentum or idle. It stays set between frames so the
   * core keeps the inverted bottom pin off while a finger rests on the list.
   * See Container::gestureActive. macOS has no drag state, so it keeps the mounted one.
   */
#if !TARGET_OS_OSX
  double scrollPhase = [self currentScrollPhase];
#else
  double scrollPhase = _state->getData().scrollPhase_;
#endif
  _publishedGesture = userScrolled || scrollPhase != SCROLL_PHASE_IDLE;

  /*
   * Every frame goes into the live report. The report is built on the mounted state, so it
   * acknowledges the rows that state hides.
   */
  ShadowListStatePatch patch;
  patch.report = [self writeLiveReportAt:scrollView.contentOffset
                            userScrolled:userScrolled
                             scrollPhase:scrollPhase
                             commitToken:echoToken
                    concealGenerationAck:_state->getData().concealGeneration_];
  BOOL needsCommit = ourMove || [self liveReportNeedsCommit:patch.report];

  SL_LOG("mm.scrollViewDidScroll: offset=(%.1f,%.1f) userScrolled=%d token=%llu seq=%llu commit=%d",
    scrollView.contentOffset.x, scrollView.contentOffset.y, userScrolled ? 1 : 0,
    (unsigned long long)echoToken, (unsigned long long)patch.report.sequence, needsCommit ? 1 : 0);
  if (needsCommit) {
    _reportedDuringStateUpdate = YES;
    [self carryScrollCommandIntoPatch:patch];
    [self commitStatePatch:patch];
  }

  // Only real user scrolls move the auto hide bars.
  [self applyStickyTransforms:userScrolled];
}

#pragma mark - Live reports

/*
 * Store where the view is in the live report and return it with its sequence. Every state
 * update also writes one first, so its sequence tells adopt() which of the two is newer.
 */
- (ShadowListLiveScroll::Report)writeLiveReportAt:(CGPoint)offset
                                     userScrolled:(BOOL)userScrolled
                                      scrollPhase:(double)scrollPhase
                                      commitToken:(uint64_t)commitToken
                             concealGenerationAck:(double)concealGenerationAck
{
  ShadowListLiveScroll::Report report;
  report.offsetX = offset.x;
  report.offsetY = offset.y;
  report.userScrolled = userScrolled;
  report.scrollPhase = scrollPhase;
  report.commitToken = (double)commitToken;
  report.concealGenerationAck = concealGenerationAck;
  report.sequence = _liveScroll ? _liveScroll->write(report) : 0;
  _lastLiveReport = report;
  return report;
}

/*
 * The user scroll flag and phase that later updates carry, which are the newest report's.
 * Before the first report they are the mounted state's.
 */
- (BOOL)currentUserScrolled
{
  if (_lastLiveReport.sequence > 0 || !_state) {
    return _lastLiveReport.userScrolled;
  }
  return _state->getData().userScrolled_;
}

- (double)currentReportedScrollPhase
{
  if (_lastLiveReport.sequence > 0 || !_state) {
    return _lastLiveReport.scrollPhase;
  }
  return _state->getData().scrollPhase_;
}

/*
 * Whether a scroll frame must become a state update. Conservative on purpose: anything the
 * core reacts to other than the offset moving inside the band sends it.
 */
- (BOOL)liveReportNeedsCommit:(const ShadowListLiveScroll::Report&)report
{
  if (!_state || !_liveScroll || report.sequence == 0 || !_hasPushedReport) {
    return YES;
  }
  const auto& mounted = _state->getData();
  // A correction just mounted, or rows hidden until a report acknowledges them.
  if (mounted.containerOffsetEnabled_ || mounted.concealGeneration_ != 0.0) {
    return YES;
  }
  // The gesture changed, a correction was echoed, or a hide was acknowledged.
  if (report.userScrolled != _lastPushedReport.userScrolled || report.scrollPhase != _lastPushedReport.scrollPhase ||
      report.commitToken != _lastPushedReport.commitToken ||
      report.concealGenerationAck != _lastPushedReport.concealGenerationAck) {
    return YES;
  }
  // UIKit clamped the offset, or a mount moved it.
  if (_applyingContentSize || _inStateUpdate) {
    return YES;
  }
  // Pull to refresh, scroll to top and drag to reorder all lean on every frame.
#if !TARGET_OS_OSX
  if (_refreshing || _refreshAwaitingSettle || (_refreshControl != nil && _refreshControl.isRefreshing)) {
    return YES;
  }
#endif
  if (_scrollingToTop || _scrollToTopJumpPending || _dragging || _dragDropPending) {
    return YES;
  }
  // The offset left the band the mounted layout pass published. An empty band never holds it.
  double offset = _horizontal ? report.offsetX : report.offsetY;
  return !(mounted.offsetBandLow_ <= offset && offset <= mounted.offsetBandHigh_);
}

/*
 * Immediate commits run the whole commit and mount on this thread right away, so a frame
 * that needs new rows gets them before it renders. Never from inside a mount, where it
 * could loop through our own state update. See SHADOWLIST_IMMEDIATE_STATE.
 */
- (EventQueue::UpdateMode)stateUpdateMode
{
  if (!shadowListImmediateStateEnabled() || _inStateUpdate || _inMountObserver || ![NSThread isMainThread]) {
    return EventQueue::UpdateMode::Asynchronous;
  }
  return EventQueue::UpdateMode::unstable_Immediate;
}

/*
 * Send a state update that patches the newest committed state, see ShadowListStatePatch.
 */
- (void)commitStatePatch:(const ShadowListStatePatch&)patch
{
  if (!_state) {
    return;
  }
  _lastPushedReport = patch.report;
  _hasPushedReport = YES;
  auto updateMode = [self stateUpdateMode];
  _state->updateState(
    [patch](const ShadowListStateData& oldData) -> StateData::Shared {
      if (patch.isNoOpOn(oldData)) {
        return nullptr;
      }
      auto nextData = std::make_shared<ShadowListStateData>(oldData);
      patch.applyTo(*nextData);
      return nextData;
    },
    updateMode);
}

/*
 * A patch with the live offset and the last echoed token, like a scroll report, and the
 * given gesture state. Built on the mounted state, so it acknowledges the rows that hides.
 */
- (ShadowListStatePatch)livePatchWithUserScrolled:(BOOL)userScrolled scrollPhase:(double)scrollPhase
{
  ShadowListStatePatch patch;
  patch.report = [self writeLiveReportAt:_scrollView.contentOffset
                            userScrolled:userScrolled
                             scrollPhase:scrollPhase
                             commitToken:_echoedToken
                    concealGenerationAck:_state ? _state->getData().concealGeneration_ : 0.0];
  [self carryScrollCommandIntoPatch:patch];
  return patch;
}

- (ShadowListStatePatch)livePatch
{
  return [self livePatchWithUserScrolled:[self currentUserScrolled] scrollPhase:[self currentReportedScrollPhase]];
}

/*
 * Copy the last scroll command into a patch. See _commandSequence.
 */
- (void)carryScrollCommandIntoPatch:(ShadowListStatePatch&)patch
{
  if (_commandSequence > 0) {
    patch.hasCommand = true;
    patch.commandIndex = _commandIndex;
    patch.commandSequence = _commandSequence;
    patch.commandViewPosition = _commandViewPosition;
  }
}

/*
 * Clear the user scroll flag once the gesture and momentum end, so a later commit is
 * not taken for a user scroll and does not cancel a real correction.
 */
- (void)clearUserScrolled
{
  if (!_state) {
    return;
  }

  const auto& mountedStateData = _state->getData();
  /*
   * Skip only if neither the mounted state nor our last update was a gesture. The mounted
   * state alone is not enough. After a pull past the top, the bounce back sends one report
   * that may not have mounted yet. Skipping then leaves it as the last state, and later
   * corrections that keep the visible content in place would be dropped as if a gesture took over.
   * _publishedGesture also covers frames that only went into the live report.
   */
  if (!_publishedGesture && !mountedStateData.userScrolled_ && mountedStateData.scrollPhase_ == SCROLL_PHASE_IDLE) {
    return;
  }
  _publishedGesture = NO;
  [self commitStatePatch:[self livePatchWithUserScrolled:NO scrollPhase:SCROLL_PHASE_IDLE]];
}

#if !TARGET_OS_OSX
/*
 * The gesture phase sent with each scroll frame. Only isTracking means a finger is down.
 * isDragging stays set during a fling, and calling that a drag would cancel scroll commands.
 * Scroll to top counts as momentum, or the inverted bottom pin would snap the view back down.
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

// macOS only gives us scrollViewDidScroll, so the callbacks below and snap to item are iOS only.
#if !TARGET_OS_OSX
/*
 * A swipe that starts on a row may be taken by a scroll view around the list, like a sideways
 * grid. That drag must also end the press, so listen to every outer scroll view's pan while
 * on screen and stop when we leave.
 */
- (void)didMoveToWindow
{
  [super didMoveToWindow];
  for (UIPanGestureRecognizer *pan in _ancestorPans) {
    [pan removeTarget:self action:@selector(ancestorDidPan:)];
  }
  [_ancestorPans removeAllObjects];
  if (self.window == nil) {
    return;
  }
  if (_ancestorPans == nil) {
    _ancestorPans = [NSHashTable weakObjectsHashTable];
  }
  for (UIView *ancestor = self.superview; ancestor != nil; ancestor = ancestor.superview) {
    if ([ancestor isKindOfClass:[UIScrollView class]]) {
      UIPanGestureRecognizer *pan = ((UIScrollView *)ancestor).panGestureRecognizer;
      [pan addTarget:self action:@selector(ancestorDidPan:)];
      [_ancestorPans addObject:pan];
    }
  }
}

- (void)ancestorDidPan:(UIPanGestureRecognizer *)pan
{
  if (pan.state == UIGestureRecognizerStateBegan) {
    CancelReactTouches(self);
  }
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
  // A swipe that began on a row is a scroll, not a press on that row.
  CancelReactTouches(self);
  // The user grabbed the list. Clear our pending move so the drag counts as a user scroll.
  SLF_TRACE("ev=drag-begin off=%.1f,%.1f", scrollView.contentOffset.x, scrollView.contentOffset.y);
  _hasAppliedOffset = NO;
  _armedToken = 0;
  // A finger takes over from scroll to top. The drag reports its own phase.
  [self cancelScrollToTop];
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
  SLF_TRACE("ev=drag-end off=%.1f,%.1f decel=%d", scrollView.contentOffset.x, scrollView.contentOffset.y, decelerate ? 1 : 0);
  if (!decelerate) {
    [self clearUserScrolled];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
  SLF_TRACE("ev=decel-end off=%.1f,%.1f", scrollView.contentOffset.x, scrollView.contentOffset.y);
  [self clearUserScrolled];
}

/*
 * Make a fling come to rest on a row edge. Pick the core's snap offset nearest to where
 * the fling would have landed.
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
 * Status bar tap. Return NO so UIKit does not animate and run ours instead.
 * A horizontal list has nothing to scroll up, so let UIKit handle it.
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

// Scroll to top duration, close to UIKit's.
static const CFTimeInterval SCROLL_TO_TOP_DURATION = 0.45;

/*
 * How far the animation travels, in screens. The core keeps about a screen of rows mounted
 * past the visible area, so this never outruns them. Longer trips jump closer first, like UIKit.
 */
static const CGFloat SCROLL_TO_TOP_ANIMATED_VIEWPORTS = 1.0;

/*
 * Largest step per frame, in screens. The animation peaks near 0.1. This stops a correction
 * mid flight from stretching a step past the mounted rows.
 */
static const CGFloat SCROLL_TO_TOP_MAX_STEP_VIEWPORTS = 0.35;

// How much of the screen at the jump target must be covered by rows before the jump lands.
static const CGFloat SCROLL_TO_TOP_JUMP_COVERAGE = 0.9;

/*
 * Longest wait for the rows at the jump target, in seconds. If they never arrive, say the
 * JS thread is busy, land anyway so the tap is not ignored.
 */
static const CFTimeInterval SCROLL_TO_TOP_JUMP_MAX_WAIT = 0.5;

- (void)startScrollToTop
{
  CGFloat top = -_scrollView.contentInset.top;
  if (_scrollingToTop || _scrollView.contentOffset.y <= top + 0.5) {
    return;
  }
  // Stop any momentum first, like UIKit does.
  [_scrollView setContentOffset:_scrollView.contentOffset animated:NO];

  SLF_TRACE("ev=stt-start off=%.1f cs=%.1f", _scrollView.contentOffset.y, _scrollView.contentSize.height);
  _scrollingToTop = YES;
  _scrollToTopProgress = 0.0;
  _scrollToTopStartTime = CACurrentMediaTime();
  _scrollToTopLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(scrollToTopTick)];
  [_scrollToTopLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

  /*
   * A long trip jumps first. Jumping right away would show a blank screen until rows mount,
   * so send the target to the core and stay put. The core renders rows there, and the jump
   * lands in the mount that brings them in. See mountingTransactionDidMount.
   */
  CGFloat viewport = _scrollView.bounds.size.height;
  CGFloat jumpY = top + viewport * SCROLL_TO_TOP_ANIMATED_VIEWPORTS;
  if (_state && viewport > 0.0 && _scrollView.contentOffset.y > jumpY + viewport) {
    _scrollToTopJumpPending = YES;
    _scrollToTopJumpY = jumpY;
    _scrollToTopJumpToken = 0;
    /*
     * The live report says the view is at the target too, so a commit for another reason
     * renders the same rows. It keeps the mounted ack, since the jump reports when it lands.
     */
    ShadowListStatePatch patch;
    patch.report = [self writeLiveReportAt:CGPointMake(_scrollView.contentOffset.x, jumpY)
                              userScrolled:YES
                               scrollPhase:SCROLL_PHASE_SETTLING
                               commitToken:0
                      concealGenerationAck:_state->getData().concealGenerationAck_];
    _publishedGesture = YES;
    [self carryScrollCommandIntoPatch:patch];
    [self commitStatePatch:patch];
  }
}

/*
 * Whether mounted rows fill the screen starting at y, so a jump there shows content right away.
 * Row frames already match the core's layout, including rows just added around the target.
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
 * Jump to the target once its rows are mounted or the wait runs out, then animate the rest.
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
   * A core correction moved the jump target. Send its token with the landing report so the
   * core knows its correction arrived and does not shift again.
   */
  if (_scrollToTopJumpToken != 0) {
    _hasAppliedOffset = YES;
    _armedToken = _scrollToTopJumpToken;
  }
  SLF_TRACE("ev=stt-land %.1f->%.1f waitedTooLong=%d", before.y, target.y, waitedTooLong ? 1 : 0);
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
 * Runs after every mount, and does nothing unless a scroll to top jump is waiting.
 * Landing here, before the frame renders, keeps the jump from showing a blank frame.
 */
- (void)mountingTransactionDidMount:(const MountingTransaction&)transaction
               withSurfaceTelemetry:(const SurfaceTelemetry&)surfaceTelemetry
{
  // Rows mounted in this transaction. Pin and shuffle once for all of them.
  if (_mountNeedsSticky) {
    _mountNeedsSticky = NO;
    [self applyStickyTransforms:NO];
  }
#if !TARGET_OS_OSX
  if (_mountNeedsDragShuffle) {
    _mountNeedsDragShuffle = NO;
    // New rows go below the dragged row and get shifted like the rest.
    if (_dragging && _draggedView) {
      [_contentView bringSubviewToFront:_draggedView];
      _stickyOrderDirty = YES;
      [self applyDragShuffle];
    }
  }
#endif
  if (_scrollToTopJumpPending) {
    _inMountObserver = YES;
    [self landScrollToTopJumpIfReady];
    _inMountObserver = NO;
  }
}

/*
 * One frame of the ease toward the top. The distance left comes from the live offset, so a
 * core correction since the last frame just makes the rest of the trip longer or shorter.
 * Steps are capped, and a capped trip keeps going past the normal duration if needed.
 */
- (void)scrollToTopTick
{
  if (!_scrollingToTop) {
    return;
  }

  // Still waiting for rows at the jump target. The mount observer usually lands it first.
  if (_scrollToTopJumpPending) {
    [self landScrollToTopJumpIfReady];
    return;
  }

  // The last frame reached the top, so finish.
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
    // Count progress by how far the capped step really went.
    progress = 1.0 - remainingFraction * (nextY - top) / (currentY - top);
  }
  _scrollToTopProgress = progress;

  if (fabs(nextY - currentY) >= 0.01) {
    SLF_TRACE("ev=stt-tick %.1f->%.1f progress=%.3f", currentY, nextY, progress);
    _scrollView.contentOffset = CGPointMake(_scrollView.contentOffset.x, nextY);
  }
}

/*
 * Stop the animation. Returns whether one was running.
 */
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
 * Stop the animation and tell the core the motion is over. Send the view's real offset,
 * since the mounted one may be older and would make the core render rows we already left.
 */
- (void)finishScrollToTop
{
  SLF_TRACE("ev=stt-finish off=%.1f", _scrollView.contentOffset.y);
  [self cancelScrollToTop];
  if (!_state) {
    return;
  }
  ShadowListStatePatch patch;
  patch.report = [self writeLiveReportAt:_scrollView.contentOffset
                            userScrolled:NO
                             scrollPhase:SCROLL_PHASE_IDLE
                             commitToken:0
                    concealGenerationAck:_state->getData().concealGenerationAck_];
  _publishedGesture = NO;
  [self carryScrollCommandIntoPatch:patch];
  [self commitStatePatch:patch];
}
#endif // !TARGET_OS_OSX

#if SHADOWLIST_FRAME_TRACE_COMPILED && !TARGET_OS_OSX
#pragma mark - Frame trace

/*
 * Log one line for each committed frame that changed, with the offset, sizes, header and
 * every visible row. Keys are cut to their last 8 characters since generated ids share a prefix.
 */
- (void)traceFrame
{
  if (!_state || !self.window) {
    return;
  }
  // Everything below is along the scroll axis, y for vertical lists and x for horizontal.
  BOOL horizontal = _horizontal;
  CGFloat offset = horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  CGFloat viewport = horizontal ? _scrollView.bounds.size.width : _scrollView.bounds.size.height;
  auto leading = ^CGFloat(CGRect frame) {
    return horizontal ? CGRectGetMinX(frame) : CGRectGetMinY(frame);
  };
  auto extent = ^CGFloat(CGRect frame) {
    return horizontal ? frame.size.width : frame.size.height;
  };
  NSMutableArray<UIView *> *rows = [NSMutableArray array];
  for (UIView *subview in _contentView.subviews) {
    if (subview.hidden || ![subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
      continue;
    }
    CGRect frame = subview.frame;
    if (leading(frame) + extent(frame) <= offset || leading(frame) >= offset + viewport) {
      continue;
    }
    [rows addObject:subview];
  }
  [rows sortUsingComparator:^NSComparisonResult(UIView *a, UIView *b) {
    CGFloat aLeading = leading(a.frame);
    CGFloat bLeading = leading(b.frame);
    return aLeading < bLeading ? NSOrderedAscending : (aLeading > bLeading ? NSOrderedDescending : NSOrderedSame);
  }];
  NSMutableString *rowsDescription = [NSMutableString string];
  for (UIView *row in rows) {
    NSString *key = [self keyOfElementView:row] ?: @"?";
    if (key.length > 8) {
      key = [key substringFromIndex:key.length - 8];
    }
    // A trailing tilde marks a row the layout pass hid with opacity 0.
    [rowsDescription appendFormat:@" %@@%.1f+%.1f%@", key, leading(row.frame) - offset, extent(row.frame),
      row.layer.opacity < 0.5 ? @"~" : @""];
  }
  UIView *header = _stickyHeaderView;
  /*
   * ph is 0 idle, 1 finger down, 2 momentum, 3 scroll to top. ins is the leading inset,
   * which the refresh control adds while spinning. ref is the refreshing prop.
   */
  int phase = _scrollView.isTracking ? 1 : (_scrollView.isDecelerating ? 2 : (_scrollingToTop ? 3 : 0));
  BOOL inverted = std::static_pointer_cast<const ShadowListViewProps>(_props)->inverted;
  // The footer and section overlay, in screen positions like the rows.
  UIView *footer = _stickyFooterView;
  UIView *overlay = _sectionHeaderOverlay;
  BOOL overlayVisible = overlay != nil && !overlay.hidden;
  NSString *signature = [NSString stringWithFormat:@"ax=%@ inv=%d off=%.1f cs=%.1f vp=%.1f ins=%.1f ph=%d ref=%d hdr=%.1f+%.1f ftr=%.1f+%.1f ovl=%.1f+%.1f stt=%d jump=%d rows=[%@ ]",
    horizontal ? @"h" : @"v", inverted ? 1 : 0, offset,
    horizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height,
    viewport, horizontal ? _scrollView.adjustedContentInset.left : _scrollView.adjustedContentInset.top, phase,
    _refreshing ? 1 : 0, header ? leading(header.frame) - offset : -1.0, header ? extent(header.frame) : 0.0,
    footer ? leading(footer.frame) - offset : -1.0, footer ? extent(footer.frame) : 0.0,
    overlayVisible ? leading(overlay.frame) - offset : -1.0, overlayVisible ? extent(overlay.frame) : 0.0,
    _scrollingToTop ? 1 : 0, _scrollToTopJumpPending ? 1 : 0, rowsDescription];
  if ([signature isEqualToString:_frameTraceLast]) {
    return;
  }
  _frameTraceLast = signature;
  SLF_TRACE("frame %s", signature.UTF8String);
}
#endif

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
 * A scroll command replaces any running momentum, from scroll to top or a fling. Stop it so
 * its next frame cannot move the view off the core's offset. A finger on the list keeps its
 * drag phase, so the core lets the drag cancel the command.
 * Returns whether momentum stopped, which makes the command's report idle.
 */
- (BOOL)yieldMomentum
{
#if !TARGET_OS_OSX
  return [self stopMomentum];
#else
  return NO;
#endif
}

#if !TARGET_OS_OSX
/*
 * Stop scroll to top or a fling. Returns YES if one was running.
 */
- (BOOL)stopMomentum
{
  BOOL yielded = [self cancelScrollToTop];
  // isDragging stays set during a fling. Only isTracking means a finger is down.
  if (_scrollView.isDecelerating && !_scrollView.isTracking) {
    // Writing the current offset stops the fling, like in startScrollToTop.
    [_scrollView setContentOffset:_scrollView.contentOffset animated:NO];
    yielded = YES;
  }
  if (yielded) {
    _publishedGesture = NO;
  }
  return yielded;
}
#endif

/*
 * Copy the last scroll command into a state update. See _commandSequence.
 */
- (void)carryScrollCommandInto:(ShadowListViewShadowNode::ConcreteState::Data&)stateData
{
  if (_commandSequence > 0) {
    stateData.containerOffsetIndex_ = _commandIndex;
    stateData.containerOffsetIndexSequence_ = _commandSequence;
    stateData.containerOffsetIndexViewPosition_ = _commandViewPosition;
  }
}

/*
 * The mounted state's offset can be many frames old during a fling. Write the live offset,
 * or the core renders rows for a place we already left. No correction is applied, and the
 * last token rides along like in scrollViewDidScroll.
 * For updates that copy the mounted state, like drag events. The update also becomes the
 * newest live report, with the user scroll flag and phase already set on stateData.
 */
- (void)carryLiveOffsetInto:(ShadowListViewShadowNode::ConcreteState::Data&)stateData
{
  stateData.containerOffsetX_ = _scrollView.contentOffset.x;
  stateData.containerOffsetY_ = _scrollView.contentOffset.y;
  stateData.containerOffsetEnabled_ = false;
  stateData.commitToken_ = (double)_echoedToken;
  stateData.concealGenerationAck_ = stateData.concealGeneration_;
  auto report = [self writeLiveReportAt:_scrollView.contentOffset
                           userScrolled:stateData.userScrolled_
                            scrollPhase:stateData.scrollPhase_
                            commitToken:_echoedToken
                   concealGenerationAck:stateData.concealGenerationAck_];
  stateData.hostSequence_ = (double)report.sequence;
  _lastPushedReport = report;
  _hasPushedReport = YES;
}

/*
 * Acknowledge a state that hides rows when no scroll report will, because its correction
 * moved nothing or we did not apply it. Otherwise the rows stay hidden until the next scroll.
 * Skip while a scroll to top jump waits, since it reports when it lands. See concealGenerationAck_.
 */
- (void)reportConcealedRowsMounted
{
  if (!_state || _scrollToTopJumpPending) {
    return;
  }
  [self commitStatePatch:[self livePatch]];
}

- (void)setStartReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  ShadowListStatePatch patch = [self livePatch];
  patch.hasStartReachedEnabled = true;
  patch.startReachedEnabled = enabled;
  [self commitStatePatch:patch];
}

- (void)setEndReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  ShadowListStatePatch patch = [self livePatch];
  patch.hasEndReachedEnabled = true;
  patch.endReachedEnabled = enabled;
  [self commitStatePatch:patch];
}

/*
 * Send a scroll command. The sequence always goes past the last one, so the same index
 * still scrolls again, and the offset is marked as ours until the core applies it.
 */
- (void)commitScrollCommandIndex:(double)index viewPosition:(double)viewPosition
{
  BOOL yielded = [self yieldMomentum];
  _commandIndex = index;
  _commandViewPosition = viewPosition;
  _commandSequence = MAX(_state->getData().containerOffsetIndexSequence_, _commandSequence) + 1;
  ShadowListStatePatch patch = yielded
    ? [self livePatchWithUserScrolled:NO scrollPhase:SCROLL_PHASE_IDLE]
    : [self livePatch];
  patch.containerOffsetEnabled = true;
  [self commitStatePatch:patch];
}

- (void)scrollToIndex:(NSInteger)index viewPosition:(double)viewPosition
{
  if (!_state) {
    return;
  }

  SLF_TRACE("ev=cmd-scroll-to-index index=%ld viewPosition=%.2f", (long)index, viewPosition);
  [self commitScrollCommandIndex:(double)index viewPosition:viewPosition];
}

- (void)scrollToOffset:(double)offset animated:(BOOL)animated
{
  if (!std::isfinite(offset)) {
    return;
  }

#if !TARGET_OS_OSX
  [self cancelScrollToTop];
#endif
  // Scroll straight to the offset. The core learns the new position from the scroll report.
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
   * Use SCROLL_TO_END_INDEX, which is -3, so the core keeps aiming at the real bottom as
   * rows get measured. animated is unused but kept for API compatibility.
   */
  (void)animated;
  SLF_TRACE("ev=cmd-scroll-to-end off=%.1f,%.1f", _scrollView.contentOffset.x, _scrollView.contentOffset.y);
  [self commitScrollCommandIndex:-3.0 viewPosition:0.0];
}

Class<RCTComponentViewProtocol> ShadowListViewCls(void)
{
  return ShadowListView.class;
}

@end
