#import "ShadowListView.h"

#import "ShadowListViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

#include <vector>

/*
 * Raise a subview above its siblings. UIKit reorders the subviews. AppKit cannot,
 * so on macOS we set the layer's z position instead. Higher wins and the default is 0.
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
 * State and methods shared by the ShadowListView categories. Objective-C++ only.
 * Pull to refresh, drag to reorder and snap to item have no clean macOS version and are
 * left out there. Their plain state stays on both platforms so shared code needs no guards.
 */
@interface ShadowListView () <RCTShadowListViewViewProtocol, RCTUIScrollViewDelegate> {
@package
  facebook::react::ShadowListViewShadowNode::ConcreteState::Shared _state;
  RCTUIScrollView *_scrollView;
#if !TARGET_OS_OSX
  // Pan gestures of the scroll views around the list. Their drags also end a row press.
  NSHashTable<UIPanGestureRecognizer *> *_ancestorPans;
#endif
  RCTUIView *_contentView;

  // Pull to refresh state lives on both platforms, but the control itself is iOS only.
  BOOL _refreshEnabled;
  BOOL _refreshing;
  /*
   * Set when refreshing ends and cleared once the spinner has fully retracted and
   * onRefreshSettle fires. The token cancels older settle timers.
   */
  BOOL _refreshAwaitingSettle;
  NSInteger _refreshSettleToken;
#if !TARGET_OS_OSX
  UIRefreshControl *_refreshControl;
  UIColor *_refreshColor;
#endif

  // Sticky header and footer, pinned again on every scroll.
  BOOL _stickyHeader;
  BOOL _stickyFooter;
  BOOL _horizontal;
  // Snap to item, and the offsets from the core where scrolling can come to rest.
  BOOL _snapToItem;
  std::vector<double> _snapOffsets;
  /*
   * Auto hide header and footer. How far each has slid away, and the last offset
   * so we can tell how far the user scrolled.
   */
  BOOL _autoHideHeader;
  BOOL _autoHideFooter;
  CGFloat _headerHidden;
  CGFloat _footerHidden;
  CGFloat _lastAutoHideOffset;
  __weak RCTUIView *_stickyHeaderView;
  __weak RCTUIView *_stickyFooterView;

  // The pinned section header overlay and where each section header sits in the list.
  std::vector<int> _stickyHeaderIndices;
  std::vector<double> _stickyHeaderOffsets;
  std::vector<double> _stickyHeaderSizes;
  __weak RCTUIView *_sectionHeaderOverlay;

  /*
   * Set when a subview may now sit above the sticky views, for example after a mount or a
   * drag pickup. The next pin raises them once instead of on every scroll frame.
   * The overlay has its own flag because it is only raised while it is shown.
   */
  BOOL _stickyOrderDirty;
  BOOL _overlayOrderDirty;
  /*
   * Rows mounted in the current transaction. Pinning and the drag shuffle run once when
   * the transaction finishes instead of once per row. See mountingTransactionDidMount.
   */
  BOOL _mountNeedsSticky;
  BOOL _mountNeedsDragShuffle;

  /*
   * Set when we applied an offset from the core and it moved the view, so the next
   * scroll report is our own move and not the user. _armedToken goes back to the core
   * so it can match the report to its correction. We decide by who moved it, not by distance.
   */
  CGPoint _appliedOffset;
  BOOL _hasAppliedOffset;
  uint64_t _armedToken;
  // Token of the last correction we reported back. Later reports carry it too.
  uint64_t _echoedToken;
  /*
   * Whether our last published state was a gesture, like a user scroll, drag or settle.
   * The mounted state lags behind, so it cannot tell us if the rest report is still due.
   * See clearUserScrolled.
   */
  BOOL _publishedGesture;
  /*
   * The last correction we shifted onto a moving view and how much of it we applied.
   * If the same token comes back with a new target, shift only the difference.
   */
  uint64_t _shiftedToken;
  CGFloat _shiftedTokenDelta;
  /*
   * The last scroll command from the engine that we stopped momentum for.
   * Matches momentumYieldToken_.
   */
  uint64_t _yieldedToken;
  /*
   * Set when a scroll report goes out during updateState:, so it knows a state that hides
   * rows was already acknowledged. See reportConcealedRowsMounted.
   */
  BOOL _reportedDuringStateUpdate;

  /*
   * Set while updateState writes the content size. A smaller size clamps the offset and
   * UIKit reports that as a scroll right away. Nobody scrolled, so it must not reach the
   * core as a user scroll, or it would unpin an inverted list and cancel corrections.
   */
  BOOL _applyingContentSize;

  /*
   * The last scrollToIndex or scrollToEnd we issued, copied into every state update.
   * Updates start from the mounted state, which may not have the command yet, and would
   * otherwise overwrite it before the core sees it. The sequence is 0 until the first command.
   */
  double _commandIndex;
  // Where scrollToIndex wants its row on screen. Sent along with _commandIndex.
  double _commandViewPosition;
  double _commandSequence;

  /*
   * Status bar tap to scroll to top, iOS only. We animate it ourselves because UIKit
   * writes fixed offsets each frame and wipes out core corrections, so content jumps.
   * Ours works from the live offset, and corrections are added on top of it.
   * See ShadowListViewState::containerOffsetBaseX_. Progress is how far it has eased so far.
   */
#if !TARGET_OS_OSX
  CADisplayLink *_scrollToTopLink;
#endif
  BOOL _scrollingToTop;
  CFTimeInterval _scrollToTopStartTime;
  CGFloat _scrollToTopProgress;
  /*
   * A long scroll to top first jumps to one screen below the top and animates the rest.
   * The jump waits for the rows there to mount so we never show a blank screen.
   * See landScrollToTopJumpIfReady. Core corrections can move the target, and the latest
   * correction token goes back to the core when the jump lands.
   */
  BOOL _scrollToTopJumpPending;
  CGFloat _scrollToTopJumpY;
  uint64_t _scrollToTopJumpToken;

  /*
   * Drag to reorder is iOS only. The gesture and animation parts are UIKit, but the plain
   * drag state is shared so mount and state code compile on macOS, where a drag never starts.
   */
  BOOL _dragEnabled;
#if !TARGET_OS_OSX
  UILongPressGestureRecognizer *_dragRecognizer;
  CADisplayLink *_dragDisplayLink;
  __weak UIView *_draggedView;
  __weak UIView *_droppedView;
  CADisplayLink *_dropSettleLink;
#endif
  BOOL _dragging;
  /*
   * Where the row was picked up and where its center is now. Rows in between shift to
   * open a gap. The indexes move the views, and the keys below are what JS gets on drop.
   */
  NSInteger _dragOriginIndex;
  NSInteger _dragInsertionIndex;
  /*
   * Keys of the picked up row and its current drop neighbor, sent in drag events
   * so JS reorders by key.
   */
  NSString *_dragOriginKey;
  NSString *_dragInsertionKey;
  // Size of the picked up row along the scroll axis. Other rows shift by this much.
  CGFloat _draggedExtent;
  // Distance from the row's leading edge to the finger.
  CGFloat _dragGrabOffset;
  // Latest touch point on screen.
  CGPoint _dragTouchInViewport;
  // After a drop, hold the shuffle until the reorder commit lands, then clear it.
  BOOL _dragDropPending;
  NSInteger _dropInsertionIndex;
  /*
   * Where the dragged row starts in the content when released, so it can animate
   * into place instead of snapping.
   */
  CGFloat _dragLeading;
  CGFloat _dropReleaseLeading;
  // Cancels an older drop fallback timer so it cannot tear down a newer drop.
  NSInteger _dropSettleToken;
}

// The row index from a row view's props, or NSNotFound for other views.
- (NSInteger)indexOfElementView:(RCTUIView *)view;

/*
 * The data key from a row view's props, or nil for other views.
 */
- (NSString *)keyOfElementView:(RCTUIView *)view;

// Pin the sticky and auto hide views again. Pass YES only for real user scrolls.
- (void)applyStickyTransforms:(BOOL)accumulate;

// Copy the live offset and the last reported token into a state update.
- (void)carryLiveOffsetInto:(facebook::react::ShadowListViewShadowNode::ConcreteState::Data&)stateData;

// Copy the last scroll command into a state update.
- (void)carryScrollCommandInto:(facebook::react::ShadowListViewShadowNode::ConcreteState::Data&)stateData;

#if !TARGET_OS_OSX
// Drag to reorder, called from mount, state updates and recycling.
- (void)handleDragGesture:(UILongPressGestureRecognizer *)gesture;
- (void)updateDrag;
- (void)applyDragShuffle;
- (void)clearDragTransforms;
- (void)teardownDrag;
// Animate the dropped row from where it was released into its place.
- (void)settleDroppedView:(UIView *)view;

/*
 * For VoiceOver users, add or remove Move up and Move down actions on a row,
 * depending on dragEnabled.
 */
- (void)applyDragAccessibilityActionsToView:(UIView *)view;
/*
 * Swap the row with its neighbor the same way a drop does. Returns NO if there is
 * no neighbor.
 */
- (BOOL)performAccessibilityMove:(UIView *)view up:(BOOL)up;
#endif

@end
