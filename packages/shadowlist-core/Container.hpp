#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>
#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Operation.hpp>
#include <shadowlist-core/Revision.hpp>

namespace azimgd::shadowlist {

/*
 * The pinned sticky section header and how far to shift it from its resting position.
 * index is UNDEFINED_INDEX when none is pinned.
 */
struct StickyHeader {
  std::size_t index = UNDEFINED_INDEX;
  double translation = 0.0;
};

static constexpr std::size_t RevisionCountFirst = 0;

static constexpr std::size_t RevisionStatusIdle = 0;
static constexpr std::size_t RevisionStatusPending = 1;

/*
 * Resolved values to publish to the scroll view for one frame.
 */
struct ContainerStateUpdate {
  // Whether anything changed and new state should be published.
  bool changed = false;

  /*
   * Whether to move the scroll view to (containerOffsetX, containerOffsetY). False means
   * leave the offset alone so we don't fight the user's scrolling.
   */
  bool applyContainerOffset = false;

  double containerOffsetX = 0.0;
  double containerOffsetY = 0.0;
  double totalContainerWidth = 0.0;
  double totalContainerHeight = 0.0;

  /*
   * Id of the operation that moved the offset this frame. The host echoes it back so the
   * core can recognise its own write. 0 when no offset was applied.
   */
  std::uint64_t commitToken = 0;
};

class Container {
public:
  // Size (width, height) used for elements not yet measured.
  std::pair<double, double> estimatedElementSize = DEFAULT_ESTIMATED_ELEMENT_SIZE;

  // Fired when scrolled near the end of the list.
  std::function<void()> onEndReachedCallback;

  // Fired when scrolled near the start of the list.
  std::function<void()> onStartReachedCallback;

  // Fired with (startIndex, endIndex) when the visible element range changes.
  std::function<void(std::size_t, std::size_t)> onVisibleIndicesChangeCallback;

  // Fired with (containerOffsetX, containerOffsetY) when the scroll offset changes.
  std::function<void(double, double)> onScrollCallback;

  /*
   * Fired with (startIndex, endIndex) when the strictly-viewable range changes: only
   * elements inside the viewport, subject to viewablePercentThreshold.
   */
  std::function<void(std::size_t, std::size_t)> onViewableIndicesChangeCallback;

  // Whether the start/end reached callbacks may fire.
  bool endReachedEnabled = true;
  bool startReachedEnabled = true;

  // Current measurement revision and its index/status.
  Revision revision = {};
  std::size_t revisionCount = RevisionCountFirst;
  std::size_t revisionStatus = RevisionStatusIdle;

  // List order: normal (top to bottom) or inverted (bottom to top).
  bool inverted = false;

  // Scroll axis: vertical (false) or horizontal (true).
  bool horizontal = false;

  // Number of columns for multi-column layout.
  std::size_t columns = 1;

  /*
   * Overscan in viewport units: how far beyond the visible window to measure and mount
   * elements on each side, as a multiple of the window size. 1.0 keeps one viewport above
   * and one below; 0 measures only the visible window.
   */
  double overscan = 1.0;

  /*
   * Snap the resting scroll position to an element edge. snapAlignment selects the
   * edge aligned to the viewport (0 = start, 1 = center, 2 = end). See getSnapOffsets.
   */
  bool snapToItem = false;
  int snapAlignment = 0;

  /*
   * Distance from an edge (as a fraction of window size) at which
   * onStartReached / onEndReached fire. 1.0 means within one windowful.
   */
  double startReachedThreshold = 1.0;
  double endReachedThreshold = 1.0;

  /*
   * Fraction (0..1) of an element inside the viewport for it to count as
   * viewable (see getViewableIndices). 0 means any overlap counts.
   */
  double viewablePercentThreshold = 0.0;

  /*
   * Size of the header (and empty) template along the scroll axis.
   * Elements are positioned after it; the total size includes it.
   */
  double headerSize = 0.0;

  /*
   * Size of the footer template along the scroll axis, included in the total size
   */
  double footerSize = 0.0;

  /*
   * Pin the header/footer template to the viewport edge instead of scrolling it
   * with content. Reserved space is unchanged, so it settles back at the extremes.
   * See getStickyHeaderOffset / getStickyFooterOffset.
   */
  bool stickyHeader = false;
  bool stickyFooter = false;

  /*
   * Element indices that are sticky section headers (ascending), set each frame.
   * Drives resolveStickyHeader; empty for a plain list.
   */
  std::vector<std::size_t> stickyIndices;

  /*
   * Last drag-event sequence emitted to JS, used to fire each onDrag* event exactly
   * once. -1 means none emitted.
   */
  double lastDragEventSequence = -1.0;

  /*
   * Intent inputs.
   *
   * These request a correction; they are not correction state. resolveScroll reads each,
   * creates the matching `operation`, and clears the request. They live across frames as a
   * request inbox: the request can arrive outside a frame (e.g. a command in adopt()), so
   * the operation is created once the revision is measured.
   */

  // Pending scrollToIndex target, or UNDEFINED_INDEX when inactive.
  std::size_t scrollToIndexTarget = UNDEFINED_INDEX;

  /*
   * Active while a scrollToEnd is converging on the bottom: retargets maxOffset
   * every frame as off-screen rows are measured, so it lands on the true end of a
   * variable-height list. Cleared once the view reaches the bottom and the total
   * stops changing. pendingScrollToEndLastTotal holds the previous frame's total,
   * tracked every frame so "stopped changing" is detectable on the first frame.
   */
  bool pendingScrollToEnd = false;
  double pendingScrollToEndLastTotal = -1.0;

  /*
   * Whether an inverted list has settled at the bottom. While false it sticks to
   * the bottom; once reached, the maintain-visible-content-position anchor takes over.
   */
  bool invertedInitialized = false;

  /*
   * The single in-flight correction (anchor plus operation).
   *
   * Scroll correction has exactly one owner: the `operation` below. Everything else in
   * this group is either its content-space target (`anchor`) or its derived bookkeeping.
   * The intent inputs above (scrollToIndexTarget / pendingScrollToEnd / invertedInitialized)
   * request an operation; they are not correction state.
   */

  /*
   * Per-frame output bit: true when this frame produced an offset the host should apply
   * (an operation drove it, an MVCP measurement nudge moved it, or the layout pass
   * reasserted it after a header reflow). Reset at the top of every resolveScroll and read
   * once by resolveStateUpdate to set applyContainerOffset. This is not the correction's
   * lifecycle flag; whether a correction is in flight is `operation.has_value()`.
   */
  bool containerOffsetCorrected = false;

  /*
   * The scroll position in content space (key + sub-offset): the single source of
   * truth for the element at the viewport edge this frame and how far we are scrolled
   * into it. Set each frame by captureAnchor (or overridden by FrameInput::suppliedAnchor)
   * and read by resolveScroll / updateElementAtIndex to keep visible content fixed while
   * off-screen elements are measured.
   */
  Anchor anchor = {};

  /*
   * The one in-flight offset correction, or none. A typed, cancellable operation whose
   * `id` is the commit token published to the host (see Operation). A fixed-offset
   * correction (ScrollToEnd / BottomPin / ShrinkClamp) carries an EndEdge anchor; an
   * MVCP / scrollToIndex correction carries an Element anchor that tracks its key as
   * nearby elements are measured.
   */
  std::optional<Operation> operation = std::nullopt;

  /*
   * Monotonic source of operation ids / commit tokens. Starts at 1 so 0 always means
   * "no operation" / "host-originated report".
   */
  std::uint64_t nextOperationId = 1;

  /*
   * Keys that must never be captured as the MVCP anchor: decoration rows (date pills,
   * unread dividers, reaction strips, padding) whose identity churns independently of
   * content. captureAnchor skips them and anchors to the nearest stable content row, so a
   * key change on decoration cannot perturb the maintained scroll position. Set each frame
   * from FrameInput::nonAnchorableKeys; empty means every row is anchorable. An explicit
   * FrameInput::suppliedAnchor is honoured even if its key is in here (the host chose it).
   */
  std::unordered_set<std::string> nonAnchorableKeys;

  /*
   * Header reserved size when the anchor was captured. A header-size change between
   * capture and reflow is not a content scroll, so MVCP subtracts
   * (headerSize - anchorHeaderSize); without it the list opens scrolled past the header.
   */
  double anchorHeaderSize = 0.0;

  /*
   * Scroll offset reported on the previous frame. A user scroll only counts as a
   * takeover when this actually changes, so a stale userScrolled flag on an unmoved
   * offset can't cancel an in-flight correction.
   */
  double lastReportedOffset = 0.0;

  /*
   * Serializes all access to one Container, which is genuinely shared across threads: in
   * Fabric a single core instance is carried forward by a list's committed shadow-node
   * clones, so adopt() / layout() / measurement feedback can run on overlapping commit
   * threads against the same Container. This mutex is essential, not defensive.
   *
   * It is RECURSIVE because:
   *   1. Every public Virtualizer entry point locks it, and several call one another
   *      (e.g. update -> measure -> recomputeTotalSize).
   *   2. An integration may hold it across a whole sequence of those calls plus the
   *      Container getters to make a frame atomic (Fabric's adopt()/layout() do this),
   *      and the inner per-method locks then re-enter on the same thread.
   *
   * Contract: hold coreMutex for any access to a shared Container. The public Virtualizer
   * methods lock it themselves, so a standalone driver (tests) need not lock explicitly.
   * The low-level getters/setters below do NOT lock; call them from inside a Virtualizer
   * entry point or while holding coreMutex.
   */
  std::recursive_mutex coreMutex;

  void startRevision();
  void endRevision();

  const Element& getElementAtIndex(std::size_t index) const;

  /*
   * Offset/size getters and setters are orientation aware: horizontal reads/writes
   * offsetX and width, vertical reads/writes offsetY and height.
   */
  double getElementOffset(std::size_t index) const;
  double getElementSize(std::size_t index) const;
  void setElementOffset(std::size_t index, double offset);
  double getContainerOffset() const;
  double getWindowContainerSize() const;

  std::size_t getElementsSize() const;

  void setWindowContainerHeight(double height);
  void setWindowContainerWidth(double width);
  void setContainerOffsetY(double offsetY);
  void setContainerOffsetX(double offsetX);

  std::string getDebugRepresentation() const;

  /*
   * Visible index range, or (UNDEFINED_INDEX, UNDEFINED_INDEX) if uninitialized.
   */
  std::pair<std::size_t, std::size_t> getVisibleIndices() const;

  /*
   * Strictly-viewable index range: elements whose visible fraction is at least
   * viewablePercentThreshold. Orientation aware (inverted returns start > end), or
   * (UNDEFINED_INDEX, UNDEFINED_INDEX) when nothing is viewable.
   */
  std::pair<std::size_t, std::size_t> getViewableIndices() const;

  void setEndReachedEnabled(bool enabled);
  void setStartReachedEnabled(bool enabled);

  /*
   * Request scrolling so the element at the given index sits at the start of the
   * viewport. The request is resolved on the next measurement.
   */
  void scrollToIndex(std::size_t index);

  /*
   * Request scrolling to the very end, retargeting the bottom as off-screen rows
   * are measured so it converges on the true end of a variable-height list.
   */
  void scrollToEnd();

  /*
   * Resolve a scrollToIndex request from an imperative command and a declarative
   * prop index. The command fires once per invocation (tracked by a monotonic
   * sequence); the prop fires when its value changes. Negative index means inactive;
   * the command takes precedence.
   */
  void requestScrollToIndex(double commandIndex, double commandSequence, int propIndex);

  /*
   * Resolve the current frame into values to publish to the scroll view. prev* are
   * the values currently held (reported scroll offset and last published size).
   */
  ContainerStateUpdate resolveStateUpdate(
    double prevContainerOffsetX,
    double prevContainerOffsetY,
    double prevTotalContainerWidth,
    double prevTotalContainerHeight) const;

  /*
   * Resting offset of the footer along the scroll axis (placed after the content).
   */
  double getFooterOffset(double footerSize) const;

  /*
   * Viewport-pinned ("sticky") offsets: the header at the viewport start, the
   * footer at the viewport end. Each falls back to its resting offset when its
   * sticky flag is unset.
   */
  double getStickyHeaderOffset() const;
  double getStickyFooterOffset(double footerSize) const;

  /*
   * Resting scroll offsets (ascending, along the scroll axis) that align an element
   * to the viewport edge selected by snapAlignment, each clamped to [0, maxOffset]
   * and deduplicated. Empty when snapToItem is unset. The integration snaps to the
   * nearest of these on scroll end.
   */
  std::vector<double> getSnapOffsets() const;

  /*
   * Resolve which sticky section header (from stickyIndices) is pinned at the
   * current scroll offset and how far to translate it. The active header is the
   * last whose resting offset is at/above the viewport start; it pins there and is
   * pushed up by the next sticky header. Returns {UNDEFINED_INDEX, 0} when nothing
   * is pinned. Not pinned for inverted lists.
   */
  StickyHeader resolveStickyHeader() const;

  /*
   * Find the index of the element with the given key, or UNDEFINED_INDEX if absent
   */
  std::size_t findElementIndexByKey(const std::string& key) const;

  /*
   * Whether a key may serve as the MVCP anchor. False only for keys in nonAnchorableKeys
   * (decoration rows). Empty key is never anchorable. See captureAnchor / nonAnchorableKeys.
   */
  bool isAnchorable(const std::string& key) const;

  /*
   * Fire the visible-indices-change and scroll callbacks if their values changed
   * since the last revision (deduplication lives here).
   */
  void dispatchObservers();

private:
  /*
   * Previously dispatched visible range, used to deduplicate onVisibleIndicesChange
   */
  std::size_t prevVisibleStartIndex = UNDEFINED_INDEX;
  std::size_t prevVisibleEndIndex = UNDEFINED_INDEX;

  /*
   * Previously dispatched viewable range, used to deduplicate onViewableIndicesChange
   */
  std::size_t prevViewableStartIndex = UNDEFINED_INDEX;
  std::size_t prevViewableEndIndex = UNDEFINED_INDEX;

  /*
   * Whether the previous revision was at the start/end edge, so reached callbacks
   * fire once on arrival instead of every frame within the threshold.
   * prevReachedElementsSize resets them when the data set changes (pagination).
   */
  bool prevReachedStart = false;
  bool prevReachedEnd = false;
  std::size_t prevReachedElementsSize = UNDEFINED_INDEX;

  /*
   * Previously dispatched scroll offset, used to deduplicate onScroll
   */
  double prevContainerOffsetX = 0.0;
  double prevContainerOffsetY = 0.0;
  bool prevContainerOffsetValid = false;

  /*
   * Last imperative scrollToIndex sequence we acted on, so the command fires once
   * per invocation (a repeated index still rescrolls because the sequence changes)
   */
  double prevScrollToIndexSequence = 0.0;

  /*
   * Last declarative containerOffsetIndex prop we acted on, so the prop fires
   * only when its value changes
   */
  int prevScrollToIndexProp = -1;
};

}
