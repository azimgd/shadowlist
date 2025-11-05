#include <shadowlist-core/Virtualizer.hpp>
#include <algorithm>

namespace azimgd::shadowlist {

void Virtualizer::measure(Container *container) {
  if (!container->inverted) {
    measureDefault(container);
  } else {
    measureInverted(container);
  }
}

void Virtualizer::measureDefault(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureDefault outside of a revision");
  }

  container->nextRevision.measurementElementStartIndex = (std::size_t)-1;
  container->nextRevision.measurementElementEndIndex = (std::size_t)-1;

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

  container->nextRevision.measurementElementStartIndex = (std::size_t)-1;
  container->nextRevision.measurementElementEndIndex = (std::size_t)-1;

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

void Virtualizer::measureFirstRevisionDefault(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureFirstRevisionDefault outside of a revision");
  }

  std::vector<Element> nextElements;
  std::vector<std::size_t> nextElementsIndices;

  std::size_t measurementElementStartIndex = -1;
  std::size_t measurementElementEndIndex = -1;

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
    if (measurementElementStartIndex == (std::size_t)-1) {
      measurementElementStartIndex = nextElementIndex;
    }
    measurementElementEndIndex = nextElementIndex;

    if (container->nextRevision.measurementElementStartIndex == (std::size_t)-1 ||
        nextElementIndex < container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (container->nextRevision.measurementElementEndIndex == (std::size_t)-1 ||
        nextElementIndex > container->nextRevision.measurementElementEndIndex) {
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
    nextElementsIndices.push_back(nextElementIndex);

    /*
     * Stop measuring if we measured enough items to display in a visible window
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
    container->nextRevision.elements[nextElementsIndices[nextElementIndex]] = nextElements[nextElementIndex];
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
  std::vector<std::size_t> nextElementsIndices;

  std::size_t measurementElementStartIndex = -1;
  std::size_t measurementElementEndIndex = -1;

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
    if (measurementElementStartIndex == (std::size_t)-1) {
      measurementElementStartIndex = nextElementIndex;
    }
    measurementElementEndIndex = nextElementIndex;

    if (container->nextRevision.measurementElementStartIndex == (std::size_t)-1 ||
        nextElementIndex < container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (container->nextRevision.measurementElementEndIndex == (std::size_t)-1 ||
        nextElementIndex > container->nextRevision.measurementElementEndIndex) {
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
    nextElementsIndices.push_back(nextElementIndex);
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
    container->nextRevision.elements[nextElementsIndices[nextElementIndex]] = nextElements[nextElementIndex];
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
  std::vector<std::size_t> nextElementsIndices;

  std::size_t measurementElementStartIndex = -1;
  std::size_t measurementElementEndIndex = -1;

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
    if (measurementElementStartIndex == (std::size_t)-1) {
      measurementElementStartIndex = nextElementIndex;
    }
    measurementElementEndIndex = nextElementIndex;

    if (container->nextRevision.measurementElementStartIndex == (std::size_t)-1 ||
        nextElementIndex > container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (container->nextRevision.measurementElementEndIndex == (std::size_t)-1 ||
        nextElementIndex < container->nextRevision.measurementElementEndIndex) {
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
    nextElementsIndices.push_back(nextElementIndex);

    /*
     * Stop measuring if we measured enough items to display in a visible window
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
    container->nextRevision.elements[nextElementsIndices[nextElementIndex]] = nextElements[nextElementIndex];
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
  std::vector<std::size_t> nextElementsIndices;

  std::size_t measurementElementStartIndex = -1;
  std::size_t measurementElementEndIndex = -1;

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
    if (measurementElementStartIndex == (std::size_t)-1) {
      measurementElementStartIndex = nextElementIndex;
    }
    measurementElementEndIndex = nextElementIndex;

    if (container->nextRevision.measurementElementStartIndex == (std::size_t)-1 ||
        nextElementIndex > container->nextRevision.measurementElementStartIndex) {
      container->nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (container->nextRevision.measurementElementEndIndex == (std::size_t)-1 ||
        nextElementIndex < container->nextRevision.measurementElementEndIndex) {
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
    nextElementsIndices.push_back(nextElementIndex);
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
    container->nextRevision.elements[nextElementsIndices[nextElementIndex]] = nextElements[nextElementIndex];
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

void Virtualizer::addElementAtIndex(Container* container, std::size_t index, std::size_t prevElementIndex) {
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

  if (container->nextRevision.measurementElementStartIndex != (std::size_t)-1) {
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

void Virtualizer::updateElementAtIndex(Container* container, std::size_t index, Size size) {
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

void Virtualizer::prependElements(Container* container, std::size_t count) {
  for (std::size_t prevElementIndex = count; prevElementIndex-- > 0;) {
    addElementAtIndex(container, 0, prevElementIndex);
  }

  /*
   * Horizontal: total width is last element's right edge, height is max element height
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

void Virtualizer::appendElements(Container* container, std::size_t count) {
  for (std::size_t prevElementIndex = 0; prevElementIndex < count; prevElementIndex++) {
    std::size_t insertIndex = container->nextRevision.elements.size();
    addElementAtIndex(container, insertIndex);
  }

  /*
   * Horizontal: total width is last element's right edge, height is max element height
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
