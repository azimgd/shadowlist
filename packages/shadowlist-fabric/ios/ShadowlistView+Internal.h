#import "ShadowlistView.h"

#import "ShadowlistViewComponentDescriptor.h"
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

#include <vector>

/*
 * Private surface shared by ShadowlistView and its categories (drag-to-reorder and
 * sticky pinning). The ivars live here - declared @package so the category methods,
 * which compile in their own translation units, can reach them - and only the
 * methods called across category boundaries are declared. Methods used within a
 * single category are left private to that category's @implementation.
 *
 * Obj-C++ only: it carries C++ types, so it must never be imported from a plain .m.
 */
@interface ShadowlistView () <RCTShadowlistViewViewProtocol, UIScrollViewDelegate> {
@package
  facebook::react::ShadowlistViewShadowNode::ConcreteState::Shared _state;
  UIScrollView * _scrollView;
  UIView * _contentView;

  /*
   * Keyboard-avoidance bottom inset (px), driven by the contentInsetBottom prop. We
   * grow the scroll view's bottom contentInset by this and slide the content up by
   * the same delta so rows behind the keyboard come into view; reversed when it
   * shrinks back to 0. Held here to diff against the next value for the delta.
   */
  CGFloat _contentInsetBottom;

  /*
   * Sticky header/footer are pinned natively here (not in the core layout) so they
   * track scrolling on the UI thread without the commit-cycle latency that made the
   * core-driven version choppy. The template views keep their resting position from
   * the shadow node; we layer a translation on top each scroll frame.
   */
  BOOL _stickyHeader;
  BOOL _stickyFooter;
  BOOL _horizontal;
  /*
   * Direction-based auto-hide: the header/footer pins to its edge and slides away as
   * the user scrolls toward the content, sliding back the other way. _headerHidden /
   * _footerHidden track how far each is currently slid away (0..size along the scroll
   * axis); _lastAutoHideOffset is the previous offset used to derive the scroll delta.
   */
  BOOL _autoHideHeader;
  BOOL _autoHideFooter;
  CGFloat _headerHidden;
  CGFloat _footerHidden;
  CGFloat _lastAutoHideOffset;
  __weak UIView * _stickyHeaderView;
  __weak UIView * _stickyFooterView;

  /*
   * Sticky section headers (SectionList). Rather than pin a virtualized element
   * (which the JS layer would have to keep mounted, arriving several commits late
   * at each section boundary - the source of the scroll lag), the active section
   * header is rendered as an always-mounted overlay template (_sectionHeaderOverlay,
   * templateType "sectionHeader"). The core publishes the resting geometry (offset +
   * size along the scroll axis) of the in-flow section headers through state; we pin
   * the overlay to the viewport start every scroll tick (mirroring
   * Container::resolveStickyHeader), pushing it up as the next in-flow header
   * arrives. The overlay never unmounts, so its position never lags - only its
   * content swaps (in JS) at a boundary, masked by the real in-flow header behind it.
   */
  std::vector<int> _stickyHeaderIndices;
  std::vector<double> _stickyHeaderOffsets;
  std::vector<double> _stickyHeaderSizes;
  __weak UIView * _sectionHeaderOverlay;

  /*
   * The last offset the core applied itself. scrollViewDidScroll fires for both the
   * core's own setContentOffset and every user scroll, so we tell them apart by
   * comparing against this rather than isDragging/isDecelerating - those are only
   * set for touch gestures, so a trackpad / mouse-wheel scroll (e.g. the iOS
   * Simulator) would look programmatic and the core would never learn the user took
   * over, latching its inverted-pin / MVCP correction and blanking the list.
   */
  CGPoint _appliedOffset;
  BOOL _hasAppliedOffset;

  /*
   * Drag-to-reorder. The whole gesture runs on the UI thread - exactly like the
   * sticky pin - so it never waits on the commit cycle. A long press picks an
   * element up; while held we translate it under the finger every CADisplayLink
   * tick, auto-scroll when it nears a viewport edge, and shuffle the siblings to open
   * a gap at the insertion point. The data order stays FIXED during the drag (no
   * re-render), and a single reorder is applied on drop. JS only ever sees
   * onDragStart and onDragEnd.
   */
  BOOL _dragEnabled;
  UILongPressGestureRecognizer * _dragRecognizer;
  CADisplayLink * _dragDisplayLink;
  __weak UIView * _draggedView;
  BOOL _dragging;
  /*
   * The data order stays FIXED during the drag. _dragOriginIndex is where the row was
   * picked up; _dragInsertionIndex is where its centre currently sits (the gap), used
   * to shuffle the siblings between them. The single reorder is applied on drop.
   */
  NSInteger _dragOriginIndex;
  NSInteger _dragInsertionIndex;
  /* Size of the picked-up row along the scroll axis - the amount each shuffled
   * sibling is offset to open the gap. */
  CGFloat _draggedExtent;
  /*
   * Distance along the scroll axis from the picked-up cell's leading edge to the
   * touch point, so the cell stays under the same spot of the finger.
   */
  CGFloat _dragGrabOffset;
  /*
   * Latest touch location in this host's (viewport) coordinates - stable while the
   * content auto-scrolls under a still finger, unlike a content-space location.
   */
  CGPoint _dragTouchInViewport;
  /*
   * After a drop, hold the shuffle transforms until JS's single reorder commit lands
   * (the dropped row's index prop reaching _dropInsertionIndex), then clear them.
   */
  BOOL _dragDropPending;
  __weak UIView * _droppedView;
  NSInteger _dropInsertionIndex;
  /*
   * Content-space leading of the dragged row at the last drag frame; captured on drop
   * (_dropReleaseLeading) so the row can animate from where the finger released it into
   * its post-reorder resting slot rather than snapping.
   */
  CGFloat _dragLeading;
  CGFloat _dropReleaseLeading;
  /*
   * Polls for the reorder commit landing (the dropped row's index prop reaching its
   * slot) so the drop settle fires reliably even when the reorder produces no host
   * state commit (a same-size reorder that does not move the viewport top publishes no
   * new state, so a state-driven landing check would miss it and snap via the net).
   */
  CADisplayLink * _dropSettleLink;
}

/*
 * Index carried by an element view's props, or NSNotFound for a non-element view.
 * Shared: the drag category reads it to track siblings, updateState to detect the
 * reorder commit landing.
 */
- (NSInteger)indexOfElementView:(UIView *)view;

/*
 * Sticky pinning (ShadowlistView+Sticky); re-pins on every scroll / commit. `accumulate`
 * is YES only for genuine user-scroll ticks, so the direction-based auto-hide advances
 * its hide/reveal amount on real scrolling but not on programmatic offset corrections
 * (MVCP, scrollToIndex, initial positioning), which would otherwise slide it spuriously.
 */
- (void)applyStickyTransforms:(BOOL)accumulate;

/* Drag-to-reorder (ShadowlistView+DragReorder); driven from mount/state/recycle. */
- (void)handleDragGesture:(UILongPressGestureRecognizer *)gesture;
- (void)updateDrag;
- (void)applyDragShuffle;
- (void)clearDragTransforms;
- (void)teardownDrag;
/* Animate the just-landed dropped row from its release point into its resting slot. */
- (void)settleDroppedView:(UIView *)view;

@end
