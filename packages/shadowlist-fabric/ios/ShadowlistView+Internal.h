#import "ShadowlistView.h"

#import "ShadowlistViewComponentDescriptor.h"
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

#include <vector>

/* Shared ivars and cross-category methods for ShadowlistView. Obj-C++ only. */
@interface ShadowlistView () <RCTShadowlistViewViewProtocol, UIScrollViewDelegate> {
@package
  facebook::react::ShadowlistViewShadowNode::ConcreteState::Shared _state;
  UIScrollView * _scrollView;
  UIView * _contentView;

  /* Keyboard-avoidance bottom inset (px); held to diff against the next value. */
  CGFloat _contentInsetBottom;

  /* Pull-to-refresh: control, whether it is installed, the controlled state, the tint. */
  UIRefreshControl * _refreshControl;
  BOOL _refreshEnabled;
  BOOL _refreshing;
  UIColor * _refreshColor;
  /* Set when refreshing ends; cleared once the retract spring quiesces, when onRefreshSettle
   * fires. The token invalidates superseded settle debounces. */
  BOOL _refreshAwaitingSettle;
  NSInteger _refreshSettleToken;

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
  __weak UIView * _stickyHeaderView;
  __weak UIView * _stickyFooterView;

  /* Active section-header overlay and the in-flow section-header geometry it pins to. */
  std::vector<int> _stickyHeaderIndices;
  std::vector<double> _stickyHeaderOffsets;
  std::vector<double> _stickyHeaderSizes;
  __weak UIView * _sectionHeaderOverlay;

  /* The last offset we applied, to distinguish echoed scrolls from user scrolls. */
  CGPoint _appliedOffset;
  BOOL _hasAppliedOffset;

  /* Drag-to-reorder: whether it is enabled, the gesture, the per-frame driver, the
   * picked-up view, and whether a drag is in progress. */
  BOOL _dragEnabled;
  UILongPressGestureRecognizer * _dragRecognizer;
  CADisplayLink * _dragDisplayLink;
  __weak UIView * _draggedView;
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
  __weak UIView * _droppedView;
  NSInteger _dropInsertionIndex;
  /* Content-space leading of the dragged row at release, so it animates into its
   * resting slot rather than snapping. */
  CGFloat _dragLeading;
  CGFloat _dropReleaseLeading;
  /* Polls for the reorder commit landing so the drop settle always fires. */
  CADisplayLink * _dropSettleLink;
  /* Invalidates a superseded drop safety-net timer so a stale drop can't tear down a newer one. */
  NSInteger _dropSettleToken;
}

/* Index from an element view's props, or NSNotFound for a non-element view. */
- (NSInteger)indexOfElementView:(UIView *)view;

/* Re-pin sticky/auto-hide views; accumulate is YES only on genuine user scrolls. */
- (void)applyStickyTransforms:(BOOL)accumulate;

/* Drag-to-reorder, driven from mount/state/recycle. */
- (void)handleDragGesture:(UILongPressGestureRecognizer *)gesture;
- (void)updateDrag;
- (void)applyDragShuffle;
- (void)clearDragTransforms;
- (void)teardownDrag;
/* Animate the just-dropped row from its release point into its resting slot. */
- (void)settleDroppedView:(UIView *)view;

@end
