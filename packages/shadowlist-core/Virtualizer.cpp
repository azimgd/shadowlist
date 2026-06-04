#include <shadowlist-core/Virtualizer.hpp>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace azimgd::shadowlist {

void Virtualizer::update(Container *container, const FrameInput &input) {
  std::lock_guard<std::recursive_mutex> lock(container->coreMutex);

  SL_LOG("update: keys=%zu prevElements=%zu off=(%.1f,%.1f) win=(%.1f,%.1f) inv=%d cols=%zu hdr=%.1f ftr=%.1f invInit=%d",
    input.keys.size(), container->revision.elements.size(),
    input.containerOffsetX, input.containerOffsetY,
    input.windowContainerWidth, input.windowContainerHeight,
    input.inverted ? 1 : 0, input.columns, input.headerSize, input.footerSize,
    container->invertedInitialized ? 1 : 0);

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
  container->startReachedThreshold = input.startReachedThreshold;
  container->endReachedThreshold = input.endReachedThreshold;
  container->viewablePercentThreshold = input.viewablePercentThreshold;
  container->estimatedElementSize = input.estimatedElementSize;

  /*
   * A genuine user scroll takes over: abandon any in-flight scroll correction
   * (the MVCP drive after a prepend, a scrollToIndex animation) so we don't snap
   * the offset back to that target every frame and lock the user out. Stale
   * data/layout re-commits arrive with this unset, so an in-flight correction
   * still survives them (step 3 of resolveScroll).
   *
   * Marking the inverted bottom pin as initialized disengages it too: once the
   * user has manually scrolled they have taken control of the position, so step 2
   * must stop re-pinning to the bottom every frame (which otherwise overrides the
   * cancel above and locks an inverted list whose pin has not settled yet). The
   * pin re-arms on its own when the list empties (see resolveScroll).
   */
  if (input.userScrolled) {
    container->pendingScroll = false;
    container->pendingAnchorActive = false;
    container->invertedInitialized = true;
  }

  /*
   * Capture the anchor element from the previous layout and the incoming scroll
   * offset so we can keep the same content in view across a reconcile
   */
  bool hadElementsBefore = !container->revision.elements.empty();
  double inputOffset = container->horizontal ? input.containerOffsetX : input.containerOffsetY;
  std::string anchorKey;
  double anchorDelta = 0.0;
  captureAnchor(container, inputOffset, anchorKey, anchorDelta);

  /*
   * Remember the anchor so the measurement feedback (updateElementAtIndex, which
   * runs later during layout) can keep this element fixed on screen
   */
  container->anchorKey = anchorKey;
  container->anchorDelta = anchorDelta;

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

  SL_LOG("  measured: total=(%.1f,%.1f) offset=(%.1f,%.1f) visible=[%zd..%zd] anchorKey=%s",
    container->revision.totalContainerWidth, container->revision.totalContainerHeight,
    container->revision.containerOffsetX, container->revision.containerOffsetY,
    static_cast<std::ptrdiff_t>(container->getVisibleIndices().first),
    static_cast<std::ptrdiff_t>(container->getVisibleIndices().second),
    anchorKey.empty() ? "(none)" : anchorKey.c_str());

  /*
   * Resolve scroll corrections. When the offset is moved (scrollToIndex, inverted
   * bottom anchor, a large MVCP shift) the visible window was selected for the old
   * offset, so re-measure to select the window matching the new offset.
   */
  if (resolveScroll(container, anchorKey, anchorDelta, hadElementsBefore)) {
    measure(container, true);
    SL_LOG("  re-measured: offset=(%.1f,%.1f) visible=[%zd..%zd] invInit=%d",
      container->revision.containerOffsetX, container->revision.containerOffsetY,
      static_cast<std::ptrdiff_t>(container->getVisibleIndices().first),
      static_cast<std::ptrdiff_t>(container->getVisibleIndices().second),
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
   * The first revision fills a window from the edge because element offsets are
   * not known yet. Once a scroll correction has been applied the offsets exist,
   * so the re-measure selects the window around the corrected offset instead
   * (otherwise the visible range would report the edge window, not the target).
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
   * Calculate average element dimensions, used to size the unmeasured portion of
   * the list. Computed once and then frozen: it must stay stable across frames so
   * the same element keeps the same offset between captureAnchor (which reads the
   * previous layout) and measure (which recomputes it). Recomputing per frame on a
   * variable-height list swings the average, moves every estimated element's
   * offset, and makes MVCP chase the moved anchor (violent scroll thrash).
   */
  if (container->revision.measurementElementCount > 0) {
    if (container->revision.averageElementWidth == 0.0) {
      container->revision.averageElementWidth = container->revision.measurementElementTotalWidth / container->revision.measurementElementCount;
    }
    if (container->revision.averageElementHeight == 0.0) {
      container->revision.averageElementHeight = container->revision.measurementElementTotalHeight / container->revision.measurementElementCount;
    }
  }
}

void Virtualizer::layoutElements(Container *container) {
  std::size_t elementsSize = container->revision.elements.size();

  double trackSize = container->horizontal
    ? container->revision.windowContainerHeight / (container->columns > 0 ? container->columns : 1)
    : container->revision.windowContainerWidth / (container->columns > 0 ? container->columns : 1);

  /*
   * Size unmeasured elements with average dimensions. Multi-column layouts force
   * the cross-axis size to the track size so every column has the same extent.
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
    Element& nextElement = container->revision.elements[nextElementIndex];

    if (container->columns > 1) {
      if (container->horizontal) {
        if (!nextElement.estimated) {
          nextElement.width = container->revision.averageElementWidth;
        }
        nextElement.height = trackSize;
      } else {
        if (!nextElement.estimated) {
          nextElement.height = container->revision.averageElementHeight;
        }
        nextElement.width = trackSize;
      }
    } else if (!nextElement.estimated) {
      nextElement.width = container->revision.averageElementWidth;
      nextElement.height = container->revision.averageElementHeight;
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

      if (container->horizontal) {
        nextElement.offsetY = trackIndex * trackSize;
        nextElement.offsetX = trackSizes[trackIndex];
        trackSizes[trackIndex] += nextElement.width + nextElement.gapX;
      } else {
        nextElement.offsetX = trackIndex * trackSize;
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
   * Along the scroll axis include the header (as a floor, for empty lists) and
   * the trailing footer. The cross axis is left as the maximum element extent.
   */
  if (container->horizontal) {
    container->revision.totalContainerWidth = std::max(maxWidth, container->headerSize) + container->footerSize;
    container->revision.totalContainerHeight = maxHeight;
  } else {
    container->revision.totalContainerHeight = std::max(maxHeight, container->headerSize) + container->footerSize;
    container->revision.totalContainerWidth = maxWidth;
  }
}

void Virtualizer::addElementAtIndex(Container *container, std::size_t index) {
  if (index > container->revision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  Element nextElement;

  auto [width, height] = container->estimatedElementSize;

  /*
   * If there is no estimate available, fall back to the average dimensions
   */
  if (width == 0.0 && height == 0.0) {
    nextElement.width = container->revision.averageElementWidth;
    nextElement.height = container->revision.averageElementHeight;
    nextElement.estimated = false;
  } else {
    nextElement.width = width;
    nextElement.height = height;
    nextElement.estimated = true;
  }
  nextElement.index = index;

  container->revision.elements.insert(container->revision.elements.begin() + index, nextElement);

  for (std::size_t nextElementIndex = index + 1; nextElementIndex < container->revision.elements.size(); nextElementIndex++) {
    container->revision.elements[nextElementIndex].index = nextElementIndex;
  }

  if (container->revision.measurementElementStartIndex != UNDEFINED_INDEX) {
    if (index <= container->revision.measurementElementStartIndex) {
      container->revision.measurementElementStartIndex++;
    }

    if (index <= container->revision.measurementElementEndIndex) {
      container->revision.measurementElementEndIndex++;
    }
  }

  if (container->revision.averageElementHeight > 0) {
    container->revision.measurementElementTotalHeight += nextElement.height;
    container->revision.measurementElementTotalWidth += nextElement.width;
    container->revision.measurementElementCount++;
  }

  recomputeElementOffsets(container, index);
}

void Virtualizer::updateElementAtIndex(Container *container, std::size_t index, Size size) {
  if (index >= container->revision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  Element& nextElement = container->revision.elements[index];

  double prevWidth = nextElement.width;
  double prevHeight = nextElement.height;

  nextElement.width = size.width;
  nextElement.height = size.height;
  nextElement.estimated = true;
  nextElement.measured = true;

  if (container->revision.averageElementHeight > 0) {
    container->revision.measurementElementTotalHeight += size.height - prevHeight;
    container->revision.measurementElementTotalWidth += size.width - prevWidth;
  }

  /*
   * Only the changed element and the elements after it can shift, so re-flow from
   * this index forward instead of re-flowing the whole list. The total size is
   * left to the caller to refresh once after a batch of measurements (via
   * recomputeTotalSize) rather than rescanning the whole list per measured child.
   */
  recomputeElementOffsets(container, index);

  /*
   * Maintain the visible content position while off-screen elements are measured:
   * keep the anchor element fixed on screen. Anchoring to a stable content
   * reference (instead of summing per-element deltas relative to the scroll offset)
   * avoids the runaway where the compensation shifts the offset, reveals new
   * elements, and compensates again.
   *
   * During an anchor-driven correction (prepend) we keep that same sticky anchor
   * fixed; while a fixed-offset correction is being driven (bottom anchor /
   * scrollToIndex) we leave it to that correction.
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
       * Only clamp the lower bound here. The total size is recomputed once after
       * this batch of measurements (see ShadowNode layout), so an upper clamp to
       * totalSize - windowSize would use a stale, too-small total mid-measurement
       * and yank the anchor (visibly breaks MVCP on inverted lists). The
       * over-scroll upper bound is enforced by resolveScroll on the next frame,
       * which runs after the total has been refreshed.
       */
      double anchoredOffset = container->getElementOffset(anchorIndex) + compensationDelta;
      if (anchoredOffset < 0.0) {
        anchoredOffset = 0.0;
      }

      double currentOffset = container->horizontal ? container->revision.containerOffsetX : container->revision.containerOffsetY;
      if (std::fabs(anchoredOffset - currentOffset) >= 0.5) {
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

void Virtualizer::prependElements(Container *container, std::size_t count) {
  for (std::size_t iteration = 0; iteration < count; iteration++) {
    addElementAtIndex(container, 0);
  }

  recomputeTotalSize(container);
}

void Virtualizer::appendElements(Container *container, std::size_t count) {
  for (std::size_t iteration = 0; iteration < count; iteration++) {
    addElementAtIndex(container, container->revision.elements.size());
  }

  recomputeTotalSize(container);
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
  }

  double windowSize = container->getWindowContainerSize();
  double totalSize = container->horizontal ? container->revision.totalContainerWidth : container->revision.totalContainerHeight;
  double currentOffset = container->horizontal ? container->revision.containerOffsetX : container->revision.containerOffsetY;
  double maxOffset = totalSize - windowSize;
  if (maxOffset < 0.0) {
    maxOffset = 0.0;
  }

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
   * 1. scrollToIndex aligns the element to the viewport start
   */
  if (container->scrollToIndexTarget != UNDEFINED_INDEX) {
    if (container->scrollToIndexTarget < elementsSize) {
      /*
       * Drive toward the target ELEMENT (anchored to the viewport start), not a
       * one-shot offset. The target index is usually far off-screen, so its offset
       * is built from estimated sizes; jumping to that fixed offset lands on the
       * wrong element once the region is really measured. Anchoring re-targets the
       * element's current offset every frame (step 3, pendingAnchorActive) as the
       * estimates are replaced by measurements, so it converges onto the element.
       */
      const std::string targetKey = container->getElementAtIndex(container->scrollToIndexTarget).key;
      requestAnchor(targetKey, 0.0);
      /*
       * An explicit scroll target takes over from the inverted bottom anchor, but
       * only when it actually applied: an out-of-range target must not silently
       * disable the bottom pin without scrolling anywhere.
       */
      container->invertedInitialized = true;
    }
    container->scrollToIndexTarget = UNDEFINED_INDEX;
  }

  /*
   * 2. Inverted lists stick to the bottom until the view actually reaches it.
   *    Covers the initial render and the empty -> populated transition, re-pinning
   *    while the total size grows as elements are measured. We only pin once the
   *    window size is known so we don't target total-0 on the first frame.
   */
  if (container->inverted && !container->invertedInitialized && elementsSize > 0 && windowSize > 0.0) {
    requestFixed(totalSize - windowSize);
    /*
     * Only consider the bottom anchor settled once there is an actual scrollable
     * bottom we have reached. While the content still fits the window (estimated
     * total <= window on the first frames) keep re-pinning, so a list whose
     * measured total grows past the window is not left stuck at the top.
     */
    if (totalSize > windowSize && std::fabs(currentOffset - container->pendingScrollOffset) < 1.0) {
      container->invertedInitialized = true;
    }
  }

  /*
   * 3. Drive an in-flight correction until the view confirms it. This survives the
   *    racing re-commits the visible-indices event triggers: a stale offset on a
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
      if (std::fabs(currentOffset - target) < 1.0) {
        container->pendingScroll = false;
        container->pendingAnchorActive = false;
      } else {
        /*
         * Reached only when |current - target| >= 1.0, i.e. the offset moved, so
         * the caller always needs to re-measure the window for the new offset.
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
      double anchoredOffset = clampOffset(container->getElementOffset(anchorIndex) + anchorDelta);
      if (std::fabs(anchoredOffset - currentOffset) >= 0.5) {
        requestAnchor(anchorKey, anchorDelta);
        container->containerOffsetCorrected = true;
        writeOffset(anchoredOffset);
        return true;
      }
    }
  }

  return false;
}

}
