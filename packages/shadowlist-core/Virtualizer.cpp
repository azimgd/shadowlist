#include <shadowlist-core/Virtualizer.hpp>
#include <algorithm>

namespace azimgd::shadowlist {

void Virtualizer::measure(Container *container) {
  if (container->columns > 1) {
    if (!container->inverted) {
      measureColumnsDefault(container);
    } else {
      measureColumnsInverted(container);
    }
  } else {
    if (!container->inverted) {
      measureDefault(container);
    } else {
      measureInverted(container);
    }
  }
}

void Virtualizer::measureDefault(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureDefault outside of a revision");
  }

  container->nextRevision.measurementElementStartIndex = UNDEFINED_INDEX;
  container->nextRevision.measurementElementEndIndex = UNDEFINED_INDEX;

  if (container->nextRevisionCount == RevisionCountFirst) {
    measureFirstRevisionDefault(container);
  } else {
    measureNextRevisionDefault(container);
  }

  /*
   * Calculate total container height and width by finding the maximum extent
   */
  double maxHeight = 0.0;
  double maxWidth = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
    const Element& nextElement = container->nextRevision.elements[nextElementIndex];

    double elementOffsetTop = nextElement.offsetY + nextElement.height;
    double elementOffsetLeft = nextElement.offsetX + nextElement.width;

    if (elementOffsetTop > maxHeight) {
      maxHeight = elementOffsetTop;
    }
    if (elementOffsetLeft > maxWidth) {
      maxWidth = elementOffsetLeft;
    }
  }

  container->nextRevision.totalContainerHeight = maxHeight;
  container->nextRevision.totalContainerWidth = maxWidth;

  if (container->nextRevisionCount == RevisionCountFirst) {
    container->nextRevision.containerOffsetY = container->nextRevision.totalContainerHeight - container->nextRevision.windowContainerHeight;
    container->nextRevision.containerOffsetX = container->nextRevision.totalContainerWidth - container->nextRevision.windowContainerWidth;
  }
}

void Virtualizer::measureInverted(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureInverted outside of a revision");
  }

  container->nextRevision.measurementElementStartIndex = UNDEFINED_INDEX;
  container->nextRevision.measurementElementEndIndex = UNDEFINED_INDEX;

  if (container->nextRevisionCount == RevisionCountFirst) {
    measureFirstRevisionInverted(container);
  } else {
    measureNextRevisionInverted(container);
  }

  /*
   * Calculate total container height and width by finding the maximum extent
   */
  double maxHeight = 0.0;
  double maxWidth = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
    const Element& nextElement = container->nextRevision.elements[nextElementIndex];

    double elementOffsetTop = nextElement.offsetY + nextElement.height;
    double elementOffsetLeft = nextElement.offsetX + nextElement.width;

    if (elementOffsetTop > maxHeight) {
      maxHeight = elementOffsetTop;
    }
    if (elementOffsetLeft > maxWidth) {
      maxWidth = elementOffsetLeft;
    }
  }

  container->nextRevision.totalContainerHeight = maxHeight;
  container->nextRevision.totalContainerWidth = maxWidth;

  if (container->nextRevisionCount == RevisionCountFirst) {
    container->nextRevision.containerOffsetY = container->nextRevision.totalContainerHeight - container->nextRevision.windowContainerHeight;
    container->nextRevision.containerOffsetX = container->nextRevision.totalContainerWidth - container->nextRevision.windowContainerWidth;
  }
}

void Virtualizer::measureColumnsDefault(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureColumnsDefault outside of a revision");
  }

  container->nextRevision.measurementElementStartIndex = UNDEFINED_INDEX;
  container->nextRevision.measurementElementEndIndex = UNDEFINED_INDEX;

  if (container->nextRevisionCount == RevisionCountFirst) {
    measureFirstRevisionColumnsDefault(container);
  } else {
    measureNextRevisionColumnsDefault(container);
  }

  /*
   * Calculate total container height and width by finding the maximum extent
   */
  double maxHeight = 0.0;
  double maxWidth = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
    const Element& nextElement = container->nextRevision.elements[nextElementIndex];

    double elementOffsetTop = nextElement.offsetY + nextElement.height;
    double elementOffsetLeft = nextElement.offsetX + nextElement.width;

    if (elementOffsetTop > maxHeight) {
      maxHeight = elementOffsetTop;
    }
    if (elementOffsetLeft > maxWidth) {
      maxWidth = elementOffsetLeft;
    }
  }

  container->nextRevision.totalContainerHeight = maxHeight;
  container->nextRevision.totalContainerWidth = maxWidth;

  if (container->nextRevisionCount == RevisionCountFirst) {
    container->nextRevision.containerOffsetY = container->nextRevision.totalContainerHeight - container->nextRevision.windowContainerHeight;
    container->nextRevision.containerOffsetX = container->nextRevision.totalContainerWidth - container->nextRevision.windowContainerWidth;
  }
}

void Virtualizer::measureColumnsInverted(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureColumnsInverted outside of a revision");
  }

  container->nextRevision.measurementElementStartIndex = UNDEFINED_INDEX;
  container->nextRevision.measurementElementEndIndex = UNDEFINED_INDEX;

  if (container->nextRevisionCount == RevisionCountFirst) {
    measureFirstRevisionColumnsInverted(container);
  } else {
    measureNextRevisionColumnsInverted(container);
  }

  /*
   * Calculate total container height and width by finding the maximum extent
   */
  double maxHeight = 0.0;
  double maxWidth = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
    const Element& nextElement = container->nextRevision.elements[nextElementIndex];

    double elementOffsetTop = nextElement.offsetY + nextElement.height;
    double elementOffsetLeft = nextElement.offsetX + nextElement.width;

    if (elementOffsetTop > maxHeight) {
      maxHeight = elementOffsetTop;
    }
    if (elementOffsetLeft > maxWidth) {
      maxWidth = elementOffsetLeft;
    }
  }

  container->nextRevision.totalContainerHeight = maxHeight;
  container->nextRevision.totalContainerWidth = maxWidth;

  if (container->nextRevisionCount == RevisionCountFirst) {
    container->nextRevision.containerOffsetY = container->nextRevision.totalContainerHeight - container->nextRevision.windowContainerHeight;
    container->nextRevision.containerOffsetX = container->nextRevision.totalContainerWidth - container->nextRevision.windowContainerWidth;
  }
}

void Virtualizer::measureFirstRevisionDefault(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureFirstRevisionDefault outside of a revision");
  }

  std::vector<Element> nextElements;
  std::vector<std::size_t> nextElementIndices;

  std::size_t measurementElementStartIndex = UNDEFINED_INDEX;
  std::size_t measurementElementEndIndex = UNDEFINED_INDEX;

  double accumulatedHeight = 0.0;
  double accumulatedWidth = 0.0;

  /*
   * We are implementing a default list so start iterating from the start
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); ++nextElementIndex) {
    /*
     * Existing element, we don't want to modify it until revision is fully complete
     */
    const Element& prevElement = container->nextRevision.elements[nextElementIndex];

    /*
     * Let's create a new temporary element which will later be swapped with the original one
     */
    Element nextElement = prevElement;

    /*
     * Running measurement is expensive,
     * hence we don't want to re-measure if we already have a cached value
     */
    if (!prevElement.estimated) {
      auto [width, height] = container->estimatedElementSize;

      /*
       * If callback returns {0, 0}, skip updating this element and mark as not measured
       */
      if (width == 0.0 && height == 0.0) {
        continue;
      }

      nextElement.width = width;
      nextElement.height = height;
      nextElement.estimated = true;
    }

    /*
     * Keep track the range of measured element indices
     */
    if (measurementElementStartIndex == UNDEFINED_INDEX) {
      measurementElementStartIndex = nextElementIndex;
    }
    measurementElementEndIndex = nextElementIndex;

    if (container->nextRevision.measurementElementStartIndex == UNDEFINED_INDEX || nextElementIndex < container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (container->nextRevision.measurementElementEndIndex == UNDEFINED_INDEX || nextElementIndex > container->nextRevision.measurementElementEndIndex) {
      container->nextRevision.measurementElementEndIndex = nextElementIndex;
    }

    accumulatedHeight += nextElement.height;
    accumulatedWidth += nextElement.width;

    container->nextRevision.measurementElementTotalHeight += nextElement.height;
    container->nextRevision.measurementElementTotalWidth += nextElement.width;
    container->nextRevision.measurementElementCount++;
    container->nextRevision.measurementElementStartIndex = measurementElementStartIndex;
    container->nextRevision.measurementElementEndIndex = measurementElementEndIndex;

    /*
     * Create an intersection of newly measured elements
     */
    nextElements.push_back(nextElement);
    nextElementIndices.push_back(nextElementIndex);

    /*
     * Stop measuring if we measured enough elements to display in a visible window
     */
    if (!container->horizontal && accumulatedHeight >= container->nextRevision.windowContainerHeight) {
      break;
    }

    if (container->horizontal && accumulatedWidth >= container->nextRevision.windowContainerWidth) {
      break;
    }
  }

  /*
   * Calculate average element dimensions, needed for estimating unmeasured portion of the list
   */
  if (container->nextRevision.measurementElementCount > 0) {
    if (container->nextRevision.averageElementWidth == 0.0) {
      container->nextRevision.averageElementWidth = container->nextRevision.measurementElementTotalWidth / container->nextRevision.measurementElementCount;
    }
    if (container->nextRevision.averageElementHeight == 0.0) {
      container->nextRevision.averageElementHeight = container->nextRevision.measurementElementTotalHeight / container->nextRevision.measurementElementCount;
    }
  }

  /*
   * Swap elements from newly measured intersection with existing ones
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < nextElements.size(); nextElementIndex++) {
    container->nextRevision.elements[nextElementIndices[nextElementIndex]] = nextElements[nextElementIndex];
  }

  /*
   * Adjust offsets for all elements in the list
   * For unmeasured elements, use average dimensions
   */
  double nextOffset = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
    Element& nextElement = container->nextRevision.elements[nextElementIndex];

    if (!nextElement.estimated) {
      nextElement.width = container->nextRevision.averageElementWidth;
      nextElement.height = container->nextRevision.averageElementHeight;
    }

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

void Virtualizer::measureNextRevisionDefault(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureNextRevisionDefault outside of a revision");
  }

  std::vector<Element> nextElements;
  std::vector<std::size_t> nextElementIndices;

  std::size_t measurementElementStartIndex = UNDEFINED_INDEX;
  std::size_t measurementElementEndIndex = UNDEFINED_INDEX;

  double containerOffset = container->getContainerOffset();
  double windowSize = container->getWindowContainerSize();

  /*
   * We are implementing a default list so start iterating from the start
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); ++nextElementIndex) {
    /*
     * Existing element, we don't want to modify it until revision is fully complete
     */
    const Element& prevElement = container->nextRevision.elements[nextElementIndex];

    /*
     * Skip elements that are outside of the visible window plus 1x window buffer
     */
    double elementOffset = container->horizontal ? prevElement.offsetX : prevElement.offsetY;
    if (elementOffset < containerOffset - windowSize * 1 ||
      elementOffset > containerOffset + windowSize * 2) {
      continue;
    }

    /*
     * Let's create a new temporary element which will later be swapped with the original one
     */
    Element nextElement = prevElement;

    /*
     * Running measurement is expensive,
     * hence we don't want to re-measure if we already have a cached value
     */
    if (!prevElement.estimated) {
      auto [width, height] = container->estimatedElementSize;

      /*
       * If callback returns {0, 0}, skip updating this element and mark as not measured
       */
      if (width == 0.0 && height == 0.0) {
        continue;
      }

      nextElement.width = width;
      nextElement.height = height;
      nextElement.estimated = true;
    }

    /*
     * Keep track the range of measured element indices
     */
    if (measurementElementStartIndex == UNDEFINED_INDEX) {
      measurementElementStartIndex = nextElementIndex;
    }
    measurementElementEndIndex = nextElementIndex;

    if (container->nextRevision.measurementElementStartIndex == UNDEFINED_INDEX || nextElementIndex < container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (container->nextRevision.measurementElementEndIndex == UNDEFINED_INDEX || nextElementIndex > container->nextRevision.measurementElementEndIndex) {
      container->nextRevision.measurementElementEndIndex = nextElementIndex;
    }

    container->nextRevision.measurementElementTotalHeight += nextElement.height;
    container->nextRevision.measurementElementTotalWidth += nextElement.width;
    container->nextRevision.measurementElementCount++;
    container->nextRevision.measurementElementStartIndex = measurementElementStartIndex;
    container->nextRevision.measurementElementEndIndex = measurementElementEndIndex;

    /*
     * Create an intersection of newly measured elements
     */
    nextElements.push_back(nextElement);
    nextElementIndices.push_back(nextElementIndex);
  }

  /*
   * Calculate average element dimensions, needed for estimating unmeasured portion of the list
   */
  if (container->nextRevision.measurementElementCount > 0) {
    if (container->nextRevision.averageElementWidth == 0.0) {
      container->nextRevision.averageElementWidth = container->nextRevision.measurementElementTotalWidth / container->nextRevision.measurementElementCount;
    }
    if (container->nextRevision.averageElementHeight == 0.0) {
      container->nextRevision.averageElementHeight = container->nextRevision.measurementElementTotalHeight / container->nextRevision.measurementElementCount;
    }
  }

  /*
   * Swap elements from newly measured intersection with existing ones
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < nextElements.size(); nextElementIndex++) {
    container->nextRevision.elements[nextElementIndices[nextElementIndex]] = nextElements[nextElementIndex];
  }

  /*
   * Adjust offsets for all elements in the list
   * For unmeasured elements, use average dimensions
   */
  double nextOffset = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
    Element& nextElement = container->nextRevision.elements[nextElementIndex];

    if (!nextElement.estimated) {
      nextElement.width = container->nextRevision.averageElementWidth;
      nextElement.height = container->nextRevision.averageElementHeight;
    }

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

void Virtualizer::measureFirstRevisionInverted(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureFirstRevisionInverted outside of a revision");
  }

  std::vector<Element> nextElements;
  std::vector<std::size_t> nextElementIndices;

  std::size_t measurementElementStartIndex = UNDEFINED_INDEX;
  std::size_t measurementElementEndIndex = UNDEFINED_INDEX;

  double accumulatedHeight = 0.0;
  double accumulatedWidth = 0.0;

  /*
   * We are implementing an inverted list so start iterating from the end
   */
  for (std::size_t nextElementIndex = container->nextRevision.elements.size(); nextElementIndex-- > 0;) {
    /*
     * Existing element, we don't want to modify it until revision is fully complete
     */
    const Element& prevElement = container->nextRevision.elements[nextElementIndex];

    /*
     * Let's create a new temporary element which will later be swapped with the original one
     */
    Element nextElement = prevElement;

    /*
     * Running measurement is expensive,
     * hence we don't want to re-measure if we already have a cached value
     */
    if (!prevElement.estimated) {
      auto [width, height] = container->estimatedElementSize;

      /*
       * If callback returns {0, 0}, skip updating this element and mark as not measured
       */
      if (width == 0.0 && height == 0.0) {
        continue;
      }

      nextElement.width = width;
      nextElement.height = height;
      nextElement.estimated = true;
    }

    /*
     * Keep track the range of measured element indices
     */
    if (measurementElementStartIndex == UNDEFINED_INDEX) {
      measurementElementStartIndex = nextElementIndex;
    }
    measurementElementEndIndex = nextElementIndex;

    if (container->nextRevision.measurementElementStartIndex == UNDEFINED_INDEX || nextElementIndex > container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (container->nextRevision.measurementElementEndIndex == UNDEFINED_INDEX || nextElementIndex < container->nextRevision.measurementElementEndIndex) {
      container->nextRevision.measurementElementEndIndex = nextElementIndex;
    }

    accumulatedHeight += nextElement.height;
    accumulatedWidth += nextElement.width;

    container->nextRevision.measurementElementTotalHeight += nextElement.height;
    container->nextRevision.measurementElementTotalWidth += nextElement.width;
    container->nextRevision.measurementElementCount++;
    container->nextRevision.measurementElementStartIndex = measurementElementStartIndex;
    container->nextRevision.measurementElementEndIndex = measurementElementEndIndex;

    /*
     * Create an intersection of newly measured elements
     */
    nextElements.push_back(nextElement);
    nextElementIndices.push_back(nextElementIndex);

    /*
     * Stop measuring if we measured enough elements to display in a visible window
     */
    if (!container->horizontal && accumulatedHeight >= container->nextRevision.windowContainerHeight) {
      break;
    }

    if (container->horizontal && accumulatedWidth >= container->nextRevision.windowContainerWidth) {
      break;
    }
  }

  /*
   * Calculate average element dimensions, needed for estimating unmeasured portion of the list
   */
  if (container->nextRevision.measurementElementCount > 0 && container->nextRevision.measurementElementCount < 10) {
    if (container->nextRevision.averageElementWidth == 0.0) {
      container->nextRevision.averageElementWidth = container->nextRevision.measurementElementTotalWidth / container->nextRevision.measurementElementCount;
    }
    if (container->nextRevision.averageElementHeight == 0.0) {
      container->nextRevision.averageElementHeight = container->nextRevision.measurementElementTotalHeight / container->nextRevision.measurementElementCount;
    }
  }

  /*
   * Swap elements from newly measured intersection with existing ones
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < nextElements.size(); nextElementIndex++) {
    container->nextRevision.elements[nextElementIndices[nextElementIndex]] = nextElements[nextElementIndex];
  }

  /*
   * Adjust offsets for all elements in the list
   * For unmeasured elements, use average dimensions
   */
  double nextOffset = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
    Element& nextElement = container->nextRevision.elements[nextElementIndex];

    if (!nextElement.estimated) {
      nextElement.width = container->nextRevision.averageElementWidth;
      nextElement.height = container->nextRevision.averageElementHeight;
    }

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

void Virtualizer::measureNextRevisionInverted(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureNextRevisionInverted outside of a revision");
  }

  std::vector<Element> nextElements;
  std::vector<std::size_t> nextElementIndices;

  std::size_t measurementElementStartIndex = UNDEFINED_INDEX;
  std::size_t measurementElementEndIndex = UNDEFINED_INDEX;

  double containerOffset = container->getContainerOffset();
  double windowSize = container->getWindowContainerSize();

  /*
   * We are implementing an inverted list so start iterating from the end
   */
  for (std::size_t nextElementIndex = container->nextRevision.elements.size(); nextElementIndex-- > 0;) {
    /*
     * Existing element, we don't want to modify it until revision is fully complete
     */
    const Element& prevElement = container->nextRevision.elements[nextElementIndex];

    /*
     * Skip elements that are outside of the visible window plus 1x window buffer
     */
    double elementOffset = container->horizontal ? prevElement.offsetX : prevElement.offsetY;
    if (elementOffset < containerOffset - windowSize * 1 ||
      elementOffset > containerOffset + windowSize * 2) {
      continue;
    }

    /*
     * Let's create a new temporary element which will later be swapped with the original one
     */
    Element nextElement = prevElement;

    /*
     * Running measurement is expensive,
     * hence we don't want to re-measure if we already have a cached value
     */
    if (!prevElement.estimated) {
      auto [width, height] = container->estimatedElementSize;

      /*
       * If callback returns {0, 0}, skip updating this element and mark as not measured
       */
      if (width == 0.0 && height == 0.0) {
        continue;
      }

      nextElement.width = width;
      nextElement.height = height;
      nextElement.estimated = true;
    }

    /*
     * Keep track the range of measured element indices
     */
    if (measurementElementStartIndex == UNDEFINED_INDEX) {
      measurementElementStartIndex = nextElementIndex;
    }
    measurementElementEndIndex = nextElementIndex;

    if (container->nextRevision.measurementElementStartIndex == UNDEFINED_INDEX || nextElementIndex > container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (container->nextRevision.measurementElementEndIndex == UNDEFINED_INDEX || nextElementIndex < container->nextRevision.measurementElementEndIndex) {
      container->nextRevision.measurementElementEndIndex = nextElementIndex;
    }

    container->nextRevision.measurementElementTotalHeight += nextElement.height;
    container->nextRevision.measurementElementTotalWidth += nextElement.width;
    container->nextRevision.measurementElementCount++;
    container->nextRevision.measurementElementStartIndex = measurementElementStartIndex;
    container->nextRevision.measurementElementEndIndex = measurementElementEndIndex;

    /*
     * Create an intersection of newly measured elements
     */
    nextElements.push_back(nextElement);
    nextElementIndices.push_back(nextElementIndex);
  }

  /*
   * Calculate average element dimensions, needed for estimating unmeasured portion of the list
   */
  if (container->nextRevision.measurementElementCount > 0 && container->nextRevision.measurementElementCount < 10) {
    if (container->nextRevision.averageElementWidth == 0.0) {
      container->nextRevision.averageElementWidth = container->nextRevision.measurementElementTotalWidth / container->nextRevision.measurementElementCount;
    }
    if (container->nextRevision.averageElementHeight == 0.0) {
      container->nextRevision.averageElementHeight = container->nextRevision.measurementElementTotalHeight / container->nextRevision.measurementElementCount;
    }
  }

  /*
   * Swap elements from newly measured intersection with existing ones
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < nextElements.size(); nextElementIndex++) {
    container->nextRevision.elements[nextElementIndices[nextElementIndex]] = nextElements[nextElementIndex];
  }

  /*
   * Adjust offsets for all elements in the list
   * For unmeasured elements, use average dimensions
   */
  double nextOffset = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
    Element& nextElement = container->nextRevision.elements[nextElementIndex];

    if (!nextElement.estimated) {
      nextElement.width = container->nextRevision.averageElementWidth;
      nextElement.height = container->nextRevision.averageElementHeight;
    }

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

void Virtualizer::addElementAtIndex(Container *container, std::size_t index, std::size_t prevElementIndex) {
  if (index > container->nextRevision.elements.size()) {
    throw std::out_of_range("Index out of bounds");
  }

  Element nextElement;

  auto [width, height] = container->estimatedElementSize;

  /*
   * If callback returns {0, 0}, mark element as not measured and use average dimensions
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

  if (container->columns > 1) {
    /*
     * Multi-column layout: recalculate offsets using sequential placement
     */
    std::vector<double> trackSizes(container->columns, 0.0);
    double trackSize = container->horizontal
      ? container->nextRevision.windowContainerHeight / container->columns
      : container->nextRevision.windowContainerWidth / container->columns;

    for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
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
     * Single-column layout: use orientation-based positioning
     */
    double nextOffset = 0.0;

    for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
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

  double widthDiff = size.width - prevWidth;
  double heightDiff = size.height - prevHeight;

  if (container->nextRevision.averageElementHeight > 0) {
    container->nextRevision.measurementElementTotalHeight += heightDiff;
    container->nextRevision.measurementElementTotalWidth += widthDiff;
  }

  if (container->columns > 1) {
    /*
     * Multi-column layout: recalculate offsets using sequential placement
     */
    std::vector<double> trackSizes(container->columns, 0.0);
    double trackSize = container->horizontal
      ? container->nextRevision.windowContainerHeight / container->columns
      : container->nextRevision.windowContainerWidth / container->columns;

    for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
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
     * Single-column layout: use orientation-based positioning
     */
    double nextOffset = 0.0;

    for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
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

void Virtualizer::prependElements(Container *container, std::size_t count) {
  for (std::size_t prevElementIndex = count; prevElementIndex-- > 0;) {
    addElementAtIndex(container, 0, prevElementIndex);
  }

  if (container->columns > 1) {
    /*
     * Multi-column: find max height/width across all elements
     */
    double maxHeight = 0.0;
    double maxWidth = 0.0;
    for (const auto& element : container->nextRevision.elements) {
      double elementBottom = element.offsetY + element.height;
      double elementRight = element.offsetX + element.width;
      if (elementBottom > maxHeight) {
        maxHeight = elementBottom;
      }
      if (elementRight > maxWidth) {
        maxWidth = elementRight;
      }
    }
    container->nextRevision.totalContainerHeight = maxHeight;
    container->nextRevision.totalContainerWidth = maxWidth;
  } else {
    /*
     * Single-column: Horizontal: total width is last element's right edge, height is max element height
     * Vertical: total height is last element's bottom edge, width is max element width
     */
    if (container->horizontal) {
      auto lastElement = container->getElementAtIndex(container->nextRevision.elements.size() - 1);
      container->nextRevision.totalContainerWidth = lastElement.offsetX + lastElement.width;

      double maxHeight = 0.0;
      for (const auto& element : container->nextRevision.elements) {
        double elementHeight = element.offsetY + element.height;
        if (elementHeight > maxHeight) {
          maxHeight = elementHeight;
        }
      }
      container->nextRevision.totalContainerHeight = maxHeight;
    } else {
      auto lastElement = container->getElementAtIndex(container->nextRevision.elements.size() - 1);
      container->nextRevision.totalContainerHeight = lastElement.offsetY + lastElement.height;

      double maxWidth = 0.0;
      for (const auto& element : container->nextRevision.elements) {
        double elementWidth = element.offsetX + element.width;
        if (elementWidth > maxWidth) {
          maxWidth = elementWidth;
        }
      }
      container->nextRevision.totalContainerWidth = maxWidth;
    }
  }
}

void Virtualizer::appendElements(Container *container, std::size_t count) {
  for (std::size_t prevElementIndex = 0; prevElementIndex < count; prevElementIndex++) {
    std::size_t insertIndex = container->nextRevision.elements.size();
    addElementAtIndex(container, insertIndex);
  }

  if (container->columns > 1) {
    /*
     * Multi-column: find max height/width across all elements
     */
    double maxHeight = 0.0;
    double maxWidth = 0.0;
    for (const auto& element : container->nextRevision.elements) {
      double elementBottom = element.offsetY + element.height;
      double elementRight = element.offsetX + element.width;
      if (elementBottom > maxHeight) {
        maxHeight = elementBottom;
      }
      if (elementRight > maxWidth) {
        maxWidth = elementRight;
      }
    }
    container->nextRevision.totalContainerHeight = maxHeight;
    container->nextRevision.totalContainerWidth = maxWidth;
  } else {
    /*
     * Single-column: Horizontal: total width is last element's right edge, height is max element height
     * Vertical: total height is last element's bottom edge, width is max element width
     */
    if (container->horizontal) {
      auto lastElement = container->getElementAtIndex(container->nextRevision.elements.size() - 1);
      container->nextRevision.totalContainerWidth = lastElement.offsetX + lastElement.width;

      double maxHeight = 0.0;
      for (const auto& element : container->nextRevision.elements) {
        double elementHeight = element.offsetY + element.height;
        if (elementHeight > maxHeight) {
          maxHeight = elementHeight;
        }
      }
      container->nextRevision.totalContainerHeight = maxHeight;
    } else {
      auto lastElement = container->getElementAtIndex(container->nextRevision.elements.size() - 1);
      container->nextRevision.totalContainerHeight = lastElement.offsetY + lastElement.height;

      double maxWidth = 0.0;
      for (const auto& element : container->nextRevision.elements) {
        double elementWidth = element.offsetX + element.width;
        if (elementWidth > maxWidth) {
          maxWidth = elementWidth;
        }
      }
      container->nextRevision.totalContainerWidth = maxWidth;
    }
  }
}

void Virtualizer::measureFirstRevisionColumnsDefault(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureFirstRevisionColumnsDefault outside of a revision");
  }

  std::vector<Element> nextElements;
  std::vector<std::size_t> nextElementIndices;
  std::vector<double> trackSizes(container->columns, 0.0);

  double trackSize = container->horizontal
    ? container->nextRevision.windowContainerHeight / container->columns
    : container->nextRevision.windowContainerWidth / container->columns;

  /*
   * Fill columns/rows to window size with sequential placement
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); ++nextElementIndex) {
    const Element& prevElement = container->nextRevision.elements[nextElementIndex];
    Element nextElement = prevElement;

    /*
     * Estimate element if not already measured
     */
    if (!prevElement.estimated) {
      auto [width, height] = container->estimatedElementSize;

      if (width == 0.0 && height == 0.0) {
        continue;
      }

      if (container->horizontal) {
        nextElement.width = width;
        nextElement.height = trackSize;
      } else {
        nextElement.width = trackSize;
        nextElement.height = height;
      }
      nextElement.estimated = true;
    } else {
      if (container->horizontal) {
        nextElement.height = trackSize;
      } else {
        nextElement.width = trackSize;
      }
    }

    /*
     * Place element sequentially: elementIndex % columns
     */
    std::size_t trackIndex = nextElementIndex % container->columns;
    if (container->horizontal) {
      nextElement.offsetY = trackIndex * trackSize;
      nextElement.offsetX = trackSizes[trackIndex];
    } else {
      nextElement.offsetX = trackIndex * trackSize;
      nextElement.offsetY = trackSizes[trackIndex];
    }
    nextElement.index = nextElementIndex;

    /*
     * Update track size
     */
    if (container->horizontal) {
      trackSizes[trackIndex] += nextElement.width + nextElement.gapX;
    } else {
      trackSizes[trackIndex] += nextElement.height + nextElement.gapY;
    }

    /*
     * Track measurement indices
     */
    if (container->nextRevision.measurementElementStartIndex == UNDEFINED_INDEX || nextElementIndex < container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (container->nextRevision.measurementElementEndIndex == UNDEFINED_INDEX || nextElementIndex > container->nextRevision.measurementElementEndIndex) {
      container->nextRevision.measurementElementEndIndex = nextElementIndex;
    }

    container->nextRevision.measurementElementTotalHeight += nextElement.height;
    container->nextRevision.measurementElementTotalWidth += nextElement.width;
    container->nextRevision.measurementElementCount++;

    nextElements.push_back(nextElement);
    nextElementIndices.push_back(nextElementIndex);

    /*
     * Stop when shortest track is filled to at least window size
     */
    double minTrackSize = *std::min_element(trackSizes.begin(), trackSizes.end());
    double windowSize = container->horizontal ? container->nextRevision.windowContainerWidth : container->nextRevision.windowContainerHeight;
    if (minTrackSize >= windowSize) {
      break;
    }
  }

  /*
   * Calculate average element dimensions
   */
  if (container->nextRevision.measurementElementCount > 0) {
    if (container->nextRevision.averageElementWidth == 0.0) {
      container->nextRevision.averageElementWidth = container->nextRevision.measurementElementTotalWidth / container->nextRevision.measurementElementCount;
    }
    if (container->nextRevision.averageElementHeight == 0.0) {
      container->nextRevision.averageElementHeight = container->nextRevision.measurementElementTotalHeight / container->nextRevision.measurementElementCount;
    }
  }

  /*
   * Swap measured elements
   */
  for (std::size_t intersectionIndex = 0; intersectionIndex < nextElements.size(); ++intersectionIndex) {
    container->nextRevision.elements[nextElementIndices[intersectionIndex]] = nextElements[intersectionIndex];
  }

  /*
   * Adjust offsets for unmeasured elements using average dimensions
   */
  for (std::size_t nextElementIndex = container->nextRevision.measurementElementEndIndex + 1; nextElementIndex < container->nextRevision.elements.size(); ++nextElementIndex) {
    Element& nextElement = container->nextRevision.elements[nextElementIndex];

    if (!nextElement.estimated) {
      if (container->horizontal) {
        nextElement.width = container->nextRevision.averageElementWidth;
        nextElement.height = trackSize;
      } else {
        nextElement.width = trackSize;
        nextElement.height = container->nextRevision.averageElementHeight;
      }
    }

    /*
     * Place sequentially: elementIndex % columns
     */
    std::size_t trackIndex = nextElementIndex % container->columns;
    if (container->horizontal) {
      nextElement.offsetY = trackIndex * trackSize;
      nextElement.offsetX = trackSizes[trackIndex];
    } else {
      nextElement.offsetX = trackIndex * trackSize;
      nextElement.offsetY = trackSizes[trackIndex];
    }
    nextElement.index = nextElementIndex;

    if (container->horizontal) {
      trackSizes[trackIndex] += nextElement.width + nextElement.gapX;
    } else {
      trackSizes[trackIndex] += nextElement.height + nextElement.gapY;
    }
  }

  /*
   * Set total container dimensions based on longest track
   */
  double maxTrackSize = *std::max_element(trackSizes.begin(), trackSizes.end());
  if (container->horizontal) {
    container->nextRevision.totalContainerWidth = maxTrackSize;
    container->nextRevision.totalContainerHeight = container->nextRevision.windowContainerHeight;
  } else {
    container->nextRevision.totalContainerHeight = maxTrackSize;
    container->nextRevision.totalContainerWidth = container->nextRevision.windowContainerWidth;
  }
}

void Virtualizer::measureNextRevisionColumnsDefault(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureNextRevisionColumnsDefault outside of a revision");
  }

  std::vector<Element> nextElements;
  std::vector<std::size_t> nextElementIndices;
  std::vector<double> trackSizes(container->columns, 0.0);

  double trackSize = container->horizontal
    ? container->nextRevision.windowContainerHeight / container->columns
    : container->nextRevision.windowContainerWidth / container->columns;
  double containerOffset = container->getContainerOffset();
  double windowSize = container->getWindowContainerSize();

  /*
   * Rebuild track sizes from existing estimated elements
   */
  for (std::size_t elementIndex = 0; elementIndex < container->nextRevision.elements.size(); ++elementIndex) {
    const Element& element = container->nextRevision.elements[elementIndex];
    if (element.estimated) {
      std::size_t trackIndex = elementIndex % container->columns;
      double trackEnd = container->horizontal
        ? element.offsetX + element.width + element.gapX
        : element.offsetY + element.height + element.gapY;
      if (trackEnd > trackSizes[trackIndex]) {
        trackSizes[trackIndex] = trackEnd;
      }
    }
  }

  /*
   * Measure elements in visible window + buffer
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); ++nextElementIndex) {
    const Element& prevElement = container->nextRevision.elements[nextElementIndex];

    /*
     * Skip elements outside visible window + 1x buffer
     */
    double elementOffset = container->horizontal ? prevElement.offsetX : prevElement.offsetY;
    if (elementOffset < containerOffset - windowSize * 1 || elementOffset > containerOffset + windowSize * 2) {
      continue;
    }

    Element nextElement = prevElement;

    /*
     * Determine track for sequential placement
     */
    std::size_t trackIndex = nextElementIndex % container->columns;

    /*
     * Measure element if not already estimated
     */
    if (!prevElement.estimated) {
      auto [width, height] = container->estimatedElementSize;

      if (width == 0.0 && height == 0.0) {
        continue;
      }

      if (container->horizontal) {
        nextElement.width = width;
        nextElement.height = trackSize;
        nextElement.offsetY = trackIndex * trackSize;
        nextElement.offsetX = trackSizes[trackIndex];
        trackSizes[trackIndex] += nextElement.width + nextElement.gapX;
      } else {
        nextElement.width = trackSize;
        nextElement.height = height;
        nextElement.offsetX = trackIndex * trackSize;
        nextElement.offsetY = trackSizes[trackIndex];
        trackSizes[trackIndex] += nextElement.height + nextElement.gapY;
      }
      nextElement.estimated = true;
    } else {
      /*
       * Already estimated, just ensure track size is correct
       */
      if (container->horizontal) {
        nextElement.height = trackSize;
      } else {
        nextElement.width = trackSize;
      }
    }

    nextElement.index = nextElementIndex;

    /*
     * Track measurement indices
     */
    if (container->nextRevision.measurementElementStartIndex == UNDEFINED_INDEX || nextElementIndex < container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (container->nextRevision.measurementElementEndIndex == UNDEFINED_INDEX || nextElementIndex > container->nextRevision.measurementElementEndIndex) {
      container->nextRevision.measurementElementEndIndex = nextElementIndex;
    }

    container->nextRevision.measurementElementTotalHeight += nextElement.height;
    container->nextRevision.measurementElementTotalWidth += nextElement.width;
    container->nextRevision.measurementElementCount++;

    nextElements.push_back(nextElement);
    nextElementIndices.push_back(nextElementIndex);
  }

  /*
   * Calculate average element dimensions
   */
  if (container->nextRevision.measurementElementCount > 0) {
    if (container->nextRevision.averageElementWidth == 0.0) {
      container->nextRevision.averageElementWidth = container->nextRevision.measurementElementTotalWidth / container->nextRevision.measurementElementCount;
    }
    if (container->nextRevision.averageElementHeight == 0.0) {
      container->nextRevision.averageElementHeight = container->nextRevision.measurementElementTotalHeight / container->nextRevision.measurementElementCount;
    }
  }

  /*
   * Swap measured elements
   */
  for (std::size_t intersectionIndex = 0; intersectionIndex < nextElements.size(); ++intersectionIndex) {
    container->nextRevision.elements[nextElementIndices[intersectionIndex]] = nextElements[intersectionIndex];
  }

  /*
   * Set total container dimensions based on longest track
   */
  double maxTrackSize = *std::max_element(trackSizes.begin(), trackSizes.end());
  if (container->horizontal) {
    container->nextRevision.totalContainerWidth = maxTrackSize;
    container->nextRevision.totalContainerHeight = container->nextRevision.windowContainerHeight;
  } else {
    container->nextRevision.totalContainerHeight = maxTrackSize;
    container->nextRevision.totalContainerWidth = container->nextRevision.windowContainerWidth;
  }
}

void Virtualizer::measureFirstRevisionColumnsInverted(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureFirstRevisionColumnsInverted outside of a revision");
  }

  std::vector<Element> nextElements;
  std::vector<std::size_t> nextElementIndices;
  std::vector<double> trackSizes(container->columns, 0.0);

  double trackSize = container->horizontal
    ? container->nextRevision.windowContainerHeight / container->columns
    : container->nextRevision.windowContainerWidth / container->columns;

  /*
   * Fill columns to window height with sequential placement, iterating in reverse
   */
  for (std::size_t reverseOffset = 0; reverseOffset < container->nextRevision.elements.size(); ++reverseOffset) {
    std::size_t nextElementIndex = container->nextRevision.elements.size() - 1 - reverseOffset;
    const Element& prevElement = container->nextRevision.elements[nextElementIndex];
    Element nextElement = prevElement;

    /*
     * Estimate element if not already measured
     */
    if (!prevElement.estimated) {
      auto [width, height] = container->estimatedElementSize;

      if (width == 0.0 && height == 0.0) {
        continue;
      }

      if (container->horizontal) {
        nextElement.width = width;
        nextElement.height = trackSize;
      } else {
        nextElement.width = trackSize;
        nextElement.height = height;
      }
      nextElement.estimated = true;
    } else {
      if (container->horizontal) {
        nextElement.height = trackSize;
      } else {
        nextElement.width = trackSize;
      }
    }

    /*
     * Track measurement indices
     */
    if (container->nextRevision.measurementElementStartIndex == UNDEFINED_INDEX || nextElementIndex > container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (container->nextRevision.measurementElementEndIndex == UNDEFINED_INDEX || nextElementIndex < container->nextRevision.measurementElementEndIndex) {
      container->nextRevision.measurementElementEndIndex = nextElementIndex;
    }

    container->nextRevision.measurementElementTotalHeight += nextElement.height;
    container->nextRevision.measurementElementTotalWidth += nextElement.width;
    container->nextRevision.measurementElementCount++;

    nextElements.push_back(nextElement);
    nextElementIndices.push_back(nextElementIndex);

    /*
     * Stop when we've accumulated enough size to fill the window
     * For multi-column, divide window size by number of columns as rough estimate
     */
    if (container->horizontal) {
      double accumulatedWidth = (container->nextRevision.measurementElementTotalWidth / container->columns);
      if (accumulatedWidth >= container->nextRevision.windowContainerWidth) {
        break;
      }
    } else {
      double accumulatedHeight = (container->nextRevision.measurementElementTotalHeight / container->columns);
      if (accumulatedHeight >= container->nextRevision.windowContainerHeight) {
        break;
      }
    }
  }

  /*
   * Calculate average element dimensions
   */
  if (container->nextRevision.measurementElementCount > 0) {
    if (container->nextRevision.averageElementWidth == 0.0) {
      container->nextRevision.averageElementWidth = container->nextRevision.measurementElementTotalWidth / container->nextRevision.measurementElementCount;
    }
    if (container->nextRevision.averageElementHeight == 0.0) {
      container->nextRevision.averageElementHeight = container->nextRevision.measurementElementTotalHeight / container->nextRevision.measurementElementCount;
    }
  }

  /*
   * Swap measured elements
   */
  for (std::size_t intersectionIndex = 0; intersectionIndex < nextElements.size(); ++intersectionIndex) {
    container->nextRevision.elements[nextElementIndices[intersectionIndex]] = nextElements[intersectionIndex];
  }

  /*
   * Recalculate offsets for all elements in forward order
   */
  std::fill(trackSizes.begin(), trackSizes.end(), 0.0);

  for (std::size_t nextElementIndex = 0; nextElementIndex < container->nextRevision.elements.size(); ++nextElementIndex) {
    Element& nextElement = container->nextRevision.elements[nextElementIndex];

    /*
     * Set track size for all elements
     */
    if (container->horizontal) {
      nextElement.height = trackSize;
    } else {
      nextElement.width = trackSize;
    }

    /*
     * Use average dimension for unmeasured elements
     */
    if (!nextElement.estimated) {
      if (container->horizontal) {
        nextElement.width = container->nextRevision.averageElementWidth;
      } else {
        nextElement.height = container->nextRevision.averageElementHeight;
      }
    }

    nextElement.index = nextElementIndex;

    /*
     * Place element sequentially: elementIndex % columns
     */
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

  /*
   * Set total container dimensions based on longest track
   */
  double maxTrackSize = *std::max_element(trackSizes.begin(), trackSizes.end());
  if (container->horizontal) {
    container->nextRevision.totalContainerWidth = maxTrackSize;
    container->nextRevision.totalContainerHeight = container->nextRevision.windowContainerHeight;
  } else {
    container->nextRevision.totalContainerHeight = maxTrackSize;
    container->nextRevision.totalContainerWidth = container->nextRevision.windowContainerWidth;
  }
}

void Virtualizer::measureNextRevisionColumnsInverted(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureNextRevisionColumnsInverted outside of a revision");
  }

  std::vector<Element> nextElements;
  std::vector<std::size_t> nextElementIndices;

  double trackSize = container->horizontal
    ? container->nextRevision.windowContainerHeight / container->columns
    : container->nextRevision.windowContainerWidth / container->columns;
  double containerOffset = container->getContainerOffset();
  double windowSize = container->getWindowContainerSize();

  /*
   * Measure elements in visible window + buffer, iterating in reverse
   */
  for (std::size_t reverseOffset = 0; reverseOffset < container->nextRevision.elements.size(); ++reverseOffset) {
    std::size_t nextElementIndex = container->nextRevision.elements.size() - 1 - reverseOffset;
    const Element& prevElement = container->nextRevision.elements[nextElementIndex];

    /*
     * Skip elements outside visible window + 1x buffer
     */
    double elementOffset = container->horizontal ? prevElement.offsetX : prevElement.offsetY;
    if (elementOffset < containerOffset - windowSize * 1 || elementOffset > containerOffset + windowSize * 2) {
      continue;
    }

    Element nextElement = prevElement;

    /*
     * Measure element if not already estimated
     */
    if (!prevElement.estimated) {
      auto [width, height] = container->estimatedElementSize;

      if (width == 0.0 && height == 0.0) {
        continue;
      }

      if (container->horizontal) {
        nextElement.width = width;
        nextElement.height = trackSize;
      } else {
        nextElement.width = trackSize;
        nextElement.height = height;
      }
      nextElement.estimated = true;
    } else {
      /*
       * Already estimated, just ensure track size is correct
       */
      if (container->horizontal) {
        nextElement.height = trackSize;
      } else {
        nextElement.width = trackSize;
      }
    }

    nextElement.index = nextElementIndex;

    /*
     * Track measurement indices
     */
    if (container->nextRevision.measurementElementStartIndex == UNDEFINED_INDEX || nextElementIndex > container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (container->nextRevision.measurementElementEndIndex == UNDEFINED_INDEX || nextElementIndex < container->nextRevision.measurementElementEndIndex) {
      container->nextRevision.measurementElementEndIndex = nextElementIndex;
    }

    container->nextRevision.measurementElementTotalHeight += nextElement.height;
    container->nextRevision.measurementElementTotalWidth += nextElement.width;
    container->nextRevision.measurementElementCount++;

    nextElements.push_back(nextElement);
    nextElementIndices.push_back(nextElementIndex);
  }

  /*
   * Calculate average element dimensions
   */
  if (container->nextRevision.measurementElementCount > 0) {
    if (container->nextRevision.averageElementWidth == 0.0) {
      container->nextRevision.averageElementWidth = container->nextRevision.measurementElementTotalWidth / container->nextRevision.measurementElementCount;
    }
    if (container->nextRevision.averageElementHeight == 0.0) {
      container->nextRevision.averageElementHeight = container->nextRevision.measurementElementTotalHeight / container->nextRevision.measurementElementCount;
    }
  }

  /*
   * Swap measured elements
   */
  for (std::size_t intersectionIndex = 0; intersectionIndex < nextElements.size(); ++intersectionIndex) {
    container->nextRevision.elements[nextElementIndices[intersectionIndex]] = nextElements[intersectionIndex];
  }

  /*
   * Set total container dimensions based on max element extent
   */
  double maxHeight = 0.0;
  double maxWidth = 0.0;
  for (const auto& element : container->nextRevision.elements) {
    double elementBottom = element.offsetY + element.height;
    double elementRight = element.offsetX + element.width;
    if (elementBottom > maxHeight) {
      maxHeight = elementBottom;
    }
    if (elementRight > maxWidth) {
      maxWidth = elementRight;
    }
  }
  container->nextRevision.totalContainerHeight = maxHeight;
  container->nextRevision.totalContainerWidth = maxWidth;
}

}
