#import "ShadowListView.h"

#import "ShadowListViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

#include <shadowlist-core/host/DragReorder.hpp>
#include <shadowlist-core/host/ScrollSync.hpp>
#include <shadowlist-core/host/StickyLayout.hpp>

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
  // Auto hide header and footer, and how far each has slid away.
  BOOL _autoHideHeader;
  BOOL _autoHideFooter;
  azimgd::shadowlist::StickyState _stickyState;
  __weak RCTUIView *_stickyHeaderView;
  __weak RCTUIView *_stickyFooterView;

  // The pinned section header overlay and where each section header sits in the list.
  std::vector<int> _stickyHeaderIndices;
  std::vector<double> _stickyHeaderOffsets;
  std::vector<double> _stickyHeaderSizes;
  __weak RCTUIView *_sectionHeaderOverlay;
  /*
   * The published lists the vectors above and _snapOffsets were copied from. The core only
   * publishes a new pointer when the values change, so a mount with the same pointers skips
   * the copy.
   */
  std::shared_ptr<const std::vector<int>> _copiedStickyHeaderIndices;
  std::shared_ptr<const std::vector<facebook::react::Float>> _copiedStickyHeaderOffsets;
  std::shared_ptr<const std::vector<facebook::react::Float>> _copiedStickyHeaderSizes;
  std::shared_ptr<const std::vector<facebook::react::Float>> _copiedSnapOffsets;

  /*
   * Keeps the scroll view and the core in step: live reports, which frames commit, the echo
   * of core corrections and scroll commands. See azimgd::shadowlist::ScrollSync.
   */
  azimgd::shadowlist::ScrollSync _scrollSync;
  /*
   * Set while a mount runs our code, so a state update sent from there waits for the next
   * event beat instead of committing inside the mount. See SHADOWLIST_IMMEDIATE_STATE.
   */
  BOOL _inStateUpdate;
  BOOL _inMountObserver;

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
   * The held row, where it was picked up and where it would drop. The indexes move the
   * views, and the keys are what JS gets on drop.
   */
  azimgd::shadowlist::DragReorder _drag;
  // Latest touch point on screen.
  CGPoint _dragTouchInViewport;
  // After a drop, hold the shuffle until the reorder commit lands, then clear it.
  BOOL _dragDropPending;
  NSInteger _dropInsertionIndex;
  // Where the dragged row was let go, so it can animate into place instead of snapping.
  CGFloat _dropReleaseLeading;
  CGFloat _dropReleaseCross;
  // Grid columns from props. Above 1 the held cell also moves across the scroll axis.
  NSInteger _columns;
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

/*
 * Send a drag event with the live offset, like every host update. Type 1 is pick up and
 * 3 is drop. Core corrections stay off from pick up until the drop.
 */
- (void)commitDragEventType:(int)type fromKey:(NSString *)fromKey toKey:(NSString *)toKey;

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
