#import "ShadowListView.h"
#import "ShadowListView+Private.h"
#import "ShadowListElementView.h"
#import "ShadowListMacScrollView.h"

#import "ShadowListViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowListViewSpec/Props.h>
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"
#import <React/RCTConversions.h>
#import <React/RCTMountingTransactionObserving.h>

#include "ShadowListScrollEvent.h"

#include <shadowlist-core/host/Snap.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

/*
 * Lets a waiting scroll to top jump land in the same mount that brings its rows in.
 */
@interface ShadowListView () <RCTMountingTransactionObserving>
@end

#if TARGET_OS_OSX
@interface ShadowListView () <ShadowListMacScrollDelegate>
@end

static double SLScrollPhaseForMacPhase(ShadowListMacScrollPhase phase)
{
  switch (phase) {
    case ShadowListMacScrollPhaseTracking:
      return azimgd::shadowlist::SCROLL_PHASE_DRAGGING;
    case ShadowListMacScrollPhaseMomentum:
      return azimgd::shadowlist::SCROLL_PHASE_SETTLING;
    case ShadowListMacScrollPhaseIdle:
      return azimgd::shadowlist::SCROLL_PHASE_IDLE;
  }
}
#endif

#if !TARGET_OS_OSX
#import <UIKit/UIGestureRecognizerSubclass.h>

@interface ShadowListView ()
- (BOOL)accessibilityScrollList:(UIAccessibilityScrollDirection)direction;
@end

/*
 * The list's scroll view. VoiceOver's three finger swipe goes to the list, which moves one
 * viewport and says which rows show.
 */
@interface ShadowListScrollView : UIScrollView
@property (nonatomic, weak) ShadowListView *listView;
@end

@implementation ShadowListScrollView
- (BOOL)accessibilityScroll:(UIAccessibilityScrollDirection)direction
{
  ShadowListView *listView = _listView;
  return listView ? [listView accessibilityScrollList:direction] : [super accessibilityScroll:direction];
}
@end

/*
 * A tap while the list is still coasting after a flick should stop the scroll, not press a row.
 * A tap while a row is swiped open closes it, not press another row.
 * RN's ScrollView does the same. We check this list and every scroll view around it at touch time.
 * The recognizer only watches and never recognizes. It takes nothing from the list or rows.
 */
@interface ShadowListStopTapRecognizer : UIGestureRecognizer
@end

@implementation ShadowListStopTapRecognizer
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  BOOL stopsPress = NO;
  for (UIView *ancestor = self.view; ancestor; ancestor = ancestor.superview) {
    if ([ancestor isKindOfClass:[UIScrollView class]] && ((UIScrollView *)ancestor).isDecelerating) {
      stopsPress = YES;
      break;
    }
  }
  // A touch outside an open row closes it instead of pressing another row.
  for (UIView *ancestor = self.view; ancestor; ancestor = ancestor.superview) {
    if ([ancestor isKindOfClass:[ShadowListView class]]) {
      stopsPress = [(ShadowListView *)ancestor closeSwipeActionsForTouchInView:touches.anyObject.view] || stopsPress;
      break;
    }
  }
  if (stopsPress) {
    // Wait until RN has seen the touch start. The cancel then reaches that press.
    __weak UIView *weakView = self.view;
    dispatch_async(dispatch_get_main_queue(), ^{
      UIView *strong = weakView;
      if (strong) {
        SLCancelReactTouches(strong);
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

}

#if SHADOWLIST_FRAME_TRACE_COMPILED && !TARGET_OS_OSX
/*
 * Frame trace for debugging scroll jumps. Off unless the app launches with
 * SHADOWLIST_FRAME_TRACE=1, on the simulator via SIMCTL_CHILD_SHADOWLIST_FRAME_TRACE=1.
 * Event lines mark each place we move the view. Frame lines show what each committed frame
 * put on screen. A correction that lands a frame late shows up as rows jumping and back.
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

/*
 * Run right after Core Animation commits, which uses order 2000000, to see the final frame.
 */
static const CFIndex SLF_TRACE_OBSERVER_ORDER = 2000001;

static void SLFrameTraceCallback(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info)
{
  [(__bridge ShadowListView *)info traceFrame];
}
#else
#define SLF_TRACE(...) ((void)0)
#endif

/*
 * The platform list view. Sticky pinning is in ShadowListView+Sticky and drag to reorder
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

#if TARGET_OS_OSX
    ShadowListMacScrollView *macScrollView = [ShadowListMacScrollView new];
    macScrollView.delegate = self;
    _scrollView = macScrollView;
#else
    ShadowListScrollView *scrollView = [ShadowListScrollView new];
    scrollView.listView = self;
    _scrollView = scrollView;
    _scrollView.delegate = self;
#endif
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
    _scrollEnabled = YES;
    _scrollsToTop = YES;
#if !TARGET_OS_OSX
    _scrollView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    _scrollView.indicatorStyle = UIScrollViewIndicatorStyleWhite;
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
    // iOS 26 sizes its edge fade from where the list sits, which left a faded band after the keyboard.
    if (@available(iOS 26.0, *)) {
      _scrollView.topEdgeEffect.hidden = YES;
      _scrollView.bottomEdgeEffect.hidden = YES;
      _scrollView.leftEdgeEffect.hidden = YES;
      _scrollView.rightEdgeEffect.hidden = YES;
    }
#endif
#endif

    _contentView = [RCTUIView new];
#if TARGET_OS_OSX
    // On macOS the scroll view scrolls its documentView, and offset and size come from it.
    _scrollView.documentView = _contentView;
#else
    [_scrollView addSubview:_contentView];
#endif

    self.contentView = _scrollView;

    // Mouse pan on macOS, long press on iOS.
    _dragRecognizer = [[SLDragGestureRecognizer alloc] initWithTarget:self action:@selector(handleDragGesture:)];
#if !TARGET_OS_OSX
    _dragRecognizer.minimumPressDuration = 0.2;
#endif
    _dragRecognizer.enabled = NO;
#if TARGET_OS_OSX
    [_contentView addGestureRecognizer:_dragRecognizer];
#else
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
    if (_dragging && _draggedView) {
      _mountNeedsDragShuffle = YES;
    }
    // Add the VoiceOver move actions. Does nothing unless reorderEnabled.
    [self applyDragAccessibilityActionsToView:childComponentView];
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
  /*
   * The dragged or dropping row can be deleted mid drag and unmount here. Stop the drag
   * so nothing is left running. A live drag still sends its end event, with no reorder.
   */
  if ((RCTUIView *)childComponentView == _draggedView || (RCTUIView *)childComponentView == _droppedView) {
    [self cancelDrag];
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
  _stickyState = {};
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
  _reorderEnabled = NO;
  _numberOfColumns = 1;
  _endDragVelocity = CGPointZero;
  _pageAnnouncementPending = NO;
  [self teardownDrag];
  _dragRecognizer.enabled = NO;
  /*
   * A recycled view must not pass its old scroll position or state to the next list.
   * Reset _state before moving the offset. setContentOffset reports a scroll right away,
   * which would otherwise reach the old list as a fake user scroll to the top.
   */
#if !TARGET_OS_OSX
  [self cancelScrollToTop];
#else
  [(ShadowListMacScrollView *)_scrollView resetScroll];
#endif
  _state.reset();
  // The live report and the echo state belong to the old list.
  _scrollSync.reset();
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
  _scrollSync.setHorizontal(_horizontal);
#if TARGET_OS_OSX
  ((ShadowListMacScrollView *)_scrollView).horizontal = _horizontal;
#endif
  _reorderEnabled = nextProps.reorderEnabled;
  _numberOfColumns = nextProps.numberOfColumns;
  _snapToItem = nextProps.snapToItem;
  // Turning drag off mid drag ends it here, before the recognizer cancel arrives.
  if (!_reorderEnabled && _dragging) {
    [self cancelDrag];
  }
  _dragRecognizer.enabled = _reorderEnabled;
  [self applyScrollViewProps:nextProps];
  /*
   * Update the VoiceOver actions only when reorderEnabled changes, since this runs on every
   * props commit. New rows get them in mountChildComponentView.
   */
  if (previousProps.reorderEnabled != nextProps.reorderEnabled) {
    for (RCTUIView *subview in _contentView.subviews) {
      if ([subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
        [self applyDragAccessibilityActionsToView:subview];
      }
    }
  }
#if !TARGET_OS_OSX
  // UIKit refresh and projected snap deceleration. A snapping list always stops fast.
  if (_snapToItem) {
    _scrollView.decelerationRate = UIScrollViewDecelerationRateFast;
  } else {
    _scrollView.decelerationRate = _decelerationRate > 0.0 ? _decelerationRate : UIScrollViewDecelerationRateNormal;
  }
  [self applyRefreshState:nextProps.refreshEnabled
                refreshing:nextProps.refreshing
                     color:RCTUIColorFromSharedColor(nextProps.refreshColor)];
#endif

  [super updateProps:props oldProps:oldProps];

  [self applyStickyTransforms:NO];
}

/*
 * The ScrollView props that map straight onto the scroll view. A running drag keeps scrolling
 * off until it ends.
 */
- (void)applyScrollViewProps:(const ShadowListViewProps&)props
{
  _scrollEnabled = props.scrollEnabled;
  if (!_dragging) {
    _scrollView.scrollEnabled = _scrollEnabled;
  }
  _scrollView.showsVerticalScrollIndicator = props.showsVerticalScrollIndicator;
  _scrollView.showsHorizontalScrollIndicator = props.showsHorizontalScrollIndicator;
  _scrollsToTop = props.scrollsToTop;
  _decelerationRate = props.decelerationRate;
  _refreshProgressViewOffset = props.refreshProgressViewOffset;
#if !TARGET_OS_OSX
  _scrollView.bounces = props.bounces;
  _scrollView.scrollsToTop = props.scrollsToTop;
  switch (props.keyboardDismissMode) {
    case ShadowListViewKeyboardDismissMode::OnDrag:
      _scrollView.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
      break;
    case ShadowListViewKeyboardDismissMode::Interactive:
      _scrollView.keyboardDismissMode = UIScrollViewKeyboardDismissModeInteractive;
      break;
    case ShadowListViewKeyboardDismissMode::None:
      _scrollView.keyboardDismissMode = UIScrollViewKeyboardDismissModeNone;
      break;
  }
#endif
}

#if !TARGET_OS_OSX
#pragma mark - Pull to refresh

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
  CGFloat offset = _refreshProgressViewOffset;
  if ((_stickyHeader || _autoHideHeader) && _stickyHeaderView) {
    offset += _stickyHeaderView.frame.size.height;
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
  // A state update sent from inside this mount waits for the next beat, see stateUpdateMode.
  _inStateUpdate = YES;

  const auto& nextStateData = _state->getData();
  _scrollSync.beginMount(nextStateData.mountedScroll(), nextStateData.liveScroll_);

  /*
   * Copy the section header positions for pinning on each scroll. A null pointer means
   * empty, see ShadowListViewState. The core only publishes a new pointer when the values
   * changed. Copy only then. Copying on every mount cost a full snap list per frame.
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
#if TARGET_OS_OSX
  // NSScrollView's contentSize is the document frame, not a separate scroll range.
  // Writing zero across the axis would resize the document twice on every mount.
  contentSize = contentFrame.size;
#endif
  BOOL contentSizeChanged = !CGSizeEqualToSize(_scrollView.contentSize, contentSize) ||
    !CGRectEqualToRect(_contentView.frame, contentFrame);
  CGPoint offsetBeforeSizeWrite = _scrollView.contentOffset;
  if (contentSizeChanged) {
    // If these writes clamp the offset, UIKit reports it right away, and that is not the user.
    _scrollSync.setApplyingContentSize(true);
#if !TARGET_OS_OSX
    _scrollView.contentSize = contentSize;
#endif
    _contentView.frame = contentFrame;
    _scrollSync.setApplyingContentSize(false);
  }
  CGPoint offsetBeforeCorrection = _scrollView.contentOffset;

  SL_LOG("mm.updateState: contentSize=(%.1f,%.1f) enabled=%d offset=(%.1f,%.1f) curOffset=(%.1f,%.1f)",
    nextStateData.totalContainerWidth_, nextStateData.totalContainerHeight_,
    nextStateData.containerOffsetEnabled_ ? 1 : 0,
    nextStateData.containerOffsetX_, nextStateData.containerOffsetY_,
    _scrollView.contentOffset.x, _scrollView.contentOffset.y);

  /*
   * Decide what the mounted correction does, see azimgd::shadowlist::ScrollSync::correction.
   * A bounce past an edge sits outside the scroll range, and the content size write above
   * pulls it back to the edge. The core measured the correction from the bounced offset. A
   * shift starts from there, or a history page landing mid bounce jumps by the bounce distance.
   */
  azimgd::shadowlist::ViewMotion motion;
  CGPoint before = _scrollView.contentOffset;
  CGPoint shiftFrom = contentSizeChanged ? offsetBeforeSizeWrite : before;
  motion.offsetX = before.x;
  motion.offsetY = before.y;
  motion.shiftFromX = shiftFrom.x;
  motion.shiftFromY = shiftFrom.y;
#if !TARGET_OS_OSX
  motion.minOffset = _horizontal ? -_scrollView.contentInset.left : -_scrollView.contentInset.top;
  motion.maxOffset = _horizontal
    ? _scrollView.contentSize.width - _scrollView.bounds.size.width + _scrollView.contentInset.right
    : _scrollView.contentSize.height - _scrollView.bounds.size.height + _scrollView.contentInset.bottom;
  motion.touching = _scrollView.isTracking;
  motion.moving = _scrollingToTop || _scrollView.isDragging || _scrollView.isDecelerating;
#else
  ShadowListMacScrollPhase macPhase = ((ShadowListMacScrollView *)_scrollView).phase;
  motion.minOffset = 0;
  motion.maxOffset = MAX(0, _horizontal
    ? _scrollView.contentSize.width - _scrollView.contentView.bounds.size.width
    : _scrollView.contentSize.height - _scrollView.contentView.bounds.size.height);
  motion.touching = macPhase == ShadowListMacScrollPhaseTracking;
  motion.moving = macPhase == ShadowListMacScrollPhaseMomentum;
#endif
  motion.ownsOffset = _dragging || _dragDropPending;
  motion.jumpPending = _scrollToTopJumpPending;
  motion.jumpOffset = _scrollToTopJumpY;
  auto action = _scrollSync.correction(motion);
  BOOL retargetsScrollToTopJump = action.kind == azimgd::shadowlist::MountAction::Kind::RetargetJump;
  if (action.kind == azimgd::shadowlist::MountAction::Kind::Animate) {
    [self animateCommandTo:CGPointMake(action.offsetX, action.offsetY)];
  } else if (retargetsScrollToTopJump) {
    // The view follows when the jump lands.
    _scrollToTopJumpY = action.offsetY;
    _scrollToTopJumpToken = action.token;
  } else if (action.kind == azimgd::shadowlist::MountAction::Kind::Write) {
#if TARGET_OS_OSX
    // Shifting would not help. A plain write never keeps a fling on macOS.
    if (action.shifted) {
      action.offsetX = nextStateData.containerOffsetX_;
      action.offsetY = nextStateData.containerOffsetY_;
    }
#endif
    // A real move calls scrollViewDidScroll right away, which echoes the token.
    _scrollSync.willWrite(action);
    _scrollView.contentOffset = CGPointMake(action.offsetX, action.offsetY);
    CGPoint after = _scrollView.contentOffset;
    _scrollSync.didWrite(fabs(after.x - before.x) >= 0.01 || fabs(after.y - before.y) >= 0.01);
  }

  /*
   * A state that hides rows waits for a report built on it. The write above usually sends
   * one. If it did not, send it here.
   */
  if (_scrollSync.concealAckDue(_scrollToTopJumpPending)) {
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
  // Move the spinner below the header, which may have a new size.
  [self applyRefreshProgressOffset];
#endif

  // A commit during a drag. Put the row back under the finger and shift the others again.
  if (_dragging) {
    [self updateDrag];
  }
  _scrollSync.endMount();
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
   * shows are placed against this offset. The core must know it, or the row the user
   * reads would move up by the spinner's height.
   */

  /*
   * The gesture phase, finger down, momentum or idle. It stays set between frames so the
   * core keeps the inverted bottom pin off while a finger rests on the list.
   * See Container::gestureActive. AppKit phases come from ShadowListMacScrollView.
   */
  azimgd::shadowlist::ScrollFrame frame;
  frame.offsetX = scrollView.contentOffset.x;
  frame.offsetY = scrollView.contentOffset.y;
#if !TARGET_OS_OSX
  frame.scrollPhase = [self currentScrollPhase];
  // Pull to refresh, scroll to top and drag to reorder all lean on every frame.
  frame.commitEveryFrame = _refreshing || _refreshAwaitingSettle ||
    (_refreshControl && _refreshControl.isRefreshing) || _scrollingToTop || _scrollToTopJumpPending ||
    _dragging || _dragDropPending;
#else
  frame.scrollPhase = SLScrollPhaseForMacPhase(((ShadowListMacScrollView *)scrollView).phase);
  frame.commitEveryFrame = _dragging || _dragDropPending;
#endif
  /*
   * Every frame goes into the live report, and only frames the core needs become a state
   * update. See azimgd::shadowlist::ScrollSync::onScroll.
   */
  auto report = _scrollSync.onScroll(frame);
  BOOL userScrolled = report.userScrolled;
  if (report.landed && _scrollSync.isLanding()) {
    // Commit this frame first. The landing command then builds on it.
    if (report.needsCommit) {
      [self commitStatePatch:report.patch];
    }
    [self landAnimatedCommand];
    [self applyStickyTransforms:NO];
    return;
  }

  SL_LOG("mm.scrollViewDidScroll: offset=(%.1f,%.1f) userScrolled=%d token=%llu seq=%llu commit=%d",
    scrollView.contentOffset.x, scrollView.contentOffset.y, userScrolled ? 1 : 0,
    (unsigned long long)_scrollSync.getEchoedToken(), (unsigned long long)report.patch.report.sequence,
    report.needsCommit ? 1 : 0);
  if (report.needsCommit) {
    [self commitStatePatch:report.patch];
  }

  // Only real user scrolls move the auto hide bars.
  [self applyStickyTransforms:userScrolled];
}

#if TARGET_OS_OSX
- (void)shadowListScrollWillBegin
{
  _scrollSync.disarm();
  _macDragEnded = NO;
  _macMomentum = NO;
  [self emitScrollEvent:"scrollBeginDrag" velocity:CGPointZero];
}

- (void)shadowListDragDidEnd
{
  if (_macDragEnded) {
    return;
  }
  _macDragEnded = YES;
  [self emitScrollEvent:"scrollEndDrag" velocity:CGPointZero];
}

- (void)shadowListMomentumWillBegin
{
  [self shadowListDragDidEnd];
  _macMomentum = YES;
  [self emitScrollEvent:"momentumScrollBegin" velocity:CGPointZero];
}

/*
 * The live scroll ended. A mouse wheel never lifted fingers and ends its drag here. A glide
 * ends its momentum.
 */
- (void)shadowListScrollDidEnd
{
  [self clearUserScrolled];
  [self shadowListDragDidEnd];
  if (_macMomentum) {
    _macMomentum = NO;
    [self emitScrollEvent:"momentumScrollEnd" velocity:CGPointZero];
  }
  if (!_snapToItem || _snapOffsets.empty() || _dragging) {
    return;
  }
  CGPoint offset = _scrollView.contentOffset;
  CGFloat target = azimgd::shadowlist::nearestSnapOffset(_snapOffsets, _horizontal ? offset.x : offset.y);
  if (_horizontal) {
    offset.x = target;
  } else {
    offset.y = target;
  }
  if (!CGPointEqualToPoint(offset, _scrollView.contentOffset)) {
    // A synchronous correction has no animation frames that could be mistaken for input.
    _scrollView.contentOffset = offset;
    [self clearUserScrolled];
  }
}
#endif

#pragma mark - State updates

/*
 * Immediate commits run the whole commit and mount on this thread right away. A frame
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
 * Send a state update that patches the newest committed state, see azimgd::shadowlist::ScrollPatch.
 */
- (void)commitStatePatch:(const azimgd::shadowlist::ScrollPatch&)patch
{
  if (!_state) {
    return;
  }
  auto updateMode = [self stateUpdateMode];
  _state->updateState(
    [patch](const ShadowListStateData& oldData) -> StateData::Shared {
      if (oldData.holdsPatch(patch)) {
        return nullptr;
      }
      auto nextData = std::make_shared<ShadowListStateData>(oldData);
      nextData->applyPatch(patch);
      return nextData;
    },
    updateMode);
}

- (azimgd::shadowlist::ScrollPatch)livePatch
{
  return _scrollSync.livePatch(_scrollView.contentOffset.x, _scrollView.contentOffset.y);
}

/*
 * Clear the user scroll flag once the gesture and momentum end. A later commit is then not
 * taken for a user scroll and does not cancel a real correction.
 */
- (void)clearUserScrolled
{
  if (!_state) {
    return;
  }
  if (auto patch = _scrollSync.clearUserScrolled(_scrollView.contentOffset.x, _scrollView.contentOffset.y)) {
    [self commitStatePatch:*patch];
  }
}

- (void)commitDragEventType:(int)type fromKey:(NSString *)fromKey toKey:(NSString *)toKey
{
  if (!_state) {
    return;
  }
  // A drag counts as a user scroll, which keeps core corrections off until the drop.
  BOOL userScrolled = type != azimgd::shadowlist::DRAG_EVENT_END;
  auto patch = _scrollSync.livePatch(
    _scrollView.contentOffset.x, _scrollView.contentOffset.y, userScrolled, _scrollSync.getCurrentScrollPhase());
  std::string dragFromKey = fromKey ? std::string(fromKey.UTF8String) : std::string();
  std::string dragToKey = toKey ? std::string(toKey.UTF8String) : std::string();
  // The sequence goes past the newest state's. Each event fires once.
  _state->updateState(
    [patch, type, dragFromKey, dragToKey](const ShadowListStateData& oldData) -> StateData::Shared {
      auto nextData = std::make_shared<ShadowListStateData>(oldData);
      nextData->applyPatch(patch);
      nextData->dragEventSequence_ = oldData.dragEventSequence_ + 1;
      nextData->dragEventType_ = (double)type;
      nextData->dragFromKey_ = dragFromKey;
      nextData->dragToKey_ = dragToKey;
      return nextData;
    },
    [self stateUpdateMode]);
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

// UIKit lifecycle callbacks. AppKit uses the adapter callbacks above.
#if !TARGET_OS_OSX
/*
 * A swipe that starts on a row may be taken by a scroll view around the list, like a sideways
 * grid. That drag must also end the press. Listen to every outer scroll view's pan while
 * on screen and stop when we leave.
 */
- (void)didMoveToWindow
{
  [super didMoveToWindow];
  for (UIPanGestureRecognizer *pan in _ancestorPans) {
    [pan removeTarget:self action:@selector(ancestorDidPan:)];
  }
  [_ancestorPans removeAllObjects];
  if (!self.window) {
    return;
  }
  if (!_ancestorPans) {
    _ancestorPans = [NSHashTable weakObjectsHashTable];
  }
  for (UIView *ancestor = self.superview; ancestor; ancestor = ancestor.superview) {
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
    SLCancelReactTouches(self);
  }
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
  // A swipe that began on a row is a scroll, not a press on that row.
  SLCancelReactTouches(self);
  // An open row closes when the list scrolls.
  [self closeSwipeActionsExcept:nil];
  [self emitScrollEvent:"scrollBeginDrag" velocity:CGPointZero];
  // The user grabbed the list. Clear our pending move so the drag counts as a user scroll.
  SLF_TRACE("ev=drag-begin off=%.1f,%.1f", scrollView.contentOffset.x, scrollView.contentOffset.y);
  _scrollSync.disarm();
  // A finger takes over from scroll to top. The drag reports its own phase.
  [self cancelScrollToTop];
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
  SLF_TRACE("ev=drag-end off=%.1f,%.1f decel=%d", scrollView.contentOffset.x, scrollView.contentOffset.y, decelerate ? 1 : 0);
  [self emitScrollEvent:"scrollEndDrag" velocity:_endDragVelocity];
  _endDragVelocity = CGPointZero;
  if (!decelerate) {
    [self clearUserScrolled];
  }
}

- (void)scrollViewWillBeginDecelerating:(UIScrollView *)scrollView
{
  [self emitScrollEvent:"momentumScrollBegin" velocity:CGPointZero];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
  SLF_TRACE("ev=decel-end off=%.1f,%.1f", scrollView.contentOffset.x, scrollView.contentOffset.y);
  [self clearUserScrolled];
  [self emitScrollEvent:"momentumScrollEnd" velocity:CGPointZero];
}

/*
 * An animated scroll of ours ended. An animated command lands now, and JS hears the end of
 * the motion like after a fling.
 */
- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView
{
  [self landAnimatedCommand];
  [self emitScrollEvent:"momentumScrollEnd" velocity:CGPointZero];
}

/*
 * Make a fling come to rest on a row edge. Pick the core's snap offset nearest to where
 * the fling would have landed.
 */
- (void)scrollViewWillEndDragging:(UIScrollView *)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint *)targetContentOffset
{
  // UIKit's velocity is already in points per millisecond.
  _endDragVelocity = velocity;
  if (!_snapToItem || _snapOffsets.empty()) {
    return;
  }

  CGFloat projected = _horizontal ? targetContentOffset->x : targetContentOffset->y;
  CGFloat best = (CGFloat)azimgd::shadowlist::nearestSnapOffset(_snapOffsets, projected);

  if (_horizontal) {
    targetContentOffset->x = best;
  } else {
    targetContentOffset->y = best;
  }
}

#pragma mark - Scroll to top

/*
 * Status bar tap. Return NO so UIKit does not animate and run ours instead.
 * A horizontal list has nothing to scroll up. Let UIKit handle it.
 */
- (BOOL)scrollViewShouldScrollToTop:(UIScrollView *)scrollView
{
  if (!_scrollsToTop) {
    return NO;
  }
  if (!_state || _horizontal) {
    return YES;
  }
  if (!_dragging && !_dragDropPending) {
    [self startScrollToTop];
  }
  return NO;
}

/*
 * Longest wait for the rows of a VoiceOver page scroll before saying which rows show.
 */
static const NSTimeInterval SL_PAGE_ANNOUNCEMENT_MAX_WAIT = 0.3;

/*
 * Scroll to top duration, close to UIKit's.
 */
static const CFTimeInterval SL_SCROLL_TO_TOP_DURATION = 0.45;

/*
 * How far the animation travels, in screens. The core keeps about a screen of rows mounted
 * past the visible area. This never outruns them. Longer trips jump closer first, like UIKit.
 */
static const CGFloat SL_SCROLL_TO_TOP_ANIMATED_VIEWPORTS = 1.0;

/*
 * Largest step per frame, in screens. The animation peaks near 0.1. This stops a correction
 * mid flight from stretching a step past the mounted rows.
 */
static const CGFloat SL_SCROLL_TO_TOP_MAX_STEP_VIEWPORTS = 0.35;

/*
 * How much of the screen at the jump target must be covered by rows before the jump lands.
 */
static const CGFloat SL_SCROLL_TO_TOP_JUMP_COVERAGE = 0.9;

/*
 * Longest wait for the rows at the jump target, in seconds. If they never arrive, say the
 * JS thread is busy, land anyway so the tap is not ignored.
 */
static const CFTimeInterval SL_SCROLL_TO_TOP_JUMP_MAX_WAIT = 0.5;

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
   * A long trip jumps first. Jumping right away would show a blank screen until rows mount.
   * Send the target to the core and stay put. The core renders rows there, and the jump
   * lands in the mount that brings them in. See mountingTransactionDidMount.
   */
  CGFloat viewport = _scrollView.bounds.size.height;
  CGFloat jumpY = top + viewport * SL_SCROLL_TO_TOP_ANIMATED_VIEWPORTS;
  if (_state && viewport > 0.0 && _scrollView.contentOffset.y > jumpY + viewport) {
    _scrollToTopJumpPending = YES;
    _scrollToTopJumpY = jumpY;
    _scrollToTopJumpToken = 0;
    /*
     * The live report says the view is at the target too. A commit for another reason
     * renders the same rows. It keeps the mounted ack, since the jump reports when it lands.
     */
    ShadowListLiveScroll::Report report;
    report.offsetX = _scrollView.contentOffset.x;
    report.offsetY = jumpY;
    report.userScrolled = true;
    report.scrollPhase = SCROLL_PHASE_SETTLING;
    report.concealGenerationAck = _state->getData().concealGenerationAck_;
    [self commitStatePatch:_scrollSync.reportPatch(report)];
  }
}

/*
 * Whether mounted rows fill the screen starting at y. A jump there then shows content right away.
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
  return covered >= (end - y) * SL_SCROLL_TO_TOP_JUMP_COVERAGE;
}

/*
 * Jump to the target once its rows are mounted or the wait runs out, then animate the rest.
 */
- (void)landScrollToTopJumpIfReady
{
  if (!_scrollToTopJumpPending) {
    return;
  }
  BOOL waitedTooLong = CACurrentMediaTime() - _scrollToTopStartTime > SL_SCROLL_TO_TOP_JUMP_MAX_WAIT;
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
    _scrollSync.arm(target.x, target.y, false, _scrollToTopJumpToken);
  }
  SLF_TRACE("ev=stt-land %.1f->%.1f waitedTooLong=%d", before.y, target.y, waitedTooLong ? 1 : 0);
  _scrollView.contentOffset = target;
  if (fabs(_scrollView.contentOffset.y - before.y) < 0.01) {
    _scrollSync.disarm();
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
  if (_mountNeedsDragShuffle) {
    _mountNeedsDragShuffle = NO;
    // New rows go below the dragged row and get shifted like the rest.
    if (_dragging && _draggedView) {
      SLRaiseSubview(_contentView, _draggedView, 4.0);
      _stickyOrderDirty = YES;
      [self applyDragShuffle];
    }
  }
  if (_scrollToTopJumpPending) {
    _inMountObserver = YES;
    [self landScrollToTopJumpIfReady];
    _inMountObserver = NO;
  }
  if (_pageAnnouncementPending && !_horizontal && [self mountedRowsCoverViewportAt:_scrollView.contentOffset.y]) {
    [self announcePageScroll];
  }
}

/*
 * One frame of the ease toward the top. The distance left comes from the live offset. A
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

  // The last frame reached the top. Finish.
  if (_scrollToTopProgress >= 1.0) {
    [self finishScrollToTop];
    return;
  }

  CFTimeInterval now = CACurrentMediaTime();
  CGFloat top = -_scrollView.contentInset.top;
  CGFloat time = MIN(1.0, (now - _scrollToTopStartTime) / SL_SCROLL_TO_TOP_DURATION);
  CGFloat eased = 1.0 - pow(1.0 - time, 3.0);
  CGFloat remainingFraction = 1.0 - _scrollToTopProgress;
  CGFloat currentY = _scrollView.contentOffset.y;
  CGFloat nextY = (time >= 1.0 || remainingFraction <= 0.0)
    ? top
    : top + (currentY - top) * (1.0 - eased) / remainingFraction;
  CGFloat progress = time >= 1.0 ? 1.0 : eased;

  CGFloat maxStep = MAX(1.0, _scrollView.bounds.size.height * SL_SCROLL_TO_TOP_MAX_STEP_VIEWPORTS);
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
  ShadowListLiveScroll::Report report;
  report.offsetX = _scrollView.contentOffset.x;
  report.offsetY = _scrollView.contentOffset.y;
  report.concealGenerationAck = _state->getData().concealGenerationAck_;
  [self commitStatePatch:_scrollSync.reportPatch(report)];
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
  BOOL overlayVisible = overlay && !overlay.hidden;
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
 * drag phase. The core lets the drag cancel the command.
 * Returns whether momentum stopped, which makes the command's report idle.
 */
- (BOOL)yieldMomentum
{
#if !TARGET_OS_OSX
  return [self stopMomentum];
#else
  return [(ShadowListMacScrollView *)_scrollView stopMomentum];
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
  return yielded;
}
#endif

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

  auto patch = [self livePatch];
  patch.hasStartReachedEnabled = true;
  patch.startReachedEnabled = enabled;
  [self commitStatePatch:patch];
}

- (void)setEndReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  auto patch = [self livePatch];
  patch.hasEndReachedEnabled = true;
  patch.endReachedEnabled = enabled;
  [self commitStatePatch:patch];
}

/*
 * Send a scroll command. The sequence always goes past the last one. The same index
 * still scrolls again, and the offset is marked as ours until the core applies it.
 * An animated command first gets the core's estimate, see animateCommandTo.
 */
- (void)commitScrollCommand:(const azimgd::shadowlist::ScrollCommand&)command
{
  BOOL yielded = [self yieldMomentum];
  if (yielded) {
    _scrollSync.momentumStopped();
  }
  [self commitStatePatch:_scrollSync.issueCommand(
    command, _scrollView.contentOffset.x, _scrollView.contentOffset.y, yielded)];
}

- (void)scrollToItem:(NSInteger)index viewPosition:(double)viewPosition viewOffset:(double)viewOffset animated:(BOOL)animated
{
  if (!_state) {
    return;
  }

  SLF_TRACE("ev=cmd-scroll-to-index index=%ld viewPosition=%.2f animated=%d", (long)index, viewPosition, animated ? 1 : 0);
  azimgd::shadowlist::ScrollCommand command;
  command.index = (double)index;
  command.viewPosition = viewPosition;
  // The core moves the resting offset by rowOffset. FlatList's viewOffset moves the other way.
  command.rowOffset = std::isfinite(viewOffset) ? -viewOffset : 0.0;
  command.animated = [self animatesCommands] && animated;
  [self commitScrollCommand:command];
}

- (void)scrollToOffset:(double)offset animated:(BOOL)animated
{
  if (!std::isfinite(offset)) {
    return;
  }

#if !TARGET_OS_OSX
  [self cancelScrollToTop];
#endif
  /*
   * An animated scroll goes through the core like scrollToItem. UIKit's own animation would
   * stop at the first correction a row measured on the way sends.
   */
  if (animated && [self animatesCommands] && _state) {
    SLF_TRACE("ev=cmd-scroll-to-offset offset=%.1f", offset);
    azimgd::shadowlist::ScrollCommand command;
    command.index = azimgd::shadowlist::SCROLL_TO_OFFSET_INDEX;
    command.rowOffset = offset;
    command.animated = YES;
    [self commitScrollCommand:command];
    return;
  }
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

  // SCROLL_TO_END_INDEX keeps the core aiming at the real bottom as rows get measured.
  SLF_TRACE("ev=cmd-scroll-to-end off=%.1f,%.1f animated=%d", _scrollView.contentOffset.x, _scrollView.contentOffset.y,
    animated ? 1 : 0);
  azimgd::shadowlist::ScrollCommand command;
  command.index = azimgd::shadowlist::SCROLL_TO_END_INDEX;
  command.animated = [self animatesCommands] && animated;
  [self commitScrollCommand:command];
}

/*
 * AppKit's scroll view has no end of animation callback. macOS commands land right away.
 */
- (BOOL)animatesCommands
{
#if TARGET_OS_OSX
  return NO;
#else
  return YES;
#endif
}

/*
 * Animate to where the core estimates an animated command lands. The frames are ours, and
 * the end lands the command exactly.
 */
- (void)animateCommandTo:(CGPoint)target
{
#if !TARGET_OS_OSX
  CGPoint current = _scrollView.contentOffset;
  CGFloat top = -_scrollView.contentInset.top;
  CGFloat maxAlong = MAX(top, _horizontal
    ? _scrollView.contentSize.width - _scrollView.bounds.size.width + _scrollView.contentInset.right
    : _scrollView.contentSize.height - _scrollView.bounds.size.height + _scrollView.contentInset.bottom);
  if (_horizontal) {
    target.x = MIN(MAX(target.x, top), maxAlong);
  } else {
    target.y = MIN(MAX(target.y, top), maxAlong);
  }
  SLF_TRACE("ev=cmd-animate %.1f->%.1f", _horizontal ? current.x : current.y, _horizontal ? target.x : target.y);
  if (fabs(target.x - current.x) < 0.5 && fabs(target.y - current.y) < 0.5) {
    [self landAnimatedCommand];
    return;
  }
  _scrollSync.arm(target.x, target.y, true);
  [_scrollView setContentOffset:target animated:YES];
#else
  [self landAnimatedCommand];
#endif
}

/*
 * Send an animated command again without the animation, which lands it exactly.
 */
- (void)landAnimatedCommand
{
  if (!_state) {
    return;
  }
  if (auto patch = _scrollSync.land(_scrollView.contentOffset.x, _scrollView.contentOffset.y)) {
    SLF_TRACE("ev=cmd-land off=%.1f,%.1f", _scrollView.contentOffset.x, _scrollView.contentOffset.y);
    [self commitStatePatch:*patch];
  }
}

/*
 * Slide every swiped row back.
 */
- (void)closeSwipeActions
{
  for (RCTUIView *subview in _contentView.subviews) {
    if ([subview isKindOfClass:[ShadowListElementView class]]) {
      [(ShadowListElementView *)subview closeSwipeActionsAnimated:YES];
    }
  }
}

#if !TARGET_OS_OSX
/*
 * Close the open rows a touch in view is not on. Returns whether any closed.
 */
- (BOOL)closeSwipeActionsForTouchInView:(UIView *)view
{
  BOOL closed = NO;
  for (RCTUIView *subview in _contentView.subviews) {
    if (![subview isKindOfClass:[ShadowListElementView class]]) {
      continue;
    }
    ShadowListElementView *elementView = (ShadowListElementView *)subview;
    if (elementView.isSwipeOpen && !elementView.isSwipedOut && ![view isDescendantOfView:elementView]) {
      [elementView closeSwipeActionsAnimated:YES];
      closed = YES;
    }
  }
  return closed;
}
#endif

/*
 * Only one row stays open. A row swiped all the way stays out while its removal runs.
 */
- (void)closeSwipeActionsExcept:(RCTUIView *)view
{
  for (RCTUIView *subview in _contentView.subviews) {
    if (subview != view && [subview isKindOfClass:[ShadowListElementView class]] &&
        ![(ShadowListElementView *)subview isSwipedOut]) {
      [(ShadowListElementView *)subview closeSwipeActionsAnimated:YES];
    }
  }
}

- (void)flashScrollIndicators
{
#if TARGET_OS_OSX
  [_scrollView flashScrollers];
#else
  [_scrollView flashScrollIndicators];
#endif
}

/*
 * Ask the core for the row at the viewport start. The answer comes back as onAnchorState.
 */
- (void)requestAnchorState
{
  if (!_state) {
    return;
  }
  [self commitStatePatch:_scrollSync.requestAnchor(_scrollView.contentOffset.x, _scrollView.contentOffset.y)];
}

#pragma mark - Scroll events

/*
 * Send a drag or momentum event with the same payload as onScroll.
 */
- (void)emitScrollEvent:(const char *)name velocity:(CGPoint)velocity
{
  if (!_eventEmitter) {
    return;
  }
  ShadowListScrollMetrics metrics;
  metrics.offsetX = _scrollView.contentOffset.x;
  metrics.offsetY = _scrollView.contentOffset.y;
  metrics.contentWidth = _contentView.frame.size.width;
  metrics.contentHeight = _contentView.frame.size.height;
  metrics.viewportWidth = _scrollView.bounds.size.width;
  metrics.viewportHeight = _scrollView.bounds.size.height;
  metrics.velocityX = velocity.x;
  metrics.velocityY = velocity.y;
  _eventEmitter->dispatchEvent(name, [metrics](facebook::jsi::Runtime& runtime) {
    return facebook::jsi::Value(shadowListScrollPayload(runtime, metrics));
  });
}

#if !TARGET_OS_OSX
#pragma mark - Accessibility

/*
 * A three finger swipe moves one viewport. Once the rows there mount, VoiceOver says which
 * rows show.
 */
- (BOOL)accessibilityScrollList:(UIAccessibilityScrollDirection)direction
{
  BOOL forward = _horizontal
    ? (direction == UIAccessibilityScrollDirectionLeft || direction == UIAccessibilityScrollDirectionNext)
    : (direction == UIAccessibilityScrollDirectionUp || direction == UIAccessibilityScrollDirectionNext);
  BOOL backward = _horizontal
    ? (direction == UIAccessibilityScrollDirectionRight || direction == UIAccessibilityScrollDirectionPrevious)
    : (direction == UIAccessibilityScrollDirectionDown || direction == UIAccessibilityScrollDirectionPrevious);
  if ((!forward && !backward) || !_state) {
    return NO;
  }
  CGFloat viewport = _horizontal ? _scrollView.bounds.size.width : _scrollView.bounds.size.height;
  CGFloat offset = _horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  CGFloat maxOffset = MAX(0.0, (_horizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height) - viewport);
  CGFloat target = MIN(MAX(offset + (forward ? viewport : -viewport), 0.0), maxOffset);
  if (fabs(target - offset) < 1.0) {
    return NO;
  }
  [self stopMomentum];
  _scrollSync.momentumStopped();
  // The page scroll is the user's, like a drag. The core follows it.
  _scrollView.contentOffset = _horizontal ? CGPointMake(target, _scrollView.contentOffset.y)
                                          : CGPointMake(_scrollView.contentOffset.x, target);
  // The jump is over at once. Report the list at rest, or corrections would wait for a gesture end.
  [self clearUserScrolled];
  _pageAnnouncementPending = YES;
  __weak ShadowListView *weakSelf = self;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(SL_PAGE_ANNOUNCEMENT_MAX_WAIT * NSEC_PER_SEC)),
    dispatch_get_main_queue(), ^{
      [weakSelf announcePageScroll];
    });
  return YES;
}

/*
 * Say which rows show, like "Rows 4 to 12 of 200". Rows are counted from the keys in props.
 */
- (void)announcePageScroll
{
  if (!_pageAnnouncementPending) {
    return;
  }
  _pageAnnouncementPending = NO;
  const auto& props = *std::static_pointer_cast<const ShadowListViewProps>(_props);
  const auto& keys = props.elementsAllKeys;
  CGRect visible = _scrollView.bounds;
  NSInteger first = NSNotFound;
  NSInteger last = NSNotFound;
  for (RCTUIView *subview in _contentView.subviews) {
    if (subview.hidden || !CGRectIntersectsRect(subview.frame, visible)) {
      continue;
    }
    NSString *key = [self keyOfElementView:subview];
    if (!key) {
      continue;
    }
    auto found = std::find(keys.begin(), keys.end(), std::string(key.UTF8String));
    if (found == keys.end()) {
      continue;
    }
    NSInteger index = (NSInteger)(found - keys.begin());
    first = first == NSNotFound ? index : MIN(first, index);
    last = last == NSNotFound ? index : MAX(last, index);
  }
  NSString *status = first == NSNotFound
    ? nil
    : [NSString stringWithFormat:@"Rows %ld to %ld of %lu", (long)first + 1, (long)last + 1, (unsigned long)keys.size()];
  UIAccessibilityPostNotification(UIAccessibilityPageScrolledNotification, status);
}
#endif

Class<RCTComponentViewProtocol> ShadowListViewCls(void)
{
  return ShadowListView.class;
}

@end
