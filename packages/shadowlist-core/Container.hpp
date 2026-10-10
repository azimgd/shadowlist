#pragma once

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Row.hpp>
#include <shadowlist-core/Operation.hpp>
#include <shadowlist-core/Revision.hpp>

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace azimgd::shadowlist {

/*
 * What to send to the scroll view for one frame.
 */
struct ContainerStateUpdate {
  /*
   * True when something changed and new state should be sent.
   */
  bool changed = false;

  /*
   * Move the scroll view to the offset below. When false, leave it alone so we don't fight the user.
   */
  bool applyOffset = false;

  double offsetX = 0.0;
  double offsetY = 0.0;
  double contentWidth = 0.0;
  double contentHeight = 0.0;

  /*
   * Id of the operation that moved the offset, or 0. The host sends it back so we know the write was ours.
   */
  std::uint64_t commitToken = 0;
};

/*
 * A range of scroll offsets, along the scroll axis, the host can move through without
 * sending the core a new frame. Inside it the core would pick the same measured range, fire
 * no edge or measured range callback, and start no correction.
 * Empty when the core needs every frame, like while a correction runs. The empty default
 * has low above high. A host that never got a band sends every frame.
 */
struct OffsetBand {
  double low = 1.0;
  double high = 0.0;

  bool isEmpty() const {
    return !(low <= high);
  }

  bool contains(double offset) const {
    return low <= offset && offset <= high;
  }
};

/*
 * Which rows count as viewable. threshold runs from 0 to 1. By default it is the share of the
 * row on screen, and with coverage set the share of the viewport the row covers. A row fully
 * on screen is always viewable under coverage. 0 means any overlap.
 */
struct ViewableRule {
  double threshold = 0.0;
  bool coverage = false;

  bool operator==(const ViewableRule& other) const {
    return threshold == other.threshold && coverage == other.coverage;
  }
};

/*
 * Which edge of a row a snap offset lines up with the viewport.
 */
enum class SnapAlignment {
  Start = 0,
  Center = 1,
  End = 2,
};

/*
 * An inclusive range of row indices, low to high. UNDEFINED_INDEX for both means empty.
 */
struct IndexRange {
  std::size_t low = UNDEFINED_INDEX;
  std::size_t high = UNDEFINED_INDEX;

  bool isEmpty() const {
    return low == UNDEFINED_INDEX || high == UNDEFINED_INDEX;
  }

  bool operator==(const IndexRange& other) const {
    return low == other.low && high == other.high;
  }

  bool operator!=(const IndexRange& other) const {
    return !(*this == other);
  }
};

class Container final {
public:
  /*
   * Width and height used for rows not measured yet.
   */
  std::pair<double, double> estimatedRowSize = DEFAULT_ESTIMATED_ROW_SIZE;

  /*
   * Called when the user scrolls near the end.
   */
  std::function<void()> onEndReachedCallback;

  /*
   * Called when the user scrolls near the start.
   */
  std::function<void()> onStartReachedCallback;

  /*
   * Called with the low and high index when the measured range changes.
   */
  std::function<void(std::size_t, std::size_t)> onMeasuredRangeChangeCallback;

  /*
   * Called with the x and y offset when the scroll offset changes.
   */
  std::function<void(double, double)> onScrollCallback;

  /*
   * Called when the viewable rows of any viewable rule change, with a low and a high index
   * per rule in rule order. A rule with no viewable row has UNDEFINED_INDEX for both.
   */
  std::function<void(const std::vector<std::size_t>&)> onViewableIndicesChangeCallback;

  /*
   * Turn the start and end reached callbacks on or off.
   */
  bool endReachedEnabled = true;
  bool startReachedEnabled = true;

  Revision revision = {};
  std::size_t revisionCount = REVISION_COUNT_FIRST;

  /*
   * An inverted list runs bottom to top.
   */
  bool inverted = false;

  bool horizontal = false;

  std::size_t numberOfColumns = 1;

  /*
   * How far past the viewport to measure and mount rows on each side, in viewport sizes.
   * 1 keeps one screen above and one below, 0 keeps only what is visible.
   */
  double overscan = 1.0;

  /*
   * Snap the resting scroll position to a row edge. snapAlignment picks the edge.
   */
  bool snapToItem = false;
  SnapAlignment snapAlignment = SnapAlignment::Start;

  /*
   * How close to an edge, in viewport sizes, before onStartReached or onEndReached fires.
   */
  double startReachedThreshold = 1.0;
  double endReachedThreshold = 1.0;

  /*
   * The rules that decide which rows are viewable, one range each.
   */
  std::vector<ViewableRule> viewableRules = {ViewableRule{}};

  /*
   * Size of the header or empty template along the scroll axis. Rows start after it.
   */
  double headerSize = 0.0;

  /*
   * Size of the footer along the scroll axis. It counts toward the total size.
   */
  double footerSize = 0.0;

  /*
   * Indices of sticky section headers in order, set each frame. Empty for a plain list.
   */
  std::vector<std::size_t> stickyIndices;

  /*
   * Previous drag event number sent to JS, to fire each drag event once. -1 means none yet.
   */
  double previousDragEventSequence = -1.0;

  /*
   * Scroll requests. resolveScroll turns each into an operation and clears it.
   * A request can arrive between frames. It waits here until the revision is measured.
   */

  /*
   * Pending scrollToRow target, or UNDEFINED_INDEX when there is none.
   */
  std::size_t scrollToRowTarget = UNDEFINED_INDEX;

  /*
   * Where the target row rests in the viewport, 0 top, 0.5 middle, 1 bottom.
   * A row taller than the viewport always lands at the top.
   */
  double scrollToRowViewPosition = 0.0;

  /*
   * A fixed distance the target row rests past its view position, like a saved scroll
   * position that sat partway into a row.
   */
  double scrollToRowViewOffset = 0.0;

  /*
   * Set while scrollToEnd closes in on the bottom as rows get measured.
   * Cleared once we reach the bottom and the total size stops changing.
   * update also sets it when rows are appended to an inverted list resting at the bottom. The list then follows them.
   */
  bool pendingScrollToEnd = false;

  /*
   * Set by scrollToStart and used up by the next resolve.
   */
  bool pendingScrollToStart = false;

  /*
   * Previous frame's total size, to tell when it stops changing.
   */
  double pendingScrollToEndPreviousTotal = -1.0;

  /*
   * Whether an inverted list has settled at the bottom. Until then it sticks to the bottom,
   * after that the normal anchor keeps the visible content in place.
   */
  bool invertedInitialized = false;

  /*
   * Set when the user scrolls an inverted list up off the bottom. The bottom pin then lets go.
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
   * During a gesture the host applies our correction on top of its live offset. Its echo
   * counts as done even after the motion stops. Otherwise we would push the view to a target
   * that ignores how far the finger moved.
   */
  std::uint64_t gestureCommitToken = 0;

  /*
   * Set while an inverted list is still settling on the bottom it opened at, until the first
   * gesture, scroll command or key change. Remeasures then keep the real bottom in view rather
   * than the top row, or the view would stop short and miss appends.
   * After the user touches the list, normal anchoring applies again.
   */
  bool invertedOpeningPin = false;

  /*
   * Whether the inverted list was at the bottom when this frame started, as the reader saw it.
   */
  bool restingAtInvertedBottom = false;

  /*
   * How much the anchor row grew, or shrank if negative, on its first real measurement.
   * That row usually sits across the top edge. commitRowSizes takes the change above
   * the viewport instead of moving the rows below. Set by applyRowSize and reset on commit.
   */
  double anchorFirstMeasurementDelta = 0.0;

  /*
   * The running correction. Only the operation below moves the offset, the rest is its target and bookkeeping.
   */

  /*
   * True when this frame made an offset the host should apply. Reset at the start of each
   * resolveScroll. It does not say whether a correction is running, operation does that.
   */
  bool offsetCorrected = false;

  /*
   * The row at the top of the viewport and how far we are scrolled into it.
   * Captured each frame and used to keep the visible content still while other rows are measured.
   */
  Anchor anchor = {};

  /*
   * The running offset correction, if any. Its id goes to the host as the commit token.
   * End corrections hold the end edge. Keeping content in place and scrollToRow hold a row by key.
   */
  std::optional<Operation> operation = std::nullopt;

  /*
   * Next operation id. Starts at 1 because 0 means no operation, a report from the host.
   */
  std::uint64_t nextCommitToken = 1;

  /*
   * Sizes the host measured ahead of time, by key, before the row was ever rendered.
   * An entry moves onto its row once the row exists and is then erased. This only holds
   * sizes that arrived early. They never feed the average, which counts real measurements only.
   * The host keeps this small. Measure a screen or two ahead, not the whole dataset.
   */
  std::unordered_map<std::string, Size> predictedSizes;

  /*
   * Save an early size for a key. Fine for keys not in the list yet, it waits for a later update.
   * To apply it to a row that already exists, use Virtualizer::applyPredictedRowSize.
   */
  void setPredictedSize(const std::string& key, Size size);

  /*
   * Keys never used as the anchor, like date pills or unread dividers whose keys come and go.
   * The anchor picks the nearest real content row instead. Empty means any row can be the anchor.
   */
  std::unordered_set<std::string> nonAnchorKeys;

  /*
   * Layout inputs from the previous offset pass. layoutRows skips the pass when nothing changed.
   * Window sizes are here because column widths depend on them.
   */
  double previousLayoutHeaderSize = -1.0;
  double previousLayoutWindowWidth = -1.0;
  double previousLayoutWindowHeight = -1.0;
  std::size_t previousLayoutNumberOfColumns = 0;
  bool previousLayoutHorizontal = false;

  /*
   * Size previously given to unmeasured rows. While it and the layout stay the same, the sizing
   * loop has nothing to do and is skipped. -1 forces the first pass.
   */
  double previousFallbackWidth = -1.0;
  double previousFallbackHeight = -1.0;

  /*
   * Bumped whenever row positions, sizes or the row list change. Snap offsets and sticky
   * headers are cached against it between frames. Starts at 1 because 0 means nothing cached.
   */
  std::uint64_t geometryVersion = 1;

  /*
   * Set on any insert, remove or reorder, since positions shift even without size changes.
   * Starts true so the first layout always runs.
   */
  bool rowStructureDirty = true;

  /*
   * First row an insert, remove or reorder touched. Rows before it kept their place, and the
   * next layout only reflows from here. Valid while rowStructureDirty is set.
   */
  std::size_t rowStructureDirtyFromIndex = 0;

  /*
   * Lowest row whose size changed outside layoutRows, or UNDEFINED_INDEX.
   * A size change only moves rows after it. The next layout starts here instead of at row 0.
   */
  std::size_t rowSizeDirtyFromIndex = UNDEFINED_INDEX;

  /*
   * Highest row whose size changed. Past it, once the new offsets match the old ones, the walk can stop.
   * Both ends are needed. Offsets can match by chance between two changed rows, and stopping
   * there would leave the later rows wrong.
   */
  std::size_t rowSizeDirtyToIndex = 0;

  /*
   * Mark a size change the next layout must reflow. A caller that reflows it itself must use
   * noteRowSizeSpan instead, or the work is done twice.
   */
  void markRowSizeDirty(std::size_t index) {
    if (index < rowSizeDirtyFromIndex) {
      rowSizeDirtyFromIndex = index;
    }
    noteRowSizeSpan(index);
  }

  /*
   * Widen the changed range without scheduling a layout reflow.
   * Used by batched sizes, where commitRowSizes reflows once and only needs to know where to stop.
   */
  void noteRowSizeSpan(std::size_t index) {
    if (index > rowSizeDirtyToIndex) {
      rowSizeDirtyToIndex = index;
    }
  }

  /*
   * Farthest any row reaches on the other axis. Tracked here because a wide row mid list would
   * be missed by the total size scan. Full passes rebuild it, partial passes only grow it.
   */
  double maxCrossAxisExtent = 0.0;

  /*
   * Offset from the previous host report. A user scroll only cancels a correction when this moves.
   * A stale userScrolled flag can't. Frames that carry our own offset write leave it alone,
   * because the host is not there yet.
   */
  double previousReportedOffset = 0.0;

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

  const Row& getRowAtIndex(std::size_t index) const;

  /*
   * These follow the scroll axis, using x and width when horizontal, y and height otherwise.
   */
  double getRowOffset(std::size_t index) const;
  double getRowSize(std::size_t index) const;
  double getOffset() const;
  double getWindowSize() const;

  std::size_t getRowCount() const;

  /*
   * The measured rows, the visible ones plus the overscan, or UNDEFINED_INDEX for both before
   * the first layout.
   */
  IndexRange getMeasuredRange() const;

  /*
   * Rows the rule calls viewable, or UNDEFINED_INDEX for both when none.
   */
  IndexRange getViewableIndices(const ViewableRule& rule = {}) const;

  /*
   * Whether the row's size can be trusted, either measured natively or predicted by the host.
   */
  bool hasTrustedSize(std::size_t index) const;

  void setEndReachedEnabled(bool enabled);
  void setStartReachedEnabled(bool enabled);

  /*
   * Ask to scroll the row at index into view. Handled on the next measurement. viewOffset
   * moves the resting offset that much further past the row's view position.
   */
  void scrollToRow(std::size_t index, double viewPosition = 0.0, double viewOffset = 0.0);

  /*
   * Ask to scroll to the end, following the bottom as rows get measured.
   */
  void scrollToEnd();

  /*
   * Ask to scroll to the very top, header included, holding the first row while rows above get measured.
   */
  void scrollToStart();

  /*
   * Ask to scroll to a content offset. It becomes a scroll to the row the offset falls in, and
   * the row is held while rows above it get measured.
   */
  void scrollToOffset(double offset);

  /*
   * Turn the scrollToRow command or prop into a request. The command runs once per call,
   * the prop runs when its value changes. A negative index means none, and the command wins.
   * commandViewOffset is the command's viewOffset, see scrollToRow. With SCROLL_TO_OFFSET_INDEX
   * it is a content offset instead, which lands as the row there and how far into it.
   */
  void requestScrollToRow(
    double commandIndex,
    double commandSequence,
    int propIndex,
    double commandViewPosition = 0.0,
    double commandViewOffset = 0.0);

  /*
   * Work out what to send to the scroll view this frame, given what it has now.
   */
  ContainerStateUpdate resolveStateUpdate(
    double previousOffsetX,
    double previousOffsetY,
    double previousContentWidth,
    double previousContentHeight) const;

  /*
   * Where the footer sits along the scroll axis, right after the content.
   */
  double getFooterStart(double footerSize) const;

  /*
   * Sorted offsets where scrolling can come to rest with a row aligned by snapAlignment.
   * Empty unless snapToItem is set. Cached until the geometry changes.
   * The returned reference belongs to the Container and is invalid after the next call.
   */
  const std::vector<double>& getSnapOffsets() const;

  /*
   * Index of the row with this key, or UNDEFINED_INDEX.
   */
  std::size_t indexOfKey(const std::string& key) const;

  /*
   * Whether a key can be the anchor. Empty keys and keys in nonAnchorKeys cannot.
   */
  bool isAnchorable(const std::string& key) const;

  /*
   * The anchor to hold still while sizes change. That is the running correction's row, or the
   * captured anchor when nothing runs, or null while an end correction owns the offset.
   * The key may be empty.
   */
  const Anchor* getCompensationAnchor() const;

  /*
   * Fire the measured range, viewable and scroll callbacks, but only when their values changed.
   */
  void dispatchObservers();

  /*
   * The offsets around the current one where a new frame would change nothing, see
   * OffsetBand. Call it after a frame, with the lock held. It is empty whenever the core
   * still has work that needs frames: a running or pending correction, a scroll command,
   * rows or sizes not laid out yet, the inverted opening pin, and scroll, viewable or
   * sticky listeners, which need every offset. Otherwise the band ends where a row enters
   * or leaves the measured range, where an edge callback or the inverted bottom pin would flip,
   * and at both ends of the scroll range. Each of those ends is pulled in by a margin. A
   * host rounding its offset still sends the frame that crosses it.
   */
  OffsetBand computeOffsetBand() const;

  /*
   * Whether a correction or a scroll command still runs and needs more frames to land.
   */
  bool hasPendingCommand() const;

private:
  /*
   * Cached snap offsets and the inputs they came from. A version of 0 means nothing cached.
   */
  mutable std::vector<double> snapOffsetsCache_;
  mutable std::uint64_t snapCacheVersion_ = 0;
  mutable bool snapCacheSnapToItem_ = false;
  mutable SnapAlignment snapCacheAlignment_ = SnapAlignment::Start;
  mutable double snapCacheWindowSize_ = -1.0;
  mutable double snapCacheTotalSize_ = -1.0;
  mutable bool snapCacheHorizontal_ = false;

  /*
   * Last measured range sent. Only changes are sent.
   */
  std::size_t previousMeasuredLow_ = UNDEFINED_INDEX;
  std::size_t previousMeasuredHigh_ = UNDEFINED_INDEX;

  /*
   * Last viewable ranges sent, two indices per rule. Only changes are sent.
   */
  std::vector<std::size_t> previousViewableRanges_;

  /*
   * Whether we were already at an edge. Reached callbacks fire once on arrival.
   * They reset when the row count changes, like after loading a page.
   */
  bool previousReachedStart_ = false;
  bool previousReachedEnd_ = false;
  std::size_t previousReachedRowCount_ = UNDEFINED_INDEX;

  /*
   * Last offset sent to onScroll. Only changes are sent.
   */
  double previousOffsetX_ = 0.0;
  double previousOffsetY_ = 0.0;
  bool previousOffsetValid_ = false;

  /*
   * Last scrollToRow command number handled. The same index can still scroll again.
   */
  double previousScrollToRowSequence_ = 0.0;

  /*
   * Last containerOffsetIndex prop handled. The prop only scrolls when its value changes.
   */
  int previousScrollToRowProp_ = -1;
};

}
