#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Observer.hpp>
#include <shadowlist-core/Error.hpp>

namespace azimgd::shadowlist {

void Container::startRevision() {
  /*
   * Revisions must be in the idle status before we start
   */
  if (this->nextRevisionStatus != RevisionStatusIdle) {
    throw InvalidOperationError("Cannot start the new revision while the previous is in progress");
  }

  this->nextRevisionStatus = RevisionStatusPending;
}

void Container::endRevision() {
  /*
   * Revisions must be in the idle status before we start
   */
  if (this->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("You cannot end the revision while the previous has not started");
  }

  this->nextRevisionCount++;
  this->nextRevisionStatus = RevisionStatusIdle;

  /*
   * Notify observer about revision end
   */
  if (this->observer) {
    this->observer->notifyEndRevision();
  }

  /*
   * Check whether we're within one screen size of either end of the list.
   * For inverted lists the start/end are visually swapped.
   */
  double containerOffset = this->getContainerOffset();
  double windowSize = this->getWindowContainerSize();
  double totalSize = this->horizontal ? this->nextRevision.totalContainerWidth : this->nextRevision.totalContainerHeight;

  bool nearLowEdge = containerOffset <= windowSize;
  bool nearHighEdge = containerOffset + windowSize >= totalSize - windowSize;

  bool reachedEnd = this->inverted ? nearLowEdge : nearHighEdge;
  bool reachedStart = this->inverted ? nearHighEdge : nearLowEdge;

  /*
   * When the whole list fits within (or near) a single window both edges are
   * technically reached; prefer the end callback so we don't double-trigger
   */
  if (reachedStart && reachedEnd) {
    reachedStart = false;
  }

  if (this->endReachedEnabled && this->onEndReachedCallback && reachedEnd) {
    this->onEndReachedCallback();
  }

  if (this->startReachedEnabled && this->onStartReachedCallback && reachedStart) {
    this->onStartReachedCallback();
  }

  this->dispatchObservers();
}

void Container::scrollToIndex(std::size_t index) {
  this->scrollToIndexTarget = index;
}

std::size_t Container::findElementIndexByKey(const std::string& key) const {
  if (key.empty()) {
    return UNDEFINED_INDEX;
  }

  for (std::size_t nextElementIndex = 0; nextElementIndex < this->nextRevision.elements.size(); nextElementIndex++) {
    if (this->nextRevision.elements[nextElementIndex].key == key) {
      return nextElementIndex;
    }
  }

  return UNDEFINED_INDEX;
}

void Container::dispatchObservers() {
  /*
   * Notify when the visible index range changes
   */
  auto visibleIndices = this->getVisibleIndices();
  if (this->onVisibleIndicesChangeCallback &&
    (visibleIndices.first != this->prevVisibleStartIndex || visibleIndices.second != this->prevVisibleEndIndex)) {
    this->onVisibleIndicesChangeCallback(visibleIndices.first, visibleIndices.second);
  }
  this->prevVisibleStartIndex = visibleIndices.first;
  this->prevVisibleEndIndex = visibleIndices.second;

  /*
   * Notify when the scroll offset changes
   */
  double containerOffsetX = this->nextRevision.containerOffsetX;
  double containerOffsetY = this->nextRevision.containerOffsetY;
  if (this->onScrollCallback &&
    (!this->prevContainerOffsetValid || containerOffsetX != this->prevContainerOffsetX || containerOffsetY != this->prevContainerOffsetY)) {
    this->onScrollCallback(containerOffsetX, containerOffsetY);
  }
  this->prevContainerOffsetX = containerOffsetX;
  this->prevContainerOffsetY = containerOffsetY;
  this->prevContainerOffsetValid = true;
}

void Container::addElementAtIndex(std::size_t index, Element nextElement) {
  if (this->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot add element outside of a revision");
  }

  if (index > this->nextRevision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  nextElement.estimated = false;
  nextElement.width = 0;
  nextElement.height = 0;
  nextElement.offsetY = 0;
  nextElement.offsetX = 0;
  nextElement.index = index;

  auto it = this->nextRevision.elements.begin() + index;
  this->nextRevision.elements.insert(it, nextElement);

  // Update indices of all elements after the inserted element
  for (std::size_t nextElementIndex = index + 1; nextElementIndex < this->nextRevision.elements.size(); nextElementIndex++) {
    this->nextRevision.elements[nextElementIndex].index = nextElementIndex;
  }
}

void Container::removeElementAtIndex(std::size_t index) {
  if (this->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot remove element outside of a revision");
  }

  if (index >= this->nextRevision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  auto it = this->nextRevision.elements.begin() + index;
  this->nextRevision.elements.erase(it);

  // Update indices of all elements after the removed element
  for (std::size_t nextElementIndex = index; nextElementIndex < this->nextRevision.elements.size(); nextElementIndex++) {
    this->nextRevision.elements[nextElementIndex].index = nextElementIndex;
  }
}

const Element Container::getElementAtIndex(std::size_t index) const {
  if (index >= this->nextRevision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  return this->nextRevision.elements[index];
}

void Container::resizeElementsTail(std::size_t size) {
  std::size_t prevElementsSize = this->nextRevision.elements.size();
  this->nextRevision.elements.resize(size);

  for (std::size_t nextElementIndex = prevElementsSize; nextElementIndex < size; nextElementIndex++) {
    this->nextRevision.elements[nextElementIndex].index = nextElementIndex;
  }
}

std::size_t Container::getElementsSize() const {
  return this->nextRevision.elements.size();
}

void Container::setWindowContainerHeight(double height) {
  if (this->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use setWindowContainerHeight outside of a revision");
  }

  this->nextRevision.setWindowContainerHeight(height);
}

void Container::setWindowContainerWidth(double width) {
  if (this->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use setWindowContainerWidth outside of a revision");
  }

  this->nextRevision.setWindowContainerWidth(width);
}

void Container::setContainerOffsetY(double offsetY) {
  if (this->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use setContainerOffsetY outside of a revision");
  }

  this->nextRevision.setContainerOffsetY(offsetY);
}

void Container::setContainerOffsetX(double offsetX) {
  if (this->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use setContainerOffsetX outside of a revision");
  }

  this->nextRevision.setContainerOffsetX(offsetX);
}

std::size_t Container::getMeasurementElementStartIndex() const {
  return this->nextRevision.measurementElementStartIndex;
}

std::size_t Container::getMeasurementElementEndIndex() const {
  return this->nextRevision.measurementElementEndIndex;
}

std::string Container::getDebugRepresentation() const {
  return this->nextRevision.getDebugRepresentation();
}

std::vector<Element> Container::getVisibleElements() const {
  std::vector<Element> visibleElements;

  std::size_t measurementElementStartIndex = this->nextRevision.measurementElementStartIndex;
  std::size_t measurementElementEndIndex = this->nextRevision.measurementElementEndIndex;

  /*
   * Skip if uninitialized
   */
  if (measurementElementStartIndex == UNDEFINED_INDEX || measurementElementEndIndex == UNDEFINED_INDEX) {
    return visibleElements;
  }

  /*
   * For inverted lists, start > end (e.g., 99 to 90)
   * For default lists, start < end (e.g., 0 to 9)
   */
  if (this->inverted) {
    /*
     * Iterate from high index to low index
     */
    for (std::size_t visibleElementIndex = measurementElementStartIndex; visibleElementIndex >= measurementElementEndIndex && visibleElementIndex != UNDEFINED_INDEX; --visibleElementIndex) {
      if (visibleElementIndex < this->nextRevision.elements.size()) {
        visibleElements.push_back(this->nextRevision.elements[visibleElementIndex]);
      }
    }
  } else {
    /*
     * Iterate from low index to high index
     */
    for (std::size_t visibleElementIndex = measurementElementStartIndex; visibleElementIndex <= measurementElementEndIndex; ++visibleElementIndex) {
      if (visibleElementIndex < this->nextRevision.elements.size()) {
        visibleElements.push_back(this->nextRevision.elements[visibleElementIndex]);
      }
    }
  }

  return visibleElements;
}

std::pair<std::size_t, std::size_t> Container::getVisibleIndices() const {
  std::size_t startIndex = this->nextRevision.measurementElementStartIndex;
  std::size_t endIndex = this->nextRevision.measurementElementEndIndex;

  /*
   * Return the visible index range, or (-1, -1) if uninitialized
   */
  if (startIndex != UNDEFINED_INDEX && endIndex != UNDEFINED_INDEX) {
    return {startIndex, endIndex};
  }

  return {UNDEFINED_INDEX, UNDEFINED_INDEX};
}

bool Container::getElementVisible(std::size_t index) const {
  std::size_t measurementElementStartIndex = this->nextRevision.measurementElementStartIndex;
  std::size_t measurementElementEndIndex = this->nextRevision.measurementElementEndIndex;

  /*
   * Skip if uninitialized
   */
  if (measurementElementStartIndex == UNDEFINED_INDEX || measurementElementEndIndex == UNDEFINED_INDEX) {
    return false;
  }

  /*
   * For inverted lists, we iterate from end to start (high index to low index)
   * so measurementElementStartIndex > measurementElementEndIndex. For example: measurementElementStartIndex=99, measurementElementEndIndex=90
   *
   * For default lists, we iterate from start to end (low index to high index)
   * so measurementElementStartIndex < measurementElementEndIndex. For example: measurementElementStartIndex=0, measurementElementEndIndex=9
   */
  if (this->inverted) {
    return (index >= measurementElementEndIndex && index <= measurementElementStartIndex);
  } else {
    return (index >= measurementElementStartIndex && index <= measurementElementEndIndex);
  }
}

double Container::getElementOffset(std::size_t index) const {
  if (index >= this->nextRevision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  const Element& nextElement = this->nextRevision.elements[index];
  return this->horizontal ? nextElement.offsetX : nextElement.offsetY;
}

double Container::getElementSize(std::size_t index) const {
  if (index >= this->nextRevision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  const Element& nextElement = this->nextRevision.elements[index];
  return this->horizontal ? nextElement.width : nextElement.height;
}

void Container::setElementOffset(std::size_t index, double offset) {
  if (this->nextRevisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use setElementOffset outside of a revision");
  }

  if (index >= this->nextRevision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  Element& nextElement = this->nextRevision.elements[index];
  if (this->horizontal) {
    nextElement.offsetX = offset;
  } else {
    nextElement.offsetY = offset;
  }
}

double Container::getContainerOffset() const {
  return this->horizontal ? this->nextRevision.containerOffsetX : this->nextRevision.containerOffsetY;
}

double Container::getWindowContainerSize() const {
  return this->horizontal ? this->nextRevision.windowContainerWidth : this->nextRevision.windowContainerHeight;
}

void Container::toggleEndReached(bool enabled) {
  this->endReachedEnabled = enabled;
}

void Container::toggleStartReached(bool enabled) {
  this->startReachedEnabled = enabled;
}

void Container::setObserver(Observer* observer) {
  this->observer = observer;
}

Observer* Container::getObserver() const {
  return this->observer;
}

}
