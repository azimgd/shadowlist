#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace azimgd::shadowlist {

/*
 * Debug-only: the key at a visible/measured index, for cross-correlating native and
 * JS logs by content (the same index means different content mid-prepend). Only referenced
 * inside SL_LOG, which is a no-op unless SHADOWLIST_DEBUG_LOG is set, so mark it
 * maybe_unused to stay clean under -Werror=unused-function (Android).
 */
namespace {
[[maybe_unused]] const char* debugKeyAt(const Container* container, std::size_t index) {
  if (index < container->revision.elements.size()) {
    const std::string& key = container->revision.elements[index].key;
    return key.empty() ? "(empty)" : key.c_str();
  }
  return "(oob)";
}

/*
 * The size an element that has never been natively measured should carry: the average
 * frozen from real measurements once there is one, otherwise the configured estimate.
 *
 * Every pass that sizes an unmeasured row uses this, so they all agree. If the measure
 * pass used the raw estimate while the layout pass used the frozen average, simply
 * bringing a row into the window would resize it and reflow everything after it, after
 * the window had already been chosen from the old geometry, and the row that shift pulled
 * into range would be left out for a frame. One shared fallback avoids both the spurious
 * reflow and that lag.
 */
std::pair<double, double> effectiveFallbackSize(const Container* container) {
  auto [estimatedWidth, estimatedHeight] = container->estimatedElementSize;
  return {
    container->revision.averageElementWidth > 0.0 ? container->revision.averageElementWidth : estimatedWidth,
    container->revision.averageElementHeight > 0.0 ? container->revision.averageElementHeight : estimatedHeight,
  };
}

/*
 * Map an in-flight operation's target anchor to its desired (unclamped) pixel offset
 * for the current revision. An EndEdge anchor (ScrollToEnd / BottomPin / ShrinkClamp)
 * resolves to maxOffset; an Element anchor resolves to its element's offset plus the
 * captured sub-offset, rederived each frame so it tracks the element as nearby rows
 * are measured. Returns false when an Element anchor's key is no longer present.
 *
 * Header-size compensation is not applied here: it only matters on the
 * frame a header size changes, where resolveScroll/updateElementAtIndex apply it inline
 * to the immediate write. The cross-frame drive uses the raw element offset.
 */
bool resolveAnchorOffset(Container* container, const Operation& operation, double maxOffset, double& outOffset) {
  if (operation.target.mode != AnchorMode::Element) {
    outOffset = maxOffset;
    return true;
  }
  std::size_t anchorIndex = container->findElementIndexByKey(operation.target.key);
  if (anchorIndex == UNDEFINED_INDEX) {
    return false;
  }
  outOffset = container->getElementOffset(anchorIndex) + operation.target.subOffset;
  return true;
}
}

void Virtualizer::update(Container* container, const FrameInput& input) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  /*
   * The frame's keys, borrowed from the caller when it owns an immutable collection for
   * this commit (see FrameInput::keysRef). Valid only for the duration of this call.
   */
  const std::vector<std::string>& inputKeys = input.keyList();
  const std::vector<std::string>& inputNonAnchorableKeys = input.nonAnchorableKeyList();

  SL_LOG("update: keys=%zu prevElements=%zu off=(%.1f,%.1f) win=(%.1f,%.1f) inv=%d cols=%zu hdr=%.1f ftr=%.1f invInit=%d",
    inputKeys.size(), container->revision.elements.size(),
    input.containerOffsetX, input.containerOffsetY,
    input.windowContainerWidth, input.windowContainerHeight,
    input.inverted ? 1 : 0, input.columns, input.headerSize, input.footerSize,
    container->invertedInitialized ? 1 : 0);

  /*
   * Previous header size, so MVCP can tell a header-size change apart from a content
   * scroll and not absorb it.
   */
  double prevHeaderSize = container->headerSize;

  /*
   * Flipping the list order relocates the bottom, so a release recorded under the old
   * order describes a place that no longer exists. Start the new order following again.
   */
  if (container->inverted != input.inverted) {
    container->invertedBottomReleased = false;
  }

  /*
   * Configure layout properties for this frame
   */
  container->inverted = input.inverted;
  container->horizontal = input.horizontal;
  container->columns = input.columns;
  container->overscan = input.overscan;
  container->materializationOverscan = input.materializationOverscan;
  container->headerSize = input.headerSize;
  container->footerSize = input.footerSize;
  container->stickyHeader = input.stickyHeader;
  container->stickyFooter = input.stickyFooter;
  container->stickyIndices = input.stickyIndices;
  container->startReachedThreshold = input.startReachedThreshold;
  container->endReachedThreshold = input.endReachedThreshold;
  container->viewablePercentThreshold = input.viewablePercentThreshold;
  container->estimatedElementSize = input.estimatedElementSize;
  container->snapToItem = input.snapToItem;
  container->snapAlignment = input.snapAlignment;

  /*
   * Anchor policy for this frame: which row keys are decoration and must not be captured
   * as the MVCP anchor. Set before captureAnchor (which reads it). Rebuilt each frame so a
   * row toggling decoration<->content takes effect immediately.
   */
  if (container->nonAnchorableKeys.size() != inputNonAnchorableKeys.size() ||
      !std::all_of(inputNonAnchorableKeys.begin(), inputNonAnchorableKeys.end(),
        [&](const std::string& ignoredKey) { return container->nonAnchorableKeys.count(ignoredKey) != 0; })) {
    container->nonAnchorableKeys.clear();
    for (const std::string& ignoredKey : inputNonAnchorableKeys) {
      container->nonAnchorableKeys.insert(ignoredKey);
    }
  }

  /*
   * Reported scroll offset along the scroll axis
   */
  double inputOffset = container->horizontal ? input.containerOffsetX : input.containerOffsetY;

  /*
   * A frame whose offset is enabled is not a host report: the layout pass published the core's
   * own offset write, and the commit that adopts that state runs update() on it before the host
   * has applied anything. It repeats the commit token the core stamped on it and keeps the
   * gesture fields of the report it was built from, so it can neither confirm the correction
   * (the echo is the host's next report) nor count as gesture travel.
   */
  bool coreOffsetWrite = input.containerOffsetEnabled;

  /*
   * Did the reported offset actually move since the last host report? An unmoved offset
   * means the user is not driving, so an in-flight correction must survive recommits.
   */
  bool userMovedOffset = !coreOffsetWrite &&
    std::fabs(inputOffset - container->lastReportedOffset) >= OFFSET_MOVED_THRESHOLD;
  /*
   * A genuine human takeover abandons any in-flight correction and disengages the inverted
   * bottom pin. The host's gesture phase is authoritative: Dragging/Settling mean a finger
   * is driving, regardless of this frame's pixel delta. The userScrolled flag is also
   * honoured (gated on a real move) so a stale flag on an unmoved offset can't cancel a
   * correction, and so integrations not yet reporting a phase keep working.
   */
  bool gestureTakeover =
    (input.userScrolled && userMovedOffset) ||
    input.scrollPhase == ScrollPhase::Dragging ||
    input.scrollPhase == ScrollPhase::Settling;
  /*
   * The subset of takeovers that cancels an explicit scroll command (scrollToEnd,
   * scrollToIndex): a finger on the list, or a user move the host does not attribute to
   * momentum. Momentum alone does not. A command issued while a fling coasts must run, yet
   * its own frame, and any momentum tick the host reported before applying it, still carries
   * the settling phase; the host stops the fling when it issues the command.
   */
  bool dragTakeover =
    input.scrollPhase == ScrollPhase::Dragging ||
    (input.userScrolled && userMovedOffset && input.scrollPhase != ScrollPhase::Settling);
  bool scrollCommandInFlight = container->operation &&
    (container->operation->type == OperationType::ScrollToEnd ||
     container->operation->type == OperationType::ScrollToKey);
  /*
   * An in-flight MVCP correction is not a scroll intent the user can overrule: it holds the
   * content the user is looking at while rows are inserted above it. A prepend that lands mid
   * fling (typically just after flinging to the top, while the list is still bouncing) is
   * committed against a report whose phase is still Settling, and update() runs twice on that
   * same report per commit. Cancelling here would drop the correction on the second pass and
   * re-capture the anchor against the already-prepended rows, so the view would never move.
   * Instead: keep it across an unmoved re-run and across the commit that carries its own
   * offset write back, shift its target by the gesture's own travel when a momentum frame
   * arrives before the host applied it, and release it to the gesture once the host echoes its
   * commit token in a report (the content is then in place). Releasing it on its own write
   * instead would leave the prepended rows to be measured with no correction in flight.
   */
  bool maintainingAnchor = container->operation &&
    container->operation->type == OperationType::MaintainAnchor &&
    container->operation->target.mode == AnchorMode::Element;
  bool echoesOperation = container->operation && !coreOffsetWrite &&
    input.commitToken == container->operation->id;
  /*
   * Travel the host reports before it applies an anchor correction moves the correction's
   * target along. On a gesture frame that is momentum or a finger. A correction that ran under
   * a gesture keeps following once the frames turn idle: the host places it onto its live
   * offset, and the end of a bounce, or the report of the live offset once momentum stops, moved
   * that offset as surely as a momentum frame did. Any other idle move is not travel the
   * correction should follow (a host clamping the offset to a shrunken content size).
   */
  if (maintainingAnchor && !echoesOperation && userMovedOffset &&
      (gestureTakeover || container->operation->id == container->gestureOperationId)) {
    container->operation->target.subOffset += inputOffset - container->lastReportedOffset;
  }
  // See Container::gestureOperationId.
  if (maintainingAnchor && echoesOperation && !gestureTakeover &&
      container->operation->id == container->gestureOperationId) {
    container->operation.reset();
  }
  if (gestureTakeover) {
    bool keepsAnchorCorrection = maintainingAnchor && !echoesOperation;
    if (!keepsAnchorCorrection && (!scrollCommandInFlight || dragTakeover)) {
      container->operation.reset();
    }
    if (dragTakeover) {
      container->pendingScrollToEnd = false;
    }
    container->invertedInitialized = true;
    container->invertedOpeningPin = false;
  }
  container->gestureActive = gestureTakeover;

  /*
   * Whether an inverted list rests at its bottom (judged like the release below, against the
   * geometry the reader was looking at), so rows appended below the newest one are followed.
   */
  bool restingAtBottom = false;

  /*
   * Engage/release the inverted bottom pin (see Container::invertedBottomReleased).
   *
   * Both sides are judged against the geometry the user was actually looking at: the
   * previous frame's total and window, taken together. Mixing frames would compare this
   * frame's (smaller) window against last frame's total, inflating the bottom by the
   * difference, so a keyboard opening or a rotation would "release" a reader who has not
   * moved at all. It is also why this sits before the reconcile/measure below.
   *
   * The release happens on a gesture frame only (a finger, not a reflow). The re-engage has
   * to accept non-gesture frames too, because a scrollToEnd drives the offset itself, so it
   * is qualified by an actual move toward the bottom instead. That movement test is what
   * stops content shrinking (a code fence collapsing) from pulling maxOffset down onto a
   * resting reader and silently re-arming the pin under them.
   *
   * The move must also be one the reader or a scroll command made. When a reply shrinks
   * under a reader parked mid-conversation (regenerate empties it), the host clamps the
   * offset down to the new bottom and the core's anchor correction then nudges it back up a
   * few points: a move toward the bottom, inside the band, that is only the echo of our own
   * write. Counting it would re-arm the pin and chase the regenerating reply to the bottom
   * for the rest of its stream.
   */
  if (container->inverted) {
    double previousOffset = container->lastReportedOffset;
    double bottomTotal = container->horizontal ? container->revision.totalContainerWidth
                                               : container->revision.totalContainerHeight;
    double bottomWindow = container->horizontal ? container->revision.windowContainerWidth
                                                : container->revision.windowContainerHeight;
    double bottomOffset = bottomTotal - bottomWindow > 0.0 ? bottomTotal - bottomWindow : 0.0;
    /*
     * Nothing to scroll: the content fits the window, so bottomOffset is clamped to 0 and
     * every rubber-band overscroll reads as "far above the bottom". Releasing there would
     * latch the flag on a list that has no bottom to leave.
     */
    bool scrollable = bottomTotal > bottomWindow;

    if (gestureTakeover && scrollable && inputOffset < bottomOffset - INVERTED_FOLLOW_BAND) {
      container->invertedBottomReleased = true;
    } else if (container->invertedBottomReleased &&
        inputOffset >= bottomOffset - INVERTED_FOLLOW_BAND &&
        inputOffset >= previousOffset + OFFSET_MOVED_THRESHOLD &&
        (input.userScrolled || container->pendingScrollToEnd ||
         (container->operation && (container->operation->type == OperationType::ScrollToEnd ||
                                   container->operation->type == OperationType::ScrollToKey)))) {
      container->invertedBottomReleased = false;
    }

    restingAtBottom = !gestureTakeover && !container->invertedBottomReleased &&
      container->invertedInitialized && !container->revision.elements.empty() &&
      bottomWindow > 0.0 && inputOffset >= bottomOffset - INVERTED_FOLLOW_BAND;
  }
  // Read by resolveScroll to hold the opening bottom (see Container::invertedOpeningPin).
  container->restingAtInvertedBottom = restingAtBottom;
  // The core's own write is not where the host is (see coreOffsetWrite).
  if (!coreOffsetWrite) {
    container->lastReportedOffset = inputOffset;
  }

  /*
   * Capture the anchor element so the same content stays in view across a reconcile.
   * An anchor authored upstream (e.g. by JS on a data commit) overrides the captured
   * one and is the source of truth for what content must stay in view this frame.
   * Either way container->anchor is authoritative from here on.
   */
  bool hadElementsBefore = !container->revision.elements.empty();
  captureAnchor(container, inputOffset);
  if (!input.suppliedAnchor.key.empty()) {
    container->anchor = input.suppliedAnchor;
  }
  container->anchorHeaderSize = prevHeaderSize;

  std::string anchorKey = container->anchor.key;
  double anchorDelta = container->anchor.subOffset;

  /*
   * Debug-only: flag the frame where the key set changed (prepend/insert/reorder),
   * the root cause of the JS<->native visible-index desync. oldFront@newIdx is how far
   * the previous top row shifted (= number of rows prepended above it).
   */
#if SHADOWLIST_DEBUG_LOG
  {
    std::size_t prevSize = container->revision.elements.size();
    std::size_t nextSize = inputKeys.size();
    bool frontChanged = prevSize && nextSize && container->revision.elements.front().key != inputKeys.front();
    if (prevSize != nextSize || frontChanged) {
      long oldFrontNewIndex = -1;
      if (prevSize) {
        const std::string& oldFront = container->revision.elements.front().key;
        for (std::size_t nextElementIndex = 0; nextElementIndex < nextSize; ++nextElementIndex) {
          if (inputKeys[nextElementIndex] == oldFront) {
            oldFrontNewIndex = static_cast<long>(nextElementIndex);
            break;
          }
        }
      }
      SL_LOG("  RECONCILE: size %zu->%zu front '%s'->'%s' oldFront@newIdx=%ld anchorKey=%s anchorDelta=%.1f",
        prevSize, nextSize,
        prevSize ? container->revision.elements.front().key.c_str() : "(none)",
        nextSize ? inputKeys.front().c_str() : "(none)",
        oldFrontNewIndex, anchorKey.empty() ? "(none)" : anchorKey.c_str(), anchorDelta);
    }
  }
#endif

  /*
   * Reconcile the element list to the incoming keys (handles insert/remove/reorder).
   * adopt() runs update() on every commit, but most commits (scrolling, the measurement
   * settle, unrelated prop changes) carry an unchanged key set. Detect that with a cheap
   * O(n) compare and skip the rebuild, which otherwise allocates a fresh element vector
   * and key map every commit; that rebuild is the bulk of the per-commit cost during the
   * multi-commit settle that follows a prepend/append.
   */
  bool keysChanged = !input.keysUnchanged && container->revision.elements.size() != inputKeys.size();
  if (!keysChanged && !input.keysUnchanged) {
    for (std::size_t nextElementIndex = 0; nextElementIndex < inputKeys.size(); ++nextElementIndex) {
      if (container->revision.elements[nextElementIndex].key != inputKeys[nextElementIndex]) {
        keysChanged = true;
        break;
      }
    }
  }
  if (keysChanged) {
    // New keys end the opening settle; an append below the newest row is followed below.
    container->invertedOpeningPin = false;
    /*
     * A chat resting at the bottom of an inverted list follows messages appended below the
     * newest one. Plain anchoring would hold the row at the viewport top, so the new rows
     * would land below the fold. Following runs as a scrollToEnd, which keeps retargeting
     * the bottom while the new rows are measured and yields to a drag. Growth in place (a
     * streaming reply) is not an append and still needs the nonAnchorKeys policy to follow.
     */
    std::string previousLastKey = restingAtBottom ? container->revision.elements.back().key : std::string();
    reconcileElements(container, inputKeys);
    if (!previousLastKey.empty()) {
      std::size_t previousLastIndex = container->findElementIndexByKey(previousLastKey);
      if (previousLastIndex != UNDEFINED_INDEX && previousLastIndex + 1 < container->revision.elements.size()) {
        container->pendingScrollToEnd = true;
      }
    }
  }

  /*
   * Stamp any ahead-of-time sizes the host staged since the last frame. Must run after the
   * reconcile (so predictions for rows this commit introduced land immediately) and before
   * measure() (so this frame's window is chosen from the predicted geometry rather than
   * from estimates it is about to replace).
   */
  consumePredictions(container);

  /*
   * Measure the revision
   */
  container->startRevision();

  /*
   * Reset a half-applied revision on any exception between start and end (e.g. measure
   * throwing), so the next frame can start cleanly instead of throwing "previous revision
   * in progress" forever and wedging the instance. On the success path endRevision() has
   * already returned the status to idle, so this guard is a no-op.
   */
  struct RevisionStatusGuard {
    Container* container;
    ~RevisionStatusGuard() {
      if (container->revisionStatus == REVISION_STATUS_PENDING) {
        container->revisionStatus = REVISION_STATUS_IDLE;
      }
    }
  } revisionStatusGuard{container};

  container->setContainerOffsetX(input.containerOffsetX);
  container->setContainerOffsetY(input.containerOffsetY);
  container->setWindowContainerWidth(input.windowContainerWidth);
  container->setWindowContainerHeight(input.windowContainerHeight);
  measure(container);

  SL_LOG("  measured: total=(%.1f,%.1f) offset=(%.1f,%.1f) visible=[%zd..%zd] visKeys=[%s..%s] anchorKey=%s anchor@newIdx=%zd",
    container->revision.totalContainerWidth, container->revision.totalContainerHeight,
    container->revision.containerOffsetX, container->revision.containerOffsetY,
    static_cast<std::ptrdiff_t>(container->getVisibleIndices().first),
    static_cast<std::ptrdiff_t>(container->getVisibleIndices().second),
    debugKeyAt(container, container->getVisibleIndices().first),
    debugKeyAt(container, container->getVisibleIndices().second),
    anchorKey.empty() ? "(none)" : anchorKey.c_str(),
    static_cast<std::ptrdiff_t>(anchorKey.empty() ? UNDEFINED_INDEX : container->findElementIndexByKey(anchorKey)));

  /*
   * Resolve scroll corrections. If the offset moved, remeasure to select the
   * visible window matching the new offset.
   */
  if (resolveScroll(container, anchorKey, anchorDelta, hadElementsBefore, !input.containerOffsetEnabled)) {
    measure(container, true);
    SL_LOG("  remeasured: offset=(%.1f,%.1f) visible=[%zd..%zd] visKeys=[%s..%s] invInit=%d",
      container->revision.containerOffsetX, container->revision.containerOffsetY,
      static_cast<std::ptrdiff_t>(container->getVisibleIndices().first),
      static_cast<std::ptrdiff_t>(container->getVisibleIndices().second),
      debugKeyAt(container, container->getVisibleIndices().first),
      debugKeyAt(container, container->getVisibleIndices().second),
      container->invertedInitialized ? 1 : 0);
  }

  /*
   * The commit token is the in-flight operation's id (assigned once at creation, preserved
   * across the settle); resolveStateUpdate publishes it on a corrected frame. No separate
   * token bookkeeping; if there is no operation, there is no token.
   */
  SL_LOG("  resolved: offset=(%.1f,%.1f) corrected=%d invInit=%d token=%llu",
    container->revision.containerOffsetX, container->revision.containerOffsetY,
    container->containerOffsetCorrected ? 1 : 0, container->invertedInitialized ? 1 : 0,
    static_cast<unsigned long long>(container->operation ? container->operation->id : 0));

  if (container->gestureActive && container->operation) {
    container->gestureOperationId = container->operation->id;
  }

  container->endRevision();
}

void Virtualizer::measure(Container* container, bool windowFromOffset) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  if (container->revisionStatus != REVISION_STATUS_PENDING) {
    throw InvalidOperationError("Cannot use measure outside of a revision");
  }

  /*
   * Reset the per-revision measurement accumulators so they reflect only the
   * elements measured in this revision (otherwise they grow unbounded)
   */
  container->revision.measurementElementStartIndex = UNDEFINED_INDEX;
  container->revision.measurementElementEndIndex = UNDEFINED_INDEX;
  container->revision.measurementElementCount = 0;
  container->revision.measurementElementTotalHeight = 0;
  container->revision.measurementElementTotalWidth = 0;

  /*
   * An insert/remove/reorder leaves every element carrying its pre-mutation offset, and
   * newly created rows carry no size at all, until a reflow runs. Selecting the window
   * from that geometry answers a question about where the rows used to be: on the frame a
   * mutation lands, a row that moved into view could be reported as outside the window and
   * therefore not mounted, leaving a blank gap until some later frame re-measures.
   *
   * So reflow first and choose the window from real positions. This also lets the window
   * pass use its ordered index seek on mutation frames instead of falling back to
   * scanning every row, which is where the extra pass pays for itself.
   */
  if (container->elementsStructureDirty) {
    layoutElements(container);
  }

  /*
   * The first revision fills a window from the edge (offsets not known yet). After a
   * scroll correction the remeasure selects the window around the corrected offset.
   */
  if (!windowFromOffset && container->revisionCount == REVISION_COUNT_FIRST) {
    measureFirstRevision(container);
  } else {
    measureNextRevision(container);
  }

  layoutElements(container);
  recomputeTotalSize(container);
}

void Virtualizer::measureFirstRevision(Container* container) {
  std::size_t elementsSize = container->revision.elements.size();
  double windowSize = container->getWindowContainerSize();
  double effectiveColumns = container->columns > 0 ? static_cast<double>(container->columns) : 1.0;

  std::size_t measuredMinIndex = UNDEFINED_INDEX;
  std::size_t measuredMaxIndex = UNDEFINED_INDEX;
  double accumulated = 0.0;

  /*
   * Default lists fill from the start, inverted lists fill from the end
   */
  for (std::size_t iteration = 0; iteration < elementsSize; ++iteration) {
    std::size_t nextElementIndex = container->inverted ? (elementsSize - 1 - iteration) : iteration;
    Element& nextElement = container->revision.elements[nextElementIndex];

    /*
     * Only assign an estimate to elements that don't already have a size.
     */
    if (!nextElement.estimated) {
      auto [width, height] = effectiveFallbackSize(container);

      /*
       * No estimate configured: leave this element unsized.
       */
      if (width == 0.0 && height == 0.0) {
        continue;
      }

      // Same reasoning as measureNextRevision: only a real size change dirties geometry.
      if (nextElement.width != width || nextElement.height != height) {
        nextElement.width = width;
        nextElement.height = height;
        // Sizes changed outside layoutElements' own loop; make sure it reflows offsets.
        container->markElementSizeDirty(nextElementIndex);
      }
      nextElement.estimated = true;
    }

    if (measuredMinIndex == UNDEFINED_INDEX || nextElementIndex < measuredMinIndex) {
      measuredMinIndex = nextElementIndex;
    }
    if (measuredMaxIndex == UNDEFINED_INDEX || nextElementIndex > measuredMaxIndex) {
      measuredMaxIndex = nextElementIndex;
    }

    container->revision.measurementElementTotalHeight += nextElement.height;
    container->revision.measurementElementTotalWidth += nextElement.width;
    container->revision.measurementElementCount++;

    accumulated += container->horizontal ? nextElement.width : nextElement.height;

    /*
     * Stop once we've filled the visible window plus `overscan` viewports of buffer past
     * the edge. For multi-column layouts the load is shared across the columns.
     */
    if (accumulated / effectiveColumns >= windowSize * (1.0 + container->overscan)) {
      break;
    }
  }

  finalizeMeasurement(container, measuredMinIndex, measuredMaxIndex);
}

void Virtualizer::measureNextRevision(Container* container) {
  std::size_t elementsSize = container->revision.elements.size();
  double containerOffset = container->getContainerOffset();
  double windowSize = container->getWindowContainerSize();

  /*
   * Measure the visible window plus `overscan` viewports of buffer on each side, so
   * scrolling reveals already-measured rows instead of blanks. overscan is in viewport
   * units (1 = one window above and one below).
   */
  double overscanSize = windowSize * container->overscan;
  double lowerBound = containerOffset - overscanSize;
  double upperBound = containerOffset + windowSize + overscanSize;

  std::size_t measuredMinIndex = UNDEFINED_INDEX;
  std::size_t measuredMaxIndex = UNDEFINED_INDEX;

  /*
   * Offsets are only trustworthy once the current structure has been reflowed:
   * reconciled elements still carry pre-reorder offsets until layoutElements runs, so a
   * row moved toward the front could terminate an ordered walk early with a stale offset.
   * In that state fall back to the exhaustive scan.
   */
  bool geometryOrdered = !container->elementsStructureDirty;

  auto elementOffsetAt = [&](std::size_t index) {
    const Element& element = container->revision.elements[index];
    return container->horizontal ? element.offsetX : element.offsetY;
  };
  auto elementSizeAt = [&](std::size_t index) {
    const Element& element = container->revision.elements[index];
    return container->horizontal ? element.width : element.height;
  };

  /*
   * Visit one candidate row: apply a size estimate if it has none and fold it into this
   * revision's measurement accumulators. Returns false when the row had to be skipped
   * because no estimate is configured.
   */
  auto visit = [&](std::size_t nextElementIndex) {
    Element& nextElement = container->revision.elements[nextElementIndex];

    if (!nextElement.estimated) {
      auto [width, height] = effectiveFallbackSize(container);

      if (width == 0.0 && height == 0.0) {
        return;
      }

      /*
       * layoutElements has already given every unmeasured row exactly this fallback size,
       * so a row crossing into the window normally changes only its state, not its size.
       * Marking geometry dirty regardless would force a full O(rows) offset reflow on every
       * frame that reveals a row nobody has visited yet.
       *
       * A fling would barely notice: it reveals a couple of rows and then spends the gesture
       * inside territory it has already been through. Dragging the scroll indicator sweeps
       * the whole list, so every frame lands in fresh rows and would pay that reflow for the
       * entire drag.
       */
      if (nextElement.width != width || nextElement.height != height) {
        nextElement.width = width;
        nextElement.height = height;
        // Sizes changed outside layoutElements' own loop; make sure it reflows offsets.
        container->markElementSizeDirty(nextElementIndex);
      }
      nextElement.estimated = true;
    }

    if (measuredMinIndex == UNDEFINED_INDEX || nextElementIndex < measuredMinIndex) {
      measuredMinIndex = nextElementIndex;
    }
    if (measuredMaxIndex == UNDEFINED_INDEX || nextElementIndex > measuredMaxIndex) {
      measuredMaxIndex = nextElementIndex;
    }

    container->revision.measurementElementTotalHeight += nextElement.height;
    container->revision.measurementElementTotalWidth += nextElement.width;
    container->revision.measurementElementCount++;
  };

  /*
   * Find the first index of an ordered track (stride `step`, starting at `first`) whose
   * trailing edge is past lowerBound: the first row that can still overlap the window.
   * Rows are laid out end to end within a track, so `offset + size` is non-decreasing
   * along it and the boundary is binary-searchable. This is what keeps a deep scroll
   * O(log N + window) instead of walking the whole prefix on every frame.
   */
  auto seekTrack = [&](std::size_t first, std::size_t step) {
    std::size_t low = 0;
    std::size_t high = first < elementsSize ? (elementsSize - 1 - first) / step + 1 : 0;
    while (low < high) {
      std::size_t mid = low + (high - low) / 2;
      std::size_t index = first + mid * step;
      if (elementOffsetAt(index) + elementSizeAt(index) <= lowerBound) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    return low;
  };

  if (geometryOrdered && container->columns <= 1) {
    /*
     * Single column: one ordered track over every index.
     */
    for (std::size_t nextElementIndex = seekTrack(0, 1); nextElementIndex < elementsSize; ++nextElementIndex) {
      if (elementOffsetAt(nextElementIndex) > upperBound) {
        break;
      }
      visit(nextElementIndex);
    }
  } else if (geometryOrdered && container->columns > 1) {
    /*
     * Multi-column: rows are assigned to tracks round-robin by `index % columns`
     * (recomputeElementOffsets), so global index order is not offset-ordered but each
     * track is. Search and walk each track independently instead of scanning all N rows.
     */
    for (std::size_t track = 0; track < container->columns && track < elementsSize; ++track) {
      std::size_t stepsPast = seekTrack(track, container->columns);
      for (std::size_t nextElementIndex = track + stepsPast * container->columns;
           nextElementIndex < elementsSize;
           nextElementIndex += container->columns) {
        if (elementOffsetAt(nextElementIndex) > upperBound) {
          break;
        }
        visit(nextElementIndex);
      }
    }
  } else {
    /*
     * Geometry is mid-reconcile and offsets cannot be trusted to be ordered: check every
     * row against the window.
     */
    for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
      double elementOffset = elementOffsetAt(nextElementIndex);
      double elementSize = elementSizeAt(nextElementIndex);

      /*
       * Interval-overlap test (mirroring captureAnchor's), not a leading-edge-only test:
       * an element whose leading edge is before lowerBound but whose body still overlaps
       * it must still be measured/counted as visible.
       */
      if (elementOffset > upperBound || elementOffset + elementSize <= lowerBound) {
        continue;
      }
      visit(nextElementIndex);
    }
  }

  finalizeMeasurement(container, measuredMinIndex, measuredMaxIndex);
}

void Virtualizer::finalizeMeasurement(Container* container, std::size_t measuredMinIndex, std::size_t measuredMaxIndex) {
  /*
   * For inverted lists we iterate from end to start so the start index is the
   * higher one (e.g. start=99, end=90). For default lists start is the lower one.
   */
  if (container->inverted) {
    container->revision.measurementElementStartIndex = measuredMaxIndex;
    container->revision.measurementElementEndIndex = measuredMinIndex;
  } else {
    container->revision.measurementElementStartIndex = measuredMinIndex;
    container->revision.measurementElementEndIndex = measuredMaxIndex;
  }

  /*
   * The average element size is not computed here: the first-revision
   * window is estimate-filled. It is frozen from real measurements in
   * recomputeTotalSize instead, so MVCP does not chase a moving anchor.
   */
}

void Virtualizer::layoutElements(Container* container) {
  std::size_t elementsSize = container->revision.elements.size();

  double trackSize = container->horizontal
    ? container->revision.windowContainerHeight / (container->columns > 0 ? container->columns : 1)
    : container->revision.windowContainerWidth / (container->columns > 0 ? container->columns : 1);

  /*
   * Size unmeasured elements with the average, falling back to the estimate until
   * the average is frozen. Multi-column layouts force the cross-axis to the track size.
   */
  auto [fallbackWidth, fallbackHeight] = effectiveFallbackSize(container);

  bool layoutParamsChanged =
    container->headerSize != container->lastLayoutHeaderSize ||
    container->footerSize != container->lastLayoutFooterSize ||
    container->revision.windowContainerWidth != container->lastLayoutWindowWidth ||
    container->revision.windowContainerHeight != container->lastLayoutWindowHeight ||
    container->columns != container->lastLayoutColumns ||
    container->horizontal != container->lastLayoutHorizontal;

  /*
   * The sizing loop below is O(elements), and it does not mark the rows it touches as
   * estimated: that flag is reserved for the window/native measurement states. Run on
   * every frame, it would revisit every far-away row of a settled list just to confirm the
   * fallback dimensions it already holds, making an ordinary scroll frame O(N).
   *
   * The loop is only capable of changing anything when the fallback dimensions changed,
   * a layout parameter changed (multi-column cross-axis is derived from the track size),
   * or the element list itself changed and brought in unsized rows. Otherwise every
   * unmeasured row already holds exactly these dimensions and the pass is a guaranteed
   * no-op, so skip it and leave the offsets alone.
   */
  bool fallbackDimensionsChanged =
    fallbackWidth != container->lastFallbackWidth ||
    fallbackHeight != container->lastFallbackHeight;

  bool anyNewlyEstimated = false;

  if (fallbackDimensionsChanged || layoutParamsChanged || container->elementsStructureDirty) {
    for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
      Element& nextElement = container->revision.elements[nextElementIndex];

      if (container->columns > 1) {
        if (container->horizontal) {
          if (!nextElement.estimated && nextElement.width != fallbackWidth) {
            nextElement.width = fallbackWidth;
            anyNewlyEstimated = true;
          }
          if (nextElement.height != trackSize) {
            nextElement.height = trackSize;
            anyNewlyEstimated = true;
          }
        } else {
          if (!nextElement.estimated && nextElement.height != fallbackHeight) {
            nextElement.height = fallbackHeight;
            anyNewlyEstimated = true;
          }
          if (nextElement.width != trackSize) {
            nextElement.width = trackSize;
            anyNewlyEstimated = true;
          }
        }
      } else if (!nextElement.estimated &&
                 (nextElement.width != fallbackWidth || nextElement.height != fallbackHeight)) {
        nextElement.width = fallbackWidth;
        nextElement.height = fallbackHeight;
        anyNewlyEstimated = true;
      }
    }

    container->lastFallbackWidth = fallbackWidth;
    container->lastFallbackHeight = fallbackHeight;
  }

  /*
   * recomputeElementOffsets is an O(elements) pass. Skip it unless something that feeds
   * offsets changed this call: a fresh fallback size above, a size stamped by the
   * measure pass (elementsSizeDirtyFromIndex), a structural change (elementsStructureDirty), or
   * a layout parameter. This keeps a plain scroll frame O(window) instead of O(N).
   */
  bool sizesDirty = container->elementsSizeDirtyFromIndex != UNDEFINED_INDEX;

  if (anyNewlyEstimated || layoutParamsChanged || container->elementsStructureDirty || sizesDirty) {
    /*
     * Reflow from the earliest row that actually moved. A structural change or a layout
     * parameter change genuinely perturbs the prefix (positions shift, the multi-column
     * track size is rederived, the header moves everything), so those start at 0, as does a
     * fresh fallback size, which the sizing loop above may have applied anywhere.
     * A recorded size change alone cannot touch anything before it, because offsets
     * accumulate strictly forward: this is the same invariant commitElementSizes relies on.
     */
    std::size_t reflowFrom =
      (anyNewlyEstimated || layoutParamsChanged || container->elementsStructureDirty)
        ? 0
        : container->elementsSizeDirtyFromIndex;

    recomputeElementOffsets(container, reflowFrom, container->elementsSizeDirtyToIndex);
    container->lastLayoutHeaderSize = container->headerSize;
    container->lastLayoutFooterSize = container->footerSize;
    container->lastLayoutWindowWidth = container->revision.windowContainerWidth;
    container->lastLayoutWindowHeight = container->revision.windowContainerHeight;
    container->lastLayoutColumns = container->columns;
    container->lastLayoutHorizontal = container->horizontal;
    container->elementsStructureDirty = false;
    container->elementsSizeDirtyFromIndex = UNDEFINED_INDEX;
    container->elementsSizeDirtyToIndex = 0;
  }
}

void Virtualizer::recomputeElementOffsets(Container* container, std::size_t fromIndex, std::size_t changedThroughIndex) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  /*
   * Every element position this list will publish is written here, so this is the one
   * place derived geometry (snap targets, sticky header positions) can become stale.
   *
   * The bump is deferred to the end and made only when an offset actually changed. A reflow
   * that rewrites each row with the offset it already had (the common case) cannot
   * invalidate anything derived from offsets, and bumping anyway would throw away every
   * cache keyed on it: the snap-offset table (one entry per row), the published sticky
   * table, and the host-side geometry cache.
   */
  bool anyOffsetChanged = false;

  std::size_t elementsSize = container->revision.elements.size();

  /*
   * Maintain Container::maxCrossAxisExtent alongside the offsets: a full pass rebuilds
   * it exactly; a partial pass only grows it (the untouched prefix keeps its share).
   */
  double crossMax = fromIndex == 0 ? 0.0 : container->maxCrossAxisExtent;

  if (fromIndex >= elementsSize) {
    container->maxCrossAxisExtent = crossMax;
    return;
  }

  if (container->columns > 1) {
    double trackSize = container->horizontal
      ? container->revision.windowContainerHeight / container->columns
      : container->revision.windowContainerWidth / container->columns;

    /*
     * Tracks start after the header along the scroll axis
     */
    std::vector<double> trackSizes(container->columns, container->headerSize);

    /*
     * Seed each track with the running edge of its last element before fromIndex.
     * The last element of every track lives within the columns elements preceding
     * fromIndex, so this only needs to look back columns positions.
     */
    for (std::size_t seedIndex = fromIndex; seedIndex-- > 0 && seedIndex + container->columns >= fromIndex;) {
      const Element& seedElement = container->revision.elements[seedIndex];
      std::size_t trackIndex = seedIndex % container->columns;
      trackSizes[trackIndex] = container->horizontal
        ? seedElement.offsetX + seedElement.width + seedElement.gapX
        : seedElement.offsetY + seedElement.height + seedElement.gapY;
    }

    for (std::size_t nextElementIndex = fromIndex; nextElementIndex < elementsSize; ++nextElementIndex) {
      Element& nextElement = container->revision.elements[nextElementIndex];
      nextElement.index = nextElementIndex;

      std::size_t trackIndex = nextElementIndex % container->columns;

      /*
       * Force the cross-axis size to the track size here too, so a reflow with a
       * freshly known window size corrects the cross extent, not just the position.
       */
      if (container->horizontal) {
        anyOffsetChanged = anyOffsetChanged || nextElement.offsetX != trackSizes[trackIndex] ||
          nextElement.offsetY != trackIndex * trackSize;
        nextElement.offsetY = trackIndex * trackSize;
        nextElement.height = trackSize;
        nextElement.offsetX = trackSizes[trackIndex];
        trackSizes[trackIndex] += nextElement.width + nextElement.gapX;
      } else {
        anyOffsetChanged = anyOffsetChanged || nextElement.offsetY != trackSizes[trackIndex] ||
          nextElement.offsetX != trackIndex * trackSize;
        nextElement.offsetX = trackIndex * trackSize;
        nextElement.width = trackSize;
        nextElement.offsetY = trackSizes[trackIndex];
        trackSizes[trackIndex] += nextElement.height + nextElement.gapY;
      }

      double crossExtent = container->horizontal
        ? nextElement.offsetY + nextElement.height
        : nextElement.offsetX + nextElement.width;
      if (crossExtent > crossMax) {
        crossMax = crossExtent;
      }
    }
  } else {
    /*
     * Elements start after the header along the scroll axis
     */
    double nextOffset = container->headerSize;

    /*
     * Seed the running offset from the element preceding fromIndex
     */
    if (fromIndex > 0) {
      const Element& prevElement = container->revision.elements[fromIndex - 1];
      nextOffset = container->horizontal
        ? prevElement.offsetX + prevElement.width + prevElement.gapX
        : prevElement.offsetY + prevElement.height + prevElement.gapY;
    }

    /*
     * This walk is the hot loop of the whole core: it runs over the suffix every time a
     * row's measured size changes, and a gesture that reveals fresh rows every frame (a
     * scroll-indicator drag) runs it on a large suffix every frame. It is memory bound, so
     * the orientation branch is hoisted out rather than re-tested per row, and `index` is
     * only written when it is actually stale: a size-only reflow never renumbers rows, and
     * the store would otherwise dirty a cache line per element for nothing.
     */
    Element* elements = container->revision.elements.data();

    /*
     * Early exit: past the last row whose size changed, the stored offsets were a valid
     * prefix sum before this pass began. So the first row beyond that point which is
     * already sitting at the offset we would write proves the running offset has
     * reconverged, and since nothing downstream changed size, every row after it is
     * already correct too.
     *
     * This is what makes an exact ahead-of-time prediction free: the reflow it triggers
     * stops within a row or two instead of walking to the end of the dataset. It also
     * collapses the very common "reflow that moves nothing" into O(1).
     *
     * Both guards matter. `changedThroughIndex` is required because a batch can change
     * rows i and j and the offsets can reconverge between them, where stopping would
     * strand everything past j. `fromIndex > 0` is required because a full pass rebuilds
     * maxCrossAxisExtent from zero and must visit every row to do so, whereas a partial
     * pass seeds it from the stored value, which already accounts for the skipped rows.
     */
    bool canStopEarly = fromIndex > 0 && changedThroughIndex != UNDEFINED_INDEX;

    if (container->horizontal) {
      for (std::size_t nextElementIndex = fromIndex; nextElementIndex < elementsSize; ++nextElementIndex) {
        Element& nextElement = elements[nextElementIndex];
        if (nextElement.index != nextElementIndex) {
          nextElement.index = nextElementIndex;
        } else if (canStopEarly && nextElementIndex > changedThroughIndex &&
                   nextElement.offsetX == nextOffset) {
          break;
        }

        anyOffsetChanged = anyOffsetChanged || nextElement.offsetX != nextOffset;
        nextElement.offsetX = nextOffset;
        nextOffset += nextElement.width + nextElement.gapX;

        double crossExtent = nextElement.offsetY + nextElement.height;
        if (crossExtent > crossMax) {
          crossMax = crossExtent;
        }
      }
    } else {
      for (std::size_t nextElementIndex = fromIndex; nextElementIndex < elementsSize; ++nextElementIndex) {
        Element& nextElement = elements[nextElementIndex];
        if (nextElement.index != nextElementIndex) {
          nextElement.index = nextElementIndex;
        } else if (canStopEarly && nextElementIndex > changedThroughIndex &&
                   nextElement.offsetY == nextOffset) {
          break;
        }

        anyOffsetChanged = anyOffsetChanged || nextElement.offsetY != nextOffset;
        nextElement.offsetY = nextOffset;
        nextOffset += nextElement.height + nextElement.gapY;

        double crossExtent = nextElement.offsetX + nextElement.width;
        if (crossExtent > crossMax) {
          crossMax = crossExtent;
        }
      }
    }
  }

  container->maxCrossAxisExtent = crossMax;

  if (anyOffsetChanged) {
    container->geometryVersion++;
  }
}

void Virtualizer::recomputeTotalSize(Container* container) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  /*
   * Total container size is the maximum element extent. Using the element edge
   * (offset + size) rather than the accumulated track size excludes the trailing
   * gap after the last element, keeping single and multi-column layouts consistent.
   * Element offsets already include the header along the scroll axis.
   */
  double maxHeight = 0.0;
  double maxWidth = 0.0;

  /*
   * Per-track scroll-axis offsets only grow with index, so the last `columns` elements
   * always contain every track's furthest edge, and scanning just that tail finds the
   * scroll-axis maximum. Cross-axis extents are not monotone by index, so the cross
   * axis folds in Container::maxCrossAxisExtent below instead.
   */
  std::size_t elementsSize = container->revision.elements.size();
  std::size_t scanColumns = container->columns > 0 ? container->columns : 1;
  std::size_t scanFrom = elementsSize > scanColumns ? elementsSize - scanColumns : 0;

  for (std::size_t nextElementIndex = scanFrom; nextElementIndex < elementsSize; ++nextElementIndex) {
    const Element& nextElement = container->revision.elements[nextElementIndex];
    double elementBottom = nextElement.offsetY + nextElement.height;
    double elementRight = nextElement.offsetX + nextElement.width;

    if (elementBottom > maxHeight) {
      maxHeight = elementBottom;
    }
    if (elementRight > maxWidth) {
      maxWidth = elementRight;
    }
  }

  /*
   * Scroll axis includes the header (a floor for empty lists) and trailing footer.
   * The cross axis is floored at the window's cross size so content spans the viewport
   * and multi-column layouts cannot collapse to a zero-width feedback loop, and takes
   * the maintained cross maximum so an element measured past the window's cross size
   * is still covered by the published content size.
   */
  if (container->horizontal) {
    container->revision.totalContainerWidth = std::max(maxWidth, container->headerSize) + container->footerSize;
    container->revision.totalContainerHeight =
      std::max({maxHeight, container->maxCrossAxisExtent, container->revision.windowContainerHeight});
  } else {
    container->revision.totalContainerHeight = std::max(maxHeight, container->headerSize) + container->footerSize;
    container->revision.totalContainerWidth =
      std::max({maxWidth, container->maxCrossAxisExtent, container->revision.windowContainerWidth});
  }

  /*
   * Freeze the average from the first batch of real measurements. The == 0.0 guard
   * freezes it once, keeping the unmeasured region stable so MVCP has a fixed anchor.
   */
  if (container->revision.measuredRealCount > 0) {
    if (container->revision.averageElementWidth == 0.0) {
      container->revision.averageElementWidth =
        container->revision.measuredRealTotalWidth / container->revision.measuredRealCount;
    }
    if (container->revision.averageElementHeight == 0.0) {
      container->revision.averageElementHeight =
        container->revision.measuredRealTotalHeight / container->revision.measuredRealCount;
    }
  }
}

bool Virtualizer::applyElementSize(Container* container, std::size_t index, Size size) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  if (index >= container->revision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  Element& nextElement = container->revision.elements[index];

  double prevWidth = nextElement.width;
  double prevHeight = nextElement.height;
  bool wasMeasured = nextElement.measured;
  bool dimensionsChanged = prevWidth != size.width || prevHeight != size.height;

  /*
   * Nothing at all to record: this row is already measured at exactly this size. Fabric
   * hands back a size for every mounted row on every layout pass, so in a steady scroll
   * almost all of these calls land here. Returning early is what keeps a layout with M
   * mounted rows from costing O(M x N): the reflow walks the whole suffix, so a mounted
   * window near the start of a 100k-row list would otherwise visit ~2M elements per layout
   * to record sizes that have not moved a pixel.
   */
  if (!dimensionsChanged && wasMeasured) {
    return false;
  }

  /*
   * The first measurement of the row commitElementSizes will anchor to (the in-flight anchor
   * correction's row, or the captured anchor when none is in flight): remember how much it
   * moved the row's trailing edge (see Container::anchorFirstMeasurementDelta).
   */
  if (!wasMeasured && dimensionsChanged && container->columns <= 1) {
    const std::string& compensationKey =
      (container->operation && container->operation->target.mode == AnchorMode::Element)
        ? container->operation->target.key
        : (!container->operation ? container->anchor.key : std::string());
    if (!compensationKey.empty() && nextElement.key == compensationKey) {
      container->anchorFirstMeasurementDelta += container->horizontal
        ? size.width - prevWidth
        : size.height - prevHeight;
    }
  }

  nextElement.width = size.width;
  nextElement.height = size.height;
  nextElement.estimated = true;
  nextElement.measured = true;

  /*
   * A real native measurement always supersedes a prediction: the prediction existed only
   * to stand in until this arrived. Clearing the flag also stops the row being counted as
   * predicted-but-unmeasured by hasTrustedSize's callers.
   */
  nextElement.predicted = false;

  /*
   * Bound the reflow that commitElementSizes is about to perform, without scheduling a
   * second one in the layout pass (see Container::noteElementSizeSpan).
   */
  container->noteElementSizeSpan(index);

  if (container->revision.averageElementHeight > 0) {
    container->revision.measurementElementTotalHeight += size.height - prevHeight;
    container->revision.measurementElementTotalWidth += size.width - prevWidth;
  }

  /*
   * Accumulate real measured sizes for the frozen average. Count each element once:
   * a first measurement adds the full size, a remeasurement adjusts by the delta.
   */
  if (wasMeasured) {
    container->revision.measuredRealTotalWidth += size.width - prevWidth;
    container->revision.measuredRealTotalHeight += size.height - prevHeight;
  } else {
    container->revision.measuredRealCount++;
    container->revision.measuredRealTotalWidth += size.width;
    container->revision.measuredRealTotalHeight += size.height;
  }

  /*
   * A first measurement that happens to match the estimate exactly still had to be
   * recorded above (it feeds the frozen average), but it moved no geometry, so the
   * caller has nothing to reflow.
   */
  return dimensionsChanged;
}

std::size_t Virtualizer::applyPredictedElementSize(Container* container, const std::string& key, Size size) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  if (key.empty()) {
    return UNDEFINED_INDEX;
  }

  std::size_t index = container->findElementIndexByKey(key);

  /*
   * The row does not exist yet, which is the normal case for a host measuring ahead of the
   * data it has committed. Stage the size; the next update() stamps it (see
   * Virtualizer::consumePredictions).
   */
  if (index >= container->revision.elements.size()) {
    container->setPredictedSize(key, size);
    return UNDEFINED_INDEX;
  }

  Element& nextElement = container->revision.elements[index];

  /*
   * A real native measurement outranks a prediction unconditionally. Returning here (rather
   * than staging) means a late prediction for an already-measured row is simply discarded,
   * so it cannot resurface and overwrite the truth after a later reconcile.
   */
  if (nextElement.measured) {
    return UNDEFINED_INDEX;
  }

  bool dimensionsChanged = nextElement.width != size.width || nextElement.height != size.height;

  double prevWidth = nextElement.width;
  double prevHeight = nextElement.height;

  nextElement.width = size.width;
  nextElement.height = size.height;
  nextElement.estimated = true;
  nextElement.predicted = true;

  /*
   * Keep this revision's measured totals consistent with the sizes they were accumulated
   * from, exactly as applyElementSize does: if this row is inside the measured window its
   * contribution just changed.
   */
  if (container->revision.averageElementHeight > 0) {
    container->revision.measurementElementTotalHeight += size.height - prevHeight;
    container->revision.measurementElementTotalWidth += size.width - prevWidth;
  }

  /*
   * Predictions do not feed Revision::measuredReal*: the frozen average is what sizes rows
   * nobody knows anything about, and it stays a sample of real native measurements.
   * Folding predictions in would let an estimate derived from other estimates masquerade as
   * measured data.
   */
  if (!dimensionsChanged) {
    return UNDEFINED_INDEX;
  }

  container->markElementSizeDirty(index);

  return index;
}

void Virtualizer::commitElementSizes(Container* container, std::size_t fromIndex) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  if (fromIndex >= container->revision.elements.size()) {
    return;
  }

  /*
   * Only this element and the ones after it can shift, so reflow from here forward.
   * The caller refreshes the total once per measurement batch.
   */
  recomputeElementOffsets(container, fromIndex, container->elementsSizeDirtyToIndex);
  container->elementsSizeDirtyToIndex = 0;

  /*
   * Keep the anchor element fixed on screen while off-screen elements are measured.
   * During an anchor-driven correction (prepend) keep that sticky anchor; a
   * fixed-offset correction (bottom anchor / scrollToIndex) is left to itself.
   */
  std::string compensationKey;
  double compensationDelta = 0.0;
  bool compensate = false;

  if (container->operation && container->operation->target.mode == AnchorMode::Element) {
    compensationKey = container->operation->target.key;
    compensationDelta = container->operation->target.subOffset;
    compensate = true;
  } else if (!container->operation && !container->anchor.key.empty()) {
    compensationKey = container->anchor.key;
    compensationDelta = container->anchor.subOffset;
    compensate = true;
  }

  if (compensate) {
    std::size_t anchorIndex = container->findElementIndexByKey(compensationKey);
    if (anchorIndex != UNDEFINED_INDEX) {
      /*
       * Raw anchor target before the lower clamp. Subtract any header-size change so
       * MVCP does not chase header growth as if it were a scroll. The shift test
       * compares this raw target (not the clamped one) so a top overscroll, where the
       * anchor has not moved, is left alone.
       */
      double rawAnchoredOffset = container->getElementOffset(anchorIndex) + compensationDelta
        - (container->headerSize - container->anchorHeaderSize);
      /*
       * An anchor row whose leading edge is above the viewport start shows only its trailing
       * part; the rows below it are what the reader sees. On its first measurement, hold its
       * trailing edge so the estimate's error lands above the viewport. A row already
       * measured keeps its leading edge held (a reply regenerating in place).
       */
      if (compensationDelta > 0.0) {
        rawAnchoredOffset += container->anchorFirstMeasurementDelta;
      }
      /*
       * Clamp only the lower bound: the total is stale mid-measurement, so an upper
       * clamp would yank the anchor. resolveScroll enforces the upper bound next frame.
       */
      double anchoredOffset = rawAnchoredOffset < 0.0 ? 0.0 : rawAnchoredOffset;

      double currentOffset = container->horizontal ? container->revision.containerOffsetX : container->revision.containerOffsetY;
      if (std::fabs(rawAnchoredOffset - currentOffset) >= OFFSET_MOVED_THRESHOLD) {
        if (container->horizontal) {
          container->revision.containerOffsetX = anchoredOffset;
        } else {
          container->revision.containerOffsetY = anchoredOffset;
        }
        container->containerOffsetCorrected = true;
      }
    }
  }
  container->anchorFirstMeasurementDelta = 0.0;

  /*
   * An inverted list resting on its newest row follows that row as it is measured. The
   * compensation above holds the anchor's leading edge, which for the newest row means
   * any growth lands below the fold: the reader at the bottom is left looking at a
   * cut-off row. resolveScroll pins the true bottom on the next frame, but a reply's
   * final growth (its footer actions and follow-ups mounting once the stream ends) is
   * measured after the last commit, and no next frame comes. Same gate as the pin in
   * resolveScroll: the anchor is the last anchorable row, the reader has not scrolled
   * away (invertedBottomReleased), no finger is down (gestureActive), and no correction
   * is in flight. The bottom is derived from the reflowed geometry because the stored
   * total is stale until the caller refreshes it after the batch.
   *
   * A pending scrollToEnd follows the bottom here too, whatever the anchor: the rows it is
   * converging on (the end of a variable-height list, or messages just appended to a chat)
   * are measured in the layout pass that mounts them, and the next frame may never come.
   * So does an inverted list still settling on the bottom it opened at and resting there
   * (see Container::invertedOpeningPin).
   */
  std::size_t elementsSize = container->revision.elements.size();
  bool followBottom = container->pendingScrollToEnd ||
    (container->inverted && container->invertedOpeningPin && container->restingAtInvertedBottom &&
     !container->invertedBottomReleased && !container->gestureActive && !container->operation);
  if (!followBottom && container->inverted && !container->invertedBottomReleased && !container->gestureActive &&
      !container->operation && !container->anchor.key.empty() &&
      container->isAnchorable(container->anchor.key)) {
    std::size_t anchorIndex = container->findElementIndexByKey(container->anchor.key);
    followBottom = anchorIndex != UNDEFINED_INDEX;
    for (std::size_t nextElementIndex = anchorIndex + 1; followBottom && nextElementIndex < elementsSize; ++nextElementIndex) {
      if (container->isAnchorable(container->revision.elements[nextElementIndex].key)) {
        followBottom = false;
      }
    }
  }
  if (followBottom) {
    std::size_t scanColumns = container->columns > 0 ? container->columns : 1;
    std::size_t scanFrom = elementsSize > scanColumns ? elementsSize - scanColumns : 0;
    double contentEnd = 0.0;
    for (std::size_t nextElementIndex = scanFrom; nextElementIndex < elementsSize; ++nextElementIndex) {
      const Element& nextElement = container->revision.elements[nextElementIndex];
      double elementEnd = container->horizontal
        ? nextElement.offsetX + nextElement.width
        : nextElement.offsetY + nextElement.height;
      if (elementEnd > contentEnd) {
        contentEnd = elementEnd;
      }
    }
    double total = std::max(contentEnd, container->headerSize) + container->footerSize;
    double bottom = total - container->getWindowContainerSize();
    if (bottom < 0.0) {
      bottom = 0.0;
    }
    double currentOffset = container->horizontal ? container->revision.containerOffsetX : container->revision.containerOffsetY;
    if (std::fabs(bottom - currentOffset) >= OFFSET_MOVED_THRESHOLD) {
      if (container->horizontal) {
        container->revision.containerOffsetX = bottom;
      } else {
        container->revision.containerOffsetY = bottom;
      }
      container->containerOffsetCorrected = true;
    }
  }
}

void Virtualizer::updateElementAtIndex(Container* container, std::size_t index, Size size) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  if (applyElementSize(container, index, size)) {
    commitElementSizes(container, index);
  }
}

/*
 * Discard every prediction, both staged and already stamped, returning predicted rows to
 * the ordinary estimate.
 *
 * For a host whose measurements are no longer valid, above all a width change, since text
 * wraps to the row width and every height measured at the old width is now wrong.
 * Keeping those sizes would be worse than never having predicted: the list would sit on
 * confidently wrong geometry that nothing marks as suspect.
 *
 * Rows that have since been measured natively are untouched. Their size is real, it does
 * not come from a prediction, and the host will re-measure them the usual way if the new
 * width changes them.
 */
void Virtualizer::invalidatePredictions(Container* container) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  container->predictedSizes.clear();

  bool anyCleared = false;
  for (Element& nextElement : container->revision.elements) {
    if (!nextElement.predicted) {
      continue;
    }

    nextElement.predicted = false;
    /*
     * Clearing `estimated` too is what actually returns the row to the fallback: it is the
     * flag every sizing pass checks, and a row left estimated would keep its stale
     * predicted size forever.
     */
    nextElement.estimated = false;
    anyCleared = true;
  }

  if (anyCleared) {
    // Every predicted row reverted to the fallback, so reflow the whole list.
    container->markElementSizeDirty(0);
  }
}

/*
 * Consume staged predictions whose rows now exist (see Container::predictedSizes).
 *
 * This walks the predictions, not the elements: the staging map holds a window's worth of
 * rows the host measured ahead, while the element list can be the whole dataset. So the
 * pass costs O(staged), not the O(elements) a per-row lookup would add to every frame, and
 * on the common frame where nothing is staged it is a single empty() check.
 *
 * It runs per frame rather than per reconcile. Predictions arrive whenever the host
 * finishes measuring, which is usually a frame that changed no keys at all, and a
 * reconcile-only drain would leave those sitting in the map until the next data commit,
 * which on a settled list may never come.
 *
 * Covering fresh rows and survivors in one place also keeps reconcileElements ignorant of
 * predictions entirely.
 */
void Virtualizer::consumePredictions(Container* container) {
  if (container->predictedSizes.empty()) {
    return;
  }

  for (auto entry = container->predictedSizes.begin(); entry != container->predictedSizes.end();) {
    std::size_t predictedIndex = container->revision.indexForKey(entry->first);
    if (predictedIndex >= container->revision.elements.size()) {
      ++entry;
      continue;
    }

    Element& predictedElement = container->revision.elements[predictedIndex];

    /*
     * A row that has been laid out natively knows better than any prediction; drop the
     * entry rather than letting it fight the real measurement on a later frame.
     */
    if (!predictedElement.measured &&
        (predictedElement.width != entry->second.width || predictedElement.height != entry->second.height)) {
      predictedElement.width = entry->second.width;
      predictedElement.height = entry->second.height;
      /*
       * Sizes changed outside layoutElements' own loop, which cannot see this through its
       * estimated checks; without this the new geometry would not be reflowed until
       * something else dirtied the list.
       */
      container->markElementSizeDirty(predictedIndex);
    }
    if (!predictedElement.measured) {
      predictedElement.estimated = true;
      predictedElement.predicted = true;
    }

    entry = container->predictedSizes.erase(entry);
  }
}

void Virtualizer::reconcileElements(Container* container, const std::vector<std::string>& nextKeys) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  std::vector<Element>& prevElements = container->revision.elements;

  /*
   * Locate surviving elements through the key->index map the previous reconcile already
   * built (Revision::elementIndexByKey) rather than constructing a throwaway
   * key->Element map, which would duplicate every key string and move every Element twice
   * and dominate the cost of an ordinary append-one or prepend-page on a large list. The
   * map is only ever written here and elements are never reordered in place, so it stays
   * in step with `prevElements`; the key check below makes that an assertion rather than
   * an assumption.
   */
  const std::unordered_map<std::string, std::size_t>& prevIndexByKey =
    container->revision.elementIndexByKey;

  /*
   * Fast path for a pure append.
   *
   * The general path below rebuilds both the element vector and the whole key->index map
   * from scratch. The map rebuild is the expensive part by a wide margin (one heap node
   * allocated and one freed per row, plus a hash and a probe each), and at 100k rows it is
   * the bulk of a ~6ms reconcile. That is a dropped frame every time a chat appends a
   * message or a page paginates in, which is the most common data change a list sees.
   *
   * An append needs none of it. Detect the shape with one forward pass of string compares
   * (tens of microseconds at 100k, versus milliseconds to rebuild), then extend the existing
   * vector and map in place: the surviving rows keep their indices, so every map entry
   * already present stays correct.
   *
   * Excludes the empty-previous case, which is a cold start rather than an append:
   * survivorCount would be 0 and the general path's frozen-average reset has to run.
   */
  if (!prevElements.empty() && nextKeys.size() > prevElements.size()) {
    bool isAppend = true;
    for (std::size_t nextElementIndex = 0; nextElementIndex < prevElements.size(); ++nextElementIndex) {
      if (prevElements[nextElementIndex].key != nextKeys[nextElementIndex]) {
        isAppend = false;
        break;
      }
    }

    if (isAppend) {
      prevElements.reserve(nextKeys.size());
      container->revision.elementIndexByKey.reserve(nextKeys.size());

      for (std::size_t nextElementIndex = prevElements.size(); nextElementIndex < nextKeys.size(); ++nextElementIndex) {
        prevElements.emplace_back();
        prevElements.back().key = nextKeys[nextElementIndex];
        prevElements.back().index = nextElementIndex;
        // setIndexForKey keeps the first occurrence of a duplicate, as the general path does.
        container->revision.setIndexForKey(nextKeys[nextElementIndex], nextElementIndex);
      }

      container->geometryVersion++;
      container->elementsStructureDirty = true;
      return;
    }
  }

  /*
   * Fast path for a pure prepend.
   *
   * Paginating older messages into a chat is a prepend, and it is the shape that hurts
   * most: the whole existing list survives, yet the general path would throw away the
   * key->index map and build a new one node by node purely because every index shifted.
   *
   * Shifting the values of the existing map instead is an O(rows) walk of nodes that are
   * already allocated (no malloc, no free, no hashing, no key copies), against a rebuild
   * that allocates and frees one node per row. The element vector still has to shift (it is
   * contiguous), but that is a move of already-owned memory and is an order of magnitude
   * cheaper than the map churn it replaces.
   */
  if (!prevElements.empty() && nextKeys.size() > prevElements.size()) {
    std::size_t prependCount = nextKeys.size() - prevElements.size();

    bool isPrepend = true;
    for (std::size_t nextElementIndex = 0; nextElementIndex < prevElements.size(); ++nextElementIndex) {
      if (prevElements[nextElementIndex].key != nextKeys[nextElementIndex + prependCount]) {
        isPrepend = false;
        break;
      }
    }

    /*
     * A prepended key that already exists further down would change which occurrence the
     * map resolves to, and the general path's first-occurrence-wins rule is easier to keep
     * by just taking the slow path for it. Rare enough to be worth the simplicity.
     */
    if (isPrepend) {
      for (std::size_t nextElementIndex = 0; nextElementIndex < prependCount; ++nextElementIndex) {
        if (container->revision.indexForKey(nextKeys[nextElementIndex]) != UNDEFINED_INDEX) {
          isPrepend = false;
          break;
        }
      }
    }

    if (isPrepend) {
      /*
       * Every surviving row's index rises by prependCount. Rather than walk the map adding
       * prependCount to each of its values (an O(rows) chase through scattered nodes),
       * shift the bias once: every stored value is read back through it, so the effect is
       * identical and the existing nodes are not touched at all.
       */
      container->revision.indexBias -= prependCount;

      prevElements.insert(prevElements.begin(), prependCount, Element{});

      container->revision.elementIndexByKey.reserve(nextKeys.size());
      for (std::size_t nextElementIndex = 0; nextElementIndex < prependCount; ++nextElementIndex) {
        prevElements[nextElementIndex].key = nextKeys[nextElementIndex];
        container->revision.setIndexForKey(nextKeys[nextElementIndex], nextElementIndex);
      }

      /*
       * Element::index is not renumbered here. Setting elementsStructureDirty forces
       * layoutElements to reflow from row 0, and recomputeElementOffsets assigns `index` to
       * every row as it goes, so a renumber loop here would be a second O(rows) pass writing
       * exactly the values the first one is about to write. Nothing reads Element::index in
       * between: update() goes reconcile -> consumePredictions (which resolves through
       * elementIndexByKey) -> measure -> layoutElements.
       */
      container->geometryVersion++;
      container->elementsStructureDirty = true;
      return;
    }
  }

  std::vector<Element> nextElements;
  nextElements.reserve(nextKeys.size());

  /*
   * Rebuild the key->index map alongside the element list so anchor lookups stay O(1)
   * (see Container::findElementIndexByKey). emplace keeps the first occurrence of a
   * duplicate key.
   */
  std::unordered_map<std::string, std::size_t> nextElementIndexByKey;
  nextElementIndexByKey.reserve(nextKeys.size());

  /*
   * The old map's values are biased by the bias in force when they were written, so they
   * have to be unbiased to be usable here. The reset to zero happens below, once the old
   * map has been read for the last time; doing it up front would make every lookup in
   * this loop resolve to the wrong row.
   */
  const std::size_t prevIndexBias = container->revision.indexBias;

  std::size_t survivorCount = 0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < nextKeys.size(); ++nextElementIndex) {
    const std::string& nextKey = nextKeys[nextElementIndex];

    /*
     * emplace keeps the first occurrence of a duplicate key, and its `second` therefore
     * says whether this is that first occurrence.
     *
     * That single bool is all the survivor check needs, with no separate "already consumed"
     * bitmap. prevIndexByKey also maps a key to its first occurrence, so a previous element
     * can only ever be claimed by the first nextKeys occurrence of its key, which is exactly
     * the case emplace reports. A later duplicate gets a fresh Element.
     */
    bool firstOccurrence = nextElementIndexByKey.emplace(nextKey, nextElementIndex).second;

    auto prevElementEntry = nextKey.empty() ? prevIndexByKey.end() : prevIndexByKey.find(nextKey);
    std::size_t prevElementIndex = prevElementEntry != prevIndexByKey.end()
      ? prevElementEntry->second - prevIndexBias
      : UNDEFINED_INDEX;
    bool survives =
      firstOccurrence &&
      prevElementIndex < prevElements.size() &&
      prevElements[prevElementIndex].key == nextKey;

    if (survives) {
      /*
       * Carries the key, measured size and estimated/measured/predicted flags across
       * untouched. Moved straight into place rather than via a named local, which would
       * cost a second move of the whole Element per survivor.
       */
      nextElements.push_back(std::move(prevElements[prevElementIndex]));
      nextElements.back().index = nextElementIndex;
      survivorCount++;
    } else {
      nextElements.emplace_back();
      nextElements.back().key = nextKey;
      nextElements.back().index = nextElementIndex;
    }
  }

  container->revision.elements = std::move(nextElements);
  container->revision.elementIndexByKey = std::move(nextElementIndexByKey);
  // The rebuilt map holds true indices.
  container->revision.indexBias = 0;

  /*
   * Positions and the key->index mapping both changed, so any geometry an integration
   * derived from the previous element list is stale (see Container::geometryVersion).
   */
  container->geometryVersion++;

  /*
   * A structural change (insert/remove/reorder) always requires the next layoutElements
   * to recompute offsets, even if no element needs a fresh fallback size (see
   * Virtualizer::layoutElements).
   */
  container->elementsStructureDirty = true;

  /*
   * Zero surviving elements means every key was replaced (a full dataset swap on a
   * reused Container, or an empty-then-refill), so the frozen average no longer
   * describes any content still in the list. Reset it so the next recomputeTotalSize
   * re-freezes from the new dataset's real measurements instead of chasing stale
   * old-dataset sizing. A partial reconcile (some survivors) keeps the existing average,
   * since it still reflects real, still-present content.
   */
  if (survivorCount == 0) {
    container->revision.averageElementWidth = 0.0;
    container->revision.averageElementHeight = 0.0;
    container->revision.measuredRealCount = 0;
    container->revision.measuredRealTotalWidth = 0.0;
    container->revision.measuredRealTotalHeight = 0.0;
  }
}

void Virtualizer::captureAnchor(Container* container, double inputOffset) {
  container->anchor = Anchor{"", 0.0, AnchorMode::Element};

  const std::vector<Element>& prevElements = container->revision.elements;
  if (prevElements.empty()) {
    return;
  }

  auto elementOffsetOf = [&](const Element& element) {
    return container->horizontal ? element.offsetX : element.offsetY;
  };
  auto elementSizeOf = [&](const Element& element) {
    return container->horizontal ? element.width : element.height;
  };

  /*
   * The anchor is the first anchorable element whose trailing edge is past the current
   * scroll offset, i.e. the stable content row sitting at the top/left of the viewport.
   * Decoration rows (date pills, unread dividers, ...) are skipped so a key change on one
   * never perturbs the maintained position; the scan walks down to the next content row.
   * firstPast remembers the literal viewport-top row so a degenerate viewport of nothing
   * but decoration still anchors somewhere instead of failing.
   *
   * Offsets are non-decreasing by index for single-column layouts (see
   * recomputeElementOffsets), so "first element whose trailing edge is past inputOffset"
   * is a monotonic boundary that can be binary-searched instead of scanned from element 0,
   * which keeps captureAnchor cheap deep into a very large list. Multi-column (grid)
   * layouts interleave tracks and lose that monotonicity, so they scan from 0.
   */
  std::size_t scanStart = 0;
  if (container->columns <= 1) {
    std::size_t low = 0;
    std::size_t high = prevElements.size();
    while (low < high) {
      std::size_t mid = low + (high - low) / 2;
      const Element& element = prevElements[mid];
      if (elementOffsetOf(element) + elementSizeOf(element) <= inputOffset) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    scanStart = low;
  }

  const Element* firstPast = nullptr;
  for (std::size_t prevElementIndex = scanStart; prevElementIndex < prevElements.size(); ++prevElementIndex) {
    const Element& prevElement = prevElements[prevElementIndex];
    double elementOffset = elementOffsetOf(prevElement);
    double elementSize = elementSizeOf(prevElement);

    if (elementOffset + elementSize <= inputOffset) {
      continue;
    }
    if (firstPast == nullptr) {
      firstPast = &prevElement;
    }
    if (container->isAnchorable(prevElement.key)) {
      /*
       * A row nobody has measured or predicted only carries the fallback size, and its real
       * size can arrive in several steps (a mounting row first lays out to zero). Holding it
       * moves everything below it by each step that lands after its first measurement. Rows
       * prepended while the view sits at the top edge are the usual case: the correction puts
       * the viewport top inside the last new row, with the content the reader was looking at
       * just below it. When a row with a trusted size starts inside the viewport, anchor that
       * row instead, so the content below the guess holds still. Single-column, non-inverted
       * lists only: grids interleave tracks, and an inverted list reads its anchor to decide
       * whether it rests on its newest row.
       */
      const Element* anchorElement = &prevElement;
      if (container->columns <= 1 && !container->inverted && !prevElement.measured && !prevElement.predicted) {
        double viewportEnd = inputOffset + container->getWindowContainerSize();
        for (std::size_t laterIndex = prevElementIndex + 1; laterIndex < prevElements.size(); ++laterIndex) {
          const Element& laterElement = prevElements[laterIndex];
          if (elementOffsetOf(laterElement) >= viewportEnd) {
            break;
          }
          if (container->isAnchorable(laterElement.key) && (laterElement.measured || laterElement.predicted)) {
            anchorElement = &laterElement;
            break;
          }
        }
      }
      container->anchor =
        Anchor{anchorElement->key, inputOffset - elementOffsetOf(*anchorElement), AnchorMode::Element};
      return;
    }
  }

  /*
   * No anchorable row is in or below the viewport. Prefer the last anchorable row overall
   * (the scrolled-past-everything case, and the all-decoration-below fallback); else the
   * literal viewport-top row even if it is decoration; else the very last row. This always
   * yields a non-empty anchor when elements exist.
   */
  for (auto reverse = prevElements.rbegin(); reverse != prevElements.rend(); ++reverse) {
    if (container->isAnchorable(reverse->key)) {
      container->anchor = Anchor{reverse->key, inputOffset - elementOffsetOf(*reverse), AnchorMode::Element};
      return;
    }
  }
  const Element& fallback = firstPast != nullptr ? *firstPast : prevElements.back();
  container->anchor = Anchor{fallback.key, inputOffset - elementOffsetOf(fallback), AnchorMode::Element};
}

bool Virtualizer::resolveScroll(
  Container* container,
  const std::string& anchorKey,
  double anchorDelta,
  bool hadElementsBefore,
  bool offsetConfirmed) {
  std::size_t elementsSize = container->revision.elements.size();
  container->containerOffsetCorrected = false;

  /*
   * Reset the inverted bottom anchoring (and drop any pending target) when the
   * list is emptied so it sticks to the bottom again once content arrives
   */
  if (elementsSize == 0) {
    container->invertedInitialized = false;
    container->invertedBottomReleased = false;
    container->invertedOpeningPin = false;
    container->operation.reset();
    container->pendingScrollToEnd = false;
  }

  double windowSize = container->getWindowContainerSize();
  double totalSize = container->horizontal ? container->revision.totalContainerWidth : container->revision.totalContainerHeight;
  double currentOffset = container->horizontal ? container->revision.containerOffsetX : container->revision.containerOffsetY;
  double maxOffset = totalSize - windowSize;
  if (maxOffset < 0.0) {
    maxOffset = 0.0;
  }

  /*
   * Track the total every frame so step 2b can tell when the bottom stops growing.
   */
  double prevTotalForScrollToEnd = container->pendingScrollToEndLastTotal;
  container->pendingScrollToEndLastTotal = totalSize;

  auto clampOffset = [&](double offset) {
    if (offset < 0.0) {
      offset = 0.0;
    }
    if (offset > maxOffset) {
      offset = maxOffset;
    }
    return offset;
  };
  auto writeOffset = [&](double offset) {
    if (container->horizontal) {
      container->revision.containerOffsetX = offset;
    } else {
      container->revision.containerOffsetY = offset;
    }
  };

  /*
   * Create an operation id, but preserve the current one when this is the same correction
   * being reasserted (same type, and same anchored key for Element anchors). A fixed-offset
   * pin like BottomPin is requested again every frame while it converges; keeping its id stable
   * is what lets the host's echo match it across the multi-frame settle. A genuinely new
   * intent (different type, or a new anchored key) gets a fresh id.
   */
  auto operationId = [&](OperationType type, AnchorMode mode, const std::string& key) -> std::uint64_t {
    if (container->operation && container->operation->type == type &&
        container->operation->target.mode == mode &&
        (mode != AnchorMode::Element || container->operation->target.key == key)) {
      return container->operation->id;
    }
    return container->nextOperationId++;
  };

  /*
   * Start (or reassert) the in-flight correction. A fixed-offset intent (bottom pin,
   * scrollToEnd, shrink clamp) carries an EndEdge anchor that resolves to maxOffset;
   * the resolution loop below drives the view there until it confirms arrival.
   */
  auto requestFixed = [&](OperationType type) {
    container->operation =
      Operation{operationId(type, AnchorMode::EndEdge, ""), type, Anchor{"", 0.0, AnchorMode::EndEdge}};
  };

  /*
   * Start (or reassert) an anchor-driven correction (MVCP, scrollToIndex). The target
   * is an Element anchor, recomputed from its key each frame so it tracks the anchor
   * as nearby elements are measured/resized while the correction is in flight.
   */
  auto requestAnchor = [&](OperationType type, const std::string& key, double delta) {
    container->operation =
      Operation{operationId(type, AnchorMode::Element, key), type, Anchor{key, delta, AnchorMode::Element}};
  };

  /*
   * 0. Content shrank below the current scroll position (e.g. a tree collapse-all),
   *    leaving the view scrolled past the new end. Drive back to the bottom. Keying on
   *    the shrink (not offset>maxOffset alone) avoids fighting an overscroll bounce.
   *    Driven through the in-flight operation so it survives recommits; step 3 clears it on arrival.
   */
  if (!container->inverted && elementsSize > 0 &&
      currentOffset > maxOffset + OFFSET_MOVED_THRESHOLD &&
      totalSize < prevTotalForScrollToEnd) {
    requestFixed(OperationType::ShrinkClamp);
  }

  /*
   * 1. scrollToIndex aligns the element to the viewport start
   */
  if (container->scrollToIndexTarget != UNDEFINED_INDEX) {
    if (container->scrollToIndexTarget < elementsSize) {
      /*
       * Drive toward the target element (anchored), not a one-shot offset: its offset
       * is estimate-built, so anchoring converges onto it as the region is measured.
       */
      const std::string targetKey = container->getElementAtIndex(container->scrollToIndexTarget).key;
      requestAnchor(OperationType::ScrollToKey, targetKey, 0.0);
      /*
       * The explicit target takes over from the inverted bottom anchor, but only when
       * it actually applied (an out-of-range target must not disable the pin).
       */
      container->invertedInitialized = true;
      container->invertedOpeningPin = false;
    }
    container->scrollToIndexTarget = UNDEFINED_INDEX;
  }

  /*
   * 2. Inverted lists stick to the bottom until the view reaches it, repinning as the
   *    total grows. Only pin once the window size is known, to avoid targeting total-0.
   */
  if (container->inverted && !container->invertedInitialized && elementsSize > 0 && windowSize > 0.0) {
    requestFixed(OperationType::BottomPin);
    container->invertedOpeningPin = true;
    /*
     * Settle only once there is a scrollable bottom we have reached; while content
     * still fits the window keep repinning so a growing list is not stuck at the top.
     * The BottomPin target is maxOffset (the EndEdge anchor resolves there).
     */
    if (offsetConfirmed &&
        totalSize > windowSize &&
        std::fabs(currentOffset - maxOffset) < OFFSET_ARRIVED_THRESHOLD) {
      container->invertedInitialized = true;
    }
  }

  /*
   * 2b. scrollToEnd retargets maxOffset every frame as the total grows, so it lands
   *     on the true end. It settles only once the view reaches the bottom on a frame
   *     where the total did not change and the last row carries a trusted size: a last
   *     row still on its estimate is measured by the layout pass that mounts it, which
   *     moves the bottom after this frame (commitElementSizes follows it while this is
   *     pending). A drag cancels it; momentum does not.
   */
  if (container->pendingScrollToEnd && elementsSize > 0 && windowSize > 0.0) {
    container->invertedOpeningPin = false;
    bool atBottom = std::fabs(currentOffset - maxOffset) < OFFSET_ARRIVED_THRESHOLD;
    bool totalStable = totalSize == prevTotalForScrollToEnd;
    if (atBottom && totalStable && container->hasTrustedSize(elementsSize - 1)) {
      container->pendingScrollToEnd = false;
    } else {
      requestFixed(OperationType::ScrollToEnd);
    }
  }

  /*
   * 3. Drive the in-flight operation until the view confirms it. The target is
   *    rederived from the operation's anchor each frame (an EndEdge anchor tracks
   *    maxOffset; an Element anchor tracks its key), so a stale offset on a later frame
   *    keeps requesting the same target instead of cancelling it. The operation clears
   *    when its anchor's key vanishes or the view arrives, then falls through to MVCP.
   */
  if (container->operation) {
    double rawTarget = 0.0;
    if (!resolveAnchorOffset(container, *container->operation, maxOffset, rawTarget)) {
      container->operation.reset();
    } else {
      double target = clampOffset(rawTarget);
      if (offsetConfirmed && std::fabs(currentOffset - target) < OFFSET_ARRIVED_THRESHOLD) {
        container->operation.reset();
      } else {
        // Offset moved, so the caller must remeasure the window for the new offset.
        container->containerOffsetCorrected = true;
        writeOffset(target);
        return true;
      }
    }
  }

  /*
   * 4. Maintain the captured anchor element's viewport position (MVCP) when no
   *    correction is already in flight. A real shift (content inserted/removed
   *    above the viewport) starts a new anchor-driven correction; an anchor that
   *    lands where the user already is (steady scroll, bounce) does nothing.
   */
  if (hadElementsBefore && !anchorKey.empty()) {
    std::size_t anchorIndex = container->findElementIndexByKey(anchorKey);
    if (anchorIndex != UNDEFINED_INDEX) {
      /*
       * An inverted list resting on its last row sits at the visual bottom. There, MVCP
       * must anchor to the true bottom (maxOffset), not the row's captured position:
       * content measured async after first paint (native markdown reporting its
       * height) grows the row, and maintaining its old position leaves the bottom edge a
       * header/footer-sized gap off the fold (the initial-load shift) and oscillates
       * against the bottom pin. Targeting maxOffset keeps the newest row pinned through the
       * settle and auto-follows new content while the user is at the bottom. Every other
       * anchor (scrolled up, prepend) keeps the normal maintain-its-position behaviour.
       *
       * "Last row" means the last anchorable row, not the raw last index: a trailing
       * decoration row (bottom padding etc.) would otherwise defeat this check, since
       * captureAnchor never anchors to it. The anchor is the last anchorable row exactly
       * when it is anchorable and nothing anchorable follows it; testing it that way
       * bounds the walk to the rows after the anchor instead of rescanning the list.
       *
       * Never while the user has scrolled up off the bottom (invertedBottomReleased): the
       * anchor can still be the last anchorable row there (a reply taller than the
       * viewport), and pinning it would pull the view out from under the reader. Nor on
       * a gesture frame (gestureActive): inside the follow band the flag is not yet
       * released, and re-pinning under a moving finger snaps the content back on every
       * touch frame. The pin waits for the finger to lift.
       *
       * A list still settling on the bottom it opened at anchors to the true bottom whatever
       * the anchor row is, as long as it rested there (see Container::invertedOpeningPin).
       */
      bool invertedBottomAnchor = false;
      if (container->inverted && !container->invertedBottomReleased && !container->gestureActive) {
        if (container->invertedOpeningPin && container->restingAtInvertedBottom) {
          invertedBottomAnchor = true;
        } else if (container->isAnchorable(container->revision.elements[anchorIndex].key)) {
          invertedBottomAnchor = true;
          for (std::size_t nextElementIndex = anchorIndex + 1; nextElementIndex < elementsSize; ++nextElementIndex) {
            if (container->isAnchorable(container->revision.elements[nextElementIndex].key)) {
              invertedBottomAnchor = false;
              break;
            }
          }
        }
      }
      double rawAnchoredOffset = invertedBottomAnchor
        ? maxOffset
        : container->getElementOffset(anchorIndex) + anchorDelta
            - (container->headerSize - container->anchorHeaderSize);
      if (std::fabs(rawAnchoredOffset - currentOffset) >= OFFSET_MOVED_THRESHOLD) {
        if (invertedBottomAnchor) {
          requestFixed(OperationType::BottomPin);
        } else {
          requestAnchor(OperationType::MaintainAnchor, anchorKey, anchorDelta);
        }
        container->containerOffsetCorrected = true;
        writeOffset(clampOffset(rawAnchoredOffset));
        return true;
      }
    }
  }

  return false;
}

}
