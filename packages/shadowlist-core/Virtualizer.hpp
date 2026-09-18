#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
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
  /*
   * The frame's ordered row keys. An integration that already owns an immutable key
   * vector for the commit (Fabric's Props) should point `keysRef` at it instead of
   * filling `keys`: the core only reads the keys during the synchronous update() call,
   * so copying the whole collection into every frame costs one allocation per row for
   * keys long enough to spill the small-string buffer, for nothing. `keys` remains the
   * owning path for standalone drivers and tests. Use keyList() to read either.
   */
  std::vector<std::string> keys;
  const std::vector<std::string>* keysRef = nullptr;

  const std::vector<std::string>& keyList() const {
    return this->keysRef != nullptr ? *this->keysRef : this->keys;
  }

  /*
   * Set only by an integration that can prove these are the very same keys the previous
   * completed update() consumed: not "probably the same", but the same immutable
   * collection, kept alive across both calls so its address cannot have been reused.
   * Fabric can: a scroll clones the shadow node with a new state and the same props
   * object, and props are immutable.
   *
   * The core otherwise compares every key against every element on every commit to
   * decide whether to reconcile. That comparison is correct but dataset-sized, and it is
   * the last O(N) step left on an ordinary scroll frame. When this flag is set the core
   * trusts it and skips straight past reconciliation. A false positive would leave the
   * core's element list out of step with the data, so leave it false when in any doubt.
   */
  bool keysUnchanged = false;

  double containerOffsetX = 0.0;
  double containerOffsetY = 0.0;
  double windowContainerWidth = 0.0;
  double windowContainerHeight = 0.0;
  double headerSize = 0.0;
  double footerSize = 0.0;
  bool inverted = false;
  /*
   * Whether an inverted list resting at its bottom scrolls onto rows appended below the
   * newest one (an assistant conversation). Off, an append keeps the rows on screen where
   * they are, like any other insert (a chat receiving messages while the reader reads).
   */
  bool followAppends = false;
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
   * Keys that must never be auto-captured as the MVCP anchor: decoration rows (date pills,
   * unread dividers, reaction strips, padding) whose identity churns independently of
   * content. The core anchors to the nearest stable content row instead, so a key change
   * on decoration cannot perturb the maintained scroll position.
   */
  std::vector<std::string> nonAnchorableKeys;

  // Borrowed alternative to nonAnchorableKeys, with the same lifetime rules as keysRef.
  const std::vector<std::string>* nonAnchorableKeysRef = nullptr;

  const std::vector<std::string>& nonAnchorableKeyList() const {
    return this->nonAnchorableKeysRef != nullptr ? *this->nonAnchorableKeysRef : this->nonAnchorableKeys;
  }
};

/*
 * Threading contract: a Container may be shared across threads (see Container::coreMutex).
 * Every public method below acquires container->coreMutex on entry, so each is safe to
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
  static void update(Container* container, const FrameInput& input);

  /*
   * Measure elements for the current revision (orientation/columns aware).
   * windowFromOffset selects the visible window from the current scroll offset
   * instead of filling from the edge.
   */
  static void measure(Container* container, bool windowFromOffset = false);

  /*
   * Recompute total container size from the maximum element extent.
   */
  static void recomputeTotalSize(Container* container);

  /*
   * Reconcile the element list to a new ordered set of keys, preserving the
   * measured state of surviving elements and creating fresh ones for new keys
   */
  static void reconcileElements(Container* container, const std::vector<std::string>& nextKeys);

  /*
   * Update measurements for existing element at specific index, then propagate the
   * resulting geometry. Equivalent to applyElementSize() followed by commitElementSizes()
   * when the size actually changed; a size identical to the one already recorded costs
   * nothing.
   */
  static void updateElementAtIndex(Container* container, std::size_t index, Size size);

  /*
   * Record a natively measured size without reflowing offsets or re-pinning the anchor.
   * Returns true when the recorded geometry actually changed, i.e. when the suffix from
   * `index` onward needs to be reflowed.
   *
   * A host that feeds back a whole batch of mounted rows in one pass (Fabric's layout)
   * should call this for each row, track the lowest index that returned true, and then
   * call commitElementSizes() once. Reflowing per row instead makes a layout with M
   * mounted rows cost O(M x N) near the start of an N-row list, even when every size is
   * unchanged.
   */
  static bool applyElementSize(Container* container, std::size_t index, Size size);

  /*
   * Record a host-supplied ahead-of-time size for the row with this key: what it will
   * measure to once laid out, computed before it was ever rendered.
   *
   * Returns the index that needs reflowing, or UNDEFINED_INDEX when nothing moved: the
   * row does not exist yet (the size is staged for the next reconcile), it has already been
   * natively measured (a real measurement always wins), or the prediction matched the size
   * it already carried. As with applyElementSize, a host applying a batch should track the
   * lowest index returned and call commitElementSizes() once for the whole batch.
   *
   * A predicted row is `estimated` (the fallback passes leave it alone) but not `measured`:
   * it still gets laid out natively when it is revealed, and that measurement supersedes
   * the prediction.
   */
  static std::size_t applyPredictedElementSize(Container* container, const std::string& key, Size size);

  /*
   * Drop every prediction, staged and already applied, so predicted rows fall back to
   * the ordinary estimate and are measured natively again. For when the host's measurements
   * stop being valid, above all a width change: text wraps to the row width, so every
   * height measured at the old one is wrong. Natively measured rows are left alone.
   */
  static void invalidatePredictions(Container* container);

  /*
   * Propagate geometry after a batch of applyElementSize() calls: reflow offsets from
   * `fromIndex` and keep the anchored row fixed on screen. Safe to skip entirely when no
   * applyElementSize() call reported a change.
   */
  static void commitElementSizes(Container* container, std::size_t fromIndex);

  /*
   * Settle a header size change after the offsets were reflowed for it. Call with the header
   * size the reflow replaced. update() calls it for a header size change in its input; Fabric
   * measures the header in its layout pass, outside update(), and calls it there. A header wholly scrolled out of view moves the offset with it, so the
   * rows on screen stay put in this same frame; a header on screen pushes the rows, and the
   * anchor is moved so no later pass or frame holds them back. Without this, an in-flight
   * anchor correction re-resolves against the reflowed rows one commit later and the content
   * jumps by the header change for a frame.
   */
  static void applyHeaderSizeChange(Container* container, double previousHeaderSize);

  /*
   * Settle a window (viewport) size change. Call after the new size is set, with the size it
   * replaced; Fabric applies the window in its layout pass, outside update(). An inverted list
   * resting at its bottom keeps it: a chat composer growing a line at a time shrinks the
   * window from the bottom, and plain anchoring holds the row at the viewport top, so the
   * newest rows would slide under the composer and, a few lines later, the reader would sit
   * outside the follow band and stop following the messages they send.
   */
  static void applyWindowSizeChange(Container* container, double previousWindowSize);

  /*
   * Recompute element offsets starting from a given index (orientation/columns aware).
   */
  static void recomputeElementOffsets(
    Container* container,
    std::size_t fromIndex,
    std::size_t changedThroughIndex = UNDEFINED_INDEX);

private:
  /*
   * Stamp staged ahead-of-time sizes (Container::predictedSizes) onto the rows that now
   * exist, consuming each entry. Runs once per frame between reconcile and measure.
   */
  static void consumePredictions(Container* container);

  /*
   * Measure a window of elements from the edge of the list (first revision)
   */
  static void measureFirstRevision(Container* container);

  /*
   * Measure the elements within the visible window plus buffer (subsequent revisions)
   */
  static void measureNextRevision(Container* container);

  /*
   * Store the measured index range (orientation aware)
   */
  static void finalizeMeasurement(Container* container, std::size_t measuredMinIndex, std::size_t measuredMaxIndex);

  /*
   * Size unmeasured elements with average dimensions and recompute all offsets
   */
  static void layoutElements(Container* container);

  /*
   * Record which element currently sits at the top/left of the viewport and how
   * far we are scrolled into it, so the position can be restored after a reconcile
   */
  static void captureAnchor(Container* container, double inputOffset);

  /*
   * Apply scroll corrections after measuring: a pending scrollToIndex, the inverted
   * bottom anchor, otherwise keep the captured anchor element at the same viewport
   * position. Returns true when the scroll offset was moved.
   */
  static bool resolveScroll(
    Container* container,
    const std::string& anchorKey,
    double anchorDelta,
    bool hadElementsBefore,
    bool offsetConfirmed);
};

}
