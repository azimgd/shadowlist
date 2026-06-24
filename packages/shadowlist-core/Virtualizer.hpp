#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <utility>

#include <cstdint>

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Element.hpp>
#include <shadowlist-core/Error.hpp>
#include <shadowlist-core/Operation.hpp>

namespace azimgd::shadowlist {

/*
 * Description of a single frame: inputs are read, resulting layout is written
 * back to the container.
 */
struct FrameInput {
  std::vector<std::string> keys;
  double containerOffsetX = 0.0;
  double containerOffsetY = 0.0;
  double windowContainerWidth = 0.0;
  double windowContainerHeight = 0.0;
  double headerSize = 0.0;
  double footerSize = 0.0;
  bool stickyHeader = false;
  bool stickyFooter = false;
  bool inverted = false;
  bool horizontal = false;
  std::size_t columns = 1;

  // Overscan in viewport units (see Container::overscan). 1.0 = one viewport on each side.
  double overscan = 1.0;

  /*
   * Element indices that pin to the viewport start once scrolled past (ascending).
   * Empty for a plain list.
   */
  std::vector<std::size_t> stickyIndices;

  double startReachedThreshold = 1.0;
  double endReachedThreshold = 1.0;
  double viewablePercentThreshold = 0.0;
  std::pair<double, double> estimatedElementSize = DEFAULT_ESTIMATED_ELEMENT_SIZE;

  /*
   * Snap the resting scroll position to an element edge. snapAlignment selects which
   * edge aligns to the viewport: 0 = start, 1 = center, 2 = end. The core only
   * computes the snap offsets (getSnapOffsets); the scroll view applies them.
   */
  bool snapToItem = false;
  int snapAlignment = 0;

  /*
   * Set for a user scroll gesture; abandons any in-flight scroll correction so the
   * user is not snapped back.
   */
  bool userScrolled = false;

  /*
   * True when containerOffsetX/Y came from a core-requested state update that the
   * host scroll view has not confirmed yet. The core may use the offset for
   * measurement, but must not treat it as proof that a pending correction arrived.
   */
  bool containerOffsetEnabled = false;

  /*
   * The commit token the host echoes back with this scroll report: the id of the
   * operation whose offset write produced it, or 0 for a report the core did not
   * cause. Lets the core recognise its own echo exactly instead of by pixel proximity.
   */
  std::uint64_t commitToken = 0;

  /*
   * The live gesture phase reported by the host. Dragging/Settling mean a human is
   * driving (a real takeover); Idle covers programmatic moves and their echoes.
   */
  ScrollPhase scrollPhase = ScrollPhase::Idle;

  /*
   * Anchor authored upstream (e.g. by JS on a data commit). When its key is empty the
   * core captures the anchor itself in key space; when set it is the source of truth
   * for what content must stay in view across this frame.
   */
  Anchor suppliedAnchor = {};

  /*
   * Keys that must never be auto-captured as the MVCP anchor: decoration rows (date pills,
   * unread dividers, reaction strips, padding) whose identity churns independently of
   * content. The core anchors to the nearest stable content row instead, so a key change
   * on decoration cannot perturb the maintained scroll position. An explicit suppliedAnchor
   * still wins even if its key is listed here.
   */
  std::vector<std::string> nonAnchorableKeys;
};

/*
 * Threading contract: a Container may be shared across threads (see Container::coreMutex).
 * Every PUBLIC method below acquires container->coreMutex on entry, so each is safe to
 * call concurrently on a shared Container and need not be externally locked. The mutex is
 * recursive, so these methods may also be invoked while an integration holds an outer lock
 * across a whole frame, and may call one another, without deadlock. The private helpers
 * assume coreMutex is already held by the public method that called them.
 */
class Virtualizer {
public:
  /*
   * Per-frame entry point: reconcile elements to the incoming keys, measure,
   * resolve scroll corrections and dispatch observer callbacks.
   */
  static void update(Container *container, const FrameInput &input);

  /*
   * Measure elements for the current revision (orientation/columns aware).
   * windowFromOffset selects the visible window from the current scroll offset
   * instead of filling from the edge.
   */
  static void measure(Container *container, bool windowFromOffset = false);

  /*
   * Recompute total container size from the maximum element extent.
   */
  static void recomputeTotalSize(Container *container);

  /*
   * Reconcile the element list to a new ordered set of keys, preserving the
   * measured state of surviving elements and creating fresh ones for new keys
   */
  static void reconcileElements(Container *container, const std::vector<std::string> &nextKeys);

  /*
   * Update measurements for existing element at specific index.
   */
  static void updateElementAtIndex(Container *container, std::size_t index, Size size);

  /*
   * Recompute element offsets starting from a given index (orientation/columns aware).
   */
  static void recomputeElementOffsets(Container *container, std::size_t fromIndex);

private:
  /*
   * Measure a window of elements from the edge of the list (first revision)
   */
  static void measureFirstRevision(Container *container);

  /*
   * Measure the elements within the visible window plus buffer (subsequent revisions)
   */
  static void measureNextRevision(Container *container);

  /*
   * Store the measured index range (orientation aware)
   */
  static void finalizeMeasurement(Container *container, std::size_t measuredMinIndex, std::size_t measuredMaxIndex);

  /*
   * Size unmeasured elements with average dimensions and recompute all offsets
   */
  static void layoutElements(Container *container);

  /*
   * Record which element currently sits at the top/left of the viewport and how
   * far we are scrolled into it, so the position can be restored after a reconcile
   */
  static void captureAnchor(Container *container, double inputOffset);

  /*
   * Apply scroll corrections after measuring: a pending scrollToIndex, the inverted
   * bottom anchor, otherwise keep the captured anchor element at the same viewport
   * position. Returns true when the scroll offset was moved.
   */
  static bool resolveScroll(
    Container *container,
    const std::string &anchorKey,
    double anchorDelta,
    bool hadElementsBefore,
    bool offsetConfirmed);
};

}
