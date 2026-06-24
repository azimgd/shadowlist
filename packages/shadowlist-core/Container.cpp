#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Error.hpp>

#include <cmath>
#include <limits>

namespace azimgd::shadowlist {

namespace {
// Debug-only: the key at an emitted index, so JS and native logs correlate by content.
[[maybe_unused]] const char* debugKeyAt(const std::vector<Element>& elements, std::size_t index) {
  if (index < elements.size()) {
    return elements[index].key.empty() ? "(empty)" : elements[index].key.c_str();
  }
  return "(oob)";
}
}

void Container::startRevision() {
  if (this->revisionStatus != RevisionStatusIdle) {
    throw InvalidOperationError("Cannot start the new revision while the previous is in progress");
  }

  this->revisionStatus = RevisionStatusPending;
}

void Container::endRevision() {
  if (this->revisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("You cannot end the revision while the previous has not started");
  }

  if (this->revision.elements.empty()) {
    this->revisionCount = RevisionCountFirst;
  } else {
    this->revisionCount++;
  }
  this->revisionStatus = RevisionStatusIdle;

  double containerOffset = this->getContainerOffset();
  double windowSize = this->getWindowContainerSize();
  double totalSize = this->horizontal ? this->revision.totalContainerWidth : this->revision.totalContainerHeight;

  bool reachingLowEdge = containerOffset <= windowSize * this->startReachedThreshold;
  bool reachingHighEdge = containerOffset + windowSize >= totalSize - windowSize * this->endReachedThreshold;

  bool reachedEnd = this->inverted ? reachingLowEdge : reachingHighEdge;
  bool reachedStart = this->inverted ? reachingHighEdge : reachingLowEdge;

  // When both edges register (a list smaller than a window), prefer the end edge.
  if (reachedStart && reachedEnd) {
    reachedStart = false;
  }

  // Reset the edge callbacks when the data set changed: reaching the new edge is a
  // fresh arrival even if the offset never left the threshold band.
  std::size_t elementsSize = this->revision.elements.size();
  if (elementsSize != this->prevReachedElementsSize) {
    this->prevReachedElementsSize = elementsSize;
    this->prevReachedEnd = false;
    this->prevReachedStart = false;
  }

  // Fire once on arrival (false->true transition), not every frame within the band.
  if (this->endReachedEnabled && this->onEndReachedCallback && reachedEnd && !this->prevReachedEnd) {
    this->onEndReachedCallback();
  }

  if (this->startReachedEnabled && this->onStartReachedCallback && reachedStart && !this->prevReachedStart) {
    this->onStartReachedCallback();
  }

  this->prevReachedEnd = reachedEnd;
  this->prevReachedStart = reachedStart;

  this->dispatchObservers();
}

void Container::scrollToIndex(std::size_t index) {
  this->scrollToIndexTarget = index;
}

void Container::scrollToEnd() {
  this->pendingScrollToEnd = true;
}

void Container::requestScrollToIndex(double commandIndex, double commandSequence, int propIndex) {
  // The imperative command takes priority over the prop. Each call bumps a counter, and
  // we act whenever that counter changes, so requesting the same index twice still
  // scrolls both times.
  bool fired = false;
  if (commandSequence != this->prevScrollToIndexSequence) {
    this->prevScrollToIndexSequence = commandSequence;
    // scrollToEnd uses the same command channel as scrollToIndex. It is told apart by a
    // special reserved value (SCROLL_TO_END_INDEX) instead of a real index.
    if (commandIndex == SCROLL_TO_END_INDEX) {
      this->scrollToEnd();
      fired = true;
    } else if (commandIndex >= 0.0 && std::isfinite(commandIndex) &&
               commandIndex < static_cast<double>(std::numeric_limits<std::size_t>::max())) {
      this->scrollToIndex(static_cast<std::size_t>(commandIndex));
      fired = true;
    }
  }

  // The prop only takes effect when its value changes. A negative value means it is off.
  if (!fired && propIndex >= 0 && propIndex != this->prevScrollToIndexProp) {
    this->scrollToIndex(static_cast<std::size_t>(propIndex));
  }
  this->prevScrollToIndexProp = propIndex;
}

ContainerStateUpdate Container::resolveStateUpdate(
  double prevContainerOffsetX,
  double prevContainerOffsetY,
  double prevTotalContainerWidth,
  double prevTotalContainerHeight) const {
  ContainerStateUpdate update;

  bool corrected = this->containerOffsetCorrected;
  bool sizeChanged =
    this->revision.totalContainerWidth != prevTotalContainerWidth ||
    this->revision.totalContainerHeight != prevTotalContainerHeight;

  update.totalContainerWidth = this->revision.totalContainerWidth;
  update.totalContainerHeight = this->revision.totalContainerHeight;

  // Adopt the core's offset only when it wants to move the view; otherwise keep
  // the reported offset so we don't fight the user.
  if (corrected) {
    update.containerOffsetX = this->revision.containerOffsetX;
    update.containerOffsetY = this->revision.containerOffsetY;
  } else {
    update.containerOffsetX = prevContainerOffsetX;
    update.containerOffsetY = prevContainerOffsetY;
  }

  update.applyContainerOffset = corrected;
  update.changed = corrected || sizeChanged;

  // Publish the in-flight correction's token (its operation id) only on a frame that
  // actually applies an offset driven by an operation, so the host echoes it back and we
  // recognise our own write. A bare measurement nudge / layout reassert with no operation
  // publishes 0; the host classifies those by causality, not by token.
  update.commitToken = (corrected && this->operation) ? this->operation->id : 0;

  return update;
}

double Container::getFooterOffset(double footerSize) const {
  double totalSize = this->horizontal ? this->revision.totalContainerWidth : this->revision.totalContainerHeight;
  return totalSize - footerSize;
}

double Container::getStickyHeaderOffset() const {
  // Pin the header to the viewport start; otherwise it rests at the content start.
  if (this->stickyHeader) {
    // Clamp to the content start so overscroll (negative offset) doesn't drag it up.
    double offset = this->getContainerOffset();
    return offset > 0.0 ? offset : 0.0;
  }
  return 0.0;
}

double Container::getStickyFooterOffset(double footerSize) const {
  // Pin the footer to the viewport end; at the bottom it equals the resting position.
  if (this->stickyFooter) {
    return this->getContainerOffset() + this->getWindowContainerSize() - footerSize;
  }
  return this->getFooterOffset(footerSize);
}

std::vector<double> Container::getSnapOffsets() const {
  std::vector<double> snapOffsets;
  if (!this->snapToItem) {
    return snapOffsets;
  }

  std::size_t elementsSize = this->revision.elements.size();
  if (elementsSize == 0) {
    return snapOffsets;
  }

  double windowSize = this->getWindowContainerSize();
  double totalSize = this->horizontal ? this->revision.totalContainerWidth : this->revision.totalContainerHeight;
  double maxOffset = totalSize - windowSize;
  if (maxOffset < 0.0) {
    maxOffset = 0.0;
  }

  // One target per element, clamped to range. Offsets only increase, so deduping
  // consecutive equal values (the head/tail collapse to 0 / maxOffset) keeps the
  // list ascending and tidy.
  snapOffsets.reserve(elementsSize);
  for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
    const Element& nextElement = this->revision.elements[nextElementIndex];
    double elementOffset = this->horizontal ? nextElement.offsetX : nextElement.offsetY;
    double elementSize = this->horizontal ? nextElement.width : nextElement.height;

    double target;
    if (this->snapAlignment == 1) {
      target = elementOffset - (windowSize - elementSize) / 2.0;
    } else if (this->snapAlignment == 2) {
      target = elementOffset + elementSize - windowSize;
    } else {
      target = elementOffset;
    }

    if (target < 0.0) {
      target = 0.0;
    }
    if (target > maxOffset) {
      target = maxOffset;
    }

    if (snapOffsets.empty() || std::fabs(target - snapOffsets.back()) > OFFSET_MOVED_THRESHOLD) {
      snapOffsets.push_back(target);
    }
  }

  return snapOffsets;
}

StickyHeader Container::resolveStickyHeader() const {
  StickyHeader result;

  // Inverted sticky headers are unsupported; leave them resting.
  if (this->stickyIndices.empty() || this->inverted) {
    return result;
  }

  std::size_t elementsSize = this->revision.elements.size();

  // Clamp to the content start so overscroll (negative offset) doesn't drag it up.
  double offset = this->getContainerOffset();
  if (offset < 0.0) {
    offset = 0.0;
  }

  // Walk the ascending stickyIndices: the last header at/above the viewport start is
  // active (pinned), the first one past it is the "next" that pushes it up.
  double activeOffset = 0.0;
  double activeSize = 0.0;
  bool hasActive = false;
  double nextOffset = 0.0;
  bool hasNext = false;

  for (std::size_t stickyIndex : this->stickyIndices) {
    if (stickyIndex >= elementsSize) {
      continue;
    }

    double elementOffset = this->getElementOffset(stickyIndex);
    if (elementOffset <= offset) {
      result.index = stickyIndex;
      activeOffset = elementOffset;
      activeSize = this->getElementSize(stickyIndex);
      hasActive = true;
    } else {
      nextOffset = elementOffset;
      hasNext = true;
      break;
    }
  }

  if (!hasActive) {
    return result;
  }

  // The pinned header sits at the viewport start, unless the next header has scrolled
  // up close enough to push it out (pinned to nextOffset - own size for a clean swap).
  double displayedTop = offset;
  if (hasNext) {
    double pushedTop = nextOffset - activeSize;
    if (pushedTop < displayedTop) {
      displayedTop = pushedTop;
    }
  }

  result.translation = displayedTop - activeOffset;
  if (result.translation < 0.0) {
    result.translation = 0.0;
  }

  return result;
}

std::size_t Container::findElementIndexByKey(const std::string& key) const {
  if (key.empty()) {
    return UNDEFINED_INDEX;
  }

  // O(1) via the key->index map maintained in Virtualizer::reconcileElements.
  auto entry = this->revision.elementIndexByKey.find(key);
  return entry != this->revision.elementIndexByKey.end() ? entry->second : UNDEFINED_INDEX;
}

bool Container::isAnchorable(const std::string& key) const {
  if (key.empty()) {
    return false;
  }
  // Fast path: no policy set means every row is anchorable.
  return this->nonAnchorableKeys.empty() ||
    this->nonAnchorableKeys.find(key) == this->nonAnchorableKeys.end();
}

void Container::dispatchObservers() {
  auto visibleIndices = this->getVisibleIndices();
  if (this->onVisibleIndicesChangeCallback &&
    (visibleIndices.first != this->prevVisibleStartIndex || visibleIndices.second != this->prevVisibleEndIndex)) {
    SL_LOG("  emit onVisibleIndicesChange(%zd, %zd) keys=[%s..%s]",
      static_cast<std::ptrdiff_t>(visibleIndices.first), static_cast<std::ptrdiff_t>(visibleIndices.second),
      debugKeyAt(this->revision.elements, visibleIndices.first),
      debugKeyAt(this->revision.elements, visibleIndices.second));
    this->onVisibleIndicesChangeCallback(visibleIndices.first, visibleIndices.second);
  }
  this->prevVisibleStartIndex = visibleIndices.first;
  this->prevVisibleEndIndex = visibleIndices.second;

  // Notify when the viewable range changes. Computed only when a listener is
  // registered, since getViewableIndices does an O(window) overlap scan.
  if (this->onViewableIndicesChangeCallback) {
    auto viewableIndices = this->getViewableIndices();
    if (viewableIndices.first != this->prevViewableStartIndex || viewableIndices.second != this->prevViewableEndIndex) {
      SL_LOG("  emit onViewableIndicesChange(%zd, %zd)",
        static_cast<std::ptrdiff_t>(viewableIndices.first), static_cast<std::ptrdiff_t>(viewableIndices.second));
      this->onViewableIndicesChangeCallback(viewableIndices.first, viewableIndices.second);
    }
    this->prevViewableStartIndex = viewableIndices.first;
    this->prevViewableEndIndex = viewableIndices.second;
  }

  double containerOffsetX = this->revision.containerOffsetX;
  double containerOffsetY = this->revision.containerOffsetY;
  if (this->onScrollCallback &&
    (!this->prevContainerOffsetValid || containerOffsetX != this->prevContainerOffsetX || containerOffsetY != this->prevContainerOffsetY)) {
    this->onScrollCallback(containerOffsetX, containerOffsetY);
  }
  this->prevContainerOffsetX = containerOffsetX;
  this->prevContainerOffsetY = containerOffsetY;
  this->prevContainerOffsetValid = true;
}

const Element& Container::getElementAtIndex(std::size_t index) const {
  if (index >= this->revision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  return this->revision.elements[index];
}

std::size_t Container::getElementsSize() const {
  return this->revision.elements.size();
}

void Container::setWindowContainerHeight(double height) {
  if (this->revisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use setWindowContainerHeight outside of a revision");
  }

  this->revision.setWindowContainerHeight(height);
}

void Container::setWindowContainerWidth(double width) {
  if (this->revisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use setWindowContainerWidth outside of a revision");
  }

  this->revision.setWindowContainerWidth(width);
}

void Container::setContainerOffsetY(double offsetY) {
  if (this->revisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use setContainerOffsetY outside of a revision");
  }

  this->revision.setContainerOffsetY(offsetY);
}

void Container::setContainerOffsetX(double offsetX) {
  if (this->revisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use setContainerOffsetX outside of a revision");
  }

  this->revision.setContainerOffsetX(offsetX);
}

std::string Container::getDebugRepresentation() const {
  return this->revision.getDebugRepresentation();
}

std::pair<std::size_t, std::size_t> Container::getVisibleIndices() const {
  std::size_t startIndex = this->revision.measurementElementStartIndex;
  std::size_t endIndex = this->revision.measurementElementEndIndex;

  if (startIndex != UNDEFINED_INDEX && endIndex != UNDEFINED_INDEX) {
    return {startIndex, endIndex};
  }

  return {UNDEFINED_INDEX, UNDEFINED_INDEX};
}

std::pair<std::size_t, std::size_t> Container::getViewableIndices() const {
  std::size_t measuredStartIndex = this->revision.measurementElementStartIndex;
  std::size_t measuredEndIndex = this->revision.measurementElementEndIndex;

  if (measuredStartIndex == UNDEFINED_INDEX || measuredEndIndex == UNDEFINED_INDEX) {
    return {UNDEFINED_INDEX, UNDEFINED_INDEX};
  }

  double viewportStart = this->getContainerOffset();
  double windowSize = this->getWindowContainerSize();
  double viewportEnd = viewportStart + windowSize;

  // Inverted lists store the window start>end; normalise to an ascending window.
  std::size_t windowLow = this->inverted ? measuredEndIndex : measuredStartIndex;
  std::size_t windowHigh = this->inverted ? measuredStartIndex : measuredEndIndex;

  std::size_t firstViewable = UNDEFINED_INDEX;
  std::size_t lastViewable = UNDEFINED_INDEX;

  for (std::size_t nextElementIndex = windowLow; nextElementIndex <= windowHigh && nextElementIndex < this->revision.elements.size(); ++nextElementIndex) {
    const Element& nextElement = this->revision.elements[nextElementIndex];
    double elementStart = this->horizontal ? nextElement.offsetX : nextElement.offsetY;
    double elementSize = this->horizontal ? nextElement.width : nextElement.height;
    if (elementSize <= 0.0) {
      continue;
    }
    double elementEnd = elementStart + elementSize;

    double overlapStart = elementStart > viewportStart ? elementStart : viewportStart;
    double overlapEnd = elementEnd < viewportEnd ? elementEnd : viewportEnd;
    double visible = overlapEnd - overlapStart;

    // Measure the visible fraction against min(element, viewport) so an element
    // taller than the viewport can still reach 1.0 by fully covering the screen.
    double referenceSize = elementSize < windowSize ? elementSize : windowSize;
    if (visible > 0.0 && referenceSize > 0.0 && (visible / referenceSize) >= this->viewablePercentThreshold) {
      if (firstViewable == UNDEFINED_INDEX) {
        firstViewable = nextElementIndex;
      }
      lastViewable = nextElementIndex;
    }
  }

  if (firstViewable == UNDEFINED_INDEX) {
    return {UNDEFINED_INDEX, UNDEFINED_INDEX};
  }

  // Match getVisibleIndices: inverted reports the higher index first.
  if (this->inverted) {
    return {lastViewable, firstViewable};
  }
  return {firstViewable, lastViewable};
}

double Container::getElementOffset(std::size_t index) const {
  if (index >= this->revision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  const Element& nextElement = this->revision.elements[index];
  return this->horizontal ? nextElement.offsetX : nextElement.offsetY;
}

double Container::getElementSize(std::size_t index) const {
  if (index >= this->revision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  const Element& nextElement = this->revision.elements[index];
  return this->horizontal ? nextElement.width : nextElement.height;
}

void Container::setElementOffset(std::size_t index, double offset) {
  if (this->revisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("Cannot use setElementOffset outside of a revision");
  }

  if (index >= this->revision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  Element& nextElement = this->revision.elements[index];
  if (this->horizontal) {
    nextElement.offsetX = offset;
  } else {
    nextElement.offsetY = offset;
  }
}

double Container::getContainerOffset() const {
  return this->horizontal ? this->revision.containerOffsetX : this->revision.containerOffsetY;
}

double Container::getWindowContainerSize() const {
  return this->horizontal ? this->revision.windowContainerWidth : this->revision.windowContainerHeight;
}

void Container::setEndReachedEnabled(bool enabled) {
  this->endReachedEnabled = enabled;
}

void Container::setStartReachedEnabled(bool enabled) {
  this->startReachedEnabled = enabled;
}

}
