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

  // Current measurement revision and its index.
  Revision revision = {};
  std::size_t revisionCount = REVISION_COUNT_FIRST;

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
   * Element indices that are sticky section headers (ascending), set each frame. The
   * integration publishes their geometry for the host to pin; empty for a plain list.
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
   * tracked every frame so "stopped changing" is detectable on the first frame. Also raised
   * by Virtualizer::update when rows are appended below the newest row of an inverted list
   * resting at its bottom, so the list follows them.
   */
  bool pendingScrollToEnd = false;
  double pendingScrollToEndLastTotal = -1.0;

  /*
   * Whether an inverted list has settled at the bottom. While false it sticks to
   * the bottom; once reached, the maintain-visible-content-position anchor takes over.
   */
  bool invertedInitialized = false;

  /*
   * Set when the user scrolls an inverted list up off the bottom. While set, the bottom pin
   * (resolveScroll's inverted bottom anchor) stands down and plain anchoring holds the view
   * still. Without it a last row taller than the viewport, or one left as the only
   * anchorable row, is re-pinned to the bottom on every frame of the drag: the finger fights
   * the pin and the view snaps back. Cleared once the offset moves back to the bottom
   * (within INVERTED_FOLLOW_BAND). It has to be a move, because content shrinking underneath
   * a resting reader brings the bottom to them, and that must not re-arm the pin on their
   * behalf. Maintained in Virtualizer::update, before the frame reconciles or measures anything.
   */
  bool invertedBottomReleased = false;

  /*
   * True for the frame being resolved when a gesture is driving the offset: a finger is
   * down, momentum is running, or this frame's report moved under a user scroll. Set by
   * Virtualizer::update from the host's phase and userScrolled flag. While set, the
   * inverted bottom pin stands down even inside INVERTED_FOLLOW_BAND: re-pinning under a
   * finger snaps the content back every frame and the drag fights the pin until it has
   * travelled the whole band. Once the finger lifts inside the band the pin re-engages
   * and returns the view to the bottom.
   */
  bool gestureActive = false;

  /*
   * Id of the last operation that was in flight on a gesture frame, or 0. While a gesture
   * drives, a host places a correction onto its live offset, which has moved on from the report
   * the correction was computed against; a report carrying that travel can be superseded by the
   * echo before the core adopts it. The echo of such an operation confirms it even once the
   * motion has stopped, instead of the core driving the view on to an absolute target that
   * leaves that travel out. Maintained by Virtualizer::update.
   */
  std::uint64_t gestureOperationId = 0;

  /*
   * Set while an inverted list is still settling on the bottom it opened at: from the first
   * bottom pin until the first gesture, scroll command or change of keys. While it is set and
   * the view rests at the bottom, a remeasure keeps the view on the true bottom rather than on
   * the row at the viewport top. Rows mounted after the pin converges are commonly measured or
   * predicted smaller above that row and larger below it; holding the row would park the view
   * short of the bottom, where appended rows are no longer followed. Once the reader has
   * touched the list, plain anchoring applies again, which a screen that stops following on
   * purpose relies on.
   */
  bool invertedOpeningPin = false;

  /*
   * Whether the inverted list rested at its bottom when the frame being resolved began, judged
   * against the geometry the reader was looking at. Set by Virtualizer::update.
   */
  bool restingAtInvertedBottom = false;

  /*
   * How much the row the measurement compensation anchors to grew (negative: shrank) on its
   * first native measurement since the last commitElementSizes. That row usually straddles
   * the viewport start, and its estimate was never what the reader saw below it, so
   * commitElementSizes absorbs this change above the viewport instead of moving the rows
   * below. Recorded by applyElementSize, consumed and reset by commitElementSizes.
   */
  double anchorFirstMeasurementDelta = 0.0;

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
   * into it. Set each frame by captureAnchor and read by resolveScroll / updateElementAtIndex to keep visible content fixed while
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
   * Ahead-of-time sizes by key: what a row will measure to, supplied by the host before
   * the row has ever been rendered (on Fabric, by measuring its text through the same
   * TextLayoutManager the real layout pass will use, so the later native measure is a
   * cache hit rather than a second measurement).
   *
   * This is a staging area, not the source of truth. A prediction is consumed once its key
   * exists as an element: Virtualizer::consumePredictions stamps them on the next update(),
   * and Virtualizer::applyPredictedElementSize stamps a row that already exists. After that
   * the size lives on the Element and the entry here is erased. So the map holds only
   * predictions that arrived before their row did, which is the normal case when the host
   * measures a screen or two ahead of the retention band.
   *
   * Predictions never feed the frozen average (Revision::measuredReal*): that average
   * exists to size rows nobody has any information about, and must keep describing real
   * native measurements only.
   *
   * Bounded by the host. The core never grows this on its own, and a host that stages
   * predictions for an entire 100k-row dataset has simply moved the cost it was trying to
   * avoid: measure a window ahead of the band, not the world.
   */
  std::unordered_map<std::string, Size> predictedSizes;

  /*
   * Stage an ahead-of-time size for `key`. Safe to call for a key that is not in the list
   * (and may never be): it simply waits to be consumed by a later update().
   *
   * This only stages. To apply a prediction to a row that already exists, and reflow the
   * geometry behind it, use Virtualizer::applyPredictedElementSize, which consumes the
   * staged entry as part of the same call.
   */
  void setPredictedSize(const std::string& key, Size size);

  /*
   * Keys that must never be captured as the MVCP anchor: decoration rows (date pills,
   * unread dividers, reaction strips, padding) whose identity churns independently of
   * content. captureAnchor skips them and anchors to the nearest stable content row, so a
   * key change on decoration cannot perturb the maintained scroll position. Set each frame
   * from FrameInput::nonAnchorableKeys; empty means every row is anchorable.
   */
  std::unordered_set<std::string> nonAnchorableKeys;

  /*
   * Layout inputs as of the last offset recompute, so layoutElements can skip that
   * O(elements) pass when none of them changed. Window sizes are included because
   * multi-column track size (and every cross-axis offset) derives from them.
   */
  double lastLayoutHeaderSize = -1.0;
  double lastLayoutFooterSize = -1.0;
  double lastLayoutWindowWidth = -1.0;
  double lastLayoutWindowHeight = -1.0;
  std::size_t lastLayoutColumns = 0;
  bool lastLayoutHorizontal = false;

  /*
   * Fallback dimensions handed to unmeasured elements by the last layoutElements pass.
   * While these (and the layout parameters) are unchanged every unmeasured element
   * already carries exactly these dimensions, so the O(elements) sizing loop is a
   * guaranteed no-op and is skipped. -1 forces the first pass to run.
   */
  double lastFallbackWidth = -1.0;
  double lastFallbackHeight = -1.0;

  /*
   * Bumped whenever element offsets/sizes are recomputed or the element list changes.
   * Lets an integration cache derived geometry (snap offsets, sticky header positions)
   * that a pure scroll-offset change cannot invalidate, instead of rebuilding it on
   * every published frame. Starts at 1 so 0 always means "nothing cached yet".
   */
  std::uint64_t geometryVersion = 1;

  /*
   * Set by reconcileElements on any insert/remove/reorder: positions shift even when
   * no element's size changed. Defaults true so the first layout always recomputes.
   */
  bool elementsStructureDirty = true;

  /*
   * Lowest element index whose size changed outside layoutElements' own loop (which
   * cannot see those changes via its estimated checks), or UNDEFINED_INDEX when none.
   *
   * An index rather than a flag because offsets accumulate strictly forward: a size change
   * at row 80,000 cannot move any row before it, so the reflow starts there instead of at
   * row 0. A single prediction landing deep in the list is the normal case, since
   * consumePredictions drains every frame.
   *
   * Merged with min() at every site that records a size change, so a batch of changes
   * reflows once from the earliest of them.
   */
  std::size_t elementsSizeDirtyFromIndex = UNDEFINED_INDEX;

  /*
   * Highest index whose size changed, the companion to the lowest above.
   *
   * The pair brackets the span a reflow actually has to propagate through. Past the
   * highest changed row the stored offsets were a valid prefix sum before this batch, so
   * once the running offset reconverges with one of them, every row after it is already
   * correct and the walk can stop (see recomputeElementOffsets).
   *
   * Both bounds are needed. The lowest alone is not enough to stop early: a batch can
   * change rows i and j, and the offsets can coincidentally reconverge at some row between
   * them, where stopping would strand everything past j.
   */
  std::size_t elementsSizeDirtyToIndex = 0;

  /*
   * Record a size change at `index` that the next layout pass must reflow through.
   *
   * For changes nothing else is going to propagate: the measure passes and predictions.
   * A caller that reflows the change itself must use noteElementSizeSpan instead, or the
   * layout pass will redo the same work a second time.
   */
  void markElementSizeDirty(std::size_t index) {
    if (index < this->elementsSizeDirtyFromIndex) {
      this->elementsSizeDirtyFromIndex = index;
    }
    this->noteElementSizeSpan(index);
  }

  /*
   * Widen the changed span without scheduling a layout-pass reflow.
   *
   * For the batched intake path: applyElementSize records sizes and the caller then calls
   * commitElementSizes once, which reflows them. That reflow still needs to know how far
   * the batch reaches so it can stop early past it, but scheduling the layout pass to
   * reflow the same rows again would cost a second full suffix walk per frame.
   */
  void noteElementSizeSpan(std::size_t index) {
    if (index > this->elementsSizeDirtyToIndex) {
      this->elementsSizeDirtyToIndex = index;
    }
  }

  /*
   * Max cross-axis element extent (offset + size on the non-scroll axis). Cross extents
   * are not monotone by index, so recomputeTotalSize's tail-only scan would miss a
   * mid-list element wider/taller than the window without this. Rebuilt exactly by full
   * recomputeElementOffsets passes, only grown by partial ones.
   */
  double maxCrossAxisExtent = 0.0;

  /*
   * Scroll offset of the last host report. A user scroll only counts as a takeover when
   * this actually changes, so a stale userScrolled flag on an unmoved offset can't cancel
   * an in-flight correction. A frame carrying the core's own offset write back
   * (FrameInput::containerOffsetEnabled) leaves it alone: the host is not there yet, and
   * the travel of the next report is measured from where the host really was.
   */
  double lastReportedOffset = 0.0;

  /*
   * Serializes all access to one Container, which is genuinely shared across threads: in
   * Fabric a single core instance is carried forward by a list's committed shadow-node
   * clones, so adopt() / layout() / measurement feedback can run on overlapping commit
   * threads against the same Container. This mutex is essential, not defensive.
   *
   * It is recursive because:
   *   1. Every public Virtualizer entry point locks it, and several call one another
   *      (e.g. update -> measure -> recomputeTotalSize).
   *   2. An integration may hold it across a whole sequence of those calls plus the
   *      Container getters to make a frame atomic (Fabric's adopt()/layout() do this),
   *      and the inner per-method locks then re-enter on the same thread.
   *
   * Contract: hold coreMutex for any access to a shared Container. The public Virtualizer
   * methods lock it themselves, so a standalone driver (tests) need not lock explicitly.
   * The low-level getters/setters below do not lock; call them from inside a Virtualizer
   * entry point or while holding coreMutex.
   */
  std::recursive_mutex coreMutex;

  /*
   * Close the frame: advance revisionCount, fire the reached callbacks and dispatch the
   * observers.
   */
  void endRevision();

  const Element& getElementAtIndex(std::size_t index) const;

  /*
   * Offset/size getters and setters are orientation aware: horizontal reads/writes
   * offsetX and width, vertical reads/writes offsetY and height.
   */
  double getElementOffset(std::size_t index) const;
  double getElementSize(std::size_t index) const;
  double getContainerOffset() const;
  double getWindowContainerSize() const;

  std::size_t getElementsSize() const;

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

  /*
   * Whether the row at `index` carries geometry the core can trust without having laid it
   * out natively: either a real native measurement, or a host-supplied prediction.
   *
   */
  bool hasTrustedSize(std::size_t index) const;

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
   * Resting scroll offsets (ascending, along the scroll axis) that align an element
   * to the viewport edge selected by snapAlignment, each clamped to [0, maxOffset]
   * and deduplicated. Empty when snapToItem is unset. The integration snaps to the
   * nearest of these on scroll end.
   *
   * There is one target per element, so this is an O(elements) build with an
   * O(elements) allocation. It is published from the layout pass, which runs far more
   * often than the geometry actually changes, so the result is cached against
   * geometryVersion and the handful of scalars it also depends on. The returned
   * reference is owned by this Container and is invalidated by the next call.
   */
  const std::vector<double>& getSnapOffsets() const;

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
   * The anchor that measurement compensation holds still: the in-flight correction's target
   * when it anchors to a row, the captured anchor when no correction is in flight, and null
   * while a fixed-offset correction (bottom pin, scrollToEnd, shrink clamp) owns the offset.
   * The key may be empty.
   */
  const Anchor* compensationAnchor() const;

  /*
   * Fire the visible-indices-change and scroll callbacks if their values changed
   * since the last revision (deduplication lives here).
   */
  void dispatchObservers();

private:
  /*
   * Memoized getSnapOffsets() result and the inputs it was built from. `snapCacheVersion`
   * of 0 means nothing is cached yet.
   */
  mutable std::vector<double> snapOffsetsCache;
  mutable std::uint64_t snapCacheVersion = 0;
  mutable bool snapCacheSnapToItem = false;
  mutable int snapCacheAlignment = -1;
  mutable double snapCacheWindowSize = -1.0;
  mutable double snapCacheTotalSize = -1.0;
  mutable bool snapCacheHorizontal = false;

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
