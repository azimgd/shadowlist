#include <shadowlist-core/Virtualizer.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace azimgd::shadowlist {

void Virtualizer::update(Container *container, const FrameInput &input) {
  /*
   * Configure layout properties for this frame
   */
  container->inverted = input.inverted;
  container->horizontal = input.horizontal;
  container->columns = input.columns;
  container->headerSize = input.headerSize;
  container->footerSize = input.footerSize;
  container->estimatedElementSize = input.estimatedElementSize;

  /*
   * Capture the anchor element from the previous layout and the incoming scroll
   * offset so we can keep the same content in view across a reconcile
   */
  bool hadElementsBefore = !container->nextRevision.elements.empty();
  double inputOffset = container->horizontal ? input.containerOffsetX : input.containerOffsetY;
  std::string anchorKey;
  double anchorDelta = 0.0;
  captureAnchor(container, inputOffset, anchorKey, anchorDelta);

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

  /*
   * Resolve scroll corrections, then end the revision (which dispatches observers)
   */
  resolveScroll(container, anchorKey, anchorDelta, hadElementsBefore);
  container->endRevision();
}

void Virtualizer::measure(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measure outside of a revision");
  }

  /*
   * Reset the per-revision measurement accumulators so they reflect only the
   * elements measured in this revision (otherwise they grow unbounded)
   */
  container->nextRevision.measurementElementStartIndex = UNDEFINED_INDEX;
  container->nextRevision.measurementElementEndIndex = UNDEFINED_INDEX;
  container->nextRevision.measurementElementCount = 0;
  container->nextRevision.measurementElementTotalHeight = 0;
  container->nextRevision.measurementElementTotalWidth = 0;

  if (container->nextRevisionCount == RevisionCountFirst) {
    measureFirstRevision(container);
  } else {
    measureNextRevision(container);
  }

  layoutElements(container);
  finalizeContainer(container);
}

void Virtualizer::measureFirstRevision(Container *container) {
  std::size_t elementsSize = container->nextRevision.elements.size();
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
    Element& nextElement = container->nextRevision.elements[nextElementIndex];

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

    container->nextRevision.measurementElementTotalHeight += nextElement.height;
    container->nextRevision.measurementElementTotalWidth += nextElement.width;
    container->nextRevision.measurementElementCount++;

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
  std::size_t elementsSize = container->nextRevision.elements.size();
  double containerOffset = container->getContainerOffset();
  double windowSize = container->getWindowContainerSize();

  std::size_t measuredMinIndex = UNDEFINED_INDEX;
  std::size_t measuredMaxIndex = UNDEFINED_INDEX;

  for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
    Element& nextElement = container->nextRevision.elements[nextElementIndex];

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

    container->nextRevision.measurementElementTotalHeight += nextElement.height;
    container->nextRevision.measurementElementTotalWidth += nextElement.width;
    container->nextRevision.measurementElementCount++;
  }

  finalizeMeasurement(container, measuredMinIndex, measuredMaxIndex);
}

void Virtualizer::finalizeMeasurement(Container *container, std::size_t measuredMinIndex, std::size_t measuredMaxIndex) {
  /*
   * For inverted lists we iterate from end to start so the start index is the
   * higher one (e.g. start=99, end=90). For default lists start is the lower one.
   */
  if (container->inverted) {
    container->nextRevision.measurementElementStartIndex = measuredMaxIndex;
    container->nextRevision.measurementElementEndIndex = measuredMinIndex;
  } else {
    container->nextRevision.measurementElementStartIndex = measuredMinIndex;
    container->nextRevision.measurementElementEndIndex = measuredMaxIndex;
  }

  /*
   * Calculate average element dimensions, needed for estimating the unmeasured
   * portion of the list. Computed once from the first measured sample.
   */
  if (container->nextRevision.measurementElementCount > 0) {
    if (container->nextRevision.averageElementWidth == 0.0) {
      container->nextRevision.averageElementWidth = container->nextRevision.measurementElementTotalWidth / container->nextRevision.measurementElementCount;
    }
    if (container->nextRevision.averageElementHeight == 0.0) {
      container->nextRevision.averageElementHeight = container->nextRevision.measurementElementTotalHeight / container->nextRevision.measurementElementCount;
    }
  }
}

void Virtualizer::layoutElements(Container *container) {
  std::size_t elementsSize = container->nextRevision.elements.size();

  double trackSize = container->horizontal
    ? container->nextRevision.windowContainerHeight / (container->columns > 0 ? container->columns : 1)
    : container->nextRevision.windowContainerWidth / (container->columns > 0 ? container->columns : 1);

  /*
   * Size unmeasured elements with average dimensions. Multi-column layouts force
   * the cross-axis size to the track size so every column has the same extent.
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
    Element& nextElement = container->nextRevision.elements[nextElementIndex];

    if (container->columns > 1) {
      if (container->horizontal) {
        if (!nextElement.estimated) {
          nextElement.width = container->nextRevision.averageElementWidth;
        }
        nextElement.height = trackSize;
      } else {
        if (!nextElement.estimated) {
          nextElement.height = container->nextRevision.averageElementHeight;
        }
        nextElement.width = trackSize;
      }
    } else if (!nextElement.estimated) {
      nextElement.width = container->nextRevision.averageElementWidth;
      nextElement.height = container->nextRevision.averageElementHeight;
    }
  }

  recomputeElementOffsets(container, 0);
}

void Virtualizer::recomputeElementOffsets(Container *container, std::size_t fromIndex) {
  std::size_t elementsSize = container->nextRevision.elements.size();
  if (fromIndex >= elementsSize) {
    return;
  }

  if (container->columns > 1) {
    double trackSize = container->horizontal
      ? container->nextRevision.windowContainerHeight / container->columns
      : container->nextRevision.windowContainerWidth / container->columns;

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
      const Element& seedElement = container->nextRevision.elements[seedIndex];
      std::size_t trackIndex = seedIndex % container->columns;
      trackSizes[trackIndex] = container->horizontal
        ? seedElement.offsetX + seedElement.width + seedElement.gapX
        : seedElement.offsetY + seedElement.height + seedElement.gapY;
    }

    for (std::size_t nextElementIndex = fromIndex; nextElementIndex < elementsSize; ++nextElementIndex) {
      Element& nextElement = container->nextRevision.elements[nextElementIndex];
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
      const Element& prevElement = container->nextRevision.elements[fromIndex - 1];
      nextOffset = container->horizontal
        ? prevElement.offsetX + prevElement.width + prevElement.gapX
        : prevElement.offsetY + prevElement.height + prevElement.gapY;
    }

    for (std::size_t nextElementIndex = fromIndex; nextElementIndex < elementsSize; ++nextElementIndex) {
      Element& nextElement = container->nextRevision.elements[nextElementIndex];
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

  for (const Element& nextElement : container->nextRevision.elements) {
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
    container->nextRevision.totalContainerWidth = std::max(maxWidth, container->headerSize) + container->footerSize;
    container->nextRevision.totalContainerHeight = maxHeight;
  } else {
    container->nextRevision.totalContainerHeight = std::max(maxHeight, container->headerSize) + container->footerSize;
    container->nextRevision.totalContainerWidth = maxWidth;
  }
}

void Virtualizer::finalizeContainer(Container *container) {
  recomputeTotalSize(container);

  /*
   * On the first revision an inverted list starts scrolled to the bottom/right.
   * Default lists keep the offset supplied by the caller (top/left).
   */
  if (container->nextRevisionCount == RevisionCountFirst && container->inverted) {
    container->nextRevision.containerOffsetY = container->nextRevision.totalContainerHeight - container->nextRevision.windowContainerHeight;
    container->nextRevision.containerOffsetX = container->nextRevision.totalContainerWidth - container->nextRevision.windowContainerWidth;
  }
}

void Virtualizer::addElementAtIndex(Container *container, std::size_t index) {
  if (index > container->nextRevision.elements.size()) {
    throw std::out_of_range("Index out of bounds");
  }

  Element nextElement;

  auto [width, height] = container->estimatedElementSize;

  /*
   * If there is no estimate available, fall back to the average dimensions
   */
  if (width == 0.0 && height == 0.0) {
    nextElement.width = container->nextRevision.averageElementWidth;
    nextElement.height = container->nextRevision.averageElementHeight;
    nextElement.estimated = false;
  } else {
    nextElement.width = width;
    nextElement.height = height;
    nextElement.estimated = true;
  }
  nextElement.index = index;

  container->nextRevision.elements.insert(container->nextRevision.elements.begin() + index, nextElement);

  for (std::size_t nextElementIndex = index + 1; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
    container->nextRevision.elements[nextElementIndex].index = nextElementIndex;
  }

  if (container->nextRevision.measurementElementStartIndex != UNDEFINED_INDEX) {
    if (index <= container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex++;
    }

    if (index <= container->nextRevision.measurementElementEndIndex) {
      container->nextRevision.measurementElementEndIndex++;
    }
  }

  if (container->nextRevision.averageElementHeight > 0) {
    container->nextRevision.measurementElementTotalHeight += nextElement.height;
    container->nextRevision.measurementElementTotalWidth += nextElement.width;
    container->nextRevision.measurementElementCount++;
  }

  recomputeElementOffsets(container, index);
}

void Virtualizer::updateElementAtIndex(Container *container, std::size_t index, Size size) {
  if (index >= container->nextRevision.elements.size()) {
    throw std::out_of_range("Index out of bounds");
  }

  Element& nextElement = container->nextRevision.elements[index];

  double prevWidth = nextElement.width;
  double prevHeight = nextElement.height;

  nextElement.width = size.width;
  nextElement.height = size.height;
  nextElement.estimated = true;
  nextElement.measured = true;

  if (container->nextRevision.averageElementHeight > 0) {
    container->nextRevision.measurementElementTotalHeight += size.height - prevHeight;
    container->nextRevision.measurementElementTotalWidth += size.width - prevWidth;
  }

  /*
   * Only the changed element and the elements after it can shift, so re-flow
   * from this index forward instead of re-flowing the whole list
   */
  recomputeElementOffsets(container, index);
}

void Virtualizer::prependElements(Container *container, std::size_t count) {
  for (std::size_t iteration = 0; iteration < count; iteration++) {
    addElementAtIndex(container, 0);
  }

  recomputeTotalSize(container);
}

void Virtualizer::appendElements(Container *container, std::size_t count) {
  for (std::size_t iteration = 0; iteration < count; iteration++) {
    addElementAtIndex(container, container->nextRevision.elements.size());
  }

  recomputeTotalSize(container);
}

void Virtualizer::reconcileElements(Container *container, const std::vector<std::string> &nextKeys) {
  std::vector<Element>& prevElements = container->nextRevision.elements;

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

  container->nextRevision.elements = std::move(nextElements);
}

void Virtualizer::captureAnchor(Container *container, double inputOffset, std::string &anchorKey, double &anchorDelta) {
  anchorKey.clear();
  anchorDelta = 0.0;

  const std::vector<Element>& prevElements = container->nextRevision.elements;
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

void Virtualizer::resolveScroll(Container *container, const std::string &anchorKey, double anchorDelta, bool hadElementsBefore) {
  double resolvedOffset = 0.0;
  bool resolved = false;

  /*
   * A pending scrollToIndex takes precedence: align the element to the viewport start
   */
  if (container->scrollToIndexTarget != UNDEFINED_INDEX) {
    if (container->scrollToIndexTarget < container->nextRevision.elements.size()) {
      resolvedOffset = container->getElementOffset(container->scrollToIndexTarget);
      resolved = true;
    }
    container->scrollToIndexTarget = UNDEFINED_INDEX;
  }

  /*
   * Otherwise keep the captured anchor element at the same viewport position so
   * inserting/removing/resizing elements above the viewport does not shift content
   */
  if (!resolved && hadElementsBefore && !anchorKey.empty()) {
    std::size_t anchorIndex = container->findElementIndexByKey(anchorKey);
    if (anchorIndex != UNDEFINED_INDEX) {
      resolvedOffset = container->getElementOffset(anchorIndex) + anchorDelta;
      resolved = true;
    }
  }

  /*
   * The inverted first-revision position is already applied by finalizeContainer
   */
  if (!resolved) {
    return;
  }

  /*
   * Keep the offset within the scrollable range
   */
  if (resolvedOffset < 0.0) {
    resolvedOffset = 0.0;
  }

  /*
   * Skip negligible corrections so a steady scroll position does not produce a
   * stream of sub-pixel offset updates back to the integration
   */
  double currentOffset = container->horizontal ? container->nextRevision.containerOffsetX : container->nextRevision.containerOffsetY;
  if (std::fabs(resolvedOffset - currentOffset) < 0.5) {
    return;
  }

  if (container->horizontal) {
    container->nextRevision.containerOffsetX = resolvedOffset;
  } else {
    container->nextRevision.containerOffsetY = resolvedOffset;
  }
}

}
