#pragma once

#include <functional>
#include <mutex>
#include <vector>
#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Revision.hpp>

namespace azimgd::shadowlist {

/*
 * The pinned sticky section header and how far to translate it from its resting
 * position. index is UNDEFINED_INDEX when none is pinned.
 */
struct StickyHeader {
  std::size_t index = UNDEFINED_INDEX;
  double translation = 0.0;
};

static constexpr std::size_t RevisionCountFirst = 0;

static constexpr std::size_t RevisionStatusIdle = 0;
static constexpr std::size_t RevisionStatusPending = 1;

/*
 * Resolved values to publish to the scroll view for a frame.
 */
struct ContainerStateUpdate {
  /*
   * Whether anything changed and new state should be published
   */
  bool changed = false;

  /*
   * Whether to move the scroll view to (containerOffsetX, containerOffsetY).
   * False means leave the offset alone (don't fight the user's scrolling).
   */
  bool applyContainerOffset = false;

  double containerOffsetX = 0.0;
  double containerOffsetY = 0.0;
  double totalContainerWidth = 0.0;
  double totalContainerHeight = 0.0;
};

class Container {
public:
  /*
   * Estimated element size for unmeasured elements (width, height)
   */
  std::pair<double, double> estimatedElementSize = DEFAULT_ESTIMATED_ELEMENT_SIZE;

  /*
   * Callback to be executed when scrolled near the end of the list
   */
  std::function<void()> onEndReachedCallback;

  /*
   * Callback to be executed when scrolled near the start of the list
   */
  std::function<void()> onStartReachedCallback;

  /*
   * Callback to be executed when the visible element range changes
   * Arguments are (startIndex, endIndex) of the visible range
   */
  std::function<void(std::size_t, std::size_t)> onVisibleIndicesChangeCallback;

  /*
   * Callback to be executed when the scroll offset changes
   * Arguments are (containerOffsetX, containerOffsetY)
   */
  std::function<void(double, double)> onScrollCallback;

  /*
   * Callback when the strictly-viewable range changes: only elements inside the
   * viewport, subject to viewablePercentThreshold. Arguments are (startIndex, endIndex).
   */
  std::function<void(std::size_t, std::size_t)> onViewableIndicesChangeCallback;

  /*
   * Enable/disable end reached callback
   */
  bool endReachedEnabled = true;

  /*
   * Enable/disable start reached callback
   */
  bool startReachedEnabled = true;

  /*
   * Current measurement revision
   */
  Revision revision = {};

  /*
   * Current active revision index
   */
  std::size_t revisionCount = RevisionCountFirst;

  /*
   * Current active revision status
   */
  std::size_t revisionStatus = RevisionStatusIdle;

  /*
   * Default / Inverted order of the list
   */
  bool inverted = false;
  
  /*
   * Horizontal / Vertical position of the list
   */
  bool horizontal = false;

  /*
   * Number of columns for multi-column layout
   */
  std::size_t columns = 1;

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
   * Last drag-event nonce emitted to JS, used to fire each onDrag* event exactly
   * once. -1 means none emitted.
   */
  double lastDragEventNonce = -1.0;

  /*
   * Pending scrollToIndex target, or UNDEFINED_INDEX when inactive
   */
  std::size_t scrollToIndexTarget = UNDEFINED_INDEX;

  /*
   * Active while a scrollToEnd is converging on the bottom: re-targets maxOffset
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
   * True when the core wants containerOffset applied to the scroll view this frame
   * (scrollToIndex, inverted bottom anchor, or an MVCP shift); false means leave
   * the scroll position to the user.
   */
  bool containerOffsetCorrected = false;

  /*
   * A scroll target the core drives the view toward until it arrives. Keeps a
   * correction alive across redundant re-commits so a stale racing offset can't
   * cancel it.
   */
  double pendingScrollOffset = 0.0;
  bool pendingScroll = false;

  /*
   * When the pending correction is an MVCP shift (e.g. prepend) the target is the
   * anchor element, so it tracks the anchor as nearby elements are measured.
   */
  std::string pendingAnchorKey = "";
  double pendingAnchorDelta = 0.0;
  bool pendingAnchorActive = false;

  /*
   * The element at the viewport edge this frame and how far we are scrolled into
   * it. Keeps visible content fixed while off-screen elements are measured.
   */
  std::string anchorKey = "";
  double anchorDelta = 0.0;

  /*
   * Header reserved size when the anchor was captured. A header-size change between
   * capture and re-flow is not a content scroll, so MVCP subtracts
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
   * Serializes access to a single container; the update and measurement passes may
   * run on overlapping threads. Recursive because a locked entry point may re-enter.
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
   * Request scrolling to the very end, re-targeting the bottom as off-screen rows
   * are measured so it converges on the true end of a variable-height list.
   */
  void scrollToEnd();

  /*
   * Resolve a scrollToIndex request from an imperative command and a declarative
   * prop index. The command fires once per invocation (tracked by a monotonic
   * nonce); the prop fires when its value changes. Negative index means inactive;
   * the command takes precedence.
   */
  void requestScrollToIndex(double commandIndex, double commandNonce, int propIndex);

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
   * prevReachedElementsSize re-arms them when the data set changes (pagination).
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
   * Last imperative scrollToIndex nonce we acted on, so the command fires once
   * per invocation (a repeated index still re-scrolls because the nonce changes)
   */
  double prevScrollToIndexNonce = 0.0;

  /*
   * Last declarative containerOffsetIndex prop we acted on, so the prop fires
   * only when its value changes
   */
  int prevScrollToIndexProp = -1;
};

}
