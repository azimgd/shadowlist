#pragma once

#import "ShadowListView.h"

#import "ShadowListViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

#include <shadowlist-core/host/DragReorder.hpp>
#include <shadowlist-core/host/ScrollSync.hpp>
#include <shadowlist-core/host/StickyLayout.hpp>

#include <string>
#include <unordered_map>
#include <vector>

NS_ASSUME_NONNULL_BEGIN

/*
 * Raise a subview above its siblings. UIKit reorders the subviews. AppKit cannot.
 * On macOS we set the layer's z position instead. Higher wins and the default is 0.
 */
static inline void SLRaiseSubview(RCTUIView *parent, RCTUIView *child, CGFloat zPosition)
{
#if TARGET_OS_OSX
  child.layer.zPosition = zPosition;
#else
  (void)zPosition;
  [parent bringSubviewToFront:child];
#endif
}

#if !TARGET_OS_OSX
/*
 * Cancel the React Native touch under this view. RN only cancels a press when its own
 * ScrollView takes over. Without this a row pressed at the start of a swipe fires on release.
 * Toggling the touch recognizer is how RN cancels touches itself, and the scroll keeps going.
 */
static inline void SLCancelReactTouches(UIView *view)
{
  static Class touchHandlerClass;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    touchHandlerClass = NSClassFromString(@"RCTSurfaceTouchHandler");
  });
  if (!touchHandlerClass) {
    return;
  }
  for (UIView *ancestor = view; ancestor; ancestor = ancestor.superview) {
    for (UIGestureRecognizer *recognizer in ancestor.gestureRecognizers) {
      if ([recognizer isKindOfClass:touchHandlerClass]) {
        recognizer.enabled = NO;
        recognizer.enabled = YES;
        return;
      }
    }
  }
}
#endif

#if SHADOWLIST_FRAME_TRACE_COMPILED && !TARGET_OS_OSX
/*
 * Frame trace for debugging scroll jumps. Off unless the app launches with
 * SHADOWLIST_FRAME_TRACE=1, on the simulator via SIMCTL_CHILD_SHADOWLIST_FRAME_TRACE=1.
 * Event lines mark each place we move the view. Frame lines show what each committed frame
 * put on screen. A correction that lands a frame late shows up as rows jumping and back.
 */
static inline BOOL SLFrameTraceEnabled(void)
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
#else
#define SLF_TRACE(...) ((void)0)
#endif

#if !TARGET_OS_OSX
/*
 * The list's scroll view. VoiceOver's three finger swipe goes to the list, which moves one
 * viewport and says which rows show.
 */
@interface ShadowListScrollView : UIScrollView
@property (nonatomic, weak, nullable) ShadowListView *listView;
@end
#endif

/*
 * State and methods shared by the ShadowListView categories. Objective-C++ only.
 * Gesture and display-link adapters keep drag geometry shared across Apple platforms.
 *
 * _scrollEnabled and the fields after it mirror the ScrollView props. A drag turns scrolling
 * off for its run and gives back _scrollEnabled. _endDragVelocity is the velocity UIKit gave
 * the end of the latest drag, for onScrollEndDrag, in points per millisecond.
 * _landCommandSequence is the sequence of the newest command that lands an animated one, 0
 * once a mounted state carries it. The animation's momentum end waits for that state, which
 * holds the exact offset. _pageAnnouncementPending holds a VoiceOver page scroll until its
 * rows mount, and _pageKeyIndices maps row keys to indices for it, built once per props.
 * On macOS _macDragEnded and _macMomentum track whether the live scroll's fingers lifted
 * and whether momentum followed.
 */
@interface ShadowListView () <RCTUIScrollViewDelegate> {
@package
  facebook::react::ShadowListViewShadowNode::ConcreteState::Shared _state;
  RCTUIScrollView *_scrollView;
#if !TARGET_OS_OSX
  NSHashTable<UIPanGestureRecognizer *> *_ancestorPans;
#endif
  RCTUIView *_contentView;

  BOOL _refreshEnabled;
  BOOL _refreshing;
  BOOL _refreshAwaitingSettle;
  NSInteger _refreshSettleToken;
#if !TARGET_OS_OSX
  UIRefreshControl *_refreshControl;
  UIColor *_refreshColor;
#endif

  BOOL _stickyHeader;
  BOOL _stickyFooter;
  BOOL _horizontal;
  BOOL _snapToItem;
  std::vector<double> _snapOffsets;
  BOOL _autoHideHeader;
  BOOL _autoHideFooter;
  azimgd::shadowlist::StickyState _stickyState;
  __weak RCTUIView *_stickyHeaderView;
  __weak RCTUIView *_stickyFooterView;

  std::vector<int> _stickyHeaderIndices;
  std::vector<double> _stickyHeaderOffsets;
  std::vector<double> _stickyHeaderSizes;
  __weak RCTUIView *_sectionHeaderOverlay;
  std::shared_ptr<const std::vector<int>> _copiedStickyHeaderIndices;
  std::shared_ptr<const std::vector<facebook::react::Float>> _copiedStickyHeaderOffsets;
  std::shared_ptr<const std::vector<facebook::react::Float>> _copiedStickyHeaderSizes;
  std::shared_ptr<const std::vector<facebook::react::Float>> _copiedSnapOffsets;

  azimgd::shadowlist::ScrollSync _scrollSync;
  BOOL _inStateUpdate;
  BOOL _inMountObserver;

  BOOL _stickyOrderDirty;
  BOOL _overlayOrderDirty;
  BOOL _mountNeedsSticky;
  BOOL _mountNeedsDragShuffle;

#if !TARGET_OS_OSX
  CADisplayLink *_scrollToTopLink;
#endif
  BOOL _scrollingToTop;
  CFTimeInterval _scrollToTopStartTime;
  CGFloat _scrollToTopProgress;
  BOOL _scrollToTopJumpPending;
  CGFloat _scrollToTopJumpY;
  uint64_t _scrollToTopJumpToken;

  BOOL _scrollEnabled;
  BOOL _scrollsToTop;
  CGFloat _decelerationRate;
  CGFloat _refreshProgressViewOffset;
  CGPoint _endDragVelocity;
  double _landCommandSequence;
  BOOL _momentumEndAfterLand;
  BOOL _pageAnnouncementPending;
  facebook::react::Props::Shared _pageKeyIndicesProps;
  std::unordered_map<std::string, NSInteger> _pageKeyIndices;
#if TARGET_OS_OSX
  BOOL _macDragEnded;
  BOOL _macMomentum;
#endif

  BOOL _reorderEnabled;
  SLDragGestureRecognizer *_dragRecognizer;
  SLDisplayLink *_dragDisplayLink;
  __weak RCTUIView *_draggedView;
  __weak RCTUIView *_droppedView;
  SLDisplayLink *_dropSettleLink;
  BOOL _dragging;
  azimgd::shadowlist::DragReorder _drag;
  CGPoint _dragTouchInViewport;
  BOOL _dragDropPending;
  NSInteger _dropInsertionIndex;
  CGFloat _dropReleaseLeading;
  CGFloat _dropReleaseCross;
  NSInteger _numberOfColumns;
  NSInteger _dropSettleToken;

#if SHADOWLIST_FRAME_TRACE_COMPILED && !TARGET_OS_OSX
  CFRunLoopObserverRef _frameTraceObserver;
  NSString *_previousFrameTrace;
#endif
}

- (NSInteger)indexOfElementView:(nullable RCTUIView *)view;

- (nullable NSString *)keyOfElementView:(nullable RCTUIView *)view;

- (void)commitStatePatch:(const azimgd::shadowlist::ScrollPatch&)patch;
- (azimgd::shadowlist::ScrollPatch)livePatch;
- (void)clearUserScrolled;

- (void)commitDragEventType:(int)type fromKey:(nullable NSString *)fromKey toKey:(nullable NSString *)toKey;

@end

@interface ShadowListView (Sticky)

- (void)applyStickyTransforms:(BOOL)accumulate;

@end

@interface ShadowListView (DragReorder)

- (void)handleDragGesture:(SLDragGestureRecognizer *)gesture;
- (void)updateDrag;
- (void)applyDragShuffle;
- (void)clearDragTransforms;
- (void)cancelDrag;
- (void)tearDownDrag;
- (void)settleDroppedView:(nullable RCTUIView *)view;

- (void)applyDragAccessibilityActionsToView:(RCTUIView *)view;
- (BOOL)performAccessibilityMove:(nullable RCTUIView *)view up:(BOOL)up;

@end

@interface ShadowListView (Commands) <RCTShadowListViewViewProtocol>

- (void)animateCommandTo:(CGPoint)target;
- (void)landAnimatedCommand;
- (void)reportConcealedRowsMounted;
#if !TARGET_OS_OSX
- (BOOL)stopMomentum;
#endif

- (void)closeSwipeActionsExcept:(nullable RCTUIView *)view;
#if !TARGET_OS_OSX
- (BOOL)closeSwipeActionsForTouchInView:(nullable UIView *)view;
#endif

@end

#if !TARGET_OS_OSX
@interface ShadowListView (Refresh)

- (void)applyRefreshState:(BOOL)enabled refreshing:(BOOL)refreshing color:(nullable UIColor *)color;
- (void)scheduleRefreshSettle;
- (void)applyRefreshProgressOffset;

@end

@interface ShadowListView (ScrollToTop)

- (BOOL)mountedRowsCoverViewportAt:(CGFloat)y;
- (void)landScrollToTopJumpIfReady;
- (BOOL)cancelScrollToTop;

@end

@interface ShadowListView (Accessibility)

- (BOOL)accessibilityScrollList:(UIAccessibilityScrollDirection)direction;
- (void)announcePageScroll;

@end
#endif

#if SHADOWLIST_FRAME_TRACE_COMPILED && !TARGET_OS_OSX
@interface ShadowListView (FrameTrace)

- (void)startFrameTrace;
- (void)stopFrameTrace;
- (void)traceFrame;

@end
#endif

NS_ASSUME_NONNULL_END
