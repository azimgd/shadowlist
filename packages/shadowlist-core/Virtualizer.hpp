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
 * Everything the host sends for one frame. The resulting layout goes back into the container.
 */
struct FrameInput {
  /*
   * Row keys in order. If the host already holds them, like Fabric props, point keysRef at them
   * instead of copying, since keys are only read during update. Tests fill keys. Read with keyList.
   */
  std::vector<std::string> keys;
  const std::vector<std::string>* keysRef = nullptr;

  const std::vector<std::string>& keyList() const {
    return this->keysRef != nullptr ? *this->keysRef : this->keys;
  }

  /*
   * Set only when these are provably the same key list the last update used, the very same
   * object kept alive in between. Fabric can tell because a scroll reuses the same props.
   * It lets us skip comparing every key on a scroll frame. Getting it wrong puts the rows out
   * of step with the data, so leave it false if in doubt.
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
   * Whether an inverted list at the bottom scrolls to show appended rows, like an assistant chat.
   * When off, appends keep the visible rows in place like any other insert.
   */
  bool followAppends = false;
  bool horizontal = false;
  std::size_t columns = 1;

  // Extra rows past the viewport, in viewport sizes. 1 means one screen on each side.
  double overscan = 1.0;

  // Sorted indexes of rows that stick to the top once scrolled past. Empty for a plain list.
  std::vector<std::size_t> stickyIndices;

  double startReachedThreshold = 1.0;
  double endReachedThreshold = 1.0;
  double viewablePercentThreshold = 0.0;
  std::pair<double, double> estimatedElementSize = DEFAULT_ESTIMATED_ELEMENT_SIZE;

  /*
   * Snap the resting position to a row edge. snapAlignment picks it: 0 start, 1 center, 2 end.
   * The core only works out the offsets, the scroll view does the snapping.
   */
  bool snapToItem = false;
  int snapAlignment = 0;

  // Set when the user scrolls. Drops any running correction so the user is not pulled back.
  bool userScrolled = false;

  /*
   * True when the offset is one we asked for that the scroll view hasn't confirmed yet.
   * Fine for measuring, but it doesn't prove the correction landed.
   */
  bool containerOffsetEnabled = false;

  // The operation id the host sends back when this report came from our offset write, or 0.
  std::uint64_t commitToken = 0;

  // The current gesture. Dragging and Settling mean the user is scrolling, Idle covers our own moves.
  ScrollPhase scrollPhase = ScrollPhase::Idle;

  /*
   * Keys never used as the anchor, like date pills or unread dividers whose keys come and go.
   * The nearest real content row is used instead.
   */
  std::vector<std::string> nonAnchorableKeys;

  // Points at the host's list instead of copying it, same rules as keysRef.
  const std::vector<std::string>* nonAnchorableKeysRef = nullptr;

  const std::vector<std::string>& nonAnchorableKeyList() const {
    return this->nonAnchorableKeysRef != nullptr ? *this->nonAnchorableKeysRef : this->nonAnchorableKeys;
  }
};

/*
 * A Container can be shared across threads. Every public method here locks coreMutex, so
 * they are safe to call from any thread, even while the caller already holds the lock.
 * Private helpers expect the lock to be held already.
 */
class Virtualizer {
public:
  /*
   * Runs once per frame. Match rows to the new keys, measure, fix up the scroll offset and fire callbacks.
   */
  static void update(Container* container, const FrameInput& input);

  /*
   * Measure rows for the current revision. With windowFromOffset the window starts at the
   * current scroll offset instead of the edge of the list.
   */
  static void measure(Container* container, bool windowFromOffset = false);

  /*
   * Work out the total content size from the farthest row.
   */
  static void recomputeTotalSize(Container* container);

  /*
   * Match the rows to a new key list. Existing rows keep their sizes, new keys get new rows.
   */
  static void reconcileElements(Container* container, const std::vector<std::string>& nextKeys);

  /*
   * Set a row's measured size and move the rows after it. Same as applyElementSize then
   * commitElementSizes. An unchanged size costs nothing.
   */
  static void updateElementAtIndex(Container* container, std::size_t index, Size size);

  /*
   * Save a measured size without moving other rows or the anchor. Returns true if the size changed.
   * For a batch, call this per row, keep the lowest index that returned true, then call
   * commitElementSizes once. Reflowing per row gets very slow on long lists.
   */
  static bool applyElementSize(Container* container, std::size_t index, Size size);

  /*
   * Save a size the host measured before the row was rendered.
   * Returns the index to reflow, or UNDEFINED_INDEX when nothing moved. That happens when the row
   * doesn't exist yet and the size waits for it, when it was already measured and the real size wins,
   * or when the size is the same. For a batch, keep the lowest index and commit once.
   * The row still gets measured natively when it shows up, and that replaces the prediction.
   */
  static std::size_t applyPredictedElementSize(Container* container, const std::string& key, Size size);

  /*
   * Throw away every predicted size, so those rows go back to the estimate until measured.
   * Use it when predictions go stale, like after a width change rewraps the text.
   * Rows measured natively keep their sizes.
   */
  static void invalidatePredictions(Container* container);

  /*
   * After a batch of applyElementSize calls, move rows from fromIndex on and keep the anchor
   * row in place. Skip it if nothing changed.
   */
  static void commitElementSizes(Container* container, std::size_t fromIndex);

  /*
   * Handle a header size change after rows moved for it. Pass the old header size.
   * update calls it, and Fabric calls it from its layout pass too. An off screen header moves the
   * offset with it so the visible rows stay put. A visible header pushes the rows and the anchor
   * follows. Without this the content jumps for one frame.
   */
  static void applyHeaderSizeChange(Container* container, double previousHeaderSize);

  /*
   * Handle a viewport size change. Call after setting the new size, passing the old one.
   * An inverted list at the bottom stays at the bottom. Otherwise a growing chat composer would
   * hide the newest messages and the list would stop following new ones.
   */
  static void applyWindowSizeChange(Container* container, double previousWindowSize);

  /*
   * Recompute row positions starting at fromIndex.
   */
  static void recomputeElementOffsets(
    Container* container,
    std::size_t fromIndex,
    std::size_t changedThroughIndex = UNDEFINED_INDEX);

private:
  /*
   * Move waiting predicted sizes onto rows that now exist. Runs each frame between reconcile and measure.
   */
  static void consumePredictions(Container* container);

  /*
   * First revision. Measure a window of rows from the edge of the list.
   */
  static void measureFirstRevision(Container* container);

  /*
   * Later revisions. Measure the visible rows plus the overscan.
   */
  static void measureNextRevision(Container* container);

  /*
   * Save which rows were measured.
   */
  static void finalizeMeasurement(Container* container, std::size_t measuredMinIndex, std::size_t measuredMaxIndex);

  /*
   * Give unmeasured rows the average size and recompute positions.
   */
  static void layoutElements(Container* container);

  /*
   * Remember the row at the top of the viewport and how far into it we are, to restore after a data change.
   */
  static void captureAnchor(Container* container, double inputOffset);

  /*
   * Backup anchors for the other rows that start inside the viewport. If a data change removes
   * the anchor row, the next visible row is held in place instead.
   */
  static std::vector<Anchor> captureFallbackAnchors(Container* container, double inputOffset);

  /*
   * Fix up the scroll offset after measuring. Handles scrollToIndex, the inverted bottom pin,
   * or else keeps the anchor row where it was. Returns true if the offset moved.
   */
  static bool resolveScroll(
    Container* container,
    const std::string& anchorKey,
    double anchorDelta,
    bool hadElementsBefore,
    bool offsetConfirmed);
};

}
