#import "ShadowlistView.h"

#import "ShadowlistViewComponentDescriptor.h"
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

#include <vector>

/* Raise a subview above its siblings. UIKit reorders the subview array via
 * bringSubviewToFront:; AppKit has no equivalent, so on macOS the (layer-backed) view's
 * z-position is used instead. Larger zPosition wins; default subviews sit at 0. */
static inline void SLRaiseSubview(RCTUIView *parent, RCTUIView *child, CGFloat zPosition)
{
#if TARGET_OS_OSX
  child.layer.zPosition = zPosition;
#else
  (void)zPosition;
  [parent bringSubviewToFront:child];
#endif
}

/* Shared ivars and cross-category methods for ShadowlistView. Obj-C++ only.
 *
 * Cross-platform via react-native-macos's RCTUIKit shims (RCTUIView / RCTUIScrollView /
 * RCTUIColor). Features without a clean AppKit equivalent — pull-to-refresh
 * (UIRefreshControl), drag-to-reorder (UILongPressGestureRecognizer + CADisplayLink) and
 * snap-to-item (which needs scroll-end delegate callbacks the macOS shim does not expose) —
 * are compiled out on macOS; their scalar state is kept on both platforms so the shared hot
 * paths need no inline guards. */
@interface ShadowlistView () <RCTShadowlistViewViewProtocol, RCTUIScrollViewDelegate> {
@package
  facebook::react::ShadowlistViewShadowNode::ConcreteState::Shared _state;
  RCTUIScrollView * _scrollView;
  RCTUIView * _contentView;

  /* Keyboard-avoidance bottom inset (px); held to diff against the next value. */
  CGFloat _contentInsetBottom;

  /* Pull-to-refresh: the controlled state and tint kept on both platforms; the control
   * itself is iOS-only (no AppKit equivalent). */
  BOOL _refreshEnabled;
  BOOL _refreshing;
  /* Set when refreshing ends; cleared once the retract spring quiesces, when onRefreshSettle
   * fires. The token invalidates superseded settle debounces. */
  BOOL _refreshAwaitingSettle;
  NSInteger _refreshSettleToken;
#if !TARGET_OS_OSX
  UIRefreshControl * _refreshControl;
  UIColor * _refreshColor;
#endif

  /* Sticky header/footer pinned to the viewport each scroll frame. */
  BOOL _stickyHeader;
  BOOL _stickyFooter;
  BOOL _horizontal;
  /* View snapping: enabled flag and the core's resting snap offsets along the scroll axis. */
  BOOL _snapToItem;
  std::vector<double> _snapOffsets;
  /* Auto-hide header/footer: how far each is currently slid away, and the previous
   * offset used to derive the scroll delta. */
  BOOL _autoHideHeader;
  BOOL _autoHideFooter;
  CGFloat _headerHidden;
  CGFloat _footerHidden;
  CGFloat _lastAutoHideOffset;
  __weak RCTUIView * _stickyHeaderView;
  __weak RCTUIView * _stickyFooterView;

  /* Active section-header overlay and the in-flow section-header geometry it pins to. */
  std::vector<int> _stickyHeaderIndices;
  std::vector<double> _stickyHeaderOffsets;
  std::vector<double> _stickyHeaderSizes;
  __weak RCTUIView * _sectionHeaderOverlay;

  /* The last offset we applied, to distinguish echoed scrolls from user scrolls. */
  CGPoint _appliedOffset;
  BOOL _hasAppliedOffset;

  /* Drag-to-reorder (iOS only): the gesture, per-frame driver, picked-up/dropped views and
   * settle poller are UIKit-specific; the scalar drag state below is shared so the mount and
   * state hot paths compile unchanged on macOS (where a drag never begins). */
  BOOL _dragEnabled;
#if !TARGET_OS_OSX
  UILongPressGestureRecognizer * _dragRecognizer;
  CADisplayLink * _dragDisplayLink;
  __weak UIView * _draggedView;
  __weak UIView * _droppedView;
  CADisplayLink * _dropSettleLink;
#endif
  BOOL _dragging;
  /* Where the row was picked up and where its centre currently sits; the siblings
   * between them are shuffled to open a gap. */
  NSInteger _dragOriginIndex;
  NSInteger _dragInsertionIndex;
  /* Size of the picked-up row along the scroll axis (the shuffle offset). */
  CGFloat _draggedExtent;
  /* Distance from the row's leading edge to the touch point. */
  CGFloat _dragGrabOffset;
  /* Latest touch location in viewport coordinates. */
  CGPoint _dragTouchInViewport;
  /* After a drop, hold the shuffle until the reorder commit lands, then clear it. */
  BOOL _dragDropPending;
  NSInteger _dropInsertionIndex;
  /* Content-space leading of the dragged row at release, so it animates into its
   * resting slot rather than snapping. */
  CGFloat _dragLeading;
  CGFloat _dropReleaseLeading;
  /* Invalidates a superseded drop safety-net timer so a stale drop can't tear down a newer one. */
  NSInteger _dropSettleToken;
}

/* Index from an element view's props, or NSNotFound for a non-element view. */
- (NSInteger)indexOfElementView:(RCTUIView *)view;

/* Re-pin sticky/auto-hide views; accumulate is YES only on genuine user scrolls. */
- (void)applyStickyTransforms:(BOOL)accumulate;

#if !TARGET_OS_OSX
/* Drag-to-reorder, driven from mount/state/recycle. */
- (void)handleDragGesture:(UILongPressGestureRecognizer *)gesture;
- (void)updateDrag;
- (void)applyDragShuffle;
- (void)clearDragTransforms;
- (void)teardownDrag;
/* Animate the just-dropped row from its release point into its resting slot. */
- (void)settleDroppedView:(UIView *)view;
#endif

@end
