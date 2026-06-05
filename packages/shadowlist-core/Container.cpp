#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Error.hpp>

namespace azimgd::shadowlist {

void Container::startRevision() {
  /*
   * Revisions must be in the idle status before we start
   */
  if (this->revisionStatus != RevisionStatusIdle) {
    throw InvalidOperationError("Cannot start the new revision while the previous is in progress");
  }

  this->revisionStatus = RevisionStatusPending;
}

void Container::endRevision() {
  /*
   * A revision must be in progress (pending) before it can end
   */
  if (this->revisionStatus != RevisionStatusPending) {
    throw InvalidOperationError("You cannot end the revision while the previous has not started");
  }

  this->revisionCount++;
  this->revisionStatus = RevisionStatusIdle;

  /*
   * Check whether we're within one screen size of either end of the list.
   * For inverted lists the start/end are visually swapped.
   */
  double containerOffset = this->getContainerOffset();
  double windowSize = this->getWindowContainerSize();
  double totalSize = this->horizontal ? this->revision.totalContainerWidth : this->revision.totalContainerHeight;

  bool nearLowEdge = containerOffset <= windowSize * this->startReachedThreshold;
  bool nearHighEdge = containerOffset + windowSize >= totalSize - windowSize * this->endReachedThreshold;

  bool reachedEnd = this->inverted ? nearLowEdge : nearHighEdge;
  bool reachedStart = this->inverted ? nearHighEdge : nearLowEdge;

  /*
   * When the whole list fits within (or near) a single window both edges are
   * technically reached; prefer the end callback so we don't double-trigger
   */
  if (reachedStart && reachedEnd) {
    reachedStart = false;
  }

  /*
   * Re-arm the edge callbacks when the data set changed (e.g. pagination appended a
   * new tail or prepended a head): reaching the new edge is a fresh arrival even
   * when the scroll offset never left the threshold band. Without this a page
   * smaller than endReachedThreshold * window keeps the user "reached" the whole
   * time, so onEndReached would never fire again and infinite scroll would stall.
   */
  std::size_t elementsSize = this->revision.elements.size();
  if (elementsSize != this->prevReachedElementsSize) {
    this->prevReachedElementsSize = elementsSize;
    this->prevReachedEnd = false;
    this->prevReachedStart = false;
  }

  /*
   * Fire once on arrival at an edge (a false->true transition), not every frame
   * the offset stays within the threshold band - otherwise an onEndReached that
   * paginates would re-fire continuously while near the edge. The flag re-arms
   * when the view leaves the band (or the data set changes, above), so reaching the
   * (new) edge again re-triggers.
   */
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

void Container::requestScrollToIndex(double commandIndex, double commandNonce, int propIndex) {
  /*
   * The imperative command takes precedence over the declarative prop. It fires
   * once per invocation: the integration bumps the nonce on every scrollToIndex
   * call, so requesting the same index again still re-scrolls (a value-only dedup
   * would swallow a repeat scroll to the same index).
   */
  bool fired = false;
  if (commandNonce != this->prevScrollToIndexNonce) {
    this->prevScrollToIndexNonce = commandNonce;
    /*
     * scrollToEnd rides the same nonce channel as scrollToIndex, distinguished by
     * the SCROLL_TO_END_INDEX sentinel, so the integrations need no extra state field.
     */
    if (commandIndex == SCROLL_TO_END_INDEX) {
      this->scrollToEnd();
      fired = true;
    } else if (commandIndex >= 0.0) {
      this->scrollToIndex(static_cast<std::size_t>(commandIndex));
      fired = true;
    }
  }

  /*
   * The declarative prop fires only when its value changes (a negative value is
   * inactive), so it provides an initial position without re-firing every frame.
   */
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

  /*
   * Only adopt the core's scroll offset when the core actually wants to move the
   * view; otherwise keep the offset the view reported so we don't fight the user
   */
  if (corrected) {
    update.containerOffsetX = this->revision.containerOffsetX;
    update.containerOffsetY = this->revision.containerOffsetY;
  } else {
    update.containerOffsetX = prevContainerOffsetX;
    update.containerOffsetY = prevContainerOffsetY;
  }

  update.applyContainerOffset = corrected;
  update.changed = corrected || sizeChanged;

  return update;
}

double Container::getFooterOffset(double footerSize) const {
  double totalSize = this->horizontal ? this->revision.totalContainerWidth : this->revision.totalContainerHeight;
  return totalSize - footerSize;
}

double Container::getStickyHeaderOffset() const {
  /*
   * Pin the header to the viewport start by tracking the scroll offset; otherwise
   * it rests at the content start (0).
   */
  if (this->stickyHeader) {
    /*
     * Clamp to the content start so rubber-band / bounce overscroll (a negative
     * offset on iOS/web) does not drag the pinned header above the viewport edge.
     */
    double offset = this->getContainerOffset();
    return offset > 0.0 ? offset : 0.0;
  }
  return 0.0;
}

double Container::getStickyFooterOffset(double footerSize) const {
  /*
   * Pin the footer to the viewport end by tracking the scroll offset. At the
   * bottom of the list (offset == totalSize - window) this equals the resting
   * position, so the footer settles seamlessly onto its reserved space.
   */
  if (this->stickyFooter) {
    return this->getContainerOffset() + this->getWindowContainerSize() - footerSize;
  }
  return this->getFooterOffset(footerSize);
}

StickyHeader Container::resolveStickyHeader() const {
  StickyHeader result;

  /*
   * Inverted sticky section headers (pin to the viewport end) are an exotic
   * combination; leave them resting so we never pin them to the wrong edge.
   */
  if (this->stickyIndices.empty() || this->inverted) {
    return result;
  }

  std::size_t elementsSize = this->revision.elements.size();

  /*
   * Clamp the offset to the content start so rubber-band / bounce overscroll (a
   * negative offset on iOS/web) does not drag the pinned header above its resting
   * position.
   */
  double offset = this->getContainerOffset();
  if (offset < 0.0) {
    offset = 0.0;
  }

  /*
   * stickyIndices is ascending, so element offsets along the scroll axis are too:
   * walk while the resting offset is at/above the viewport start to find the active
   * (pinned) header, and keep the first one past it as the "next" that pushes it up.
   */
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

  /*
   * The pinned header sits at the viewport start (offset), unless the next sticky
   * header has scrolled up close enough to push it out: then its displayed top is
   * pinned to the next header's top minus its own size, so the two swap seamlessly.
   */
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

  for (std::size_t nextElementIndex = 0; nextElementIndex < this->revision.elements.size(); nextElementIndex++) {
    if (this->revision.elements[nextElementIndex].key == key) {
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
    SL_LOG("  emit onVisibleIndicesChange(%zd, %zd)",
      static_cast<std::ptrdiff_t>(visibleIndices.first), static_cast<std::ptrdiff_t>(visibleIndices.second));
    this->onVisibleIndicesChangeCallback(visibleIndices.first, visibleIndices.second);
  }
  this->prevVisibleStartIndex = visibleIndices.first;
  this->prevVisibleEndIndex = visibleIndices.second;

  /*
   * Notify when the strictly-viewable element range changes. Only computed when a
   * listener is registered - getViewableIndices does an O(window) overlap scan, so
   * there is no reason to pay for it on every frame when nothing consumes it.
   */
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

  /*
   * Notify when the scroll offset changes
   */
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

  /*
   * Return the visible index range, or (-1, -1) if uninitialized
   */
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

  /*
   * The measured window is stored start>end for inverted lists; normalise it to an
   * ascending [lo, hi] scan so the viewability test reads the same either way.
   */
  std::size_t lo = this->inverted ? measuredEndIndex : measuredStartIndex;
  std::size_t hi = this->inverted ? measuredStartIndex : measuredEndIndex;

  std::size_t firstViewable = UNDEFINED_INDEX;
  std::size_t lastViewable = UNDEFINED_INDEX;

  for (std::size_t nextElementIndex = lo; nextElementIndex <= hi && nextElementIndex < this->revision.elements.size(); ++nextElementIndex) {
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

    /*
     * Measure the visible fraction against min(element, viewport): an element
     * taller than the viewport can never reach 100% of itself, so without this it
     * would never count as viewable at a high threshold even when it fills the
     * screen. Clamping the denominator makes "fully covers the viewport" == 1.0.
     */
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

  /*
   * Match the orientation convention of getVisibleIndices: inverted reports the
   * higher index first.
   */
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
