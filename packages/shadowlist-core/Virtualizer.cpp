#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace azimgd::shadowlist {

/*
 * Debug only. Gives the key at an index so native and JS logs can be matched by content,
 * since an index points at different rows during a prepend. It is only used inside SL_LOG,
 * so it is marked maybe_unused to keep Android builds with -Werror=unused-function clean.
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
 * Size for a row that was never measured: the frozen average of real measurements once
 * there is one, otherwise the configured estimate.
 * Every pass uses this same value. If two passes disagreed, a row entering the window
 * would resize, reflow the rows after it, and a row pulled into range would miss a frame.
 */
std::pair<double, double> effectiveFallbackSize(const Container* container) {
  auto [estimatedWidth, estimatedHeight] = container->estimatedElementSize;
  return {
    container->revision.averageElementWidth > 0.0 ? container->revision.averageElementWidth : estimatedWidth,
    container->revision.averageElementHeight > 0.0 ? container->revision.averageElementHeight : estimatedHeight,
  };
}

/*
 * Writable scroll offset along the scroll axis.
 */
double& scrollAxisOffset(Container* container) {
  return container->horizontal ? container->revision.containerOffsetX : container->revision.containerOffsetY;
}

/*
 * Move the scroll offset and flag the frame so the host applies it.
 */
void correctOffset(Container* container, double offset) {
  scrollAxisOffset(container) = offset;
  container->containerOffsetCorrected = true;
}

/*
 * Correct the offset only when probe is a real move from the current offset.
 * Probe is the target before any clamp, so it equals offset unless the caller clamps.
 */
bool correctOffsetIfMoved(Container* container, double offset, double probe) {
  if (std::fabs(probe - scrollAxisOffset(container)) < OFFSET_MOVED_THRESHOLD) {
    return false;
  }
  correctOffset(container, offset);
  return true;
}

/*
 * Move the running element correction and the captured anchor by the same delta.
 */
void shiftAnchors(Container* container, double delta) {
  if (container->operation && container->operation->target.mode == AnchorMode::Element) {
    container->operation->target.subOffset += delta;
  }
  if (!container->anchor.key.empty() && container->anchor.mode == AnchorMode::Element) {
    container->anchor.subOffset += delta;
  }
}

/*
 * True when an inverted list sits within INVERTED_FOLLOW_BAND of its bottom.
 */
bool atInvertedBottom(double offset, double total, double window) {
  return offset >= std::max(0.0, total - window) - INVERTED_FOLLOW_BAND;
}

/*
 * True when this is the newest content row, ignoring trailing decoration rows.
 */
bool isLastAnchorable(const Container* container, std::size_t index) {
  const std::vector<Element>& elements = container->revision.elements;
  if (index >= elements.size() || !container->isAnchorable(elements[index].key)) {
    return false;
  }
  for (std::size_t nextElementIndex = index + 1; nextElementIndex < elements.size(); ++nextElementIndex) {
    if (container->isAnchorable(elements[nextElementIndex].key)) {
      return false;
    }
  }
  return true;
}

/*
 * Furthest right and bottom edges of the content, read from the last row of each column.
 * That is enough along the scroll axis. Across it, callers also use maxCrossAxisExtent.
 */
Size tailExtent(const Container* container) {
  const std::vector<Element>& elements = container->revision.elements;
  std::size_t scanColumns = container->columns > 0 ? container->columns : 1;
  std::size_t scanFrom = elements.size() > scanColumns ? elements.size() - scanColumns : 0;
  Size extent{0.0, 0.0};
  for (std::size_t nextElementIndex = scanFrom; nextElementIndex < elements.size(); ++nextElementIndex) {
    const Element& nextElement = elements[nextElementIndex];
    extent.width = std::max(extent.width, nextElement.offsetX + nextElement.width);
    extent.height = std::max(extent.height, nextElement.offsetY + nextElement.height);
  }
  return extent;
}

/*
 * Give a row the fallback size if it has none and widen the measured range to include it.
 * Returns false when there is no estimate to give it.
 */
bool visitMeasuredElement(
  Container* container,
  std::size_t nextElementIndex,
  std::size_t& measuredMinIndex,
  std::size_t& measuredMaxIndex) {
  Element& nextElement = container->revision.elements[nextElementIndex];

  if (!nextElement.estimated) {
    auto [width, height] = effectiveFallbackSize(container);

    if (width == 0.0 && height == 0.0) {
      return false;
    }

    /*
     * Layout already gave unmeasured rows this size, so usually nothing changes here.
     * Only mark sizes dirty on a real change. Otherwise dragging the scroll indicator
     * would reflow the whole list on every frame.
     */
    if (nextElement.width != width || nextElement.height != height) {
      nextElement.width = width;
      nextElement.height = height;
      // The size changed outside the layout loop, so make sure it reflows offsets.
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
  return true;
}

/*
 * Reflow a single column. Built once per axis so the loop skips the orientation check.
 */
template <double Element::*offset, double Element::*size, double Element::*crossOffset, double Element::*crossSize>
void reflowSingleTrack(
  Element* elements,
  std::size_t fromIndex,
  std::size_t elementsSize,
  std::size_t changedThroughIndex,
  bool canStopEarly,
  double nextOffset,
  double& crossMax,
  bool& anyOffsetChanged) {
  for (std::size_t nextElementIndex = fromIndex; nextElementIndex < elementsSize; ++nextElementIndex) {
    Element& nextElement = elements[nextElementIndex];
    if (nextElement.index != nextElementIndex) {
      nextElement.index = nextElementIndex;
    } else if (canStopEarly && nextElementIndex > changedThroughIndex &&
               nextElement.*offset == nextOffset) {
      break;
    }

    anyOffsetChanged = anyOffsetChanged || nextElement.*offset != nextOffset;
    nextElement.*offset = nextOffset;
    nextOffset += nextElement.*size;

    double crossExtent = nextElement.*crossOffset + nextElement.*crossSize;
    if (crossExtent > crossMax) {
      crossMax = crossExtent;
    }
  }
}

/*
 * Reflow several columns. Rows go to columns in turn and take the column's width.
 */
template <double Element::*offset, double Element::*size, double Element::*crossOffset, double Element::*crossSize>
void reflowTracks(
  Container* container,
  std::size_t fromIndex,
  double trackSize,
  double& crossMax,
  bool& anyOffsetChanged) {
  std::vector<Element>& elements = container->revision.elements;
  std::size_t columns = container->columns;

  // Columns start below the header.
  std::vector<double> trackSizes(columns, container->headerSize);

  /*
   * Start each column at the end of its last row before fromIndex. Those rows are all
   * within one row per column back.
   */
  for (std::size_t seedIndex = fromIndex; seedIndex-- > 0 && seedIndex + columns >= fromIndex;) {
    const Element& seedElement = elements[seedIndex];
    trackSizes[seedIndex % columns] = seedElement.*offset + seedElement.*size;
  }

  for (std::size_t nextElementIndex = fromIndex; nextElementIndex < elements.size(); ++nextElementIndex) {
    Element& nextElement = elements[nextElementIndex];
    nextElement.index = nextElementIndex;

    std::size_t trackIndex = nextElementIndex % columns;

    // Set the width too, so a reflow after the window size is known fixes it.
    anyOffsetChanged = anyOffsetChanged || nextElement.*offset != trackSizes[trackIndex] ||
      nextElement.*crossOffset != trackIndex * trackSize;
    nextElement.*crossOffset = trackIndex * trackSize;
    nextElement.*crossSize = trackSize;
    nextElement.*offset = trackSizes[trackIndex];
    trackSizes[trackIndex] += nextElement.*size;

    double crossExtent = nextElement.*crossOffset + nextElement.*crossSize;
    if (crossExtent > crossMax) {
      crossMax = crossExtent;
    }
  }
}

/*
 * How far past the top of the viewport the anchored row should sit.
 * Keeping the visible content in place stores this in pixels. A scroll to a key works it
 * out again each frame from the free space around the row, so it stays right while the
 * row's size or the window size is still settling.
 */
double resolveAnchorSubOffset(Container* container, const Operation& operation, std::size_t anchorIndex) {
  if (operation.type != OperationType::ScrollToKey) {
    return operation.target.subOffset;
  }
  double freeSpace = container->getWindowContainerSize() - container->getElementSize(anchorIndex);
  return freeSpace > 0.0 ? -operation.viewPosition * freeSpace : 0.0;
}

/*
 * Turn the running operation's target into an unclamped offset for this frame.
 * An end edge target is maxOffset. A row target is the row's offset plus its sub offset,
 * worked out each frame so it follows the row while nearby rows get measured.
 * Returns false when the row's key is gone. Header size changes are handled elsewhere.
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
  outOffset = container->getElementOffset(anchorIndex) + resolveAnchorSubOffset(container, operation, anchorIndex);
  return true;
}
}

void Virtualizer::update(Container* container, const FrameInput& input) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  // The keys may be borrowed from the caller, so they are only valid during this call.
  const std::vector<std::string>& inputKeys = input.keyList();
  const std::vector<std::string>& inputNonAnchorableKeys = input.nonAnchorableKeyList();

  SL_LOG("update: keys=%zu prevElements=%zu off=(%.1f,%.1f) win=(%.1f,%.1f) inv=%d cols=%zu hdr=%.1f ftr=%.1f invInit=%d total=%.1f dirtyFrom=%zd enabled=%d corrected=%d coreOff=%.1f",
    inputKeys.size(), container->revision.elements.size(),
    input.containerOffsetX, input.containerOffsetY,
    input.windowContainerWidth, input.windowContainerHeight,
    input.inverted ? 1 : 0, input.columns, input.headerSize, input.footerSize,
    container->invertedInitialized ? 1 : 0,
    container->horizontal ? container->revision.totalContainerWidth : container->revision.totalContainerHeight,
    static_cast<std::ptrdiff_t>(container->elementsSizeDirtyFromIndex), input.containerOffsetEnabled ? 1 : 0,
    container->containerOffsetCorrected ? 1 : 0, container->getContainerOffset());

  // Remember the old header size so a change can be settled after the rows reflow.
  double previousHeaderSize = container->headerSize;

  // Flipping the list order moves the bottom, so start following the bottom again.
  if (container->inverted != input.inverted) {
    container->invertedBottomReleased = false;
  }

  container->inverted = input.inverted;
  container->horizontal = input.horizontal;
  container->columns = input.columns;
  container->overscan = input.overscan;
  container->headerSize = input.headerSize;
  container->footerSize = input.footerSize;
  // Compare first, so an unchanged list costs no copy.
  const std::vector<std::size_t>& inputStickyIndices = input.stickyIndexList();
  if (container->stickyIndices != inputStickyIndices) {
    container->stickyIndices = inputStickyIndices;
  }
  container->startReachedThreshold = input.startReachedThreshold;
  container->endReachedThreshold = input.endReachedThreshold;
  container->viewablePercentThreshold = input.viewablePercentThreshold;
  container->estimatedElementSize = input.estimatedElementSize;
  container->snapToItem = input.snapToItem;
  container->snapAlignment = input.snapAlignment;

  /*
   * Decoration rows must never become the anchor. This runs before captureAnchor reads it,
   * and is rebuilt every frame so a row switching roles takes effect right away.
   */
  if (!input.nonAnchorableKeysUnchanged &&
      (container->nonAnchorableKeys.size() != inputNonAnchorableKeys.size() ||
       !std::all_of(inputNonAnchorableKeys.begin(), inputNonAnchorableKeys.end(),
         [&](const std::string& ignoredKey) { return container->nonAnchorableKeys.count(ignoredKey) != 0; }))) {
    container->nonAnchorableKeys.clear();
    for (const std::string& ignoredKey : inputNonAnchorableKeys) {
      container->nonAnchorableKeys.insert(ignoredKey);
    }
  }

  double inputOffset = container->horizontal ? input.containerOffsetX : input.containerOffsetY;

  /*
   * An enabled offset is our own write coming back before the host applied it, not a host
   * report. It can't confirm the correction or count as gesture travel.
   */
  bool coreOffsetWrite = input.containerOffsetEnabled;

  // If the offset has not moved, the user is not scrolling, so a running correction survives.
  bool userMovedOffset = !coreOffsetWrite &&
    std::fabs(inputOffset - container->lastReportedOffset) >= OFFSET_MOVED_THRESHOLD;
  /*
   * When the user takes over, drop any running correction and stop pinning to the bottom.
   * The host's gesture phase decides. userScrolled only counts with a real move, so a stale
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
  bool scrollCommandInFlight = container->operation &&
    (container->operation->type == OperationType::ScrollToEnd ||
     container->operation->type == OperationType::ScrollToKey ||
     container->operation->type == OperationType::ScrollToStart);
  /*
   * A correction that keeps the visible content in place is not something a gesture cancels.
   * A prepend during a fling runs update twice on the same settling report, and dropping the
   * correction on the second pass would leave the view unmoved. So keep it until the host
   * reports back its commit token, and shift it by any momentum travel before then.
   */
  bool maintainingAnchor = container->operation &&
    container->operation->type == OperationType::MaintainAnchor &&
    container->operation->target.mode == AnchorMode::Element;
  bool echoesOperation = container->operation && !coreOffsetWrite &&
    input.commitToken == container->operation->id;
  /*
   * Scrolling reported before the host applies the correction moves its target along.
   * A correction started during a gesture keeps following after the frames go idle, since
   * the end of a bounce still moves the offset. Other idle moves, like a host clamping to
   * shorter content, are not followed.
   */
  if (maintainingAnchor && !echoesOperation && userMovedOffset &&
      (gestureTakeover || container->operation->id == container->gestureOperationId)) {
    container->operation->target.subOffset += inputOffset - container->lastReportedOffset;
  }
  // A correction made during a gesture is done once the host reports it back while idle.
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

  // Whether an inverted list rests at its bottom, so rows appended below get followed.
  bool restingAtBottom = false;

  /*
   * Pin or release the inverted list's bottom.
   * Judge it with last frame's total and window together. Mixing frames would make a
   * keyboard opening or a rotation release a reader who never moved. That is also why
   * this runs before the reconcile below.
   * Only a gesture releases the pin. Pinning again needs a real move toward the bottom made
   * by the user or a scroll command, so shrinking content or our own correction can't
   * quietly pin the reader again.
   */
  if (container->inverted) {
    double previousOffset = container->lastReportedOffset;
    double bottomTotal = container->horizontal ? container->revision.totalContainerWidth
                                               : container->revision.totalContainerHeight;
    double bottomWindow = container->horizontal ? container->revision.windowContainerWidth
                                                : container->revision.windowContainerHeight;
    bool atBottom = atInvertedBottom(inputOffset, bottomTotal, bottomWindow);
    // Content that fits the window has no bottom to leave, so a bounce must not release it.
    bool scrollable = bottomTotal > bottomWindow;

    if (gestureTakeover && scrollable && !atBottom) {
      container->invertedBottomReleased = true;
    } else if (container->invertedBottomReleased && atBottom &&
        inputOffset >= previousOffset + OFFSET_MOVED_THRESHOLD &&
        (input.userScrolled || container->pendingScrollToEnd ||
         (container->operation && (container->operation->type == OperationType::ScrollToEnd ||
                                   container->operation->type == OperationType::ScrollToKey)))) {
      container->invertedBottomReleased = false;
    }

    restingAtBottom = !gestureTakeover && !container->invertedBottomReleased &&
      container->invertedInitialized && !container->revision.elements.empty() &&
      bottomWindow > 0.0 && atBottom;
  }
  // resolveScroll reads this to hold the bottom while the list opens.
  container->restingAtInvertedBottom = restingAtBottom;
  // Our own write is not where the host is, so don't record it.
  if (!coreOffsetWrite) {
    container->lastReportedOffset = inputOffset;
  }

  // Capture the anchor row so the same content stays in view through the reconcile.
  bool hadElementsBefore = !container->revision.elements.empty();
  captureAnchor(container, inputOffset);

  std::string anchorKey = container->anchor.key;
  double anchorDelta = container->anchor.subOffset;

  /*
   * Debug only. Log the frame where the keys changed, which is when JS and native indexes
   * drift apart. oldFront@newIdx is how many rows were prepended above the old top row.
   */
#if SHADOWLIST_DEBUG_LOG
  {
    std::size_t previousSize = container->revision.elements.size();
    std::size_t nextSize = inputKeys.size();
    bool frontChanged = previousSize && nextSize && container->revision.elements.front().key != inputKeys.front();
    if (previousSize != nextSize || frontChanged) {
      long previousFrontNextIndex = -1;
      if (previousSize) {
        const std::string& previousFront = container->revision.elements.front().key;
        for (std::size_t nextElementIndex = 0; nextElementIndex < nextSize; ++nextElementIndex) {
          if (inputKeys[nextElementIndex] == previousFront) {
            previousFrontNextIndex = static_cast<long>(nextElementIndex);
            break;
          }
        }
      }
      SL_LOG("  RECONCILE: size %zu->%zu front '%s'->'%s' oldFront@newIdx=%ld anchorKey=%s anchorDelta=%.1f",
        previousSize, nextSize,
        previousSize ? container->revision.elements.front().key.c_str() : "(none)",
        nextSize ? inputKeys.front().c_str() : "(none)",
        previousFrontNextIndex, anchorKey.empty() ? "(none)" : anchorKey.c_str(), anchorDelta);
    }
  }
#endif

  /*
   * Match the rows to the new keys. Most commits don't change the keys, so compare them
   * first and skip the rebuild, which is the main cost of each commit after a prepend.
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
    // New keys end the opening settle.
    container->invertedOpeningPin = false;
    /*
     * With followAppends, an inverted list resting at the bottom scrolls to new rows
     * appended below. By default they land below the fold. Following runs as a scroll to
     * the end, which yields to a drag. A reply growing in place is not an append.
     */
    std::string previousLastKey = restingAtBottom && input.followAppends
      ? container->revision.elements.back().key
      : std::string();
    std::vector<Anchor> fallbackAnchors = captureFallbackAnchors(container, inputOffset);
    reconcileElements(container, inputKeys);
    /*
     * The anchor row may have been removed, like a refresh that drops the top post while
     * adding new ones. Hold the next row that was on screen instead.
     */
    if (!anchorKey.empty() && container->findElementIndexByKey(anchorKey) == UNDEFINED_INDEX) {
      for (const Anchor& candidate : fallbackAnchors) {
        if (container->findElementIndexByKey(candidate.key) != UNDEFINED_INDEX) {
          SL_LOG("  anchor fallback: %s -> %s sub=%.1f", anchorKey.c_str(), candidate.key.c_str(), candidate.subOffset);
          container->anchor = candidate;
          anchorKey = candidate.key;
          anchorDelta = candidate.subOffset;
          break;
        }
      }
    }
    if (!previousLastKey.empty()) {
      std::size_t previousLastIndex = container->findElementIndexByKey(previousLastKey);
      if (previousLastIndex != UNDEFINED_INDEX && previousLastIndex + 1 < container->revision.elements.size()) {
        container->pendingScrollToEnd = true;
      }
    }
  }

  /*
   * Apply sizes the host predicted since the last frame. This must run after the reconcile,
   * so new rows get theirs, and before measure, so the window uses the predicted sizes.
   */
  consumePredictions(container);

  container->revision.containerOffsetX = input.containerOffsetX;
  container->revision.containerOffsetY = input.containerOffsetY;
  double previousWindowSize = container->getWindowContainerSize();
  container->revision.windowContainerWidth = input.windowContainerWidth;
  container->revision.windowContainerHeight = input.windowContainerHeight;
  applyWindowSizeChange(container, previousWindowSize);
  anchorDelta = container->anchor.subOffset;
  measure(container);

  /*
   * Settle a header size change the same way the Fabric layout pass does, so the code below
   * doesn't read it as a scroll. An offset moved here is our own write, so it can't confirm
   * a running correction, and it is published even if nothing else corrects.
   */
  bool headerMovedOffset = false;
  if (hadElementsBefore && container->headerSize != previousHeaderSize) {
    double offsetBeforeHeader = container->getContainerOffset();
    container->containerOffsetCorrected = false;
    applyHeaderSizeChange(container, previousHeaderSize);
    anchorDelta = container->anchor.subOffset;
    headerMovedOffset = container->containerOffsetCorrected && container->getContainerOffset() != offsetBeforeHeader;
    if (headerMovedOffset) {
      measure(container, true);
    }
  }

  SL_LOG("  measured: total=(%.1f,%.1f) offset=(%.1f,%.1f) visible=[%zd..%zd] visKeys=[%s..%s] anchorKey=%s anchor@newIdx=%zd",
    container->revision.totalContainerWidth, container->revision.totalContainerHeight,
    container->revision.containerOffsetX, container->revision.containerOffsetY,
    static_cast<std::ptrdiff_t>(container->getVisibleIndices().first),
    static_cast<std::ptrdiff_t>(container->getVisibleIndices().second),
    debugKeyAt(container, container->getVisibleIndices().first),
    debugKeyAt(container, container->getVisibleIndices().second),
    anchorKey.empty() ? "(none)" : anchorKey.c_str(),
    static_cast<std::ptrdiff_t>(anchorKey.empty() ? UNDEFINED_INDEX : container->findElementIndexByKey(anchorKey)));

  // Apply scroll corrections, and pick the window again if the offset moved.
  bool offsetConfirmed = !input.containerOffsetEnabled && !headerMovedOffset;
  bool scrollCorrected = resolveScroll(container, anchorKey, anchorDelta, hadElementsBefore, offsetConfirmed);
  if (headerMovedOffset) {
    container->containerOffsetCorrected = true;
  }
  if (scrollCorrected) {
    measure(container, true);
    SL_LOG("  remeasured: offset=(%.1f,%.1f) visible=[%zd..%zd] visKeys=[%s..%s] invInit=%d",
      container->revision.containerOffsetX, container->revision.containerOffsetY,
      static_cast<std::ptrdiff_t>(container->getVisibleIndices().first),
      static_cast<std::ptrdiff_t>(container->getVisibleIndices().second),
      debugKeyAt(container, container->getVisibleIndices().first),
      debugKeyAt(container, container->getVisibleIndices().second),
      container->invertedInitialized ? 1 : 0);
  }

  // The commit token is just the running operation's id. No operation means no token.
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

  // Reset the measured range so it reflects only this pass.
  container->revision.measurementElementStartIndex = UNDEFINED_INDEX;
  container->revision.measurementElementEndIndex = UNDEFINED_INDEX;

  /*
   * After an insert, remove or reorder, rows still hold their old offsets. Reflow first so
   * the window is chosen from real positions and no row that moved into view is left blank.
   * It also lets the window pass use its fast search instead of scanning every row.
   */
  if (container->elementsStructureDirty) {
    layoutElements(container);
  }

  // The first revision fills from the edge. After a correction, use the corrected offset.
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

  // Fill from the start, or from the end for an inverted list.
  for (std::size_t iteration = 0; iteration < elementsSize; ++iteration) {
    std::size_t nextElementIndex = container->inverted ? (elementsSize - 1 - iteration) : iteration;
    if (!visitMeasuredElement(container, nextElementIndex, measuredMinIndex, measuredMaxIndex)) {
      continue;
    }

    const Element& nextElement = container->revision.elements[nextElementIndex];
    accumulated += container->horizontal ? nextElement.width : nextElement.height;

    // Stop once the window plus the overscan buffer is full, shared across columns.
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
   * Measure the window plus a buffer on each side so scrolling shows rows, not blanks.
   * Overscan counts in window heights, so 1 means one window above and one below.
   */
  double overscanSize = windowSize * container->overscan;
  double lowerBound = containerOffset - overscanSize;
  double upperBound = containerOffset + windowSize + overscanSize;

  std::size_t measuredMinIndex = UNDEFINED_INDEX;
  std::size_t measuredMaxIndex = UNDEFINED_INDEX;

  // Before a reflow, offsets may be stale and out of order, so scan every row instead.
  bool geometryOrdered = !container->elementsStructureDirty;

  auto elementOffsetAt = [&](std::size_t index) {
    const Element& element = container->revision.elements[index];
    return container->horizontal ? element.offsetX : element.offsetY;
  };
  auto elementSizeAt = [&](std::size_t index) {
    const Element& element = container->revision.elements[index];
    return container->horizontal ? element.width : element.height;
  };

  auto visit = [&](std::size_t nextElementIndex) {
    visitMeasuredElement(container, nextElementIndex, measuredMinIndex, measuredMaxIndex);
  };

  /*
   * Binary search a column for the first row whose end is past lowerBound.
   * Rows in a column sit end to end, so this works, and deep scrolls avoid walking every row.
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
    for (std::size_t nextElementIndex = seekTrack(0, 1); nextElementIndex < elementsSize; ++nextElementIndex) {
      if (elementOffsetAt(nextElementIndex) > upperBound) {
        break;
      }
      visit(nextElementIndex);
    }
  } else if (geometryOrdered && container->columns > 1) {
    // Rows go to columns in turn, so each column is in order. Search each one separately.
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
    // Offsets are not in order yet, so check every row against the window.
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

void Virtualizer::finalizeMeasurement(Container* container, std::size_t measuredMinIndex, std::size_t measuredMaxIndex) {
  // An inverted list runs backwards, so its start index is the higher one.
  if (container->inverted) {
    container->revision.measurementElementStartIndex = measuredMaxIndex;
    container->revision.measurementElementEndIndex = measuredMinIndex;
  } else {
    container->revision.measurementElementStartIndex = measuredMinIndex;
    container->revision.measurementElementEndIndex = measuredMaxIndex;
  }
}

void Virtualizer::layoutElements(Container* container) {
  std::size_t elementsSize = container->revision.elements.size();

  double trackSize = container->horizontal
    ? container->revision.windowContainerHeight / (container->columns > 0 ? container->columns : 1)
    : container->revision.windowContainerWidth / (container->columns > 0 ? container->columns : 1);

  // Unmeasured rows get the average size, or the estimate until there is an average.
  auto [fallbackWidth, fallbackHeight] = effectiveFallbackSize(container);

  /*
   * Only inputs that move rows count. The footer and the window size along the scroll axis
   * never do, so a chat composer resizing the list doesn't walk every row. Columns take
   * their width from the window's cross size, so that one counts.
   */
  bool layoutParamsChanged =
    container->headerSize != container->lastLayoutHeaderSize ||
    (container->horizontal
      ? container->revision.windowContainerHeight != container->lastLayoutWindowHeight
      : container->revision.windowContainerWidth != container->lastLayoutWindowWidth) ||
    container->columns != container->lastLayoutColumns ||
    container->horizontal != container->lastLayoutHorizontal;

  /*
   * The sizing loop visits every row, so only run it when it can change something:
   * a new fallback size, a new layout setting, or new rows. Otherwise every scroll frame
   * would walk the whole list for nothing.
   */
  bool fallbackDimensionsChanged =
    fallbackWidth != container->lastFallbackWidth ||
    fallbackHeight != container->lastFallbackHeight;

  bool anyNewlyEstimated = false;

  if (fallbackDimensionsChanged || layoutParamsChanged || container->elementsStructureDirty) {
    if (container->columns > 1) {
      double Element::*size = container->horizontal ? &Element::width : &Element::height;
      double Element::*crossSize = container->horizontal ? &Element::height : &Element::width;
      double fallbackSize = container->horizontal ? fallbackWidth : fallbackHeight;
      for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
        Element& nextElement = container->revision.elements[nextElementIndex];
        if (!nextElement.estimated && nextElement.*size != fallbackSize) {
          nextElement.*size = fallbackSize;
          anyNewlyEstimated = true;
        }
        if (nextElement.*crossSize != trackSize) {
          nextElement.*crossSize = trackSize;
          anyNewlyEstimated = true;
        }
      }
    } else {
      for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
        Element& nextElement = container->revision.elements[nextElementIndex];
        if (!nextElement.estimated &&
            (nextElement.width != fallbackWidth || nextElement.height != fallbackHeight)) {
          nextElement.width = fallbackWidth;
          nextElement.height = fallbackHeight;
          anyNewlyEstimated = true;
        }
      }
    }

    container->lastFallbackWidth = fallbackWidth;
    container->lastFallbackHeight = fallbackHeight;
  }

  // Recomputing offsets walks every row, so skip it unless a size, row or setting changed.
  bool sizesDirty = container->elementsSizeDirtyFromIndex != UNDEFINED_INDEX;

  if (anyNewlyEstimated || layoutParamsChanged || container->elementsStructureDirty || sizesDirty) {
    /*
     * Reflow from the first row that moved. Row, setting or fallback changes can move
     * anything, so they start at 0. A size change only moves the rows after it.
     */
    std::size_t reflowFrom =
      (anyNewlyEstimated || layoutParamsChanged || container->elementsStructureDirty)
        ? 0
        : container->elementsSizeDirtyFromIndex;

    recomputeElementOffsets(container, reflowFrom, container->elementsSizeDirtyToIndex);
    container->lastLayoutHeaderSize = container->headerSize;
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
   * All row positions are written here, so this is where snap and sticky caches go stale.
   * Only bump the geometry version when an offset really changed, or those caches get
   * thrown away for nothing.
   */
  bool anyOffsetChanged = false;

  std::size_t elementsSize = container->revision.elements.size();

  // A full pass rebuilds the widest extent. A partial pass can only grow it.
  double crossMax = fromIndex == 0 ? 0.0 : container->maxCrossAxisExtent;

  if (fromIndex >= elementsSize) {
    container->maxCrossAxisExtent = crossMax;
    return;
  }

  if (container->columns > 1) {
    double trackSize = container->horizontal
      ? container->revision.windowContainerHeight / container->columns
      : container->revision.windowContainerWidth / container->columns;
    if (container->horizontal) {
      reflowTracks<&Element::offsetX, &Element::width, &Element::offsetY, &Element::height>(
        container, fromIndex, trackSize, crossMax, anyOffsetChanged);
    } else {
      reflowTracks<&Element::offsetY, &Element::height, &Element::offsetX, &Element::width>(
        container, fromIndex, trackSize, crossMax, anyOffsetChanged);
    }
  } else {
    // Rows start below the header, or right after the row before fromIndex.
    double nextOffset = container->headerSize;

    if (fromIndex > 0) {
      const Element& previousElement = container->revision.elements[fromIndex - 1];
      nextOffset = container->horizontal
        ? previousElement.offsetX + previousElement.width
        : previousElement.offsetY + previousElement.height;
    }

    /*
     * This is the hottest loop in the core. The orientation check is kept out of it, and
     * a row's index is only written when it is stale, to avoid needless memory writes.
     */
    Element* elements = container->revision.elements.data();

    /*
     * Past the last changed row, stop at the first row already at the right offset.
     * Every row after it is correct too, so a reflow that moves nothing costs almost nothing.
     * Two guards: don't stop between two changed rows, and a full pass from 0 must visit
     * every row to rebuild the widest extent.
     */
    bool canStopEarly = fromIndex > 0 && changedThroughIndex != UNDEFINED_INDEX;

    if (container->horizontal) {
      reflowSingleTrack<&Element::offsetX, &Element::width, &Element::offsetY, &Element::height>(
        elements, fromIndex, elementsSize, changedThroughIndex, canStopEarly, nextOffset, crossMax, anyOffsetChanged);
    } else {
      reflowSingleTrack<&Element::offsetY, &Element::height, &Element::offsetX, &Element::width>(
        elements, fromIndex, elementsSize, changedThroughIndex, canStopEarly, nextOffset, crossMax, anyOffsetChanged);
    }
  }

  container->maxCrossAxisExtent = crossMax;

  if (anyOffsetChanged) {
    container->geometryVersion++;
  }
}

void Virtualizer::recomputeTotalSize(Container* container) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  // The content size is the furthest row edge. Row offsets already include the header.
  Size extent = tailExtent(container);

  /*
   * Along the scroll axis, add the footer and never go below the header.
   * Across it, never go below the window, so columns can't collapse to zero width,
   * and cover any row measured wider than the window.
   */
  if (container->horizontal) {
    container->revision.totalContainerWidth = std::max(extent.width, container->headerSize) + container->footerSize;
    container->revision.totalContainerHeight =
      std::max({extent.height, container->maxCrossAxisExtent, container->revision.windowContainerHeight});
  } else {
    container->revision.totalContainerHeight = std::max(extent.height, container->headerSize) + container->footerSize;
    container->revision.totalContainerWidth =
      std::max({extent.width, container->maxCrossAxisExtent, container->revision.windowContainerWidth});
  }

  /*
   * Freeze the average once, from the first real measurements. Unmeasured rows then keep
   * a stable size and the visible content can be held in place.
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

  double previousWidth = nextElement.width;
  double previousHeight = nextElement.height;
  bool wasMeasured = nextElement.measured;
  bool dimensionsChanged = previousWidth != size.width || previousHeight != size.height;

  /*
   * Already measured at this size. Fabric reports every mounted row on every layout pass,
   * so most calls stop here. Without this, each one would reflow the rest of the list.
   */
  if (!dimensionsChanged && wasMeasured) {
    return false;
  }

  // On the anchor row's first measurement, remember how far its bottom edge moved.
  if (!wasMeasured && dimensionsChanged && container->columns <= 1) {
    const Anchor* compensationAnchor = container->compensationAnchor();
    if (compensationAnchor != nullptr && !compensationAnchor->key.empty() && nextElement.key == compensationAnchor->key) {
      container->anchorFirstMeasurementDelta += container->horizontal
        ? size.width - previousWidth
        : size.height - previousHeight;
    }
  }

  nextElement.width = size.width;
  nextElement.height = size.height;
  nextElement.estimated = true;
  nextElement.measured = true;

  // A real measurement always replaces a prediction.
  nextElement.predicted = false;

  // Limit the coming reflow without scheduling a second one in the layout pass.
  container->noteElementSizeSpan(index);

  // Add to the running total for the average. A remeasure only adds the difference.
  if (wasMeasured) {
    container->revision.measuredRealTotalWidth += size.width - previousWidth;
    container->revision.measuredRealTotalHeight += size.height - previousHeight;
  } else {
    container->revision.measuredRealCount++;
    container->revision.measuredRealTotalWidth += size.width;
    container->revision.measuredRealTotalHeight += size.height;
  }

  // A first measurement equal to the estimate still counts for the average but moves nothing.
  return dimensionsChanged;
}

std::size_t Virtualizer::applyPredictedElementSize(Container* container, const std::string& key, Size size) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  if (key.empty()) {
    return UNDEFINED_INDEX;
  }

  std::size_t index = container->findElementIndexByKey(key);

  // The row doesn't exist yet, which is normal when measuring ahead. Save it for the next update.
  if (index >= container->revision.elements.size()) {
    container->setPredictedSize(key, size);
    return UNDEFINED_INDEX;
  }

  Element& nextElement = container->revision.elements[index];

  // A real measurement always wins. Drop a late prediction so it can't come back later.
  if (nextElement.measured) {
    return UNDEFINED_INDEX;
  }

  bool dimensionsChanged = nextElement.width != size.width || nextElement.height != size.height;

  nextElement.width = size.width;
  nextElement.height = size.height;
  nextElement.estimated = true;
  nextElement.predicted = true;

  // Predictions stay out of the average, which only counts real measurements.
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

  // Only this row and the ones after it move. The caller updates the total once per batch.
  recomputeElementOffsets(container, fromIndex, container->elementsSizeDirtyToIndex);
  container->elementsSizeDirtyToIndex = 0;

  /*
   * Keep the anchor row still while rows off screen get measured.
   * Corrections that aim at a fixed offset, like the bottom, handle themselves.
   */
  const Anchor* compensationAnchor = container->compensationAnchor();
  if (compensationAnchor != nullptr) {
    double compensationDelta = compensationAnchor->subOffset;
    std::size_t anchorIndex = container->findElementIndexByKey(compensationAnchor->key);
    if (anchorIndex != UNDEFINED_INDEX) {
      // Compare the target before clamping, so a bounce at the top is left alone.
      double rawAnchoredOffset = container->getElementOffset(anchorIndex) + compensationDelta;
      /*
       * When the anchor row starts above the viewport, the reader sees its bottom part.
       * On its first measurement, hold its bottom edge so the size error lands off screen.
       */
      if (compensationDelta > 0.0) {
        rawAnchoredOffset += container->anchorFirstMeasurementDelta;
      }
      /*
       * Only clamp at zero. The total is stale while measuring, so clamping the top end
       * would yank the anchor. resolveScroll clamps it next frame.
       */
      double anchoredOffset = rawAnchoredOffset < 0.0 ? 0.0 : rawAnchoredOffset;
      correctOffsetIfMoved(container, anchoredOffset, rawAnchoredOffset);
    }
  }
  container->anchorFirstMeasurementDelta = 0.0;

  /*
   * An inverted list resting on its newest row follows that row as it grows. Otherwise the
   * growth lands below the fold, and a reply that finishes after the last commit gets no
   * next frame to fix it. It only follows when the reader hasn't scrolled away, no finger
   * is down and no correction is running. The stored total is stale here, so the bottom
   * comes from the reflowed rows.
   * A pending scroll to the end, or a list still settling on the bottom it opened at,
   * follows the bottom here too for the same reason.
   */
  bool followBottom = container->pendingScrollToEnd ||
    (container->inverted && container->invertedOpeningPin && container->restingAtInvertedBottom &&
     !container->invertedBottomReleased && !container->gestureActive && !container->operation);
  if (!followBottom && container->inverted && !container->invertedBottomReleased && !container->gestureActive &&
      !container->operation && !container->anchor.key.empty()) {
    followBottom = isLastAnchorable(container, container->findElementIndexByKey(container->anchor.key));
  }
  if (followBottom) {
    Size extent = tailExtent(container);
    double contentEnd = container->horizontal ? extent.width : extent.height;
    double total = std::max(contentEnd, container->headerSize) + container->footerSize;
    double bottom = std::max(0.0, total - container->getWindowContainerSize());
    correctOffsetIfMoved(container, bottom, bottom);
  }
}

void Virtualizer::applyHeaderSizeChange(Container* container, double previousHeaderSize) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  double delta = container->headerSize - previousHeaderSize;
  if (delta == 0.0 || container->revision.elements.empty()) {
    return;
  }

  const double offset = scrollAxisOffset(container);

  /*
   * A running anchor correction already knows where the rows belong, so resolve it against
   * the reflowed rows. Its last written offset may be clamped and can't be trusted here.
   */
  if (container->operation && container->operation->target.mode == AnchorMode::Element) {
    std::size_t anchorIndex = container->findElementIndexByKey(container->operation->target.key);
    if (anchorIndex != UNDEFINED_INDEX) {
      double target = container->getElementOffset(anchorIndex) + container->operation->target.subOffset;
      target = target < 0.0 ? 0.0 : target;
      SL_LOG("  headerSizeChange: %.1f->%.1f offset=%.1f->%.1f resolved op=%llu",
        previousHeaderSize, container->headerSize, offset, target,
        static_cast<unsigned long long>(container->operation->id));
      correctOffsetIfMoved(container, target, target);
      return;
    }
  }

  // The header was fully scrolled off, so every row on screen moved. Move the offset with them.
  SL_LOG("  headerSizeChange: %.1f->%.1f offset=%.1f branch=%s anchorSub=%.1f",
    previousHeaderSize, container->headerSize, offset,
    (offset > 0.0 && offset >= previousHeaderSize) ? "hold" : "push", container->anchor.subOffset);
  if (offset > 0.0 && offset >= previousHeaderSize) {
    correctOffset(container, std::max(0.0, offset + delta));
    return;
  }

  /*
   * The header is on screen and pushes the rows down. Keep the offset and shift the anchor
   * instead, so it still points at this offset.
   */
  shiftAnchors(container, -delta);
}

void Virtualizer::applyWindowSizeChange(Container* container, double previousWindowSize) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  double windowSize = container->getWindowContainerSize();
  if (!container->inverted || container->revision.elements.empty() ||
      previousWindowSize <= 0.0 || windowSize <= 0.0 ||
      std::fabs(windowSize - previousWindowSize) < OFFSET_MOVED_THRESHOLD ||
      !container->invertedInitialized || container->invertedBottomReleased || container->gestureActive) {
    return;
  }

  // Judge against the old window, or a big shrink would look like the reader scrolled away.
  double total = container->horizontal ? container->revision.totalContainerWidth : container->revision.totalContainerHeight;
  double offset = scrollAxisOffset(container);
  if (!atInvertedBottom(offset, total, previousWindowSize)) {
    return;
  }

  /*
   * Jump to the new bottom now and keep following it as a scroll to the end, since rows
   * mounting into a bigger window get measured later.
   */
  container->pendingScrollToEnd = true;
  double bottom = std::max(0.0, total - windowSize);
  SL_LOG("  windowSizeChange: %.1f->%.1f offset=%.1f->%.1f", previousWindowSize, windowSize, offset, bottom);
  if (!correctOffsetIfMoved(container, bottom, bottom)) {
    return;
  }

  // Move the anchor too, or holding the content in place would pull the view back.
  shiftAnchors(container, bottom - offset);
}

void Virtualizer::updateElementAtIndex(Container* container, std::size_t index, Size size) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

#if SHADOWLIST_DEBUG_LOG
  double tracePreviousTotal = container->horizontal ? container->revision.totalContainerWidth : container->revision.totalContainerHeight;
  double tracePreviousSize = index < container->revision.elements.size()
    ? (container->horizontal ? container->revision.elements[index].width : container->revision.elements[index].height)
    : 0.0;
#endif
  if (applyElementSize(container, index, size)) {
    commitElementSizes(container, index);
    SL_LOG("  replaceChild size: index=%zu %.1f->%.1f total=%.1f corrected=%d anchor=%s",
      index, tracePreviousSize, container->horizontal ? size.width : size.height, tracePreviousTotal,
      container->containerOffsetCorrected ? 1 : 0, container->anchor.key.c_str());
  }
}

/*
 * Throw away every prediction, saved or applied, and fall back to the estimate.
 * Used when predictions go stale, mostly on a width change since text rewraps.
 * Rows measured for real keep their size.
 */
void Virtualizer::invalidatePredictions(Container* container) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  SL_LOG("  invalidatePredictions: staged=%zu", container->predictedSizes.size());
  container->predictedSizes.clear();

  bool anyCleared = false;
  for (Element& nextElement : container->revision.elements) {
    if (!nextElement.predicted) {
      continue;
    }

    nextElement.predicted = false;
    // Clear estimated too, or the row would keep its stale predicted size forever.
    nextElement.estimated = false;
    anyCleared = true;
  }

  if (anyCleared) {
    // Predicted rows went back to the fallback size, so reflow the whole list.
    container->markElementSizeDirty(0);
  }
}

/*
 * Apply saved predictions whose rows now exist.
 * Walk the few saved predictions, not the whole list. Run every frame, not just when keys
 * change, since predictions usually arrive on frames with no new data.
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

    // A row already measured for real ignores the prediction, and the entry is dropped.
    if (!predictedElement.measured &&
        (predictedElement.width != entry->second.width || predictedElement.height != entry->second.height)) {
      SL_LOG("  prediction: index=%zu %.1f->%.1f estimated=%d",
        predictedIndex, container->horizontal ? predictedElement.width : predictedElement.height,
        container->horizontal ? entry->second.width : entry->second.height, predictedElement.estimated ? 1 : 0);
      predictedElement.width = entry->second.width;
      predictedElement.height = entry->second.height;
      // The layout loop can't see this change, so mark it for a reflow.
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

  std::vector<Element>& previousElements = container->revision.elements;

  /*
   * Find surviving rows through the key map the last reconcile built, instead of a
   * throwaway copy. It stays in step with the rows, and the key check below makes sure.
   */
  const std::unordered_map<std::string, std::size_t>& previousIndexByKey =
    container->revision.elementIndexByKey;

  /*
   * Fast path for a plain append, the most common change.
   * Rebuilding the key map costs about 6ms at 100k rows, a dropped frame. An append keeps
   * every old index, so just extend the rows and the map in place.
   * An empty list is a cold start, not an append, and takes the general path.
   */
  if (!previousElements.empty() && nextKeys.size() > previousElements.size()) {
    bool isAppend = true;
    for (std::size_t nextElementIndex = 0; nextElementIndex < previousElements.size(); ++nextElementIndex) {
      if (previousElements[nextElementIndex].key != nextKeys[nextElementIndex]) {
        isAppend = false;
        break;
      }
    }

    if (isAppend) {
      previousElements.reserve(nextKeys.size());
      container->revision.elementIndexByKey.reserve(nextKeys.size());

      for (std::size_t nextElementIndex = previousElements.size(); nextElementIndex < nextKeys.size(); ++nextElementIndex) {
        previousElements.emplace_back();
        previousElements.back().key = nextKeys[nextElementIndex];
        previousElements.back().index = nextElementIndex;
        // A duplicate key keeps its first index, same as the general path.
        container->revision.setIndexForKey(nextKeys[nextElementIndex], nextElementIndex);
      }

      container->geometryVersion++;
      container->elementsStructureDirty = true;
      return;
    }
  }

  /*
   * Fast path for a plain prepend, like loading older chat messages.
   * Every old row survives with a shifted index, so keep the map instead of rebuilding it.
   */
  if (!previousElements.empty() && nextKeys.size() > previousElements.size()) {
    std::size_t prependCount = nextKeys.size() - previousElements.size();

    bool isPrepend = true;
    for (std::size_t nextElementIndex = 0; nextElementIndex < previousElements.size(); ++nextElementIndex) {
      if (previousElements[nextElementIndex].key != nextKeys[nextElementIndex + prependCount]) {
        isPrepend = false;
        break;
      }
    }

    // A prepended key that already exists takes the slow path. It is rare.
    if (isPrepend) {
      for (std::size_t nextElementIndex = 0; nextElementIndex < prependCount; ++nextElementIndex) {
        if (container->revision.indexForKey(nextKeys[nextElementIndex]) != UNDEFINED_INDEX) {
          isPrepend = false;
          break;
        }
      }
    }

    if (isPrepend) {
      // Every old index grows by prependCount. Shift the bias once instead of each map entry.
      container->revision.indexBias -= prependCount;

      previousElements.insert(previousElements.begin(), prependCount, Element{});

      container->revision.elementIndexByKey.reserve(nextKeys.size());
      for (std::size_t nextElementIndex = 0; nextElementIndex < prependCount; ++nextElementIndex) {
        previousElements[nextElementIndex].key = nextKeys[nextElementIndex];
        container->revision.setIndexForKey(nextKeys[nextElementIndex], nextElementIndex);
      }

      /*
       * Don't renumber rows here. The reflow from row 0 sets every index anyway, and nothing
       * reads the index before then.
       */
      container->geometryVersion++;
      container->elementsStructureDirty = true;
      return;
    }
  }

  std::vector<Element> nextElements;
  nextElements.reserve(nextKeys.size());

  // Rebuild the key map with the rows so anchor lookups stay fast. Duplicates keep their first index.
  std::unordered_map<std::string, std::size_t> nextElementIndexByKey;
  nextElementIndexByKey.reserve(nextKeys.size());

  /*
   * Old map values still carry the old bias. Only reset it after the last read below,
   * or every lookup in this loop finds the wrong row.
   */
  const std::size_t previousIndexBias = container->revision.indexBias;

  std::size_t survivorCount = 0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < nextKeys.size(); ++nextElementIndex) {
    const std::string& nextKey = nextKeys[nextElementIndex];

    /*
     * Only the first copy of a key can take over the old row, since both maps keep the
     * first copy. A later duplicate gets a new row.
     */
    bool firstOccurrence = nextElementIndexByKey.emplace(nextKey, nextElementIndex).second;

    auto previousElementEntry = nextKey.empty() ? previousIndexByKey.end() : previousIndexByKey.find(nextKey);
    std::size_t previousElementIndex = previousElementEntry != previousIndexByKey.end()
      ? previousElementEntry->second - previousIndexBias
      : UNDEFINED_INDEX;
    bool survives =
      firstOccurrence &&
      previousElementIndex < previousElements.size() &&
      previousElements[previousElementIndex].key == nextKey;

    if (survives) {
      // Keep the row's size and flags. Move it straight into place to avoid a second copy.
      nextElements.push_back(std::move(previousElements[previousElementIndex]));
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

  // Rows moved, so any geometry derived from the old list is stale.
  container->geometryVersion++;

  // Rows were added, removed or moved, so the next layout must recompute offsets.
  container->elementsStructureDirty = true;

  /*
   * No row survived, so the whole dataset was swapped. Reset the average so it is taken
   * again from the new rows. If some rows survived, the old average still fits.
   */
  if (survivorCount == 0) {
    container->revision.averageElementWidth = 0.0;
    container->revision.averageElementHeight = 0.0;
    container->revision.measuredRealCount = 0;
    container->revision.measuredRealTotalWidth = 0.0;
    container->revision.measuredRealTotalHeight = 0.0;
  }
}

std::vector<Anchor> Virtualizer::captureFallbackAnchors(Container* container, double inputOffset) {
  std::vector<Anchor> candidates;
  const std::vector<Element>& elements = container->revision.elements;
  std::size_t anchorIndex = container->findElementIndexByKey(container->anchor.key);
  if (anchorIndex == UNDEFINED_INDEX || container->anchor.mode != AnchorMode::Element) {
    return candidates;
  }

  /*
   * Collect the anchorable rows after the anchor that start inside the viewport, each with
   * the offset that holds it in place. In a grid, keep going until every column has passed
   * the bottom of the viewport.
   */
  double viewportEnd = inputOffset + container->getWindowContainerSize();
  std::size_t columns = container->columns > 0 ? container->columns : 1;
  std::size_t tracksPast = 0;
  for (std::size_t index = anchorIndex + 1; index < elements.size() && tracksPast < columns; ++index) {
    double elementOffset = container->getElementOffset(index);
    if (elementOffset >= viewportEnd) {
      ++tracksPast;
      continue;
    }
    if (container->isAnchorable(elements[index].key)) {
      candidates.push_back(Anchor{elements[index].key, inputOffset - elementOffset, AnchorMode::Element});
    }
  }
  return candidates;
}

void Virtualizer::captureAnchor(Container* container, double inputOffset) {
  container->anchor = Anchor{"", 0.0, AnchorMode::Element};

  const std::vector<Element>& previousElements = container->revision.elements;
  if (previousElements.empty()) {
    return;
  }

  auto elementOffsetOf = [&](const Element& element) {
    return container->horizontal ? element.offsetX : element.offsetY;
  };
  auto elementSizeOf = [&](const Element& element) {
    return container->horizontal ? element.width : element.height;
  };

  /*
   * The anchor is the first content row at the top of the viewport. Decoration rows like
   * date pills are skipped. firstPast keeps the actual top row in case the viewport holds
   * only decoration.
   * A single column is in order, so binary search for the start. Grids scan from 0.
   */
  std::size_t scanStart = 0;
  if (container->columns <= 1) {
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
    if (container->isAnchorable(previousElement.key)) {
      /*
       * A row with only a guessed size may change size in several steps, moving everything
       * below it each time. This is common right after a prepend at the top. If a row with a
       * known size starts inside the viewport, anchor that one instead.
       * Only for single column lists that aren't inverted. Inverted lists use their anchor
       * to tell whether they rest on the newest row.
       */
      const Element* anchorElement = &previousElement;
      if (container->columns <= 1 && !container->inverted && !previousElement.measured && !previousElement.predicted) {
        double viewportEnd = inputOffset + container->getWindowContainerSize();
        for (std::size_t laterIndex = previousElementIndex + 1; laterIndex < previousElements.size(); ++laterIndex) {
          const Element& laterElement = previousElements[laterIndex];
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
   * No content row is in or below the viewport. Use the last content row, then the top row
   * even if it is decoration, then the very last row. There is always an anchor.
   */
  for (auto reverse = previousElements.rbegin(); reverse != previousElements.rend(); ++reverse) {
    if (container->isAnchorable(reverse->key)) {
      container->anchor = Anchor{reverse->key, inputOffset - elementOffsetOf(*reverse), AnchorMode::Element};
      return;
    }
  }
  const Element& fallback = firstPast != nullptr ? *firstPast : previousElements.back();
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

  // An emptied list starts over, so an inverted list sticks to the bottom again when rows arrive.
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

  // Track the total every frame so step 2b can tell when the bottom stops growing.
  double previousTotalForScrollToEnd = container->pendingScrollToEndLastTotal;
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

  /*
   * Reuse the id when the same correction is requested again, so the host's reply still
   * matches it over several frames. A new type or a new row gets a new id.
   */
  auto operationId = [&](OperationType type, AnchorMode mode, const std::string& key) -> std::uint64_t {
    if (container->operation && container->operation->type == type &&
        container->operation->target.mode == mode &&
        (mode != AnchorMode::Element || container->operation->target.key == key)) {
      return container->operation->id;
    }
    return container->nextOperationId++;
  };

  // Start or repeat a correction that aims at the end, until the view gets there.
  auto requestFixed = [&](OperationType type) {
    if (container->operation && container->operation->type != type) {
      SL_LOG("  op replaced: type=%d key=%s -> type=%d", static_cast<int>(container->operation->type),
        container->operation->target.key.c_str(), static_cast<int>(type));
    }
    container->operation =
      Operation{operationId(type, AnchorMode::EndEdge, ""), type, Anchor{"", 0.0, AnchorMode::EndEdge}};
  };

  /*
   * Start or repeat a correction that aims at a row. The target is looked up by key each
   * frame, so it follows the row while nearby rows get measured.
   */
  auto requestAnchor = [&](OperationType type, const std::string& key, double delta, double viewPosition = 0.0) {
    if (container->operation && (container->operation->type != type || container->operation->target.key != key)) {
      SL_LOG("  op replaced: type=%d key=%s -> type=%d key=%s", static_cast<int>(container->operation->type),
        container->operation->target.key.c_str(), static_cast<int>(type), key.c_str());
    }
    container->operation =
      Operation{operationId(type, AnchorMode::Element, key), type, Anchor{key, delta, AnchorMode::Element}, viewPosition};
  };

  /*
   * Set when a scroll command arrives this frame. The command owns the offset, so holding
   * the old visible content must not pull the view back.
   */
  bool commandRequestedThisFrame = false;

  /*
   * Step 0. Content shrank and left the view past the new end, like collapsing a tree.
   * Go back to the bottom. Checking for a shrink avoids fighting a bounce.
   * It runs as an operation so it survives recommits, and step 3 clears it on arrival.
   */
  if (!container->inverted && elementsSize > 0 &&
      currentOffset > maxOffset + OFFSET_MOVED_THRESHOLD &&
      totalSize < previousTotalForScrollToEnd) {
    /*
     * Only when no anchor will place the view. The shrink is often above the viewport, and
     * the row on screen moved up with it. Steps 3 and 4 follow that row and clamp when
     * needed. Jumping to the end would show rows further down instead.
     */
    bool anchorPlacesView = false;
    if (container->operation && container->operation->target.mode == AnchorMode::Element) {
      anchorPlacesView = container->findElementIndexByKey(container->operation->target.key) != UNDEFINED_INDEX;
    } else if (!container->operation && hadElementsBefore && !anchorKey.empty()) {
      std::size_t anchorIndex = container->findElementIndexByKey(anchorKey);
      anchorPlacesView = anchorIndex != UNDEFINED_INDEX &&
        std::fabs(container->getElementOffset(anchorIndex) + anchorDelta - currentOffset) >= OFFSET_MOVED_THRESHOLD;
    }
    if (!anchorPlacesView) {
      requestFixed(OperationType::ShrinkClamp);
    }
  }

  // Step 1. Scroll to an index.
  if (container->scrollToIndexTarget != UNDEFINED_INDEX) {
    if (container->scrollToIndexTarget < elementsSize) {
      /*
       * Follow the row itself, not a fixed offset. Its offset is a guess at first, and
       * following the row settles on it as the area gets measured.
       */
      const std::string targetKey = container->getElementAtIndex(container->scrollToIndexTarget).key;
      // The view position travels with the operation and is applied again each frame.
      requestAnchor(OperationType::ScrollToKey, targetKey, 0.0, container->scrollToIndexViewPosition);
      commandRequestedThisFrame = true;
      // This replaces the bottom pin, but only for an index in range.
      container->invertedInitialized = true;
      container->invertedOpeningPin = false;
    }
    container->scrollToIndexTarget = UNDEFINED_INDEX;
  }

  /*
   * Step 1b. Scroll to the start. It holds the first row below the header so it keeps up
   * with measuring. It is a command, so momentum neither cancels nor moves it.
   */
  if (container->pendingScrollToStart) {
    container->pendingScrollToStart = false;
    if (elementsSize > 0) {
      std::size_t leading = 0;
      if (container->getElementOffset(elementsSize - 1) < container->getElementOffset(0)) {
        leading = elementsSize - 1;
      }
      requestAnchor(
        OperationType::ScrollToStart,
        container->getElementAtIndex(leading).key,
        -container->getElementOffset(leading));
      commandRequestedThisFrame = true;
      container->invertedInitialized = true;
      container->invertedOpeningPin = false;
    }
  }

  /*
   * Step 2. An inverted list sticks to the bottom until it gets there, following it as the
   * content grows. Wait for the window size so the target is right.
   */
  if (container->inverted && !container->invertedInitialized && elementsSize > 0 && windowSize > 0.0) {
    requestFixed(OperationType::BottomPin);
    container->invertedOpeningPin = true;
    // Done only once the list can scroll and sits at the bottom. Until then keep pinning.
    if (offsetConfirmed &&
        totalSize > windowSize &&
        std::fabs(currentOffset - maxOffset) < OFFSET_ARRIVED_THRESHOLD) {
      container->invertedInitialized = true;
    }
  }

  /*
   * Step 2b. Scroll to the end, aiming again every frame as the content grows. It is done
   * when the view is at the bottom, the total held still and the last row has a real size.
   * A drag cancels it, momentum does not.
   */
  if (container->pendingScrollToEnd && elementsSize > 0 && windowSize > 0.0) {
    container->invertedOpeningPin = false;
    bool atBottom = std::fabs(currentOffset - maxOffset) < OFFSET_ARRIVED_THRESHOLD;
    bool totalStable = totalSize == previousTotalForScrollToEnd;
    if (atBottom && totalStable && container->hasTrustedSize(elementsSize - 1)) {
      container->pendingScrollToEnd = false;
    } else {
      requestFixed(OperationType::ScrollToEnd);
    }
  }

  /*
   * Step 3. Keep driving the running operation until the view gets there. The target is
   * worked out again each frame. It ends when its row is gone or the view arrives.
   */
  if (container->operation) {
    double rawTarget = 0.0;
    if (!resolveAnchorOffset(container, *container->operation, maxOffset, rawTarget)) {
      container->operation.reset();
    } else {
      double target = clampOffset(rawTarget);
      /*
       * A host report at the target ends the operation. So does our own write, if the host
       * last reported the target too. Without that, a resting list only sees its own writes
       * and republishes on every commit, about 500 times for one streaming reply.
       */
      bool reportedAtTarget = std::fabs(container->lastReportedOffset - target) < OFFSET_ARRIVED_THRESHOLD;
      if ((offsetConfirmed || reportedAtTarget) &&
          std::fabs(currentOffset - target) < OFFSET_ARRIVED_THRESHOLD) {
        SL_LOG("  op arrived: type=%d key=%s index=%zd target=%.1f", static_cast<int>(container->operation->type),
          container->operation->target.key.c_str(),
          static_cast<std::ptrdiff_t>(container->operation->target.mode == AnchorMode::Element
            ? container->findElementIndexByKey(container->operation->target.key) : UNDEFINED_INDEX),
          target);
        container->operation.reset();
        /*
         * A command that is already where it wants to be is done. Holding the old anchor
         * would move the view to a row further down after a data swap.
         */
        if (commandRequestedThisFrame) {
          return false;
        }
      } else {
        // The offset moved, so the caller picks the window again.
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
    std::size_t anchorIndex = container->findElementIndexByKey(anchorKey);
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
      if (container->inverted && !container->invertedBottomReleased && !container->gestureActive) {
        if (container->invertedOpeningPin && container->restingAtInvertedBottom) {
          invertedBottomAnchor = true;
        } else {
          invertedBottomAnchor = isLastAnchorable(container, anchorIndex);
        }
      }
      double rawAnchoredOffset = invertedBottomAnchor
        ? maxOffset
        : container->getElementOffset(anchorIndex) + anchorDelta;
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
