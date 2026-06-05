#pragma once

#include <functional>
#include <mutex>
#include <vector>
#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Revision.hpp>

namespace azimgd::shadowlist {

/*
 * Which sticky section header is currently pinned to the viewport edge and how far
 * to translate it from its resting position along the scroll axis. index is
 * UNDEFINED_INDEX when none is pinned (scrolled above the first sticky header).
 */
struct StickyHeader {
  std::size_t index = UNDEFINED_INDEX;
  double translation = 0.0;
};

static constexpr std::size_t RevisionCountFirst = 0;

static constexpr std::size_t RevisionStatusIdle = 0;
static constexpr std::size_t RevisionStatusPending = 1;

/*
 * Result of resolving a frame into the values an integration should publish to
 * its platform scroll view. Keeps the "what changed / should we move the view"
 * decision in the core so every integration applies it the same way.
 */
struct ContainerStateUpdate {
  /*
   * Whether anything changed and the integration should publish new state
   */
  bool changed = false;

  /*
   * Whether the integration should move the scroll view to (containerOffsetX,
   * containerOffsetY). False means the offset is the view's own position and must
   * be left alone (so we never fight the user's scrolling).
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
   * Callback to be executed when the strictly-visible (viewable) element range
   * changes. Unlike onVisibleIndicesChangeCallback (the mounted window, which
   * includes an off-screen buffer) this reports only elements actually inside the
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
   * How close to an edge (as a fraction of the window size) the scroll offset must
   * be before onStartReached / onEndReached fire. 1.0 reproduces the default of
   * "within one windowful of the edge"; 0.5 is half a window, etc.
   */
  double startReachedThreshold = 1.0;
  double endReachedThreshold = 1.0;

  /*
   * Fraction (0..1) of an element that must be inside the viewport for it to count
   * as viewable (see getViewableIndices). 0 means any overlap counts.
   */
  double viewablePercentThreshold = 0.0;

  /*
   * Size of the header (and empty) template along the scroll axis
   * Elements are positioned after the header and the total size includes it
   */
  double headerSize = 0.0;

  /*
   * Size of the footer template along the scroll axis, included in the total size
   */
  double footerSize = 0.0;

  /*
   * When set, the header/footer template is pinned to the viewport edge instead of
   * scrolling with the content: the header stays at the viewport start and the
   * footer at the viewport end. The reserved header/footer space in the content is
   * unchanged, so the template settles back onto it at the scroll extremes. See
   * getStickyHeaderOffset / getStickyFooterOffset.
   */
  bool stickyHeader = false;
  bool stickyFooter = false;

  /*
   * Element indices that are sticky section headers (ascending), set each frame
   * from FrameInput. Drives resolveStickyHeader; empty for a plain list.
   */
  std::vector<std::size_t> stickyIndices;

  /*
   * Pending scrollToIndex target, or UNDEFINED_INDEX when inactive
   */
  std::size_t scrollToIndexTarget = UNDEFINED_INDEX;

  /*
   * Active while a scrollToEnd is converging on the bottom. Unlike a one-shot jump
   * to the current (estimated) content size, this re-targets maxOffset every frame
   * as off-screen rows are measured and the total grows, so it lands on the true
   * end of a variable-height list. Cleared once the view has reached the bottom and
   * the total has stopped changing (see resolveScroll). pendingScrollToEndLastTotal
   * holds the previous frame's total - tracked every frame, not just while the flag
   * is set - so "stopped changing" can be detected on the very first frame too.
   */
  bool pendingScrollToEnd = false;
  double pendingScrollToEndLastTotal = -1.0;

  /*
   * Whether an inverted list has settled at the bottom. While false the list
   * sticks to the bottom (initial render / empty -> populated); once the view
   * actually reaches the bottom the maintain-visible-content-position anchor takes over
   */
  bool invertedInitialized = false;

  /*
   * Set by the scroll resolution each frame: true when the core wants the
   * integration to apply containerOffset to the scroll view (scrollToIndex,
   * inverted bottom anchor, or a maintain-visible-content-position shift). When
   * false the integration must leave the scroll position to the user.
   */
  bool containerOffsetCorrected = false;

  /*
   * A scroll target the core actively drives the view toward until the view
   * reports it has arrived. This keeps a correction alive across the redundant
   * re-commits that the visible-indices event triggers, so a stale offset on a
   * racing frame cannot cancel it.
   */
  double pendingScrollOffset = 0.0;
  bool pendingScroll = false;

  /*
   * When the pending correction is a maintain-visible-content-position shift
   * (e.g. prepend) the target is the anchor element rather than a fixed offset,
   * so it tracks the anchor as nearby elements are measured and resized.
   */
  std::string pendingAnchorKey = "";
  double pendingAnchorDelta = 0.0;
  bool pendingAnchorActive = false;

  /*
   * The element at the viewport edge this frame and how far we are scrolled into
   * it. Used to keep the visible content fixed while off-screen elements are
   * measured (their real size differs from the estimate).
   */
  std::string anchorKey = "";
  double anchorDelta = 0.0;

  /*
   * The scroll offset the platform reported on the previous frame (along the
   * scroll axis). A user scroll only counts as the user taking over when this
   * actually changes: the userScrolled flag latches on the platform, so a stale
   * one on an unmoved offset (e.g. a prepend committed while the user is paused at
   * the top) must not cancel an in-flight correction. See Virtualizer::update.
   */
  double lastReportedOffset = 0.0;

  /*
   * Serializes access to a single container. An integration may drive the update
   * pass and the layout/measurement-feedback pass from different threads that can
   * overlap, so every entry point that mutates or reads the revision takes this.
   * Recursive because a locked entry point may re-enter another one.
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
   * Strictly-viewable index range: elements inside the viewport whose visible
   * fraction is at least viewablePercentThreshold. Orientation aware (inverted
   * returns start > end, matching getVisibleIndices), or (UNDEFINED_INDEX,
   * UNDEFINED_INDEX) when nothing is viewable.
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
   * Request scrolling to the very end of the content. The correction drives toward
   * the bottom, re-targeting it as off-screen rows are measured, so it converges on
   * the true end of a variable-height list instead of a stale estimate.
   */
  void scrollToEnd();

  /*
   * Resolve a scrollToIndex request from an imperative command and a declarative
   * prop index. The imperative command fires once per invocation, tracked by a
   * monotonic nonce so repeating the same index re-scrolls; the declarative prop
   * fires whenever its value changes. Both use a negative index to mean
   * "inactive", and the imperative command takes precedence.
   */
  void requestScrollToIndex(double commandIndex, double commandNonce, int propIndex);

  /*
   * Resolve the current frame into the values an integration should publish to its
   * platform scroll view. prev* are the values the platform currently holds (the
   * view's reported scroll offset and the last published content size).
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
   * Viewport-pinned ("sticky") offsets along the scroll axis: the header tracks the
   * scroll offset to stay at the viewport start, the footer at the viewport end.
   * Each falls back to its resting offset when the corresponding sticky flag is
   * unset.
   *
   * Kept here so the pin geometry has a single tested definition. The Fabric
   * integration applies the pin natively in the scroll callback (the commit cycle
   * is too slow to pin smoothly), but a core-driven integration - e.g. WASM - can
   * use these directly.
   */
  double getStickyHeaderOffset() const;
  double getStickyFooterOffset(double footerSize) const;

  /*
   * Resolve which sticky section header (from stickyIndices) is pinned at the
   * current scroll offset and how far to translate it from its resting position
   * along the scroll axis. The active header is the last whose resting offset is
   * at/above the viewport start; it pins to the viewport start and is pushed up by
   * the next sticky header once that header's top crosses into the active header's
   * own size. Returns {UNDEFINED_INDEX, 0} when nothing is pinned.
   *
   * The per-frame pin is applied natively (on the UI thread, like the single sticky
   * header/footer template) because the commit cycle is too slow to track scrolling
   * smoothly; the integrations mirror this exact geometry. Kept here so it has one
   * tested definition and a core-driven integration (WASM) can use it directly.
   * Not pinned for inverted lists (an exotic combination); returns none there.
   */
  StickyHeader resolveStickyHeader() const;

  /*
   * Find the index of the element with the given key, or UNDEFINED_INDEX if absent
   */
  std::size_t findElementIndexByKey(const std::string& key) const;

  /*
   * Fire the visible-indices-change and scroll callbacks if their values changed
   * since the last revision (deduplication lives here so integrations don't repeat it)
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
   * Whether the previous revision was already at the start/end edge, so the
   * reached callbacks fire once on arrival (a false->true transition) instead of
   * every frame the offset stays within the threshold. prevReachedElementsSize
   * re-arms them when the data set changes (pagination), so reaching a newly
   * appended/prepended edge counts as a fresh arrival even within the band.
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
