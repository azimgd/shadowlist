#pragma once

#import "ShadowListView.h"

#import "ShadowListViewComponentDescriptor.h"
#include "ShadowListScrollEvent.h"
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

#import <QuartzCore/QuartzCore.h>

#include <shadowlist-core/host/DragBoard.hpp>
#include <shadowlist-core/host/DragReorder.hpp>
#include <shadowlist-core/host/RowKeyStore.hpp>
#include <shadowlist-core/host/ScrollSync.hpp>
#include <shadowlist-core/host/StickyLayout.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

NS_ASSUME_NONNULL_BEGIN

@class ShadowListDragBoard;

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

/*
 * How long a dropped row, or a row's picture on a drag board, takes to settle into its place,
 * and how long the other rows take to slide, in seconds. The core sets both for every host.
 */
static const NSTimeInterval SL_DROP_SETTLE_DURATION = azimgd::shadowlist::DRAG_DROP_DURATION_MS / 1000.0;
static const NSTimeInterval SL_DRAG_SHIFT_DURATION = azimgd::shadowlist::DRAG_SHIFT_DURATION_MS / 1000.0;

/*
 * The clock the core's drag timers run on, in milliseconds.
 */
static inline double SLDragNowMs(void)
{
  return CACurrentMediaTime() * 1000.0;
}

/*
 * Whether a view paints a background, which is the shape its lift shadow takes.
 */
static inline BOOL SLViewPaintsBackground(RCTUIView *view)
{
  CGColorRef color = view.layer.backgroundColor;
  return color && CGColorGetAlpha(color) > 0.0;
}

/*
 * The shape a lifted row's shadow takes, in the row's coordinates: the first view with a
 * background down the row's single child chain, with its corner radius, or the row's bounds
 * when the chain branches first. A row with a transparent root and a rounded card inside its
 * padding then casts the card's shadow. Android's ShadowListCellView.CONTENT_OUTLINE finds
 * the same shape. The path is cheap to shade every frame, unlike the shadow of the content.
 */
static inline CGPathRef SLCreateDragLiftShadowPath(RCTUIView *row)
{
  RCTUIView *shape = row;
  while (shape && !SLViewPaintsBackground(shape)) {
    NSArray *subviews = shape.subviews;
    shape = subviews.count == 1 ? (RCTUIView *)subviews.firstObject : nil;
  }
  CGRect rect = shape ? [shape convertRect:shape.bounds toView:row] : row.bounds;
  CGFloat radius = shape ? shape.layer.cornerRadius : row.layer.cornerRadius;
  radius = MAX(0.0, MIN(radius, MIN(rect.size.width, rect.size.height) / 2.0));
  return CGPathCreateWithRoundedRect(rect, radius, radius, NULL);
}

/*
 * The shadow of a row picked up for a drag, shaped from shapeSource, on its own layer or on
 * the layer of its picture.
 */
static inline void SLApplyDragLiftShadow(RCTUIView *shapeSource, CALayer *layer)
{
  CGPathRef path = SLCreateDragLiftShadowPath(shapeSource);
  layer.shadowPath = path;
  CGPathRelease(path);
  layer.shadowColor = [RCTUIColor blackColor].CGColor;
  layer.shadowOpacity = azimgd::shadowlist::DRAG_LIFT_SHADOW_OPACITY;
  layer.shadowRadius = azimgd::shadowlist::DRAG_LIFT_SHADOW_RADIUS;
  layer.shadowOffset = CGSizeMake(0.0, azimgd::shadowlist::DRAG_LIFT_SHADOW_OFFSET);
}

static inline void SLClearDragLiftShadow(CALayer *layer)
{
  layer.shadowOpacity = 0.0;
  layer.shadowPath = NULL;
}

/*
 * A drag key for JS, empty when the row has none.
 */
static inline NSString *SLDragKey(const std::string& key)
{
  return [NSString stringWithUTF8String:key.c_str()] ?: @"";
}

/*
 * Drag haptics, shared by every list. SLPrepareDragFeedback wakes the haptic engine while a
 * press holds, SLDragLiftFeedback thumps as a row lifts, and SLDragSelectionFeedback ticks
 * when the drop spot or the hovered list changes. Each list's core DragFeedback decides when
 * to tick. On macOS the trackpad ticks under a force touch drag, and prepare does nothing.
 * See ShadowListView+DragReorder.mm.
 */
void SLPrepareDragFeedback(void);
void SLDragLiftFeedback(void);
void SLDragSelectionFeedback(void);

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
 * the end of the latest drag, for onScrollEndDrag, in points per millisecond. _scrollTracker
 * throttles onScroll, which this view sends from its scroll callback while _scrollEventEnabled.
 * _landCommandSequence is the sequence of the newest command that lands an animated one, 0
 * once a mounted state carries it. The animation's momentum end waits for that state, which
 * holds the exact offset. _pageAnnouncementPending holds a VoiceOver page scroll until its
 * rows mount. _pageKeyIndices maps the rowKeys of a list without a key store to indices, built
 * once per props, see rowIndexOfKey.
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

  std::vector<int> _stickyIndices;
  std::vector<double> _stickyOffsets;
  std::vector<double> _stickySizes;
  __weak RCTUIView *_sectionHeaderOverlay;
  std::shared_ptr<const std::vector<int>> _copiedStickyIndices;
  std::shared_ptr<const std::vector<facebook::react::Float>> _copiedStickyOffsets;
  std::shared_ptr<const std::vector<facebook::react::Float>> _copiedStickySizes;
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
  CGFloat _progressViewOffset;
  CGPoint _endDragVelocity;
  BOOL _scrollEventEnabled;
  facebook::react::ShadowListScrollTracker _scrollTracker;
  double _landCommandSequence;
  BOOL _momentumEndAfterLand;
  BOOL _pageAnnouncementPending;
  facebook::react::Props::Shared _pageKeyIndicesProps;
  std::unordered_map<std::string, NSInteger> _pageKeyIndices;
  // The key store snapshot currentRowKeys last read, which keeps its keys alive.
  std::shared_ptr<const azimgd::shadowlist::RowKeysSnapshot> _rowKeysSnapshot;
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

  /*
   * Ticks the list's ShadowListMountDriver on display frames while it has a paced step or a
   * viewability report waiting, see pokeMountDriver.
   */
  SLDisplayLink *_mountTickLink;
  BOOL _dragging;
  azimgd::shadowlist::DragReorder _drag;
  CGPoint _dragTouchInViewport;
  /*
   * The finger in window points. A drag board scrolls the view around this list, which moves
   * _dragTouchInViewport under a finger that holds still. Each drag frame on a board reads it
   * again from the window point. _dragBoardScroll scrolls the view around the board sideways
   * once the finger heads for its edge. _dragFeedback decides the haptic ticks. _dropSettle
   * waits for a drop's reorder to land. _dragLastTickTime is the previous drag frame's time,
   * for edge scroll speeds given per second, 0 before the first frame.
   */
  CGPoint _dragTouchInWindow;
  azimgd::shadowlist::DragBoardScroll _dragBoardScroll;
  azimgd::shadowlist::DragFeedback _dragFeedback;
  azimgd::shadowlist::DropSettle _dropSettle;
  CFTimeInterval _dragLastTickTime;
  BOOL _dragDropPending;
  CGFloat _dropReleaseLeading;
  CGFloat _dropReleaseCross;
  NSInteger _numberOfColumns;

  /*
   * The drag board this list is on, see ShadowListDragBoard. _dragBoard is nil while
   * dragBoardId is empty. _dragBoardToken names this list for the board's core, unique on the
   * board where dragListId may not be. _dragBoardHiddenView is the row the board hid while its
   * picture floats above the window. _dragBoardDropPending holds the gap a row left in its
   * home list until the data change that removes it lands. _dragBoardLandingKey is the key of
   * the row dropped in this list, until it mounts here. _dragBoardAnimateShifts makes the
   * next shuffle slide, when the row came home or left this list as a target. _dragListLabel
   * names this list in the Move to actions of the board's other lists. _dragBoardMovedKey is
   * the key of a row VoiceOver moved into this list, until its data holds it or
   * _dragBoardMovedDeadline passes.
   */
  NSString *_dragBoardId;
  NSString *_dragListId;
  NSString *_dragListLabel;
  ShadowListDragBoard *_dragBoard;
  std::string _dragBoardToken;
  __weak RCTUIView *_dragBoardHiddenView;
  BOOL _dragBoardDropPending;
  NSString *_dragBoardLandingKey;
  BOOL _dragBoardAnimateShifts;
  NSString *_dragBoardMovedKey;
  CFTimeInterval _dragBoardMovedDeadline;

#if SHADOWLIST_FRAME_TRACE_COMPILED && !TARGET_OS_OSX
  CFRunLoopObserverRef _frameTraceObserver;
  NSString *_previousFrameTrace;
#endif
}

- (NSInteger)indexOfCellView:(nullable RCTUIView *)view;

- (nullable NSString *)keyOfCellView:(nullable RCTUIView *)view;
- (BOOL)rowKeysContainKey:(nullable NSString *)key;
- (std::size_t)rowIndexOfKey:(const std::string&)key;
- (std::size_t)rowKeyCount;

- (void)commitStatePatch:(const azimgd::shadowlist::ScrollPatch&)patch;
- (azimgd::shadowlist::ScrollPatch)livePatch;
- (void)clearUserScrolled;

- (void)commitDragEventType:(int)type sourceKey:(nullable NSString *)sourceKey destinationKey:(nullable NSString *)destinationKey;
- (void)commitDragEventType:(int)type
                  sourceKey:(nullable NSString *)sourceKey
             destinationKey:(nullable NSString *)destinationKey
          destinationListId:(nullable NSString *)destinationListId
           destinationIndex:(NSInteger)destinationIndex;

- (void)pokeMountDriver;
- (void)stopMountTicks;

@end

@interface ShadowListView (Sticky)

- (void)applyStickyTransforms:(BOOL)accumulate;

@end

@interface ShadowListView (DragReorder)

- (void)handleDragGesture:(SLDragGestureRecognizer *)gesture;
- (void)updateDrag;
- (void)updateDragInsertion;
- (void)applyDragShuffle;
- (CGRect)restingFrameForView:(RCTUIView *)view;
- (void)applyDragAutoScrollForTouch:(CGPoint)touch dtSeconds:(double)dtSeconds;
- (void)noteDragInsertionForFeedback;
- (void)setDragTouch:(CGPoint)location;
- (void)clearDragTransforms;
- (void)cancelDrag;
- (void)tearDownDrag;
- (void)settleDroppedView:(nullable RCTUIView *)view;

- (void)applyDragAccessibilityActionsToView:(RCTUIView *)view;
- (BOOL)performAccessibilityMove:(nullable RCTUIView *)view up:(BOOL)up;

@end

/*
 * Dragging a row between the lists of one drag board, see ShadowListDragBoard.mm. The board
 * runs on iOS with a long press and on macOS with a mouse drag. VoiceOver moves a row to
 * another list of the board through its Move to actions.
 */
@interface ShadowListView (DragBoard)

- (void)applyDragBoardId:(NSString *)boardId listId:(NSString *)listId label:(NSString *)label;
- (void)leaveDragBoard;
- (void)withdrawFromDragBoardFlight;
- (void)dragBoardDidPickUpView:(RCTUIView *)view key:(nullable NSString *)heldKey;
- (BOOL)updateDragBoard;
- (BOOL)applyDragBoardAutoScrollWithDt:(double)dtSeconds;
- (BOOL)finishDragBoard;
- (void)cancelDragBoard;
- (void)tearDownDragBoard;
- (void)dragBoardMountDidFinish;
- (void)dragBoardViewWillUnmount:(RCTUIView *)view;
- (void)dragBoardFlightDidEnd;
- (CGPoint)dragBoardReturnOffsetForPlacedCenter:(CGPoint)placed;
- (NSArray<SLAccessibilityCustomAction *> *)dragBoardAccessibilityActionsForView:(RCTUIView *)view;
- (BOOL)performAccessibilityMove:(nullable RCTUIView *)view toList:(nullable ShadowListView *)list;
- (void)refreshOwnDragAccessibilityActions;
- (void)refreshDragBoardAccessibilityActions;

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
