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
   * Reported scroll offset along the scroll axis
   */
  double inputOffset = container->horizontal ? input.containerOffsetX : input.containerOffsetY;

  /*
   * A genuine user scroll abandons any in-flight correction and disengages the
   * inverted bottom pin. Only yield when the offset actually moved: an unmoved offset
   * means the user is not driving, so an in-flight correction must survive re-commits.
   */
  bool userMovedOffset = std::fabs(inputOffset - container->lastReportedOffset) >= OFFSET_MOVED_EPSILON;
  if (input.userScrolled && userMovedOffset) {
    container->pendingScroll = false;
    container->pendingAnchorActive = false;
    container->pendingScrollToEnd = false;
    container->invertedInitialized = true;
  }
  container->lastReportedOffset = inputOffset;

  /*
   * Capture the anchor element so the same content stays in view across a reconcile
   */
  bool hadElementsBefore = !container->revision.elements.empty();
  std::string anchorKey;
  double anchorDelta = 0.0;
  captureAnchor(container, inputOffset, anchorKey, anchorDelta);

  /*
   * Remember the anchor so updateElementAtIndex can keep this element fixed on screen
   */
  container->anchorKey = anchorKey;
  container->anchorDelta = anchorDelta;
  container->anchorHeaderSize = prevHeaderSize;

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
   * Reconcile the element list to the incoming keys (handles insert/remove/reorder)
   */
  reconcileElements(container, input.keys);

  /*
   * Measure the revision
   */
  container->startRevision();
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
   * Resolve scroll corrections. If the offset moved, re-measure to select the
   * visible window matching the new offset.
   */
  if (resolveScroll(container, anchorKey, anchorDelta, hadElementsBefore)) {
    measure(container, true);
    SL_LOG("  re-measured: offset=(%.1f,%.1f) visible=[%zd..%zd] visKeys=[%s..%s] invInit=%d",
      container->revision.containerOffsetX, container->revision.containerOffsetY,
      static_cast<std::ptrdiff_t>(container->getVisibleIndices().first),
      static_cast<std::ptrdiff_t>(container->getVisibleIndices().second),
      visKeyAt(container, container->getVisibleIndices().first),
      visKeyAt(container, container->getVisibleIndices().second),
      container->invertedInitialized ? 1 : 0);
  }

  SL_LOG("  resolved: offset=(%.1f,%.1f) corrected=%d invInit=%d",
    container->revision.containerOffsetX, container->revision.containerOffsetY,
    container->containerOffsetCorrected ? 1 : 0, container->invertedInitialized ? 1 : 0);

  container->endRevision();
}

void Virtualizer::measure(Container *container, bool windowFromOffset) {
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
   * scroll correction the re-measure selects the window around the corrected offset.
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
     * Running measurement is expensive,
     * hence we don't want to re-measure if we already have a cached value
     */
    if (!nextElement.estimated) {
      auto [width, height] = container->estimatedElementSize;

      /*
       * If there is no estimate available, skip updating this element
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
     * Stop measuring once we measured enough elements to fill the visible window
     * For multi-column layouts the load is shared across the columns
     */
    if (accumulated / effectiveColumns >= windowSize) {
      break;
    }
  }

  finalizeMeasurement(container, measuredMinIndex, measuredMaxIndex);
}

void Virtualizer::measureNextRevision(Container *container) {
  std::size_t elementsSize = container->revision.elements.size();
  double containerOffset = container->getContainerOffset();
  double windowSize = container->getWindowContainerSize();

  std::size_t measuredMinIndex = UNDEFINED_INDEX;
  std::size_t measuredMaxIndex = UNDEFINED_INDEX;

  for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
    Element& nextElement = container->revision.elements[nextElementIndex];

    /*
     * Skip elements that are outside of the visible window plus 1x window buffer
     */
    double elementOffset = container->horizontal ? nextElement.offsetX : nextElement.offsetY;
    if (elementOffset < containerOffset - windowSize ||
      elementOffset > containerOffset + windowSize * 2) {
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
       * Force the cross-axis size to the track size here too, so a re-flow with a
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
   * a first measurement adds the full size, a re-measurement adjusts by the delta.
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
   * Only this element and the ones after it can shift, so re-flow from here forward.
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

  if (container->pendingScroll && container->pendingAnchorActive) {
    compensationKey = container->pendingAnchorKey;
    compensationDelta = container->pendingAnchorDelta;
    compensate = true;
  } else if (!container->pendingScroll && !container->anchorKey.empty()) {
    compensationKey = container->anchorKey;
    compensationDelta = container->anchorDelta;
    compensate = true;
  }

  if (compensate) {
    std::size_t anchorIndex = container->findElementIndexByKey(compensationKey);
    if (anchorIndex != UNDEFINED_INDEX) {
      /*
       * Raw anchor target before the lower clamp. Subtract any header-size change so
       * MVCP does not chase header growth as if it were a scroll. The shift test
       * compares this raw target (not the clamped one) so a top over-scroll, where the
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
      if (std::fabs(rawAnchoredOffset - currentOffset) >= OFFSET_MOVED_EPSILON) {
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
  std::vector<Element>& prevElements = container->revision.elements;

  /*
   * Index the existing elements by key so surviving elements keep their
   * measured state (size, estimated/measured flags) across the update
   */
  std::unordered_map<std::string, Element> prevElementsByKey;
  prevElementsByKey.reserve(prevElements.size());
  for (Element& prevElement : prevElements) {
    if (!prevElement.key.empty()) {
      prevElementsByKey.emplace(prevElement.key, prevElement);
    }
  }

  std::vector<Element> nextElements;
  nextElements.reserve(nextKeys.size());

  for (std::size_t nextElementIndex = 0; nextElementIndex < nextKeys.size(); nextElementIndex++) {
    const std::string& nextKey = nextKeys[nextElementIndex];

    auto prevElementEntry = prevElementsByKey.find(nextKey);
    if (prevElementEntry != prevElementsByKey.end()) {
      Element nextElement = prevElementEntry->second;
      nextElement.index = nextElementIndex;
      nextElements.push_back(nextElement);
    } else {
      Element nextElement;
      nextElement.key = nextKey;
      nextElement.index = nextElementIndex;
      nextElements.push_back(nextElement);
    }
  }

  container->revision.elements = std::move(nextElements);
}

void Virtualizer::captureAnchor(Container *container, double inputOffset, std::string &anchorKey, double &anchorDelta) {
  anchorKey.clear();
  anchorDelta = 0.0;

  const std::vector<Element>& prevElements = container->revision.elements;
  if (prevElements.empty()) {
    return;
  }

  /*
   * The anchor is the first element whose trailing edge is past the current
   * scroll offset, i.e. the element sitting at the top/left of the viewport
   */
  for (const Element& prevElement : prevElements) {
    double elementOffset = container->horizontal ? prevElement.offsetX : prevElement.offsetY;
    double elementSize = container->horizontal ? prevElement.width : prevElement.height;

    if (elementOffset + elementSize > inputOffset) {
      anchorKey = prevElement.key;
      anchorDelta = inputOffset - elementOffset;
      return;
    }
  }

  /*
   * Scrolled past every element, anchor to the last one
   */
  const Element& lastElement = prevElements.back();
  anchorKey = lastElement.key;
  double lastElementOffset = container->horizontal ? lastElement.offsetX : lastElement.offsetY;
  anchorDelta = inputOffset - lastElementOffset;
}

bool Virtualizer::resolveScroll(Container *container, const std::string &anchorKey, double anchorDelta, bool hadElementsBefore) {
  std::size_t elementsSize = container->revision.elements.size();
  container->containerOffsetCorrected = false;

  /*
   * Re-arm the inverted bottom anchoring (and drop any pending target) when the
   * list is emptied so it sticks to the bottom again once content arrives
   */
  if (elementsSize == 0) {
    container->invertedInitialized = false;
    container->pendingScroll = false;
    container->pendingAnchorActive = false;
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
   * Drive toward a fixed offset (bottom anchor, scrollToIndex)
   */
  auto requestFixed = [&](double offset) {
    container->pendingScrollOffset = clampOffset(offset);
    container->pendingScroll = true;
    container->pendingAnchorActive = false;
  };

  /*
   * Drive so the anchor element stays at the same viewport position. The target is
   * recomputed from the anchor each frame, so it tracks the anchor as nearby
   * elements are measured/resized while the correction is in flight (prepend).
   */
  auto requestAnchor = [&](const std::string &key, double delta) {
    container->pendingAnchorKey = key;
    container->pendingAnchorDelta = delta;
    container->pendingScroll = true;
    container->pendingAnchorActive = true;
  };

  /*
   * 0. Content shrank below the current scroll position (e.g. a tree collapse-all),
   *    leaving the view scrolled past the new end. Drive back to the bottom. Keying on
   *    the shrink (not offset>maxOffset alone) avoids fighting an over-scroll bounce.
   *    Driven through pendingScroll so it survives re-commits; step 3 clears it on arrival.
   */
  if (!container->inverted && elementsSize > 0 &&
      currentOffset > maxOffset + OFFSET_MOVED_EPSILON &&
      totalSize < prevTotalForScrollToEnd) {
    requestFixed(maxOffset);
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
      requestAnchor(targetKey, 0.0);
      /*
       * The explicit target takes over from the inverted bottom anchor, but only when
       * it actually applied (an out-of-range target must not disable the pin).
       */
      container->invertedInitialized = true;
    }
    container->scrollToIndexTarget = UNDEFINED_INDEX;
  }

  /*
   * 2. Inverted lists stick to the bottom until the view reaches it, re-pinning as the
   *    total grows. Only pin once the window size is known, to avoid targeting total-0.
   */
  if (container->inverted && !container->invertedInitialized && elementsSize > 0 && windowSize > 0.0) {
    requestFixed(totalSize - windowSize);
    /*
     * Settle only once there is a scrollable bottom we have reached; while content
     * still fits the window keep re-pinning so a growing list is not stuck at the top.
     */
    if (totalSize > windowSize && std::fabs(currentOffset - container->pendingScrollOffset) < OFFSET_ARRIVED_EPSILON) {
      container->invertedInitialized = true;
    }
  }

  /*
   * 2b. scrollToEnd re-targets maxOffset every frame as the total grows, so it lands
   *     on the true end. It settles only once the view reaches the bottom on a frame
   *     where the total did not change. A user scroll cancels it.
   */
  if (container->pendingScrollToEnd && elementsSize > 0 && windowSize > 0.0) {
    bool atBottom = std::fabs(currentOffset - maxOffset) < OFFSET_ARRIVED_EPSILON;
    bool totalStable = totalSize == prevTotalForScrollToEnd;
    if (atBottom && totalStable) {
      container->pendingScrollToEnd = false;
    } else {
      requestFixed(maxOffset);
    }
  }

  /*
   * 3. Drive an in-flight correction until the view confirms it; a stale offset on a
   *    later frame keeps requesting the same target instead of cancelling it.
   */
  if (container->pendingScroll) {
    double target = container->pendingScrollOffset;

    if (container->pendingAnchorActive) {
      std::size_t anchorIndex = container->findElementIndexByKey(container->pendingAnchorKey);
      if (anchorIndex == UNDEFINED_INDEX) {
        container->pendingScroll = false;
        container->pendingAnchorActive = false;
      } else {
        target = clampOffset(container->getElementOffset(anchorIndex) + container->pendingAnchorDelta);
      }
    } else {
      target = clampOffset(container->pendingScrollOffset);
    }

    if (container->pendingScroll) {
      if (std::fabs(currentOffset - target) < OFFSET_ARRIVED_EPSILON) {
        container->pendingScroll = false;
        container->pendingAnchorActive = false;
      } else {
        /*
         * Offset moved, so the caller must re-measure the window for the new offset.
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
       * Raw anchor target before clamping, excluding a header-size change. The shift
       * test compares this raw target so an over-scroll, where the anchor has not
       * moved, leaves MVCP out of the way; a real shift still fires the correction.
       */
      double rawAnchoredOffset = container->getElementOffset(anchorIndex) + anchorDelta
        - (container->headerSize - container->anchorHeaderSize);
      if (std::fabs(rawAnchoredOffset - currentOffset) >= OFFSET_MOVED_EPSILON) {
        requestAnchor(anchorKey, anchorDelta);
        container->containerOffsetCorrected = true;
        writeOffset(clampOffset(rawAnchoredOffset));
        return true;
      }
    }
  }

  return false;
}

}
