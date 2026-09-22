#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Error.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace azimgd::shadowlist {

namespace {
/*
 * Debug only. Looks up the key at an index so JS and native logs can be matched up.
 */
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
   * Start is always row 0 and end the last row, even when inverted.
   * Don't swap them like FlatList does. In an inverted chat onStartReached would fire at the
   * bottom, each prepend would re-arm it, and loading earlier messages would loop forever.
   */
  bool reachedEnd = reachingHighEdge;
  bool reachedStart = reachingLowEdge;

  // A list shorter than the viewport touches both edges, so report only the end.
  if (reachedStart && reachedEnd) {
    reachedStart = false;
  }

  // New data means the edge is new too, even if the offset never moved.
  std::size_t elementsSize = this->revision.elements.size();
  if (elementsSize != this->previousReachedElementsSize) {
    this->previousReachedElementsSize = elementsSize;
    this->previousReachedEnd = false;
    this->previousReachedStart = false;
  }

  // Fire once when we arrive at an edge, not on every frame near it.
  if (this->endReachedEnabled && this->onEndReachedCallback && reachedEnd && !this->previousReachedEnd) {
    this->onEndReachedCallback();
  }

  if (this->startReachedEnabled && this->onStartReachedCallback && reachedStart && !this->previousReachedStart) {
    this->onStartReachedCallback();
  }

  this->previousReachedEnd = reachedEnd;
  this->previousReachedStart = reachedStart;

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
  // The command wins over the prop. Each call bumps a counter, so the same index can scroll twice.
  bool fired = false;
  if (commandSequence != this->previousScrollToIndexSequence) {
    this->previousScrollToIndexSequence = commandSequence;
    // scrollToEnd comes through the same command with SCROLL_TO_END_INDEX as the index.
    if (commandIndex == SCROLL_TO_END_INDEX) {
      this->scrollToEnd();
      fired = true;
    } else if (commandIndex >= 0.0 && std::isfinite(commandIndex) &&
               commandIndex < static_cast<double>(std::numeric_limits<std::size_t>::max())) {
      // A missing position means the top. Anything else is clamped between 0 and 1 to keep the row on screen.
      double viewPosition = std::isfinite(commandViewPosition)
        ? std::min(1.0, std::max(0.0, commandViewPosition))
        : 0.0;
      this->scrollToIndex(static_cast<std::size_t>(commandIndex), viewPosition);
      fired = true;
    }
  }

  // The prop only scrolls when its value changes. A negative value turns it off.
  if (!fired && propIndex >= 0 && propIndex != this->previousScrollToIndexProp) {
    this->scrollToIndex(static_cast<std::size_t>(propIndex));
  }
  this->previousScrollToIndexProp = propIndex;
}

ContainerStateUpdate Container::resolveStateUpdate(
  double previousContainerOffsetX,
  double previousContainerOffsetY,
  double previousTotalContainerWidth,
  double previousTotalContainerHeight) const {
  ContainerStateUpdate update;

  bool corrected = this->containerOffsetCorrected;
  bool sizeChanged =
    this->revision.totalContainerWidth != previousTotalContainerWidth ||
    this->revision.totalContainerHeight != previousTotalContainerHeight;

  update.totalContainerWidth = this->revision.totalContainerWidth;
  update.totalContainerHeight = this->revision.totalContainerHeight;

  // Use our offset only when we want to move the view. Otherwise keep the reported one so we don't fight the user.
  if (corrected) {
    update.containerOffsetX = this->revision.containerOffsetX;
    update.containerOffsetY = this->revision.containerOffsetY;
  } else {
    update.containerOffsetX = previousContainerOffsetX;
    update.containerOffsetY = previousContainerOffsetY;
  }

  update.applyContainerOffset = corrected;
  update.changed = corrected || sizeChanged;

  /*
   * Send the operation id only when an operation moved the offset, so the host can send it back.
   * Offset changes without an operation send 0.
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

  // Scrolling alone never changes the snap offsets, so reuse them unless the geometry or settings changed.
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
   * One target per row, clamped to the scroll range. Rows near either end collapse onto
   * the same value, so skip repeats.
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

  return this->revision.indexForKey(key);
}

bool Container::isAnchorable(const std::string& key) const {
  if (key.empty()) {
    return false;
  }
  // With no keys excluded, every row can be the anchor.
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
    (visibleIndices.first != this->previousVisibleStartIndex || visibleIndices.second != this->previousVisibleEndIndex)) {
    SL_LOG("  emit onVisibleIndicesChange(%zd, %zd) keys=[%s..%s]",
      static_cast<std::ptrdiff_t>(visibleIndices.first), static_cast<std::ptrdiff_t>(visibleIndices.second),
      debugKeyAt(this->revision.elements, visibleIndices.first),
      debugKeyAt(this->revision.elements, visibleIndices.second));
    this->onVisibleIndicesChangeCallback(visibleIndices.first, visibleIndices.second);
  }
  this->previousVisibleStartIndex = visibleIndices.first;
  this->previousVisibleEndIndex = visibleIndices.second;

  // Only work out the viewable rows when someone listens, since it scans the window.
  if (this->onViewableIndicesChangeCallback) {
    auto viewableIndices = this->getViewableIndices();
    if (viewableIndices.first != this->previousViewableStartIndex || viewableIndices.second != this->previousViewableEndIndex) {
      SL_LOG("  emit onViewableIndicesChange(%zd, %zd)",
        static_cast<std::ptrdiff_t>(viewableIndices.first), static_cast<std::ptrdiff_t>(viewableIndices.second));
      this->onViewableIndicesChangeCallback(viewableIndices.first, viewableIndices.second);
    }
    this->previousViewableStartIndex = viewableIndices.first;
    this->previousViewableEndIndex = viewableIndices.second;
  }

  double containerOffsetX = this->revision.containerOffsetX;
  double containerOffsetY = this->revision.containerOffsetY;
  if (this->onScrollCallback &&
    (!this->previousContainerOffsetValid || containerOffsetX != this->previousContainerOffsetX || containerOffsetY != this->previousContainerOffsetY)) {
    this->onScrollCallback(containerOffsetX, containerOffsetY);
  }
  this->previousContainerOffsetX = containerOffsetX;
  this->previousContainerOffsetY = containerOffsetY;
  this->previousContainerOffsetValid = true;
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

  // Inverted lists store the range backwards, so flip it.
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
     * Compare against the smaller of row and viewport, so a row taller than the screen
     * can still count as fully visible.
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

  // Like getVisibleIndices, inverted lists report the higher index first.
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
