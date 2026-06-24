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
[[maybe_unused]] const char* visKeyAt(const Container *container, std::size_t index) {
  if (index < container->revision.elements.size()) {
    const std::string& key = container->revision.elements[index].key;
    return key.empty() ? "(empty)" : key.c_str();
  }
  return "(oob)";
}

/*
 * Map an in-flight operation's target anchor to its desired (unclamped) pixel offset
 * for the current revision. An EndEdge anchor (ScrollToEnd / BottomPin / ShrinkClamp)
 * resolves to maxOffset; an Element anchor resolves to its element's offset plus the
 * captured sub-offset, rederived each frame so it tracks the element as nearby rows
 * are measured. Returns false when an Element anchor's key is no longer present.
 *
 * Header-size compensation is intentionally NOT applied here: it only matters on the
 * frame a header size changes, where resolveScroll/updateElementAtIndex apply it inline
 * to the immediate write. The cross-frame drive uses the raw element offset.
 */
bool resolveAnchorOffset(Container *container, const Operation &operation, double maxOffset, double &outOffset) {
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

void Virtualizer::update(Container *container, const FrameInput &input) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  SL_LOG("update: keys=%zu prevElements=%zu off=(%.1f,%.1f) win=(%.1f,%.1f) inv=%d cols=%zu hdr=%.1f ftr=%.1f invInit=%d",
    input.keys.size(), container->revision.elements.size(),
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
   * Configure layout properties for this frame
   */
  container->inverted = input.inverted;
  container->horizontal = input.horizontal;
  container->columns = input.columns;
  container->overscan = input.overscan;
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
  container->nonAnchorableKeys.clear();
  for (const std::string &ignoredKey : input.nonAnchorableKeys) {
    container->nonAnchorableKeys.insert(ignoredKey);
  }

  /*
   * Reported scroll offset along the scroll axis
   */
  double inputOffset = container->horizontal ? input.containerOffsetX : input.containerOffsetY;

  /*
   * Did the reported offset actually move since the last frame? An unmoved offset means
   * the user is not driving, so an in-flight correction must survive recommits.
   */
  bool userMovedOffset = std::fabs(inputOffset - container->lastReportedOffset) >= OFFSET_MOVED_THRESHOLD;
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
  if (gestureTakeover) {
    container->operation.reset();
    container->pendingScrollToEnd = false;
    container->invertedInitialized = true;
  }
  container->lastReportedOffset = inputOffset;

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
    std::size_t nextSize = input.keys.size();
    bool frontChanged = prevSize && nextSize && container->revision.elements.front().key != input.keys.front();
    if (prevSize != nextSize || frontChanged) {
      long oldFrontNewIdx = -1;
      if (prevSize) {
        const std::string& oldFront = container->revision.elements.front().key;
        for (std::size_t i = 0; i < nextSize; ++i) {
          if (input.keys[i] == oldFront) { oldFrontNewIdx = static_cast<long>(i); break; }
        }
      }
      SL_LOG("  RECONCILE: size %zu->%zu front '%s'->'%s' oldFront@newIdx=%ld anchorKey=%s anchorDelta=%.1f",
        prevSize, nextSize,
        prevSize ? container->revision.elements.front().key.c_str() : "(none)",
        nextSize ? input.keys.front().c_str() : "(none)",
        oldFrontNewIdx, anchorKey.empty() ? "(none)" : anchorKey.c_str(), anchorDelta);
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
  bool keysChanged = container->revision.elements.size() != input.keys.size();
  if (!keysChanged) {
    for (std::size_t nextElementIndex = 0; nextElementIndex < input.keys.size(); nextElementIndex++) {
      if (container->revision.elements[nextElementIndex].key != input.keys[nextElementIndex]) {
        keysChanged = true;
        break;
      }
    }
  }
  if (keysChanged) {
    reconcileElements(container, input.keys);
  }

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
    Container *container;
    ~RevisionStatusGuard() {
      if (container->revisionStatus == RevisionStatusPending) {
        container->revisionStatus = RevisionStatusIdle;
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
    visKeyAt(container, container->getVisibleIndices().first),
    visKeyAt(container, container->getVisibleIndices().second),
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
      visKeyAt(container, container->getVisibleIndices().first),
      visKeyAt(container, container->getVisibleIndices().second),
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

  container->endRevision();
}

void Virtualizer::measure(Container *container, bool windowFromOffset) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  if (container->revisionStatus != RevisionStatusPending) {
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
   * The first revision fills a window from the edge (offsets not known yet). After a
   * scroll correction the remeasure selects the window around the corrected offset.
   */
  if (!windowFromOffset && container->revisionCount == RevisionCountFirst) {
    measureFirstRevision(container);
  } else {
    measureNextRevision(container);
  }

  layoutElements(container);
  recomputeTotalSize(container);
}

void Virtualizer::measureFirstRevision(Container *container) {
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
      auto [width, height] = container->estimatedElementSize;

      /*
       * No estimate configured: leave this element unsized.
       */
      if (width == 0.0 && height == 0.0) {
        continue;
      }

      nextElement.width = width;
      nextElement.height = height;
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

void Virtualizer::measureNextRevision(Container *container) {
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

  for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
    Element& nextElement = container->revision.elements[nextElementIndex];

    // Skip elements outside the visible window plus overscan buffer.
    double elementOffset = container->horizontal ? nextElement.offsetX : nextElement.offsetY;
    if (elementOffset < lowerBound || elementOffset > upperBound) {
      continue;
    }

    if (!nextElement.estimated) {
      auto [width, height] = container->estimatedElementSize;

      if (width == 0.0 && height == 0.0) {
        continue;
      }

      nextElement.width = width;
      nextElement.height = height;
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
  }

  finalizeMeasurement(container, measuredMinIndex, measuredMaxIndex);
}

void Virtualizer::finalizeMeasurement(Container *container, std::size_t measuredMinIndex, std::size_t measuredMaxIndex) {
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
   * The average element size is intentionally NOT computed here: the first-revision
   * window is estimate-filled. It is frozen from real measurements in
   * recomputeTotalSize instead, so MVCP does not chase a moving anchor.
   */
}

void Virtualizer::layoutElements(Container *container) {
  std::size_t elementsSize = container->revision.elements.size();

  double trackSize = container->horizontal
    ? container->revision.windowContainerHeight / (container->columns > 0 ? container->columns : 1)
    : container->revision.windowContainerWidth / (container->columns > 0 ? container->columns : 1);

  /*
   * Size unmeasured elements with the average, falling back to the estimate until
   * the average is frozen. Multi-column layouts force the cross-axis to the track size.
   */
  auto [estimatedWidth, estimatedHeight] = container->estimatedElementSize;
  double fallbackWidth = container->revision.averageElementWidth > 0.0
    ? container->revision.averageElementWidth : estimatedWidth;
  double fallbackHeight = container->revision.averageElementHeight > 0.0
    ? container->revision.averageElementHeight : estimatedHeight;

  for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
    Element& nextElement = container->revision.elements[nextElementIndex];

    if (container->columns > 1) {
      if (container->horizontal) {
        if (!nextElement.estimated) {
          nextElement.width = fallbackWidth;
        }
        nextElement.height = trackSize;
      } else {
        if (!nextElement.estimated) {
          nextElement.height = fallbackHeight;
        }
        nextElement.width = trackSize;
      }
    } else if (!nextElement.estimated) {
      nextElement.width = fallbackWidth;
      nextElement.height = fallbackHeight;
    }
  }

  recomputeElementOffsets(container, 0);
}

void Virtualizer::recomputeElementOffsets(Container *container, std::size_t fromIndex) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  std::size_t elementsSize = container->revision.elements.size();
  if (fromIndex >= elementsSize) {
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
        nextElement.offsetY = trackIndex * trackSize;
        nextElement.height = trackSize;
        nextElement.offsetX = trackSizes[trackIndex];
        trackSizes[trackIndex] += nextElement.width + nextElement.gapX;
      } else {
        nextElement.offsetX = trackIndex * trackSize;
        nextElement.width = trackSize;
        nextElement.offsetY = trackSizes[trackIndex];
        trackSizes[trackIndex] += nextElement.height + nextElement.gapY;
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

    for (std::size_t nextElementIndex = fromIndex; nextElementIndex < elementsSize; ++nextElementIndex) {
      Element& nextElement = container->revision.elements[nextElementIndex];
      nextElement.index = nextElementIndex;

      if (container->horizontal) {
        nextElement.offsetX = nextOffset;
        nextOffset += nextElement.width + nextElement.gapX;
      } else {
        nextElement.offsetY = nextOffset;
        nextOffset += nextElement.height + nextElement.gapY;
      }
    }
  }
}

void Virtualizer::recomputeTotalSize(Container *container) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  /*
   * Total container size is the maximum element extent. Using the element edge
   * (offset + size) rather than the accumulated track size excludes the trailing
   * gap after the last element, keeping single and multi-column layouts consistent.
   * Element offsets already include the header along the scroll axis.
   */
  double maxHeight = 0.0;
  double maxWidth = 0.0;

  for (const Element& nextElement : container->revision.elements) {
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
   * and multi-column layouts cannot collapse to a zero-width feedback loop.
   */
  if (container->horizontal) {
    container->revision.totalContainerWidth = std::max(maxWidth, container->headerSize) + container->footerSize;
    container->revision.totalContainerHeight = std::max(maxHeight, container->revision.windowContainerHeight);
  } else {
    container->revision.totalContainerHeight = std::max(maxHeight, container->headerSize) + container->footerSize;
    container->revision.totalContainerWidth = std::max(maxWidth, container->revision.windowContainerWidth);
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

void Virtualizer::updateElementAtIndex(Container *container, std::size_t index, Size size) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  if (index >= container->revision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  Element& nextElement = container->revision.elements[index];

  double prevWidth = nextElement.width;
  double prevHeight = nextElement.height;
  bool wasMeasured = nextElement.measured;

  nextElement.width = size.width;
  nextElement.height = size.height;
  nextElement.estimated = true;
  nextElement.measured = true;

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
   * Only this element and the ones after it can shift, so reflow from here forward.
   * The caller refreshes the total once per measurement batch.
   */
  recomputeElementOffsets(container, index);

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
}

void Virtualizer::reconcileElements(Container *container, const std::vector<std::string> &nextKeys) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  std::vector<Element>& prevElements = container->revision.elements;

  /*
   * Index the existing elements by key so surviving elements keep their
   * measured state (size, estimated/measured flags) across the update
   */
  std::unordered_map<std::string, Element> prevElementsByKey;
  prevElementsByKey.reserve(prevElements.size());
  for (Element& prevElement : prevElements) {
    if (!prevElement.key.empty()) {
      std::string prevKey = prevElement.key;
      prevElementsByKey.emplace(std::move(prevKey), std::move(prevElement));
    }
  }

  std::vector<Element> nextElements;
  nextElements.reserve(nextKeys.size());

  // Rebuild the key->index map alongside the element list so anchor lookups stay O(1)
  // (see Container::findElementIndexByKey). emplace keeps the first occurrence of a
  // duplicate key.
  std::unordered_map<std::string, std::size_t> nextElementIndexByKey;
  nextElementIndexByKey.reserve(nextKeys.size());

  for (std::size_t nextElementIndex = 0; nextElementIndex < nextKeys.size(); nextElementIndex++) {
    const std::string& nextKey = nextKeys[nextElementIndex];

    nextElementIndexByKey.emplace(nextKey, nextElementIndex);

    auto prevElementEntry = prevElementsByKey.find(nextKey);
    if (prevElementEntry != prevElementsByKey.end()) {
      Element nextElement = std::move(prevElementEntry->second);
      nextElement.index = nextElementIndex;
      nextElements.push_back(std::move(nextElement));
    } else {
      Element nextElement;
      nextElement.key = nextKey;
      nextElement.index = nextElementIndex;
      nextElements.push_back(std::move(nextElement));
    }
  }

  container->revision.elements = std::move(nextElements);
  container->revision.elementIndexByKey = std::move(nextElementIndexByKey);
}

void Virtualizer::captureAnchor(Container *container, double inputOffset) {
  container->anchor = Anchor{"", 0.0, AnchorMode::Element};

  const std::vector<Element>& prevElements = container->revision.elements;
  if (prevElements.empty()) {
    return;
  }

  auto elementOffsetOf = [&](const Element& element) {
    return container->horizontal ? element.offsetX : element.offsetY;
  };

  /*
   * The anchor is the first ANCHORABLE element whose trailing edge is past the current
   * scroll offset, i.e. the stable content row sitting at the top/left of the viewport.
   * Decoration rows (date pills, unread dividers, ...) are skipped so a key change on one
   * never perturbs the maintained position; the scan walks down to the next content row.
   * firstPast remembers the literal viewport-top row so a degenerate viewport of nothing
   * but decoration still anchors somewhere instead of failing.
   */
  const Element* firstPast = nullptr;
  for (const Element& prevElement : prevElements) {
    double elementOffset = elementOffsetOf(prevElement);
    double elementSize = container->horizontal ? prevElement.width : prevElement.height;

    if (elementOffset + elementSize <= inputOffset) {
      continue;
    }
    if (firstPast == nullptr) {
      firstPast = &prevElement;
    }
    if (container->isAnchorable(prevElement.key)) {
      container->anchor = Anchor{prevElement.key, inputOffset - elementOffset, AnchorMode::Element};
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
  Container *container,
  const std::string &anchorKey,
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
    if (offset < 0.0) offset = 0.0;
    if (offset > maxOffset) offset = maxOffset;
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
   * Create an operation id, but preserve the current one when this is the SAME correction
   * being reasserted (same type, and same anchored key for Element anchors). A fixed-offset
   * pin like BottomPin is requested again every frame while it converges; keeping its id stable
   * is what lets the host's echo match it across the multi-frame settle. A genuinely new
   * intent (different type, or a new anchored key) gets a fresh id.
   */
  auto operationId = [&](OperationType type, AnchorMode mode, const std::string &key) -> std::uint64_t {
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
  auto requestAnchor = [&](OperationType type, const std::string &key, double delta) {
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
       * Drive toward the target ELEMENT (anchored), not a one-shot offset: its offset
       * is estimate-built, so anchoring converges onto it as the region is measured.
       */
      const std::string targetKey = container->getElementAtIndex(container->scrollToIndexTarget).key;
      requestAnchor(OperationType::ScrollToKey, targetKey, 0.0);
      /*
       * The explicit target takes over from the inverted bottom anchor, but only when
       * it actually applied (an out-of-range target must not disable the pin).
       */
      container->invertedInitialized = true;
    }
    container->scrollToIndexTarget = UNDEFINED_INDEX;
  }

  /*
   * 2. Inverted lists stick to the bottom until the view reaches it, repinning as the
   *    total grows. Only pin once the window size is known, to avoid targeting total-0.
   */
  if (container->inverted && !container->invertedInitialized && elementsSize > 0 && windowSize > 0.0) {
    requestFixed(OperationType::BottomPin);
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
   *     where the total did not change. A user scroll cancels it.
   */
  if (container->pendingScrollToEnd && elementsSize > 0 && windowSize > 0.0) {
    bool atBottom = std::fabs(currentOffset - maxOffset) < OFFSET_ARRIVED_THRESHOLD;
    bool totalStable = totalSize == prevTotalForScrollToEnd;
    if (atBottom && totalStable) {
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
        /*
         * Offset moved, so the caller must remeasure the window for the new offset.
         */
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
       * must anchor to the TRUE bottom (maxOffset), not the row's captured position:
       * content measured async after first paint (native markdown reporting its
       * height) grows the row, and maintaining its old position leaves the bottom edge a
       * header/footer-sized gap off the fold (the initial-load shift) and oscillates
       * against the bottom pin. Targeting maxOffset keeps the newest row pinned through the
       * settle and auto-follows new content while the user is at the bottom. Every other
       * anchor (scrolled up, prepend) keeps the normal maintain-its-position behaviour.
       */
      bool invertedBottomAnchor =
        container->inverted && elementsSize > 0 && anchorIndex == elementsSize - 1;
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
