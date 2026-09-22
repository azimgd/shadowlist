#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Element.hpp>
#include <shadowlist-core/Operation.hpp>
#include <shadowlist-core/Revision.hpp>

namespace azimgd::shadowlist {

/*
 * What to send to the scroll view for one frame.
 */
struct ContainerStateUpdate {
  // True when something changed and new state should be sent.
  bool changed = false;

  // Move the scroll view to the offset below. When false, leave it alone so we don't fight the user.
  bool applyContainerOffset = false;

  double containerOffsetX = 0.0;
  double containerOffsetY = 0.0;
  double totalContainerWidth = 0.0;
  double totalContainerHeight = 0.0;

  // Id of the operation that moved the offset, or 0. The host sends it back so we know the write was ours.
  std::uint64_t commitToken = 0;
};

class Container {
public:
  // Width and height used for rows not measured yet.
  std::pair<double, double> estimatedElementSize = DEFAULT_ESTIMATED_ELEMENT_SIZE;

  // Called when the user scrolls near the end.
  std::function<void()> onEndReachedCallback;

  // Called when the user scrolls near the start.
  std::function<void()> onStartReachedCallback;

  // Called with the start and end index when the visible rows change.
  std::function<void(std::size_t, std::size_t)> onVisibleIndicesChangeCallback;

  // Called with the x and y offset when the scroll offset changes.
  std::function<void(double, double)> onScrollCallback;

  /*
   * Called with the start and end index when the viewable rows change.
   * Only rows inside the viewport count, using viewablePercentThreshold.
   */
  std::function<void(std::size_t, std::size_t)> onViewableIndicesChangeCallback;

  // Turn the start and end reached callbacks on or off.
  bool endReachedEnabled = true;
  bool startReachedEnabled = true;

  // The current revision and its number.
  Revision revision = {};
  std::size_t revisionCount = REVISION_COUNT_FIRST;

  // An inverted list runs bottom to top.
  bool inverted = false;

  bool horizontal = false;

  std::size_t columns = 1;

  /*
   * How far past the viewport to measure and mount rows on each side, in viewport sizes.
   * 1 keeps one screen above and one below, 0 keeps only what is visible.
   */
  double overscan = 1.0;

  /*
   * Snap the resting scroll position to a row edge.
   * snapAlignment picks the edge: 0 start, 1 center, 2 end.
   */
  bool snapToItem = false;
  int snapAlignment = 0;

  // How close to an edge, in viewport sizes, before onStartReached or onEndReached fires.
  double startReachedThreshold = 1.0;
  double endReachedThreshold = 1.0;

  // How much of a row, from 0 to 1, must be on screen to count as viewable. 0 means any overlap.
  double viewablePercentThreshold = 0.0;

  // Size of the header or empty template along the scroll axis. Rows start after it.
  double headerSize = 0.0;

  // Size of the footer along the scroll axis. It counts toward the total size.
  double footerSize = 0.0;

  // Indexes of sticky section headers in order, set each frame. Empty for a plain list.
  std::vector<std::size_t> stickyIndices;

  // Last drag event number sent to JS, so each drag event fires once. -1 means none yet.
  double lastDragEventSequence = -1.0;

  /*
   * Scroll requests. resolveScroll turns each into an operation and clears it.
   * A request can arrive between frames, so it waits here until the revision is measured.
   */

  // Pending scrollToIndex target, or UNDEFINED_INDEX when there is none.
  std::size_t scrollToIndexTarget = UNDEFINED_INDEX;

  /*
   * Where the target row rests in the viewport, 0 top, 0.5 middle, 1 bottom.
   * A row taller than the viewport always lands at the top.
   */
  double scrollToIndexViewPosition = 0.0;

  /*
   * Set while scrollToEnd closes in on the bottom as rows get measured.
   * Cleared once we reach the bottom and the total size stops changing.
   * update also sets it when rows are appended to an inverted list resting at the bottom, so it follows them.
   */
  bool pendingScrollToEnd = false;
  // Set by scrollToStart and used up by the next resolve.
  bool pendingScrollToStart = false;
  // Last frame's total size, to tell when it stops changing.
  double pendingScrollToEndLastTotal = -1.0;

  /*
   * Whether an inverted list has settled at the bottom. Until then it sticks to the bottom,
   * after that the normal anchor keeps the visible content in place.
   */
  bool invertedInitialized = false;

  /*
   * Set when the user scrolls an inverted list up off the bottom, so the bottom pin lets go.
   * Without it a tall last row gets pinned again every frame and the view snaps back under the finger.
   * Cleared only when the user scrolls back to the bottom. Content shrinking under a still reader
   * must not turn the pin back on. Updated in update before anything is measured.
   */
  bool invertedBottomReleased = false;

  /*
   * True while the user drives the offset: a finger is down, momentum runs, or the user just scrolled.
   * The bottom pin waits while this is set, even near the bottom, or the drag fights it every frame.
   * When the finger lifts near the bottom the pin takes over again.
   */
  bool gestureActive = false;

  /*
   * Id of the last operation running during a gesture, or 0.
   * During a gesture the host applies our correction on top of its live offset, so its echo
   * counts as done even after the motion stops. Otherwise we would push the view to a target
   * that ignores how far the finger moved.
   */
  std::uint64_t gestureOperationId = 0;

  /*
   * Set while an inverted list is still settling on the bottom it opened at, until the first
   * gesture, scroll command or key change. Remeasures then keep the real bottom in view rather
   * than the top row, or the view would stop short and miss appends.
   * After the user touches the list, normal anchoring applies again.
   */
  bool invertedOpeningPin = false;

  // Whether the inverted list was at the bottom when this frame started, as the reader saw it.
  bool restingAtInvertedBottom = false;

  /*
   * How much the anchor row grew, or shrank if negative, on its first real measurement.
   * That row usually sits across the top edge, so commitElementSizes takes the change above
   * the viewport instead of moving the rows below. Set by applyElementSize and reset on commit.
   */
  double anchorFirstMeasurementDelta = 0.0;

  // The running correction. Only the operation below moves the offset, the rest is its target and bookkeeping.

  /*
   * True when this frame made an offset the host should apply. Reset at the start of each
   * resolveScroll. It does not say whether a correction is running, operation does that.
   */
  bool containerOffsetCorrected = false;

  /*
   * The row at the top of the viewport and how far we are scrolled into it.
   * Captured each frame and used to keep the visible content still while other rows are measured.
   */
  Anchor anchor = {};

  /*
   * The running offset correction, if any. Its id goes to the host as the commit token.
   * End corrections hold the end edge. Keeping content in place and scrollToIndex hold a row by key.
   */
  std::optional<Operation> operation = std::nullopt;

  // Next operation id. Starts at 1 because 0 means no operation, a report from the host.
  std::uint64_t nextOperationId = 1;

  /*
   * Sizes the host measured ahead of time, by key, before the row was ever rendered.
   * An entry moves onto its row once the row exists and is then erased, so this only holds
   * sizes that arrived early. They never feed the average, which counts real measurements only.
   * The host keeps this small. Measure a screen or two ahead, not the whole dataset.
   */
  std::unordered_map<std::string, Size> predictedSizes;

  /*
   * Save an early size for a key. Fine for keys not in the list yet, it waits for a later update.
   * To apply it to a row that already exists, use Virtualizer::applyPredictedElementSize.
   */
  void setPredictedSize(const std::string& key, Size size);

  /*
   * Keys never used as the anchor, like date pills or unread dividers whose keys come and go.
   * The anchor picks the nearest real content row instead. Empty means any row can be the anchor.
   */
  std::unordered_set<std::string> nonAnchorableKeys;

  /*
   * Layout inputs from the last offset pass, so layoutElements can skip it when nothing changed.
   * Window sizes are here because column widths depend on them.
   */
  double lastLayoutHeaderSize = -1.0;
  double lastLayoutFooterSize = -1.0;
  double lastLayoutWindowWidth = -1.0;
  double lastLayoutWindowHeight = -1.0;
  std::size_t lastLayoutColumns = 0;
  bool lastLayoutHorizontal = false;

  /*
   * Size last given to unmeasured rows. While it and the layout stay the same, the sizing
   * loop has nothing to do and is skipped. -1 forces the first pass.
   */
  double lastFallbackWidth = -1.0;
  double lastFallbackHeight = -1.0;

  /*
   * Bumped whenever row positions, sizes or the row list change, so snap offsets and sticky
   * headers can be cached between frames. Starts at 1 because 0 means nothing cached.
   */
  std::uint64_t geometryVersion = 1;

  /*
   * Set on any insert, remove or reorder, since positions shift even without size changes.
   * Starts true so the first layout always runs.
   */
  bool elementsStructureDirty = true;

  /*
   * Lowest row whose size changed outside layoutElements, or UNDEFINED_INDEX.
   * A size change only moves rows after it, so the next layout starts here instead of at row 0.
   */
  std::size_t elementsSizeDirtyFromIndex = UNDEFINED_INDEX;

  /*
   * Highest row whose size changed. Past it, once the new offsets match the old ones, the walk can stop.
   * Both ends are needed. Offsets can match by chance between two changed rows, and stopping
   * there would leave the later rows wrong.
   */
  std::size_t elementsSizeDirtyToIndex = 0;

  /*
   * Mark a size change the next layout must reflow. A caller that reflows it itself must use
   * noteElementSizeSpan instead, or the work is done twice.
   */
  void markElementSizeDirty(std::size_t index) {
    if (index < this->elementsSizeDirtyFromIndex) {
      this->elementsSizeDirtyFromIndex = index;
    }
    this->noteElementSizeSpan(index);
  }

  /*
   * Widen the changed range without scheduling a layout reflow.
   * Used by batched sizes, where commitElementSizes reflows once and only needs to know where to stop.
   */
  void noteElementSizeSpan(std::size_t index) {
    if (index > this->elementsSizeDirtyToIndex) {
      this->elementsSizeDirtyToIndex = index;
    }
  }

  /*
   * Farthest any row reaches on the other axis. Tracked here because a wide row mid list would
   * be missed by the total size scan. Full passes rebuild it, partial passes only grow it.
   */
  double maxCrossAxisExtent = 0.0;

  /*
   * Offset from the last host report. A user scroll only cancels a correction when this moves,
   * so a stale userScrolled flag can't. Frames that carry our own offset write leave it alone,
   * because the host is not there yet.
   */
  double lastReportedOffset = 0.0;

  /*
   * Guards the Container, which really is shared across threads. In Fabric, shadow node clones
   * share one core, and adopt, layout and measurement can run at the same time on commit threads.
   * It is recursive because public Virtualizer methods lock it and call each other, and Fabric
   * holds it across a whole frame of calls.
   * Hold it for any access to a shared Container. Virtualizer methods lock it themselves, but
   * the getters and setters below do not.
   */
  std::recursive_mutex coreMutex;

  /*
   * Finish the frame: bump revisionCount, fire the reached callbacks and notify observers.
   */
  void endRevision();

  const Element& getElementAtIndex(std::size_t index) const;

  /*
   * These follow the scroll axis, using x and width when horizontal, y and height otherwise.
   */
  double getElementOffset(std::size_t index) const;
  double getElementSize(std::size_t index) const;
  double getContainerOffset() const;
  double getWindowContainerSize() const;

  std::size_t getElementsSize() const;

  /*
   * Visible rows, or UNDEFINED_INDEX for both before the first layout.
   */
  std::pair<std::size_t, std::size_t> getVisibleIndices() const;

  /*
   * Rows with at least viewablePercentThreshold on screen, or UNDEFINED_INDEX for both when none.
   * Inverted lists return a start greater than the end.
   */
  std::pair<std::size_t, std::size_t> getViewableIndices() const;

  /*
   * Whether the row's size can be trusted, either measured natively or predicted by the host.
   */
  bool hasTrustedSize(std::size_t index) const;

  void setEndReachedEnabled(bool enabled);
  void setStartReachedEnabled(bool enabled);

  // Ask to scroll the row at index into view. Handled on the next measurement.
  void scrollToIndex(std::size_t index, double viewPosition = 0.0);

  /*
   * Ask to scroll to the end, following the bottom as rows get measured.
   */
  void scrollToEnd();

  /*
   * Ask to scroll to the very top, header included, holding the first row while rows above get measured.
   */
  void scrollToStart();

  /*
   * Turn the scrollToIndex command or prop into a request. The command runs once per call,
   * the prop runs when its value changes. A negative index means none, and the command wins.
   */
  void requestScrollToIndex(double commandIndex, double commandSequence, int propIndex, double commandViewPosition = 0.0);

  /*
   * Work out what to send to the scroll view this frame, given what it has now.
   */
  ContainerStateUpdate resolveStateUpdate(
    double previousContainerOffsetX,
    double previousContainerOffsetY,
    double previousTotalContainerWidth,
    double previousTotalContainerHeight) const;

  /*
   * Where the footer sits along the scroll axis, right after the content.
   */
  double getFooterOffset(double footerSize) const;

  /*
   * Sorted offsets where scrolling can come to rest with a row aligned by snapAlignment.
   * Empty unless snapToItem is set. Cached until the geometry changes.
   * The returned reference belongs to the Container and is invalid after the next call.
   */
  const std::vector<double>& getSnapOffsets() const;

  /*
   * Index of the row with this key, or UNDEFINED_INDEX.
   */
  std::size_t findElementIndexByKey(const std::string& key) const;

  /*
   * Whether a key can be the anchor. Empty keys and keys in nonAnchorableKeys cannot.
   */
  bool isAnchorable(const std::string& key) const;

  /*
   * The anchor to hold still while sizes change. That is the running correction's row, or the
   * captured anchor when nothing runs, or null while an end correction owns the offset.
   * The key may be empty.
   */
  const Anchor* compensationAnchor() const;

  /*
   * Fire the visible rows and scroll callbacks, but only when their values changed.
   */
  void dispatchObservers();

private:
  // Cached snap offsets and the inputs they came from. A version of 0 means nothing cached.
  mutable std::vector<double> snapOffsetsCache;
  mutable std::uint64_t snapCacheVersion = 0;
  mutable bool snapCacheSnapToItem = false;
  mutable int snapCacheAlignment = -1;
  mutable double snapCacheWindowSize = -1.0;
  mutable double snapCacheTotalSize = -1.0;
  mutable bool snapCacheHorizontal = false;

  // Last visible range sent, so we only send changes.
  std::size_t previousVisibleStartIndex = UNDEFINED_INDEX;
  std::size_t previousVisibleEndIndex = UNDEFINED_INDEX;

  // Last viewable range sent, so we only send changes.
  std::size_t previousViewableStartIndex = UNDEFINED_INDEX;
  std::size_t previousViewableEndIndex = UNDEFINED_INDEX;

  /*
   * Whether we were already at an edge, so reached callbacks fire once on arrival.
   * They reset when the row count changes, like after loading a page.
   */
  bool previousReachedStart = false;
  bool previousReachedEnd = false;
  std::size_t previousReachedElementsSize = UNDEFINED_INDEX;

  // Last offset sent to onScroll, so we only send changes.
  double previousContainerOffsetX = 0.0;
  double previousContainerOffsetY = 0.0;
  bool previousContainerOffsetValid = false;

  // Last scrollToIndex command number handled, so the same index can still scroll again.
  double previousScrollToIndexSequence = 0.0;

  // Last containerOffsetIndex prop handled, so it only scrolls when the value changes.
  int previousScrollToIndexProp = -1;
};

}
