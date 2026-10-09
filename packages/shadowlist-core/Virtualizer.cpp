#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/VirtualizerInternal.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>

namespace azimgd::shadowlist {

/*
 * Debug only. Gives the key at an index so native and JS logs can be matched by content,
 * since an index points at different rows during a prepend. It is only used inside SL_LOG.
 * It is marked maybe_unused to keep Android builds with -Werror=unused-function clean.
 */
namespace {
[[maybe_unused]] const char* debugKeyAt(const Container& container, std::size_t index) {
  if (index < container.revision.elements.size()) {
    const std::string& key = container.revision.elements[index].key;
    return key.empty() ? "(empty)" : key.c_str();
  }
  return "(oob)";
}

/*
 * Give a row the fallback size if it has none and widen the measured range to include it.
 * Returns false when there is no estimate to give it.
 */
bool visitMeasuredElement(
  Container& container,
  std::size_t nextElementIndex,
  std::size_t& measuredMinIndex,
  std::size_t& measuredMaxIndex) {
  Element& nextElement = container.revision.elements[nextElementIndex];

  if (!nextElement.estimated) {
    auto [width, height] = effectiveFallbackSize(container);

    if (width == 0.0 && height == 0.0) {
      return false;
    }

    /*
     * Layout already gave unmeasured rows this size. Usually nothing changes here.
     * Only mark sizes dirty on a real change. Otherwise dragging the scroll indicator
     * would reflow the whole list on every frame.
     */
    if (nextElement.width != width || nextElement.height != height) {
      nextElement.width = width;
      nextElement.height = height;
      // The size changed outside the layout loop. Make sure it reflows offsets.
      container.markElementSizeDirty(nextElementIndex);
    }
    nextElement.estimated = true;
  }

  if (measuredMinIndex == UNDEFINED_INDEX || nextElementIndex < measuredMinIndex) {
    measuredMinIndex = nextElementIndex;
  }
  if (measuredMaxIndex == UNDEFINED_INDEX || nextElementIndex > measuredMaxIndex) {
    measuredMaxIndex = nextElementIndex;
  }
  return true;
}

/*
 * Turn the running operation's target into an unclamped offset for this frame.
 * An end edge target is maxOffset. A row target is the row's offset plus its sub offset,
 * worked out each frame so it follows the row while nearby rows get measured.
 * Returns false when the row's key is gone. Header size changes are handled elsewhere.
 */
bool resolveAnchorOffset(Container& container, const Operation& operation, double maxOffset, double& outOffset) {
  if (operation.target.mode != AnchorMode::Element) {
    outOffset = maxOffset;
    return true;
  }
  std::size_t anchorIndex = container.findElementIndexByKey(operation.target.key);
  if (anchorIndex == UNDEFINED_INDEX) {
    return false;
  }
  outOffset = container.getElementOffset(anchorIndex) + resolveAnchorSubOffset(container, operation, anchorIndex);
  return true;
}
}

void Virtualizer::update(Container& container, const FrameInput& input) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  // The keys may be borrowed from the caller. They are only valid during this call.
  const std::vector<std::string>& inputKeys = input.getKeyList();
  const std::vector<std::string>& inputNonAnchorableKeys = input.getNonAnchorableKeyList();

  SL_LOG("update: keys=%zu prevElements=%zu off=(%.1f,%.1f) win=(%.1f,%.1f) inv=%d cols=%zu hdr=%.1f ftr=%.1f invInit=%d total=%.1f dirtyFrom=%zd enabled=%d corrected=%d coreOff=%.1f",
    inputKeys.size(), container.revision.elements.size(),
    input.containerOffsetX, input.containerOffsetY,
    input.windowContainerWidth, input.windowContainerHeight,
    input.inverted ? 1 : 0, input.columns, input.headerSize, input.footerSize,
    container.invertedInitialized ? 1 : 0,
    container.horizontal ? container.revision.totalContainerWidth : container.revision.totalContainerHeight,
    static_cast<std::ptrdiff_t>(container.elementsSizeDirtyFromIndex), input.containerOffsetEnabled ? 1 : 0,
    container.containerOffsetCorrected ? 1 : 0, container.getContainerOffset());

  // Remember the old header size so a change can be settled after the rows reflow.
  double previousHeaderSize = container.headerSize;

  // Flipping the list order moves the bottom. Start following the bottom again.
  if (container.inverted != input.inverted) {
    container.invertedBottomReleased = false;
  }

  container.inverted = input.inverted;
  container.horizontal = input.horizontal;
  container.columns = input.columns;
  container.overscan = input.overscan;
  container.headerSize = input.headerSize;
  container.footerSize = input.footerSize;
  // Compare first. An unchanged list costs no copy.
  const std::vector<std::size_t>& inputStickyIndices = input.getStickyIndexList();
  if (container.stickyIndices != inputStickyIndices) {
    container.stickyIndices = inputStickyIndices;
  }
  container.startReachedThreshold = input.startReachedThreshold;
  container.endReachedThreshold = input.endReachedThreshold;
  // Compare first. Rules rarely change and a copy costs an allocation.
  const std::vector<ViewableRule>& inputViewableRules = input.getViewableRules();
  if (container.viewableRules != inputViewableRules) {
    container.viewableRules = inputViewableRules;
  }
  container.estimatedElementSize = input.estimatedElementSize;
  container.snapToItem = input.snapToItem;
  container.snapAlignment = input.snapAlignment;

  /*
   * Decoration rows must never become the anchor. This runs before captureAnchor reads it,
   * and is rebuilt every frame so a row switching roles takes effect right away.
   */
  if (!input.nonAnchorableKeysUnchanged &&
      (container.nonAnchorableKeys.size() != inputNonAnchorableKeys.size() ||
       !std::all_of(inputNonAnchorableKeys.begin(), inputNonAnchorableKeys.end(),
         [&](const std::string& ignoredKey) { return container.nonAnchorableKeys.count(ignoredKey) != 0; }))) {
    container.nonAnchorableKeys.clear();
    for (const std::string& ignoredKey : inputNonAnchorableKeys) {
      container.nonAnchorableKeys.insert(ignoredKey);
    }
  }

  double inputOffset = container.horizontal ? input.containerOffsetX : input.containerOffsetY;

  /*
   * An enabled offset is our own write coming back before the host applied it, not a host
   * report. It can't confirm the correction or count as gesture travel.
   */
  bool coreOffsetWrite = input.containerOffsetEnabled;

  // A running correction survives while the offset has not moved. The user is not scrolling.
  bool userMovedOffset = !coreOffsetWrite &&
    std::fabs(inputOffset - container.lastReportedOffset) >= OFFSET_MOVED_THRESHOLD;
  /*
   * When the user takes over, drop any running correction and stop pinning to the bottom.
   * The host's gesture phase decides. userScrolled only counts with a real move. A stale
   * flag can't cancel a correction and hosts without a phase still work.
   */
  bool gestureTakeover =
    (input.userScrolled && userMovedOffset) ||
    input.scrollPhase == ScrollPhase::Dragging ||
    input.scrollPhase == ScrollPhase::Settling;
  /*
   * Only a finger, or a user move that isn't momentum, cancels a scroll command.
   * A command sent during a fling must still run, even though its first frames report
   * the settling phase. The host stops the fling itself.
   */
  bool dragTakeover =
    input.scrollPhase == ScrollPhase::Dragging ||
    (input.userScrolled && userMovedOffset && input.scrollPhase != ScrollPhase::Settling);
  bool scrollCommandInFlight = container.operation &&
    (container.operation->type == OperationType::ScrollToEnd ||
     container.operation->type == OperationType::ScrollToKey ||
     container.operation->type == OperationType::ScrollToStart);
  /*
   * A correction that keeps the visible content in place is not something a gesture cancels.
   * A prepend during a fling runs update twice on the same settling report, and dropping the
   * correction on the second pass would leave the view unmoved. So keep it until the host
   * reports back its commit token, and shift it by any momentum travel before then.
   */
  bool maintainingAnchor = container.operation &&
    container.operation->type == OperationType::MaintainAnchor &&
    container.operation->target.mode == AnchorMode::Element;
  bool echoesOperation = container.operation && !coreOffsetWrite &&
    input.commitToken == container.operation->id;
  /*
   * Scrolling reported before the host applies the correction moves its target along.
   * A correction started during a gesture keeps following after the frames go idle, since
   * the end of a bounce still moves the offset. Other idle moves, like a host clamping to
   * shorter content, are not followed.
   */
  if (maintainingAnchor && !echoesOperation && userMovedOffset &&
      (gestureTakeover || container.operation->id == container.gestureOperationId)) {
    container.operation->target.subOffset += inputOffset - container.lastReportedOffset;
  }
  // A correction made during a gesture is done once the host reports it back while idle.
  if (maintainingAnchor && echoesOperation && !gestureTakeover &&
      container.operation->id == container.gestureOperationId) {
    container.operation.reset();
  }
  if (gestureTakeover) {
    bool keepsAnchorCorrection = maintainingAnchor && !echoesOperation;
    if (!keepsAnchorCorrection && (!scrollCommandInFlight || dragTakeover)) {
      container.operation.reset();
    }
    if (dragTakeover) {
      container.pendingScrollToEnd = false;
    }
    container.invertedInitialized = true;
    container.invertedOpeningPin = false;
  }
  container.gestureActive = gestureTakeover;

  // Whether an inverted list rests at its bottom. Rows appended there get followed.
  bool restingAtBottom = false;

  /*
   * Pin or release the inverted list's bottom.
   * Judge it with last frame's total and window together. Mixing frames would make a
   * keyboard opening or a rotation release a reader who never moved. That is also why
   * this runs before the reconcile below.
   * Only a gesture releases the pin. Pinning again needs a real move toward the bottom made
   * by the user or a scroll command. Shrinking content or our own correction can't
   * quietly pin the reader again.
   */
  if (container.inverted) {
    double previousOffset = container.lastReportedOffset;
    double bottomTotal = container.horizontal ? container.revision.totalContainerWidth
                                               : container.revision.totalContainerHeight;
    double bottomWindow = container.horizontal ? container.revision.windowContainerWidth
                                                : container.revision.windowContainerHeight;
    bool atBottom = atInvertedBottom(inputOffset, bottomTotal, bottomWindow);
    // Content that fits the window has no bottom to leave. A bounce must not release it.
    bool scrollable = bottomTotal > bottomWindow;

    if (gestureTakeover && scrollable && !atBottom) {
      container.invertedBottomReleased = true;
    } else if (container.invertedBottomReleased && atBottom &&
        inputOffset >= previousOffset + OFFSET_MOVED_THRESHOLD &&
        (input.userScrolled || container.pendingScrollToEnd ||
         (container.operation && (container.operation->type == OperationType::ScrollToEnd ||
                                   container.operation->type == OperationType::ScrollToKey)))) {
      container.invertedBottomReleased = false;
    }

    restingAtBottom = !gestureTakeover && !container.invertedBottomReleased &&
      container.invertedInitialized && !container.revision.elements.empty() &&
      bottomWindow > 0.0 && atBottom;
  }
  // resolveScroll reads this to hold the bottom while the list opens.
  container.restingAtInvertedBottom = restingAtBottom;
  // Our own write is not where the host is. Don't record it.
  if (!coreOffsetWrite) {
    container.lastReportedOffset = inputOffset;
  }

  // Capture the anchor row so the same content stays in view through the reconcile.
  bool hadElementsBefore = !container.revision.elements.empty();
  captureAnchor(container, inputOffset);

  std::string anchorKey = container.anchor.key;
  double anchorDelta = container.anchor.subOffset;

  /*
   * Debug only. Log the frame where the keys changed, which is when JS and native indices
   * drift apart. oldFront@newIdx is how many rows were prepended above the old top row.
   */
#if SHADOWLIST_DEBUG_LOG
  {
    std::size_t previousSize = container.revision.elements.size();
    std::size_t nextSize = inputKeys.size();
    bool frontChanged = previousSize && nextSize && container.revision.elements.front().key != inputKeys.front();
    if (previousSize != nextSize || frontChanged) {
      std::size_t previousFrontNextIndex = UNDEFINED_INDEX;
      if (previousSize) {
        const std::string& previousFront = container.revision.elements.front().key;
        for (std::size_t nextElementIndex = 0; nextElementIndex < nextSize; ++nextElementIndex) {
          if (inputKeys[nextElementIndex] == previousFront) {
            previousFrontNextIndex = nextElementIndex;
            break;
          }
        }
      }
      SL_LOG("  RECONCILE: size %zu->%zu front '%s'->'%s' oldFront@newIdx=%zd anchorKey=%s anchorDelta=%.1f",
        previousSize, nextSize,
        previousSize ? container.revision.elements.front().key.c_str() : "(none)",
        nextSize ? inputKeys.front().c_str() : "(none)",
        static_cast<std::ptrdiff_t>(previousFrontNextIndex), anchorKey.empty() ? "(none)" : anchorKey.c_str(), anchorDelta);
    }
  }
#endif

  /*
   * Match the rows to the new keys. Most commits don't change the keys. Compare them
   * first and skip the rebuild, which is the main cost of each commit after a prepend.
   */
  bool keysChanged = !input.keysUnchanged && container.revision.elements.size() != inputKeys.size();
  if (!keysChanged && !input.keysUnchanged) {
    for (std::size_t nextElementIndex = 0; nextElementIndex < inputKeys.size(); ++nextElementIndex) {
      if (container.revision.elements[nextElementIndex].key != inputKeys[nextElementIndex]) {
        keysChanged = true;
        break;
      }
    }
  }
  if (keysChanged) {
    // New keys end the opening settle.
    container.invertedOpeningPin = false;
    /*
     * With followAppends, an inverted list resting at the bottom scrolls to new rows
     * appended below. By default they land below the fold. Following runs as a scroll to
     * the end, which yields to a drag. A reply growing in place is not an append.
     */
    std::string previousLastKey = restingAtBottom && input.followAppends
      ? container.revision.elements.back().key
      : std::string();
    std::vector<Anchor> fallbackAnchors = captureFallbackAnchors(container, inputOffset);
    std::size_t survivors = reconcileElements(container, inputKeys, input.keyEdit);
    /*
     * An inverted list whose rows were all replaced, like another conversation, opens on the
     * new bottom the way a fresh list does.
     */
    if (container.inverted && hadElementsBefore && survivors == 0 && !container.revision.elements.empty()) {
      SL_LOG("  inverted swap: %zu rows, pinning to the bottom again", container.revision.elements.size());
      container.invertedInitialized = false;
      container.invertedBottomReleased = false;
      container.invertedOpeningPin = false;
      container.operation.reset();
      container.pendingScrollToEnd = false;
      anchorKey.clear();
      fallbackAnchors.clear();
    }
    /*
     * The anchor row may have been removed, like a refresh that drops the top post while
     * adding new ones. Hold the next row that was on screen instead.
     */
    if (!anchorKey.empty() && container.findElementIndexByKey(anchorKey) == UNDEFINED_INDEX) {
      for (const Anchor& candidate : fallbackAnchors) {
        if (container.findElementIndexByKey(candidate.key) != UNDEFINED_INDEX) {
          SL_LOG("  anchor fallback: %s -> %s sub=%.1f", anchorKey.c_str(), candidate.key.c_str(), candidate.subOffset);
          container.anchor = candidate;
          anchorKey = candidate.key;
          anchorDelta = candidate.subOffset;
          break;
        }
      }
    }
    if (!previousLastKey.empty()) {
      std::size_t previousLastIndex = container.findElementIndexByKey(previousLastKey);
      if (previousLastIndex != UNDEFINED_INDEX && previousLastIndex + 1 < container.revision.elements.size()) {
        container.pendingScrollToEnd = true;
      }
    }
  }

  /*
   * Apply sizes the host predicted since the last frame. This runs after the reconcile to
   * give new rows theirs, and before measure for the window to use the predicted sizes.
   */
  consumePredictions(container);

  container.revision.containerOffsetX = input.containerOffsetX;
  container.revision.containerOffsetY = input.containerOffsetY;
  double previousWindowSize = container.getWindowContainerSize();
  container.revision.windowContainerWidth = input.windowContainerWidth;
  container.revision.windowContainerHeight = input.windowContainerHeight;
  /*
   * A window change that moves the offset, like an inverted list following its bottom as the
   * composer grows, is our own write and gets published even when nothing else corrects.
   */
  double offsetBeforeWindow = container.getContainerOffset();
  container.containerOffsetCorrected = false;
  applyWindowSizeChange(container, previousWindowSize);
  bool windowMovedOffset = container.containerOffsetCorrected && container.getContainerOffset() != offsetBeforeWindow;
  anchorDelta = container.anchor.subOffset;
  measure(container);

  /*
   * Settle a header size change the same way the Fabric layout pass does. The code below
   * then doesn't read it as a scroll. An offset moved here is our own write. It can't confirm
   * a running correction, and it is published even if nothing else corrects.
   */
  bool headerMovedOffset = false;
  if (hadElementsBefore && container.headerSize != previousHeaderSize) {
    double offsetBeforeHeader = container.getContainerOffset();
    container.containerOffsetCorrected = false;
    applyHeaderSizeChange(container, previousHeaderSize);
    anchorDelta = container.anchor.subOffset;
    headerMovedOffset = container.containerOffsetCorrected && container.getContainerOffset() != offsetBeforeHeader;
    if (headerMovedOffset) {
      measure(container, true);
    }
  }

  SL_LOG("  measured: total=(%.1f,%.1f) offset=(%.1f,%.1f) visible=[%zd..%zd] visKeys=[%s..%s] anchorKey=%s anchor@newIdx=%zd",
    container.revision.totalContainerWidth, container.revision.totalContainerHeight,
    container.revision.containerOffsetX, container.revision.containerOffsetY,
    static_cast<std::ptrdiff_t>(container.getVisibleIndices().first),
    static_cast<std::ptrdiff_t>(container.getVisibleIndices().second),
    debugKeyAt(container, container.getVisibleIndices().first),
    debugKeyAt(container, container.getVisibleIndices().second),
    anchorKey.empty() ? "(none)" : anchorKey.c_str(),
    static_cast<std::ptrdiff_t>(anchorKey.empty() ? UNDEFINED_INDEX : container.findElementIndexByKey(anchorKey)));

  // Apply scroll corrections, and pick the window again if the offset moved.
  bool offsetConfirmed = !input.containerOffsetEnabled && !headerMovedOffset && !windowMovedOffset;
  bool scrollCorrected = resolveScroll(container, anchorKey, anchorDelta, hadElementsBefore, offsetConfirmed);
  if (headerMovedOffset || windowMovedOffset) {
    container.containerOffsetCorrected = true;
  }
  if (scrollCorrected) {
    measure(container, true);
    SL_LOG("  remeasured: offset=(%.1f,%.1f) visible=[%zd..%zd] visKeys=[%s..%s] invInit=%d",
      container.revision.containerOffsetX, container.revision.containerOffsetY,
      static_cast<std::ptrdiff_t>(container.getVisibleIndices().first),
      static_cast<std::ptrdiff_t>(container.getVisibleIndices().second),
      debugKeyAt(container, container.getVisibleIndices().first),
      debugKeyAt(container, container.getVisibleIndices().second),
      container.invertedInitialized ? 1 : 0);
  }

  // The commit token is just the running operation's id. No operation means no token.
  SL_LOG("  resolved: offset=(%.1f,%.1f) corrected=%d invInit=%d token=%llu",
    container.revision.containerOffsetX, container.revision.containerOffsetY,
    container.containerOffsetCorrected ? 1 : 0, container.invertedInitialized ? 1 : 0,
    static_cast<unsigned long long>(container.operation ? container.operation->id : 0));

  if (container.gestureActive && container.operation) {
    container.gestureOperationId = container.operation->id;
  }

  container.endRevision();
}

void Virtualizer::measure(Container& container, bool windowFromOffset) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  // Reset the measured range so it reflects only this pass.
  container.revision.measurementElementStartIndex = UNDEFINED_INDEX;
  container.revision.measurementElementEndIndex = UNDEFINED_INDEX;

  /*
   * After an insert, remove or reorder, rows still hold their old offsets. Reflow first so
   * the window is chosen from real positions and no row that moved into view is left blank.
   * It also lets the window pass use its fast search instead of scanning every row.
   */
  if (container.elementsStructureDirty) {
    layoutElements(container);
  }

  // The first revision fills from the edge. After a correction, use the corrected offset.
  if (!windowFromOffset && container.revisionCount == REVISION_COUNT_FIRST) {
    measureFirstRevision(container);
  } else {
    measureNextRevision(container);
  }

  layoutElements(container);
  recomputeTotalSize(container);
}

void Virtualizer::measureFirstRevision(Container& container) {
  std::size_t elementsSize = container.revision.elements.size();
  double windowSize = container.getWindowContainerSize();
  double effectiveColumns = container.columns > 0 ? static_cast<double>(container.columns) : 1.0;

  std::size_t measuredMinIndex = UNDEFINED_INDEX;
  std::size_t measuredMaxIndex = UNDEFINED_INDEX;
  double accumulated = 0.0;

  // Fill from the start, or from the end for an inverted list.
  for (std::size_t iteration = 0; iteration < elementsSize; ++iteration) {
    std::size_t nextElementIndex = container.inverted ? (elementsSize - 1 - iteration) : iteration;
    if (!visitMeasuredElement(container, nextElementIndex, measuredMinIndex, measuredMaxIndex)) {
      continue;
    }

    const Element& nextElement = container.revision.elements[nextElementIndex];
    accumulated += container.horizontal ? nextElement.width : nextElement.height;

    // Stop once the window plus the overscan buffer is full, shared across columns.
    if (accumulated / effectiveColumns >= windowSize * (1.0 + container.overscan)) {
      break;
    }
  }

  finalizeMeasurement(container, measuredMinIndex, measuredMaxIndex);
}

void Virtualizer::measureNextRevision(Container& container) {
  std::size_t elementsSize = container.revision.elements.size();
  double containerOffset = container.getContainerOffset();
  double windowSize = container.getWindowContainerSize();

  /*
   * Measure the window plus a buffer on each side so scrolling shows rows, not blanks.
   * Overscan counts in window heights. 1 means one window above and one below.
   */
  double overscanSize = windowSize * container.overscan;
  double lowerBound = containerOffset - overscanSize;
  double upperBound = containerOffset + windowSize + overscanSize;

  std::size_t measuredMinIndex = UNDEFINED_INDEX;
  std::size_t measuredMaxIndex = UNDEFINED_INDEX;

  // Before a reflow, offsets may be stale and out of order. Scan every row instead.
  bool geometryOrdered = !container.elementsStructureDirty;

  auto elementOffsetAt = [&](std::size_t index) {
    const Element& element = container.revision.elements[index];
    return container.horizontal ? element.offsetX : element.offsetY;
  };
  auto elementSizeAt = [&](std::size_t index) {
    const Element& element = container.revision.elements[index];
    return container.horizontal ? element.width : element.height;
  };

  auto visit = [&](std::size_t nextElementIndex) {
    visitMeasuredElement(container, nextElementIndex, measuredMinIndex, measuredMaxIndex);
  };

  /*
   * Binary search a column for the first row whose end is past lowerBound.
   * This works because rows in a column sit end to end. Deep scrolls avoid walking every row.
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

  if (geometryOrdered && container.columns <= 1) {
    for (std::size_t nextElementIndex = seekTrack(0, 1); nextElementIndex < elementsSize; ++nextElementIndex) {
      if (elementOffsetAt(nextElementIndex) > upperBound) {
        break;
      }
      visit(nextElementIndex);
    }
  } else if (geometryOrdered && container.columns > 1) {
    // Rows go to columns in turn. Each column is in order. Search each one separately.
    for (std::size_t track = 0; track < container.columns && track < elementsSize; ++track) {
      std::size_t stepsPast = seekTrack(track, container.columns);
      for (std::size_t nextElementIndex = track + stepsPast * container.columns;
           nextElementIndex < elementsSize;
           nextElementIndex += container.columns) {
        if (elementOffsetAt(nextElementIndex) > upperBound) {
          break;
        }
        visit(nextElementIndex);
      }
    }
  } else {
    // Offsets are not in order yet. Check every row against the window.
    for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
      double elementOffset = elementOffsetAt(nextElementIndex);
      double elementSize = elementSizeAt(nextElementIndex);

      // A row that starts above the window but still overlaps it counts too.
      if (elementOffset > upperBound || elementOffset + elementSize <= lowerBound) {
        continue;
      }
      visit(nextElementIndex);
    }
  }

  finalizeMeasurement(container, measuredMinIndex, measuredMaxIndex);
}

void Virtualizer::finalizeMeasurement(
  Container& container,
  std::size_t measuredMinIndex,
  std::size_t measuredMaxIndex) {
  // An inverted list runs backwards. Its start index is the higher one.
  if (container.inverted) {
    container.revision.measurementElementStartIndex = measuredMaxIndex;
    container.revision.measurementElementEndIndex = measuredMinIndex;
  } else {
    container.revision.measurementElementStartIndex = measuredMinIndex;
    container.revision.measurementElementEndIndex = measuredMaxIndex;
  }
}

std::vector<Anchor> Virtualizer::captureFallbackAnchors(Container& container, double inputOffset) {
  std::vector<Anchor> candidates;
  const std::vector<Element>& elements = container.revision.elements;
  std::size_t anchorIndex = container.findElementIndexByKey(container.anchor.key);
  if (anchorIndex == UNDEFINED_INDEX || container.anchor.mode != AnchorMode::Element) {
    return candidates;
  }

  /*
   * Collect the anchorable rows after the anchor that start inside the viewport, each with
   * the offset that holds it in place. In a grid, keep going until every column has passed
   * the bottom of the viewport.
   */
  double viewportEnd = inputOffset + container.getWindowContainerSize();
  std::size_t columns = container.columns > 0 ? container.columns : 1;
  std::size_t tracksPast = 0;
  for (std::size_t index = anchorIndex + 1; index < elements.size() && tracksPast < columns; ++index) {
    double elementOffset = container.getElementOffset(index);
    if (elementOffset >= viewportEnd) {
      ++tracksPast;
      continue;
    }
    if (container.isAnchorable(elements[index].key)) {
      candidates.push_back(Anchor{elements[index].key, inputOffset - elementOffset, AnchorMode::Element});
    }
  }
  return candidates;
}

void Virtualizer::captureAnchor(Container& container, double inputOffset) {
  const std::string previousAnchorKey = container.anchor.key;
  container.anchor = Anchor{"", 0.0, AnchorMode::Element};

  const std::vector<Element>& previousElements = container.revision.elements;
  if (previousElements.empty()) {
    return;
  }

  auto elementOffsetOf = [&](const Element& element) {
    return container.horizontal ? element.offsetX : element.offsetY;
  };
  auto elementSizeOf = [&](const Element& element) {
    return container.horizontal ? element.width : element.height;
  };

  /*
   * In a grid the tracks move independently as rows get measured. A new anchor each frame
   * lets the row the reader was on drift. Keep the previous anchor while it is still on screen.
   */
  if (container.columns > 1 && !previousAnchorKey.empty() && container.isAnchorable(previousAnchorKey)) {
    std::size_t previousAnchorIndex = container.findElementIndexByKey(previousAnchorKey);
    if (previousAnchorIndex != UNDEFINED_INDEX) {
      const Element& previousAnchor = previousElements[previousAnchorIndex];
      double start = elementOffsetOf(previousAnchor);
      if (start + elementSizeOf(previousAnchor) > inputOffset && start < inputOffset + container.getWindowContainerSize()) {
        container.anchor = Anchor{previousAnchorKey, inputOffset - start, AnchorMode::Element};
        return;
      }
    }
  }

  /*
   * The anchor is the first content row at the top of the viewport. Decoration rows like
   * date pills are skipped. firstPast keeps the actual top row in case the viewport holds
   * only decoration.
   * A single column is in order. Binary search for the start. Grids scan from 0.
   */
  std::size_t scanStart = 0;
  if (container.columns <= 1) {
    std::size_t low = 0;
    std::size_t high = previousElements.size();
    while (low < high) {
      std::size_t mid = low + (high - low) / 2;
      const Element& element = previousElements[mid];
      if (elementOffsetOf(element) + elementSizeOf(element) <= inputOffset) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    scanStart = low;
  }

  const Element* firstPast = nullptr;
  for (std::size_t previousElementIndex = scanStart; previousElementIndex < previousElements.size(); ++previousElementIndex) {
    const Element& previousElement = previousElements[previousElementIndex];
    double elementOffset = elementOffsetOf(previousElement);
    double elementSize = elementSizeOf(previousElement);

    if (elementOffset + elementSize <= inputOffset) {
      continue;
    }
    if (firstPast == nullptr) {
      firstPast = &previousElement;
    }
    if (container.isAnchorable(previousElement.key)) {
      /*
       * A row with only a guessed size may change size in several steps, moving everything
       * below it each time. This is common right after a prepend at the top. If a row with a
       * known size starts inside the viewport, anchor that one instead.
       * Only for single column lists that aren't inverted. Inverted lists use their anchor
       * to tell whether they rest on the newest row.
       */
      const Element* anchorElement = &previousElement;
      if (container.columns <= 1 && !container.inverted && !previousElement.measured && !previousElement.predicted) {
        double viewportEnd = inputOffset + container.getWindowContainerSize();
        for (std::size_t laterIndex = previousElementIndex + 1; laterIndex < previousElements.size(); ++laterIndex) {
          const Element& laterElement = previousElements[laterIndex];
          if (elementOffsetOf(laterElement) >= viewportEnd) {
            break;
          }
          if (container.isAnchorable(laterElement.key) && (laterElement.measured || laterElement.predicted)) {
            anchorElement = &laterElement;
            break;
          }
        }
      }
      container.anchor =
        Anchor{anchorElement->key, inputOffset - elementOffsetOf(*anchorElement), AnchorMode::Element};
      return;
    }
  }

  /*
   * No content row is in or below the viewport. Use the last content row, then the top row
   * even if it is decoration, then the very last row. There is always an anchor.
   */
  for (auto reverse = previousElements.rbegin(); reverse != previousElements.rend(); ++reverse) {
    if (container.isAnchorable(reverse->key)) {
      container.anchor = Anchor{reverse->key, inputOffset - elementOffsetOf(*reverse), AnchorMode::Element};
      return;
    }
  }
  const Element& fallback = firstPast != nullptr ? *firstPast : previousElements.back();
  container.anchor = Anchor{fallback.key, inputOffset - elementOffsetOf(fallback), AnchorMode::Element};
}

bool Virtualizer::resolveScroll(
  Container& container,
  const std::string& anchorKey,
  double anchorDelta,
  bool hadElementsBefore,
  bool offsetConfirmed) {
  std::size_t elementsSize = container.revision.elements.size();
  container.containerOffsetCorrected = false;

  // An emptied list starts over. An inverted list sticks to the bottom again when rows arrive.
  if (elementsSize == 0) {
    container.invertedInitialized = false;
    container.invertedBottomReleased = false;
    container.invertedOpeningPin = false;
    container.operation.reset();
    container.pendingScrollToEnd = false;
  }

  double windowSize = container.getWindowContainerSize();
  double totalSize = container.horizontal ? container.revision.totalContainerWidth : container.revision.totalContainerHeight;
  double currentOffset = container.horizontal ? container.revision.containerOffsetX : container.revision.containerOffsetY;
  double maxOffset = totalSize - windowSize;
  if (maxOffset < 0.0) {
    maxOffset = 0.0;
  }

  // Track the total every frame so step 2b can tell when the bottom stops growing.
  double previousTotalForScrollToEnd = container.pendingScrollToEndLastTotal;
  container.pendingScrollToEndLastTotal = totalSize;

  auto clampOffset = [&](double offset) {
    if (offset < 0.0) {
      offset = 0.0;
    }
    if (offset > maxOffset) {
      offset = maxOffset;
    }
    return offset;
  };

  /*
   * Reuse the id when the same correction is requested again. The host's reply then still
   * matches it over several frames. A new type or a new row gets a new id.
   */
  auto operationId = [&](OperationType type, AnchorMode mode, const std::string& key) -> std::uint64_t {
    if (container.operation && container.operation->type == type &&
        container.operation->target.mode == mode &&
        (mode != AnchorMode::Element || container.operation->target.key == key)) {
      return container.operation->id;
    }
    return container.nextOperationId++;
  };

  // Start or repeat a correction that aims at the end, until the view gets there.
  auto requestFixed = [&](OperationType type) {
    if (container.operation && container.operation->type != type) {
      SL_LOG("  op replaced: type=%d key=%s -> type=%d", static_cast<int>(container.operation->type),
        container.operation->target.key.c_str(), static_cast<int>(type));
    }
    container.operation =
      Operation{operationId(type, AnchorMode::EndEdge, ""), type, Anchor{"", 0.0, AnchorMode::EndEdge}};
  };

  /*
   * Start or repeat a correction that aims at a row. The target is looked up by key each
   * frame. It follows the row while nearby rows get measured.
   */
  auto requestAnchor = [&](OperationType type, const std::string& key, double delta, double viewPosition = 0.0,
                         double rowOffset = 0.0) {
    if (container.operation && (container.operation->type != type || container.operation->target.key != key)) {
      SL_LOG("  op replaced: type=%d key=%s -> type=%d key=%s", static_cast<int>(container.operation->type),
        container.operation->target.key.c_str(), static_cast<int>(type), key.c_str());
    }
    container.operation =
      Operation{operationId(type, AnchorMode::Element, key), type, Anchor{key, delta, AnchorMode::Element}, viewPosition,
        rowOffset};
  };

  /*
   * Set when a scroll command arrives this frame. The command owns the offset. Holding
   * the old visible content must not pull the view back.
   */
  bool commandRequestedThisFrame = false;

  /*
   * Step 0. Content shrank and left the view past the new end, like collapsing a tree.
   * Go back to the bottom. Checking for a shrink avoids fighting a bounce.
   * It runs as an operation so it survives recommits, and step 3 clears it on arrival.
   */
  if (!container.inverted && elementsSize > 0 &&
      currentOffset > maxOffset + OFFSET_MOVED_THRESHOLD &&
      totalSize < previousTotalForScrollToEnd) {
    /*
     * Only when no anchor will place the view. The shrink is often above the viewport, and
     * the row on screen moved up with it. Steps 3 and 4 follow that row and clamp when
     * needed. Jumping to the end would show rows further down instead.
     */
    bool anchorPlacesView = false;
    if (container.operation && container.operation->target.mode == AnchorMode::Element) {
      anchorPlacesView = container.findElementIndexByKey(container.operation->target.key) != UNDEFINED_INDEX;
    } else if (!container.operation && hadElementsBefore && !anchorKey.empty()) {
      std::size_t anchorIndex = container.findElementIndexByKey(anchorKey);
      anchorPlacesView = anchorIndex != UNDEFINED_INDEX &&
        std::fabs(container.getElementOffset(anchorIndex) + anchorDelta - currentOffset) >= OFFSET_MOVED_THRESHOLD;
    }
    if (!anchorPlacesView) {
      requestFixed(OperationType::ShrinkClamp);
    }
  }

  // Step 1. Scroll to an index.
  if (container.scrollToIndexTarget != UNDEFINED_INDEX) {
    if (container.scrollToIndexTarget < elementsSize) {
      /*
       * Follow the row itself, not a fixed offset. Its offset is a guess at first, and
       * following the row settles on it as the area gets measured.
       */
      const std::string targetKey = container.getElementAtIndex(container.scrollToIndexTarget).key;
      // The view position travels with the operation and is applied again each frame.
      requestAnchor(OperationType::ScrollToKey, targetKey, 0.0, container.scrollToIndexViewPosition,
        container.scrollToIndexRowOffset);
      commandRequestedThisFrame = true;
      // This replaces the bottom pin, but only for an index in range.
      container.invertedInitialized = true;
      container.invertedOpeningPin = false;
    }
    container.scrollToIndexTarget = UNDEFINED_INDEX;
  }

  /*
   * Step 1b. Scroll to the start. It holds the first row below the header so it keeps up
   * with measuring. Momentum neither cancels nor moves it because it is a command.
   */
  if (container.pendingScrollToStart) {
    container.pendingScrollToStart = false;
    if (elementsSize > 0) {
      std::size_t leading = 0;
      if (container.getElementOffset(elementsSize - 1) < container.getElementOffset(0)) {
        leading = elementsSize - 1;
      }
      requestAnchor(
        OperationType::ScrollToStart,
        container.getElementAtIndex(leading).key,
        -container.getElementOffset(leading));
      commandRequestedThisFrame = true;
      container.invertedInitialized = true;
      container.invertedOpeningPin = false;
    }
  }

  /*
   * Step 2. An inverted list sticks to the bottom until it gets there, following it as the
   * content grows. Wait for the window size so the target is right.
   */
  if (container.inverted && !container.invertedInitialized && elementsSize > 0 && windowSize > 0.0) {
    requestFixed(OperationType::BottomPin);
    container.invertedOpeningPin = true;
    // Done only once the list can scroll and sits at the bottom. Until then keep pinning.
    if (offsetConfirmed &&
        totalSize > windowSize &&
        std::fabs(currentOffset - maxOffset) < OFFSET_ARRIVED_THRESHOLD) {
      container.invertedInitialized = true;
    }
  }

  /*
   * Step 2b. Scroll to the end, aiming again every frame as the content grows. It is done
   * when the view is at the bottom, the total held still and the last row has a real size.
   * A drag cancels it, momentum does not.
   */
  if (container.pendingScrollToEnd && elementsSize > 0 && windowSize > 0.0) {
    container.invertedOpeningPin = false;
    bool atBottom = std::fabs(currentOffset - maxOffset) < OFFSET_ARRIVED_THRESHOLD;
    bool totalStable = totalSize == previousTotalForScrollToEnd;
    if (atBottom && totalStable && container.hasTrustedSize(elementsSize - 1)) {
      container.pendingScrollToEnd = false;
    } else {
      requestFixed(OperationType::ScrollToEnd);
    }
  }

  /*
   * Step 3. Keep driving the running operation until the view gets there. The target is
   * worked out again each frame. It ends when its row is gone or the view arrives.
   */
  if (container.operation) {
    double rawTarget = 0.0;
    if (!resolveAnchorOffset(container, *container.operation, maxOffset, rawTarget)) {
      container.operation.reset();
    } else {
      double target = clampOffset(rawTarget);
      /*
       * A host report at the target ends the operation. So does our own write, if the host
       * last reported the target too. Without that, a resting list only sees its own writes
       * and republishes on every commit, about 500 times for one streaming reply.
       */
      bool reportedAtTarget = std::fabs(container.lastReportedOffset - target) < OFFSET_ARRIVED_THRESHOLD;
      if ((offsetConfirmed || reportedAtTarget) &&
          std::fabs(currentOffset - target) < OFFSET_ARRIVED_THRESHOLD) {
        SL_LOG("  op arrived: type=%d key=%s index=%zd target=%.1f", static_cast<int>(container.operation->type),
          container.operation->target.key.c_str(),
          static_cast<std::ptrdiff_t>(container.operation->target.mode == AnchorMode::Element
            ? container.findElementIndexByKey(container.operation->target.key) : UNDEFINED_INDEX),
          target);
        container.operation.reset();
        /*
         * A command that is already where it wants to be is done. Holding the old anchor
         * would move the view to a row further down after a data swap.
         */
        if (commandRequestedThisFrame) {
          return false;
        }
      } else {
        // The offset moved. The caller picks the window again.
        correctOffset(container, target);
        return true;
      }
    }
  }

  /*
   * Step 4. With no correction running, keep the anchor row where it was on screen.
   * Rows added or removed above start a correction. Plain scrolling does nothing.
   */
  if (hadElementsBefore && !anchorKey.empty()) {
    std::size_t anchorIndex = container.findElementIndexByKey(anchorKey);
    if (anchorIndex != UNDEFINED_INDEX) {
      /*
       * An inverted list resting on its newest content row holds the true bottom instead of
       * the row's old position. The row can grow after first paint, and holding its position
       * would leave a gap at the bottom and fight the bottom pin. Trailing decoration rows
       * don't count as the newest row.
       * Not when the user has scrolled up, or the view gets pulled from under them. Not while
       * a finger is down either, or the content snaps back on every touch frame.
       * A list still settling on the bottom it opened at always holds the bottom.
       */
      bool invertedBottomAnchor = false;
      if (container.inverted && !container.invertedBottomReleased && !container.gestureActive) {
        if (container.invertedOpeningPin && container.restingAtInvertedBottom) {
          invertedBottomAnchor = true;
        } else {
          invertedBottomAnchor = isLastAnchorable(container, anchorIndex);
        }
      }
      double rawAnchoredOffset = invertedBottomAnchor
        ? maxOffset
        : container.getElementOffset(anchorIndex) + anchorDelta;
      if (std::fabs(rawAnchoredOffset - currentOffset) >= OFFSET_MOVED_THRESHOLD) {
        if (invertedBottomAnchor) {
          requestFixed(OperationType::BottomPin);
        } else {
          requestAnchor(OperationType::MaintainAnchor, anchorKey, anchorDelta);
        }
        correctOffset(container, clampOffset(rawAnchoredOffset));
        return true;
      }
    }
  }

  return false;
}

}
