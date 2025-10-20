#include <shadowlist-core/Virtualizer.hpp>
#include <algorithm>

namespace azimgd::shadowlist {

void Virtualizer::measureDefault(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureDefault outside of a revision");
  }

  Revision nextRevision;
  nextRevision.elements = container->nextRevision.elements;
  nextRevision.windowContainerWidth = container->nextRevision.windowContainerWidth;
  nextRevision.windowContainerHeight = container->nextRevision.windowContainerHeight;
  nextRevision.containerOffsetY = container->nextRevision.containerOffsetY;
  nextRevision.containerOffsetX = container->nextRevision.containerOffsetX;
  nextRevision.mvcpDiffHeight = container->nextRevision.mvcpDiffHeight;
  nextRevision.mvcpDiffWidth = container->nextRevision.mvcpDiffWidth;
  nextRevision.averageElementWidth = container->nextRevision.averageElementWidth;
  nextRevision.averageElementHeight = container->nextRevision.averageElementHeight;
  nextRevision.measurementElementStartIndex = (std::size_t)-1;
  nextRevision.measurementElementEndIndex = (std::size_t)-1;

  if (container->nextRevisionCount == RevisionCountFirst) {
    measureFirstRevisionDefault(container, nextRevision);
  } else {
    measureNextRevisionDefault(container, nextRevision);
  }

  /*
   * Calculate total container height and width by finding the maximum extent
   */
  double maxHeight = 0.0;
  double maxWidth = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < nextRevision.elements.size(); nextElementIndex++) {
    const Element& nextElement = nextRevision.elements[nextElementIndex];

    double elementOffsetTop = nextElement.offsetY + nextElement.height;
    double elementOffsetLeft = nextElement.offsetX + nextElement.width;

    if (elementOffsetTop > maxHeight) {
      maxHeight = elementOffsetTop;
    }
    if (elementOffsetLeft > maxWidth) {
      maxWidth = elementOffsetLeft;
    }
  }

  nextRevision.totalContainerHeight = maxHeight;
  nextRevision.totalContainerWidth = maxWidth;

  if (container->nextRevisionCount == RevisionCountFirst) {
    nextRevision.containerOffsetY = nextRevision.totalContainerHeight - nextRevision.windowContainerHeight;
    nextRevision.containerOffsetX = nextRevision.totalContainerWidth - nextRevision.windowContainerWidth;
  }

  container->nextRevision = nextRevision;
}

void Virtualizer::measureInverted(Container *container) {
  if (container->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use measureInverted outside of a revision");
  }

  Revision nextRevision;
  nextRevision.elements = container->nextRevision.elements;
  nextRevision.windowContainerWidth = container->nextRevision.windowContainerWidth;
  nextRevision.windowContainerHeight = container->nextRevision.windowContainerHeight;
  nextRevision.containerOffsetY = container->nextRevision.containerOffsetY;
  nextRevision.containerOffsetX = container->nextRevision.containerOffsetX;
  nextRevision.mvcpDiffHeight = container->nextRevision.mvcpDiffHeight;
  nextRevision.mvcpDiffWidth = container->nextRevision.mvcpDiffWidth;
  nextRevision.averageElementWidth = container->nextRevision.averageElementWidth;
  nextRevision.averageElementHeight = container->nextRevision.averageElementHeight;
  nextRevision.measurementElementStartIndex = (std::size_t)-1;
  nextRevision.measurementElementEndIndex = (std::size_t)-1;

  if (container->nextRevisionCount == RevisionCountFirst) {
    measureFirstRevisionInverted(container, nextRevision);
  } else {
    measureNextRevisionInverted(container, nextRevision);
  }

  /*
   * Calculate total container height and width by finding the maximum extent
   */
  double maxHeight = 0.0;
  double maxWidth = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < nextRevision.elements.size(); nextElementIndex++) {
    const Element& nextElement = nextRevision.elements[nextElementIndex];

    double elementOffsetTop = nextElement.offsetY + nextElement.height;
    double elementOffsetLeft = nextElement.offsetX + nextElement.width;

    if (elementOffsetTop > maxHeight) {
      maxHeight = elementOffsetTop;
    }
    if (elementOffsetLeft > maxWidth) {
      maxWidth = elementOffsetLeft;
    }
  }

  nextRevision.totalContainerHeight = maxHeight;
  nextRevision.totalContainerWidth = maxWidth;

  if (container->nextRevisionCount == RevisionCountFirst) {
    nextRevision.containerOffsetY = nextRevision.totalContainerHeight - nextRevision.windowContainerHeight;
    nextRevision.containerOffsetX = nextRevision.totalContainerWidth - nextRevision.windowContainerWidth;
  }

  container->nextRevision = nextRevision;
}

void Virtualizer::measureFirstRevisionDefault(Container *container, Revision &nextRevision) {
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
  for (std::size_t nextElementIndex = 0; nextElementIndex < nextRevision.elements.size(); ++nextElementIndex) {
    /*
     * Existing element, we don't want to modify it until revision is fully complete
     */
    const Element& prevElement = nextRevision.elements[nextElementIndex];

    /*
     * Let's create a new temporary element which will later be swapped with the original one
     */
    Element nextElement = prevElement;

    /*
     * Running measurement is expensive,
     * hence we don't want to re-measure if we already have a cached value
     */
    if (!prevElement.measured) {
      auto [width, height] = container->measurementCallback(nextElementIndex);

      /*
       * If callback returns {0, 0}, skip updating this element and mark as not measured
       */
      if (width == 0.0 && height == 0.0) {
        continue;
      }

      nextElement.width = width;
      nextElement.height = height;
      nextElement.measured = true;
    }

    /*
     * Keep track the range of measured element indices
     */
    if (measurementElementStartIndex == (std::size_t)-1) {
      measurementElementStartIndex = nextElementIndex;
    }
    measurementElementEndIndex = nextElementIndex;

    if (nextRevision.measurementElementStartIndex == (std::size_t)-1 ||
        nextElementIndex < nextRevision.measurementElementStartIndex) {
      nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (nextRevision.measurementElementEndIndex == (std::size_t)-1 ||
        nextElementIndex > nextRevision.measurementElementEndIndex) {
      nextRevision.measurementElementEndIndex = nextElementIndex;
    }

    accumulatedHeight += nextElement.height;
    accumulatedWidth += nextElement.width;

    nextRevision.measurementContainerHeight += nextElement.height;
    nextRevision.measurementElementCount++;

    /*
     * Create an intersection of newly measured elements
     */
    nextElements.push_back(nextElement);
    nextElementsIndices.push_back(nextElementIndex);

    /*
     * Stop measuring if we measured enough items to display in a visible window
     */
    if (!container->horizontal && accumulatedHeight >= nextRevision.windowContainerHeight) {
      break;
    }

    if (container->horizontal && accumulatedWidth >= nextRevision.windowContainerWidth) {
      break;
    }
  }

  /*
   * Calculate average element dimensions, needed for estimating unmeasured portion of the list
   */
  if (nextRevision.measurementElementCount > 0) {
    if (nextRevision.averageElementWidth == 0.0) {
      nextRevision.averageElementWidth = nextRevision.measurementContainerWidth / nextRevision.measurementElementCount;
    }
    if (nextRevision.averageElementHeight == 0.0) {
      nextRevision.averageElementHeight = nextRevision.measurementContainerHeight / nextRevision.measurementElementCount;
    }
  }

  /*
   * Swap elements from newly measured intersection with existing ones
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < nextElements.size(); nextElementIndex++) {
    nextRevision.elements[nextElementsIndices[nextElementIndex]] = nextElements[nextElementIndex];
  }

  /*
   * Adjust offsets for all elements in the list
   * For unmeasured elements, use average dimensions
   */
  double nextOffset = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < nextRevision.elements.size(); nextElementIndex++) {
    Element& nextElement = nextRevision.elements[nextElementIndex];

    if (!nextElement.measured) {
      nextElement.width = nextRevision.averageElementWidth;
      nextElement.height = nextRevision.averageElementHeight;
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

void Virtualizer::measureNextRevisionDefault(Container *container, Revision &nextRevision) {
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
  for (std::size_t nextElementIndex = 0; nextElementIndex < nextRevision.elements.size(); ++nextElementIndex) {
    /*
     * Existing element, we don't want to modify it until revision is fully complete
     */
    const Element& prevElement = nextRevision.elements[nextElementIndex];

    /*
     * Skip elements that are outside of the visible window plus 1x window buffer
     */
    double elementOffset = container->horizontal ? prevElement.offsetX : prevElement.offsetY;
    if (elementOffset < containerOffset - windowSize * 0.5 ||
      elementOffset > containerOffset + windowSize * 1.5) {
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
    if (!prevElement.measured) {
      auto [width, height] = container->measurementCallback(nextElementIndex);

      /*
       * If callback returns {0, 0}, skip updating this element and mark as not measured
       */
      if (width == 0.0 && height == 0.0) {
        continue;
      }

      nextElement.width = width;
      nextElement.height = height;
      nextElement.measured = true;
    }

    /*
     * Keep track the range of measured element indices
     */
    if (measurementElementStartIndex == (std::size_t)-1) {
      measurementElementStartIndex = nextElementIndex;
    }
    measurementElementEndIndex = nextElementIndex;

    if (nextRevision.measurementElementStartIndex == (std::size_t)-1 ||
        nextElementIndex < nextRevision.measurementElementStartIndex) {
      nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (nextRevision.measurementElementEndIndex == (std::size_t)-1 ||
        nextElementIndex > nextRevision.measurementElementEndIndex) {
      nextRevision.measurementElementEndIndex = nextElementIndex;
    }

    nextRevision.measurementContainerHeight += nextElement.height;
    nextRevision.measurementElementCount++;

    /*
     * Create an intersection of newly measured elements
     */
    nextElements.push_back(nextElement);
    nextElementsIndices.push_back(nextElementIndex);
  }

  /*
   * Calculate average element dimensions, needed for estimating unmeasured portion of the list
   */
  if (nextRevision.measurementElementCount > 0) {
    if (nextRevision.averageElementWidth == 0.0) {
      nextRevision.averageElementWidth = nextRevision.measurementContainerWidth / nextRevision.measurementElementCount;
    }
    if (nextRevision.averageElementHeight == 0.0) {
      nextRevision.averageElementHeight = nextRevision.measurementContainerHeight / nextRevision.measurementElementCount;
    }
  }

  /*
   * Swap elements from newly measured intersection with existing ones
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < nextElements.size(); nextElementIndex++) {
    nextRevision.elements[nextElementsIndices[nextElementIndex]] = nextElements[nextElementIndex];
  }

  /*
   * Adjust offsets for all elements in the list
   * For unmeasured elements, use average dimensions
   */
  double nextOffset = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < nextRevision.elements.size(); nextElementIndex++) {
    Element& nextElement = nextRevision.elements[nextElementIndex];

    if (!nextElement.measured) {
      nextElement.width = nextRevision.averageElementWidth;
      nextElement.height = nextRevision.averageElementHeight;
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

void Virtualizer::measureFirstRevisionInverted(Container *container, Revision &nextRevision) {
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
  for (std::size_t nextElementIndex = nextRevision.elements.size(); nextElementIndex-- > 0;) {
    /*
     * Existing element, we don't want to modify it until revision is fully complete
     */
    const Element& prevElement = nextRevision.elements[nextElementIndex];

    /*
     * Let's create a new temporary element which will later be swapped with the original one
     */
    Element nextElement = prevElement;

    /*
     * Running measurement is expensive,
     * hence we don't want to re-measure if we already have a cached value
     */
    if (!prevElement.measured) {
      auto [width, height] = container->measurementCallback(nextElementIndex);

      /*
       * If callback returns {0, 0}, skip updating this element and mark as not measured
       */
      if (width == 0.0 && height == 0.0) {
        continue;
      }

      nextElement.width = width;
      nextElement.height = height;
      nextElement.measured = true;
    }

    /*
     * Keep track the range of measured element indices
     */
    if (measurementElementStartIndex == (std::size_t)-1) {
      measurementElementStartIndex = nextElementIndex;
    }
    measurementElementEndIndex = nextElementIndex;

    if (nextRevision.measurementElementStartIndex == (std::size_t)-1 ||
        nextElementIndex > nextRevision.measurementElementStartIndex) {
      nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (nextRevision.measurementElementEndIndex == (std::size_t)-1 ||
        nextElementIndex < nextRevision.measurementElementEndIndex) {
      nextRevision.measurementElementEndIndex = nextElementIndex;
    }

    accumulatedHeight += nextElement.height;
    accumulatedWidth += nextElement.width;

    nextRevision.measurementContainerHeight += nextElement.height;
    nextRevision.measurementElementCount++;

    /*
     * Create an intersection of newly measured elements
     */
    nextElements.push_back(nextElement);
    nextElementsIndices.push_back(nextElementIndex);

    /*
     * Stop measuring if we measured enough items to display in a visible window
     */
    if (!container->horizontal && accumulatedHeight >= nextRevision.windowContainerHeight) {
      break;
    }

    if (container->horizontal && accumulatedWidth >= nextRevision.windowContainerWidth) {
      break;
    }
  }

  /*
   * Calculate average element dimensions, needed for estimating unmeasured portion of the list
   */
  if (nextRevision.measurementElementCount > 0 && nextRevision.measurementElementCount < 10) {
    if (nextRevision.averageElementWidth == 0.0) {
      nextRevision.averageElementWidth = nextRevision.measurementContainerWidth / nextRevision.measurementElementCount;
    }
    if (nextRevision.averageElementHeight == 0.0) {
      nextRevision.averageElementHeight = nextRevision.measurementContainerHeight / nextRevision.measurementElementCount;
    }
  }

  /*
   * Swap elements from newly measured intersection with existing ones
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < nextElements.size(); nextElementIndex++) {
    nextRevision.elements[nextElementsIndices[nextElementIndex]] = nextElements[nextElementIndex];
  }

  /*
   * Adjust offsets for all elements in the list
   * For unmeasured elements, use average dimensions
   */
  double nextOffset = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < nextRevision.elements.size(); nextElementIndex++) {
    Element& nextElement = nextRevision.elements[nextElementIndex];

    if (!nextElement.measured) {
      nextElement.width = nextRevision.averageElementWidth;
      nextElement.height = nextRevision.averageElementHeight;
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

void Virtualizer::measureNextRevisionInverted(Container *container, Revision &nextRevision) {
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
  for (std::size_t nextElementIndex = nextRevision.elements.size(); nextElementIndex-- > 0;) {
    /*
     * Existing element, we don't want to modify it until revision is fully complete
     */
    const Element& prevElement = nextRevision.elements[nextElementIndex];

    /*
     * Skip elements that are outside of the visible window plus 1x window buffer
     */
    double elementOffset = container->horizontal ? prevElement.offsetX : prevElement.offsetY;
    if (elementOffset < containerOffset - windowSize * 0.5 ||
      elementOffset > containerOffset + windowSize * 1.5) {
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
    if (!prevElement.measured) {
      auto [width, height] = container->measurementCallback(nextElementIndex);

      /*
       * If callback returns {0, 0}, skip updating this element and mark as not measured
       */
      if (width == 0.0 && height == 0.0) {
        continue;
      }

      nextElement.width = width;
      nextElement.height = height;
      nextElement.measured = true;
    }

    /*
     * Keep track the range of measured element indices
     */
    if (measurementElementStartIndex == (std::size_t)-1) {
      measurementElementStartIndex = nextElementIndex;
    }
    measurementElementEndIndex = nextElementIndex;

    if (nextRevision.measurementElementStartIndex == (std::size_t)-1 ||
        nextElementIndex > nextRevision.measurementElementStartIndex) {
      nextRevision.measurementElementStartIndex = nextElementIndex;
    }
    if (nextRevision.measurementElementEndIndex == (std::size_t)-1 ||
        nextElementIndex < nextRevision.measurementElementEndIndex) {
      nextRevision.measurementElementEndIndex = nextElementIndex;
    }

    nextRevision.measurementContainerHeight += nextElement.height;
    nextRevision.measurementElementCount++;

    /*
     * Create an intersection of newly measured elements
     */
    nextElements.push_back(nextElement);
    nextElementsIndices.push_back(nextElementIndex);
  }

  /*
   * Calculate average element dimensions, needed for estimating unmeasured portion of the list
   */
  if (nextRevision.measurementElementCount > 0 && nextRevision.measurementElementCount < 10) {
    if (nextRevision.averageElementWidth == 0.0) {
      nextRevision.averageElementWidth = nextRevision.measurementContainerWidth / nextRevision.measurementElementCount;
    }
    if (nextRevision.averageElementHeight == 0.0) {
      nextRevision.averageElementHeight = nextRevision.measurementContainerHeight / nextRevision.measurementElementCount;
    }
  }

  /*
   * Swap elements from newly measured intersection with existing ones
   */
  for (std::size_t nextElementIndex = 0; nextElementIndex < nextElements.size(); nextElementIndex++) {
    nextRevision.elements[nextElementsIndices[nextElementIndex]] = nextElements[nextElementIndex];
  }

  /*
   * Adjust offsets for all elements in the list
   * For unmeasured elements, use average dimensions
   */
  double nextOffset = 0.0;

  for (std::size_t nextElementIndex = 0; nextElementIndex < nextRevision.elements.size(); nextElementIndex++) {
    Element& nextElement = nextRevision.elements[nextElementIndex];

    if (!nextElement.measured) {
      nextElement.width = nextRevision.averageElementWidth;
      nextElement.height = nextRevision.averageElementHeight;
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

void Virtualizer::addElementAtIndex(Container* container, std::size_t index, Revision& nextRevision, std::size_t prevElementIndex) {
  if (index > container->nextRevision.elements.size()) {
    throw std::out_of_range("Index out of bounds");
  }

  Element nextElement;

  std::size_t nextElementIndex = (prevElementIndex == (std::size_t)-1) ? index : prevElementIndex;
  auto [width, height] = container->measurementCallback(nextElementIndex);

  /*
   * If callback returns {0, 0}, mark element as not measured and use average dimensions
   */
  if (width == 0.0 && height == 0.0) {
    nextElement.width = nextRevision.averageElementWidth;
    nextElement.height = nextRevision.averageElementHeight;
    nextElement.measured = false;
  } else {
    nextElement.width = width;
    nextElement.height = height;
    nextElement.measured = true;
  }
  nextElement.index = index;

  container->nextRevision.elements.insert(container->nextRevision.elements.begin() + index, nextElement);

  for (std::size_t nextElementIndex = index + 1; nextElementIndex < container->nextRevision.elements.size(); nextElementIndex++) {
    container->nextRevision.elements[nextElementIndex].index = nextElementIndex;
  }

  if (index < container->nextRevision.elements.size() - 1) {
    container->nextRevision.mvcpDiffWidth += (nextElement.width + nextElement.gapX);
    container->nextRevision.mvcpDiffHeight += (nextElement.height + nextElement.gapY);
  }

  if (nextRevision.measurementElementStartIndex != (std::size_t)-1) {
    if (index <= nextRevision.measurementElementStartIndex) {
      nextRevision.measurementElementStartIndex++;
    }

    if (index <= nextRevision.measurementElementEndIndex) {
      nextRevision.measurementElementEndIndex++;
    }
  }

  if (nextRevision.averageElementHeight > 0) {
    nextRevision.measurementContainerHeight += nextElement.height;
    nextRevision.measurementContainerWidth += nextElement.width;
    nextRevision.measurementElementCount++;
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

void Virtualizer::updateElementAtIndex(Container* container, std::size_t index, Revision& nextRevision, Size size) {
  if (index >= container->nextRevision.elements.size()) {
    throw std::out_of_range("Index out of bounds");
  }

  Element& nextElement = container->nextRevision.elements[index];

  double prevWidth = nextElement.width;
  double prevHeight = nextElement.height;

  nextElement.width = size.width;
  nextElement.height = size.height;
  nextElement.measured = true;

  double widthDiff = size.width - prevWidth;
  double heightDiff = size.height - prevHeight;

  double containerOffset = container->getContainerOffset();
  double elementOffset = container->horizontal ? nextElement.offsetX : nextElement.offsetY;

  if (elementOffset < containerOffset) {
    container->nextRevision.mvcpDiffWidth += widthDiff;
    container->nextRevision.mvcpDiffHeight += heightDiff;
  }

  if (nextRevision.averageElementHeight > 0) {
    nextRevision.measurementContainerHeight += heightDiff;
    nextRevision.measurementContainerWidth += widthDiff;
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

void Virtualizer::prependElements(Container* container, std::size_t count, Revision& nextRevision) {
  for (std::size_t prevElementIndex = count; prevElementIndex-- > 0;) {
    addElementAtIndex(container, 0, nextRevision, prevElementIndex);
  }
}

void Virtualizer::appendElements(Container* container, std::size_t count, Revision& nextRevision) {
  for (std::size_t prevElementIndex = 0; prevElementIndex < count; prevElementIndex++) {
    std::size_t insertIndex = container->nextRevision.elements.size();
    addElementAtIndex(container, insertIndex, nextRevision);
  }
}

}
