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
  if (revision.elements.empty()) {
    revisionCount = REVISION_COUNT_FIRST;
  } else {
    revisionCount++;
  }

  double containerOffset = getContainerOffset();
  double windowSize = getWindowContainerSize();
  double totalSize = horizontal ? revision.totalContainerWidth : revision.totalContainerHeight;

  bool reachingLowEdge = containerOffset <= windowSize * startReachedThreshold;
  bool reachingHighEdge = containerOffset + windowSize >= totalSize - windowSize * endReachedThreshold;

  /*
   * Start is always row 0 and end the last row, even when inverted.
   * Swapping them breaks inverted chats: onStartReached would fire at the bottom, each
   * prepend would re-arm it, and loading earlier messages would loop forever.
   */
  bool reachedEnd = reachingHighEdge;
  bool reachedStart = reachingLowEdge;

  /*
   * A list shorter than the viewport touches both edges. Report only the end. A list that
   * scrolls reports both, or a short chat scrolled to its top could never load older rows.
   */
  if (reachedStart && reachedEnd && totalSize <= windowSize) {
    reachedStart = false;
  }

  // New data means the edge is new too, even if the offset never moved.
  std::size_t elementsSize = revision.elements.size();
  if (elementsSize != previousReachedElementsSize_) {
    previousReachedElementsSize_ = elementsSize;
    previousReachedEnd_ = false;
    previousReachedStart_ = false;
  }

  // Fire once when we arrive at an edge, not on every frame near it.
  if (endReachedEnabled && onEndReachedCallback && reachedEnd && !previousReachedEnd_) {
    onEndReachedCallback();
  }

  if (startReachedEnabled && onStartReachedCallback && reachedStart && !previousReachedStart_) {
    onStartReachedCallback();
  }

  previousReachedEnd_ = reachedEnd;
  previousReachedStart_ = reachedStart;

  dispatchObservers();
}

void Container::scrollToIndex(std::size_t index, double viewPosition, double rowOffset) {
  scrollToIndexTarget = index;
  scrollToIndexViewPosition = viewPosition;
  scrollToIndexRowOffset = rowOffset;
}

void Container::scrollToEnd() {
  pendingScrollToEnd = true;
}

void Container::scrollToStart() {
  pendingScrollToStart = true;
  pendingScrollToEnd = false;
  scrollToIndexTarget = UNDEFINED_INDEX;
}

void Container::scrollToOffset(double offset) {
  std::size_t count = getElementsSize();
  // A NaN offset matches no row and would carry NaN into the row offset. Treat it as the top.
  if (std::isnan(offset) || offset <= 0.0 || count == 0) {
    scrollToStart();
    return;
  }
  // The row reaching furthest back that still covers the offset. In a grid the first column wins.
  std::size_t best = UNDEFINED_INDEX;
  double bestLeading = 0.0;
  for (std::size_t index = 0; index < count; ++index) {
    double leading = getElementOffset(index);
    if (leading + getElementSize(index) <= offset) {
      continue;
    }
    if (best == UNDEFINED_INDEX || leading < bestLeading) {
      best = index;
      bestLeading = leading;
    }
  }
  if (best == UNDEFINED_INDEX) {
    scrollToEnd();
    return;
  }
  scrollToIndex(best, 0.0, offset - bestLeading);
}

void Container::requestScrollToIndex(
  double commandIndex,
  double commandSequence,
  int propIndex,
  double commandViewPosition,
  double commandRowOffset) {
  /*
   * The command wins over the prop. Each call bumps a counter. The same index can scroll
   * twice, and a lower sequence is an older state that must not run the command again.
   */
  bool fired = false;
  if (commandSequence > previousScrollToIndexSequence_) {
    SL_LOG("  scroll command: index=%.0f sequence=%.0f position=%.2f", commandIndex, commandSequence, commandViewPosition);
    previousScrollToIndexSequence_ = commandSequence;
    // scrollToEnd comes through the same command with SCROLL_TO_END_INDEX as the index.
    if (commandIndex == SCROLL_TO_END_INDEX) {
      scrollToEnd();
      fired = true;
    } else if (commandIndex == SCROLL_TO_OFFSET_INDEX) {
      scrollToOffset(std::isfinite(commandRowOffset) ? commandRowOffset : 0.0);
      fired = true;
    } else if (commandIndex >= 0.0 && std::isfinite(commandIndex) &&
               commandIndex < static_cast<double>(std::numeric_limits<std::size_t>::max())) {
      // A missing position means the top. Anything else is clamped between 0 and 1 to keep the row on screen.
      double viewPosition = std::isfinite(commandViewPosition)
        ? std::min(1.0, std::max(0.0, commandViewPosition))
        : 0.0;
      double rowOffset = std::isfinite(commandRowOffset) ? commandRowOffset : 0.0;
      scrollToIndex(static_cast<std::size_t>(commandIndex), viewPosition, rowOffset);
      fired = true;
    }
  }

  // The prop only scrolls when its value changes. A negative value turns it off.
  if (!fired && propIndex >= 0 && propIndex != previousScrollToIndexProp_) {
    scrollToIndex(static_cast<std::size_t>(propIndex));
  }
  previousScrollToIndexProp_ = propIndex;
}

ContainerStateUpdate Container::resolveStateUpdate(
  double previousContainerOffsetX,
  double previousContainerOffsetY,
  double previousTotalContainerWidth,
  double previousTotalContainerHeight) const {
  ContainerStateUpdate update;

  bool corrected = containerOffsetCorrected;
  bool sizeChanged =
    revision.totalContainerWidth != previousTotalContainerWidth ||
    revision.totalContainerHeight != previousTotalContainerHeight;

  update.totalContainerWidth = revision.totalContainerWidth;
  update.totalContainerHeight = revision.totalContainerHeight;

  // Use our offset only when we want to move the view. Otherwise keep the reported one so we don't fight the user.
  if (corrected) {
    update.containerOffsetX = revision.containerOffsetX;
    update.containerOffsetY = revision.containerOffsetY;
  } else {
    update.containerOffsetX = previousContainerOffsetX;
    update.containerOffsetY = previousContainerOffsetY;
  }

  update.applyContainerOffset = corrected;
  update.changed = corrected || sizeChanged;

  /*
   * Send the operation id only when an operation moved the offset. The host sends it back.
   * Offset changes without an operation send 0.
   */
  update.commitToken = (corrected && operation) ? operation->id : 0;

  return update;
}

double Container::getFooterOffset(double footerExtent) const {
  double totalSize = horizontal ? revision.totalContainerWidth : revision.totalContainerHeight;
  return totalSize - footerExtent;
}

const std::vector<double>& Container::getSnapOffsets() const {
  double windowSize = getWindowContainerSize();
  double totalSize = horizontal ? revision.totalContainerWidth : revision.totalContainerHeight;

  // Scrolling alone never changes the snap offsets. Reuse them unless the geometry or settings changed.
  if (snapCacheVersion_ == geometryVersion &&
      snapCacheSnapToItem_ == snapToItem &&
      snapCacheAlignment_ == snapAlignment &&
      snapCacheWindowSize_ == windowSize &&
      snapCacheTotalSize_ == totalSize &&
      snapCacheHorizontal_ == horizontal) {
    return snapOffsetsCache_;
  }

  snapCacheVersion_ = geometryVersion;
  snapCacheSnapToItem_ = snapToItem;
  snapCacheAlignment_ = snapAlignment;
  snapCacheWindowSize_ = windowSize;
  snapCacheTotalSize_ = totalSize;
  snapCacheHorizontal_ = horizontal;

  std::vector<double>& snapOffsets = snapOffsetsCache_;
  snapOffsets.clear();

  if (!snapToItem) {
    return snapOffsets;
  }

  std::size_t elementsSize = revision.elements.size();
  if (elementsSize == 0) {
    return snapOffsets;
  }

  double maxOffset = totalSize - windowSize;
  if (maxOffset < 0.0) {
    maxOffset = 0.0;
  }

  /*
   * One target per row, clamped to the scroll range, plus both ends of the range. A header
   * or footer can then still be scrolled into view. Rows near either end collapse onto the
   * same value. Grid tracks are not in order. Sort and skip repeats.
   */
  snapOffsets.reserve(elementsSize + 2);
  snapOffsets.push_back(0.0);
  snapOffsets.push_back(maxOffset);
  for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
    const Element& nextElement = revision.elements[nextElementIndex];
    double elementOffset = horizontal ? nextElement.offsetX : nextElement.offsetY;
    double elementSize = horizontal ? nextElement.width : nextElement.height;

    double target;
    if (snapAlignment == 1) {
      target = elementOffset - (windowSize - elementSize) / 2.0;
    } else if (snapAlignment == 2) {
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

    if (std::fabs(target - snapOffsets.back()) > OFFSET_MOVED_THRESHOLD) {
      snapOffsets.push_back(target);
    }
  }

  std::sort(snapOffsets.begin(), snapOffsets.end());
  snapOffsets.erase(
    std::unique(snapOffsets.begin(), snapOffsets.end(),
      [](double previous, double next) { return next - previous <= OFFSET_MOVED_THRESHOLD; }),
    snapOffsets.end());
  return snapOffsets;
}

std::size_t Container::findElementIndexByKey(const std::string& key) const {
  if (key.empty()) {
    return UNDEFINED_INDEX;
  }

  return revision.indexForKey(key);
}

bool Container::isAnchorable(const std::string& key) const {
  if (key.empty()) {
    return false;
  }
  // With no keys excluded, every row can be the anchor.
  return nonAnchorableKeys.empty() ||
    nonAnchorableKeys.find(key) == nonAnchorableKeys.end();
}

const Anchor* Container::getCompensationAnchor() const {
  if (!operation) {
    return &anchor;
  }
  return operation->target.mode == AnchorMode::Element ? &operation->target : nullptr;
}

void Container::dispatchObservers() {
  /*
   * Before the host gives the viewport a size, the window holds only the row at the offset.
   * Reporting that would unmount the rows the first frame shows.
   */
  if (getWindowContainerSize() <= 0.0) {
    return;
  }
  auto visibleIndices = getVisibleIndices();
  if (onVisibleIndicesChangeCallback &&
    (visibleIndices.first != previousVisibleStartIndex_ || visibleIndices.second != previousVisibleEndIndex_)) {
    SL_LOG("  emit onVisibleIndicesChange(%zd, %zd) keys=[%s..%s]",
      static_cast<std::ptrdiff_t>(visibleIndices.first), static_cast<std::ptrdiff_t>(visibleIndices.second),
      debugKeyAt(revision.elements, visibleIndices.first),
      debugKeyAt(revision.elements, visibleIndices.second));
    onVisibleIndicesChangeCallback(visibleIndices.first, visibleIndices.second);
  }
  previousVisibleStartIndex_ = visibleIndices.first;
  previousVisibleEndIndex_ = visibleIndices.second;

  // Only work out the viewable rows when someone listens, since it scans the window.
  if (onViewableIndicesChangeCallback) {
    std::vector<std::size_t> ranges;
    ranges.reserve(viewableRules.size() * 2);
    for (const auto& rule : viewableRules) {
      auto viewableIndices = getViewableIndices(rule);
      ranges.push_back(viewableIndices.first);
      ranges.push_back(viewableIndices.second);
    }
    if (ranges != previousViewableRanges_) {
      SL_LOG("  emit onViewableIndicesChange(%zd, %zd) rules=%zu",
        static_cast<std::ptrdiff_t>(ranges.empty() ? UNDEFINED_INDEX : ranges[0]),
        static_cast<std::ptrdiff_t>(ranges.empty() ? UNDEFINED_INDEX : ranges[1]), viewableRules.size());
      previousViewableRanges_ = ranges;
      // Hand out the local copy. A callback that runs another frame rewrites the member under it.
      onViewableIndicesChangeCallback(ranges);
    }
  }

  double containerOffsetX = revision.containerOffsetX;
  double containerOffsetY = revision.containerOffsetY;
  if (onScrollCallback &&
    (!previousContainerOffsetValid_ || containerOffsetX != previousContainerOffsetX_ || containerOffsetY != previousContainerOffsetY_)) {
    onScrollCallback(containerOffsetX, containerOffsetY);
  }
  previousContainerOffsetX_ = containerOffsetX;
  previousContainerOffsetY_ = containerOffsetY;
  previousContainerOffsetValid_ = true;
}

bool Container::hasPendingCommand() const {
  return operation.has_value() || pendingScrollToEnd || pendingScrollToStart ||
    scrollToIndexTarget != UNDEFINED_INDEX;
}

OffsetBand Container::computeOffsetBand() const {
  const OffsetBand empty;
  const std::vector<Element>& elements = revision.elements;
  std::size_t elementsSize = elements.size();

  // Before the first real frame the window fills from the edge, not from the offset.
  if (elementsSize == 0 || revisionCount == REVISION_COUNT_FIRST) {
    return empty;
  }

  // Corrections and scroll commands settle over several frames.
  if (hasPendingCommand() || containerOffsetCorrected) {
    return empty;
  }

  // Rows or sizes the next frame would still lay out. Positions are about to move.
  if (elementsStructureDirty || elementsSizeDirtyFromIndex != UNDEFINED_INDEX) {
    return empty;
  }
  auto [estimatedWidth, estimatedHeight] = estimatedElementSize;
  double fallbackWidth = revision.averageElementWidth > 0.0 ? revision.averageElementWidth : estimatedWidth;
  double fallbackHeight = revision.averageElementHeight > 0.0 ? revision.averageElementHeight : estimatedHeight;
  /*
   * Same inputs as the row reflow in layoutElements. The footer and the window size along the
   * scroll axis never move rows, and the band is recomputed on the frame that changes them.
   */
  bool crossWindowChanged = horizontal
    ? revision.windowContainerHeight != previousLayoutWindowHeight
    : revision.windowContainerWidth != previousLayoutWindowWidth;
  if (fallbackWidth != previousFallbackWidth || fallbackHeight != previousFallbackHeight ||
      headerSize != previousLayoutHeaderSize || crossWindowChanged ||
      columns != previousLayoutColumns || horizontal != previousLayoutHorizontal) {
    return empty;
  }

  // These listeners need every offset, not just the window.
  if (onScrollCallback || onViewableIndicesChangeCallback || !stickyIndices.empty()) {
    return empty;
  }

  // An inverted list still settling on the bottom it opened at follows every frame.
  if (inverted && (!invertedInitialized || invertedOpeningPin)) {
    return empty;
  }

  double offset = getContainerOffset();
  double windowSize = getWindowContainerSize();
  double totalSize = horizontal ? revision.totalContainerWidth : revision.totalContainerHeight;
  double maxOffset = std::max(0.0, totalSize - windowSize);
  std::size_t measuredStart = revision.measurementElementStartIndex;
  std::size_t measuredEnd = revision.measurementElementEndIndex;
  if (!(windowSize > 0.0) || !std::isfinite(offset) || measuredStart == UNDEFINED_INDEX ||
      measuredEnd == UNDEFINED_INDEX) {
    return empty;
  }

  // Past either end of the scroll range, like a bounce or a pull to refresh, send every frame.
  if (offset < 0.0 || offset > maxOffset) {
    return empty;
  }

  /*
   * Start from the whole scroll range, whose ends are real offsets the host can rest on,
   * then pull each side in to the nearest offset where something flips.
   */
  double low = 0.0;
  double high = maxOffset;
  auto addFlip = [&](double flip) {
    if (!std::isfinite(flip)) {
      return;
    }
    if (flip <= offset) {
      low = std::max(low, flip + OFFSET_BAND_MARGIN);
    } else {
      high = std::min(high, flip - OFFSET_BAND_MARGIN);
    }
  };

  /*
   * A row is in the window while it ends past offset minus the overscan and starts before
   * the far edge plus the overscan. So it enters and leaves at these two offsets. Only rows
   * near the window matter. Each column is in order. The nearest flips come from rows
   * at most a couple of columns outside the measured range.
   */
  double overscanSize = windowSize * overscan;
  std::size_t columnCount = columns > 0 ? columns : 1;
  std::size_t reach = 2 * columnCount + 2;
  std::size_t lowIndex = std::min(measuredStart, measuredEnd);
  std::size_t highIndex = std::max(measuredStart, measuredEnd);
  std::size_t fromIndex = lowIndex > reach ? lowIndex - reach : 0;
  std::size_t toIndex = std::min(elementsSize - 1, highIndex + reach);
  for (std::size_t index = fromIndex; index <= toIndex; ++index) {
    const Element& element = elements[index];
    double elementStart = horizontal ? element.offsetX : element.offsetY;
    double elementSize = horizontal ? element.width : element.height;
    addFlip(elementStart - windowSize - overscanSize);
    addFlip(elementStart + elementSize + overscanSize);
  }

  // The edge callbacks flip where they start and stop counting as reached.
  addFlip(windowSize * startReachedThreshold);
  addFlip(totalSize - windowSize - windowSize * endReachedThreshold);

  // The inverted bottom pin flips where it starts counting as at the bottom.
  if (inverted) {
    addFlip(maxOffset - INVERTED_FOLLOW_BAND);
  }

  OffsetBand band;
  band.low = low;
  band.high = high;
  if (!band.contains(offset)) {
    return empty;
  }
  return band;
}

const Element& Container::getElementAtIndex(std::size_t index) const {
  if (index >= revision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  return revision.elements[index];
}

std::size_t Container::getElementsSize() const {
  return revision.elements.size();
}

std::pair<std::size_t, std::size_t> Container::getVisibleIndices() const {
  std::size_t startIndex = revision.measurementElementStartIndex;
  std::size_t endIndex = revision.measurementElementEndIndex;

  if (startIndex != UNDEFINED_INDEX && endIndex != UNDEFINED_INDEX) {
    return {startIndex, endIndex};
  }

  return {UNDEFINED_INDEX, UNDEFINED_INDEX};
}

std::pair<std::size_t, std::size_t> Container::getViewableIndices(const ViewableRule& rule) const {
  std::size_t measuredStartIndex = revision.measurementElementStartIndex;
  std::size_t measuredEndIndex = revision.measurementElementEndIndex;

  if (measuredStartIndex == UNDEFINED_INDEX || measuredEndIndex == UNDEFINED_INDEX) {
    return {UNDEFINED_INDEX, UNDEFINED_INDEX};
  }

  double viewportStart = getContainerOffset();
  double windowSize = getWindowContainerSize();
  double viewportEnd = viewportStart + windowSize;

  // Inverted lists store the range backwards. Flip it.
  std::size_t windowLow = inverted ? measuredEndIndex : measuredStartIndex;
  std::size_t windowHigh = inverted ? measuredStartIndex : measuredEndIndex;

  std::size_t firstViewable = UNDEFINED_INDEX;
  std::size_t lastViewable = UNDEFINED_INDEX;

  for (std::size_t nextElementIndex = windowLow; nextElementIndex <= windowHigh && nextElementIndex < revision.elements.size(); ++nextElementIndex) {
    const Element& nextElement = revision.elements[nextElementIndex];
    double elementStart = horizontal ? nextElement.offsetX : nextElement.offsetY;
    double elementSize = horizontal ? nextElement.width : nextElement.height;
    if (elementSize <= 0.0) {
      continue;
    }
    double elementEnd = elementStart + elementSize;

    double overlapStart = elementStart > viewportStart ? elementStart : viewportStart;
    double overlapEnd = elementEnd < viewportEnd ? elementEnd : viewportEnd;
    double visible = overlapEnd - overlapStart;

    /*
     * Compare against the smaller of row and viewport. A row taller than the screen
     * can still count as fully visible. Coverage compares against the viewport, and a row
     * fully on screen always counts.
     */
    double referenceSize = rule.coverage ? windowSize : (elementSize < windowSize ? elementSize : windowSize);
    bool fullyVisible = rule.coverage && visible >= elementSize;
    if (visible > 0.0 && referenceSize > 0.0 && (fullyVisible || (visible / referenceSize) >= rule.threshold)) {
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
  if (inverted) {
    return {lastViewable, firstViewable};
  }
  return {firstViewable, lastViewable};
}

void Container::setPredictedSize(const std::string& key, Size size) {
  if (key.empty()) {
    return;
  }
  predictedSizes[key] = size;
}

bool Container::hasTrustedSize(std::size_t index) const {
  if (index >= revision.elements.size()) {
    return false;
  }

  const Element& nextElement = revision.elements[index];
  return nextElement.measured || nextElement.predicted;
}

double Container::getElementOffset(std::size_t index) const {
  if (index >= revision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  const Element& nextElement = revision.elements[index];
  return horizontal ? nextElement.offsetX : nextElement.offsetY;
}

double Container::getElementSize(std::size_t index) const {
  if (index >= revision.elements.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  const Element& nextElement = revision.elements[index];
  return horizontal ? nextElement.width : nextElement.height;
}

double Container::getContainerOffset() const {
  return horizontal ? revision.containerOffsetX : revision.containerOffsetY;
}

double Container::getWindowContainerSize() const {
  return horizontal ? revision.windowContainerWidth : revision.windowContainerHeight;
}

void Container::setEndReachedEnabled(bool enabled) {
  endReachedEnabled = enabled;
}

void Container::setStartReachedEnabled(bool enabled) {
  startReachedEnabled = enabled;
}

}
