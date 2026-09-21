#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Error.hpp>

#include <algorithm>
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

void Container::endRevision() {
  if (this->revision.elements.empty()) {
    this->revisionCount = REVISION_COUNT_FIRST;
  } else {
    this->revisionCount++;
  }

  double containerOffset = this->getContainerOffset();
  double windowSize = this->getWindowContainerSize();
  double totalSize = this->horizontal ? this->revision.totalContainerWidth : this->revision.totalContainerHeight;

  bool reachingLowEdge = containerOffset <= windowSize * this->startReachedThreshold;
  bool reachingHighEdge = containerOffset + windowSize >= totalSize - windowSize * this->endReachedThreshold;

  /*
   * Start is element 0 and end is the last element, in every orientation. `inverted` only
   * pins the resting position to the end; it does not flip the data order, so the start
   * edge stays at offset 0 and the end edge at the end of the content. Swapping the edges
   * for inverted lists (FlatList's convention, where inverting also reverses the render
   * order) would make onStartReached fire at the bottom of an inverted chat: every prepend
   * it triggers changes the element count, which re-arms the edge below, so the callback
   * would fire again on the next frame and "load earlier" would run in an endless loop.
   */
  bool reachedEnd = reachingHighEdge;
  bool reachedStart = reachingLowEdge;

  // When both edges register (a list smaller than a window), prefer the end edge.
  if (reachedStart && reachedEnd) {
    reachedStart = false;
  }

  /*
   * Reset the edge callbacks when the data set changed: reaching the new edge is a
   * fresh arrival even if the offset never left the threshold band.
   */
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

void Container::scrollToIndex(std::size_t index, double viewPosition) {
  this->scrollToIndexTarget = index;
  this->scrollToIndexViewPosition = viewPosition;
}

void Container::scrollToEnd() {
  this->pendingScrollToEnd = true;
}

void Container::scrollToStart() {
  this->pendingScrollToStart = true;
  this->pendingScrollToEnd = false;
  this->scrollToIndexTarget = UNDEFINED_INDEX;
}

void Container::requestScrollToIndex(double commandIndex, double commandSequence, int propIndex, double commandViewPosition) {
  /*
   * The imperative command takes priority over the prop. Each call bumps a counter, and
   * we act whenever that counter changes, so requesting the same index twice still
   * scrolls both times.
   */
  bool fired = false;
  if (commandSequence != this->prevScrollToIndexSequence) {
    this->prevScrollToIndexSequence = commandSequence;
    /*
     * scrollToEnd uses the same command channel as scrollToIndex. It is told apart by a
     * special reserved value (SCROLL_TO_END_INDEX) instead of a real index.
     */
    if (commandIndex == SCROLL_TO_END_INDEX) {
      this->scrollToEnd();
      fired = true;
    } else if (commandIndex >= 0.0 && std::isfinite(commandIndex) &&
               commandIndex < static_cast<double>(std::numeric_limits<std::size_t>::max())) {
      // A non-finite fraction carries no position at all and means the start edge;
      // anything else is clamped into [0, 1], the range that keeps the row on screen.
      double viewPosition = std::isfinite(commandViewPosition)
        ? std::min(1.0, std::max(0.0, commandViewPosition))
        : 0.0;
      this->scrollToIndex(static_cast<std::size_t>(commandIndex), viewPosition);
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

  /*
   * Adopt the core's offset only when it wants to move the view; otherwise keep
   * the reported offset so we don't fight the user.
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

  /*
   * Publish the in-flight correction's token (its operation id) only on a frame that
   * actually applies an offset driven by an operation, so the host echoes it back and we
   * recognise our own write. A bare measurement nudge / layout reassert with no operation
   * publishes 0; the host classifies those by causality, not by token.
   */
  update.commitToken = (corrected && this->operation) ? this->operation->id : 0;

  return update;
}

double Container::getFooterOffset(double footerSize) const {
  double totalSize = this->horizontal ? this->revision.totalContainerWidth : this->revision.totalContainerHeight;
  return totalSize - footerSize;
}

const std::vector<double>& Container::getSnapOffsets() const {
  double windowSize = this->getWindowContainerSize();
  double totalSize = this->horizontal ? this->revision.totalContainerWidth : this->revision.totalContainerHeight;

  /*
   * Snap targets derive from element geometry plus these scalars, and from nothing that
   * changes on a plain scroll. Rebuild only when one of them moved; the layout pass calls
   * this on every published frame.
   */
  if (this->snapCacheVersion == this->geometryVersion &&
      this->snapCacheSnapToItem == this->snapToItem &&
      this->snapCacheAlignment == this->snapAlignment &&
      this->snapCacheWindowSize == windowSize &&
      this->snapCacheTotalSize == totalSize &&
      this->snapCacheHorizontal == this->horizontal) {
    return this->snapOffsetsCache;
  }

  this->snapCacheVersion = this->geometryVersion;
  this->snapCacheSnapToItem = this->snapToItem;
  this->snapCacheAlignment = this->snapAlignment;
  this->snapCacheWindowSize = windowSize;
  this->snapCacheTotalSize = totalSize;
  this->snapCacheHorizontal = this->horizontal;

  std::vector<double>& snapOffsets = this->snapOffsetsCache;
  snapOffsets.clear();

  if (!this->snapToItem) {
    return snapOffsets;
  }

  std::size_t elementsSize = this->revision.elements.size();
  if (elementsSize == 0) {
    return snapOffsets;
  }

  double maxOffset = totalSize - windowSize;
  if (maxOffset < 0.0) {
    maxOffset = 0.0;
  }

  /*
   * One target per element, clamped to range. Offsets only increase, so deduping
   * consecutive equal values (the head/tail collapse to 0 / maxOffset) keeps the
   * list ascending and tidy.
   */
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

std::size_t Container::findElementIndexByKey(const std::string& key) const {
  if (key.empty()) {
    return UNDEFINED_INDEX;
  }

  // O(1) via the key->index map maintained in Virtualizer::reconcileElements.
  return this->revision.indexForKey(key);
}

bool Container::isAnchorable(const std::string& key) const {
  if (key.empty()) {
    return false;
  }
  // Fast path: no policy set means every row is anchorable.
  return this->nonAnchorableKeys.empty() ||
    this->nonAnchorableKeys.find(key) == this->nonAnchorableKeys.end();
}

const Anchor* Container::compensationAnchor() const {
  if (!this->operation) {
    return &this->anchor;
  }
  return this->operation->target.mode == AnchorMode::Element ? &this->operation->target : nullptr;
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

  /*
   * Notify when the viewable range changes. Computed only when a listener is
   * registered, since getViewableIndices does an O(window) overlap scan.
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

    /*
     * Measure the visible fraction against min(element, viewport) so an element
     * taller than the viewport can still reach 1.0 by fully covering the screen.
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

  // Match getVisibleIndices: inverted reports the higher index first.
  if (this->inverted) {
    return {lastViewable, firstViewable};
  }
  return {firstViewable, lastViewable};
}

void Container::setPredictedSize(const std::string& key, Size size) {
  if (key.empty()) {
    return;
  }
  this->predictedSizes[key] = size;
}

bool Container::hasTrustedSize(std::size_t index) const {
  if (index >= this->revision.elements.size()) {
    return false;
  }

  const Element& nextElement = this->revision.elements[index];
  return nextElement.measured || nextElement.predicted;
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
