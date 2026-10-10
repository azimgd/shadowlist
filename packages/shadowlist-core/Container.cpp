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
[[maybe_unused]] const char* debugKeyAt(const std::vector<Row>& rows, std::size_t index) {
  if (index < rows.size()) {
    return rows[index].key.empty() ? "(empty)" : rows[index].key.c_str();
  }
  return "(oob)";
}
}

void Container::endRevision() {
  if (revision.rows.empty()) {
    revisionCount = REVISION_COUNT_FIRST;
  } else {
    revisionCount++;
  }

  double offset = getOffset();
  double windowSize = getWindowSize();
  double totalSize = horizontal ? revision.contentWidth : revision.contentHeight;

  bool reachingLowEdge = offset <= windowSize * startReachedThreshold;
  bool reachingHighEdge = offset + windowSize >= totalSize - windowSize * endReachedThreshold;

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
  std::size_t rowCount = revision.rows.size();
  if (rowCount != previousReachedRowCount_) {
    previousReachedRowCount_ = rowCount;
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

void Container::scrollToRow(std::size_t index, double viewPosition, double viewOffset) {
  scrollToRowTarget = index;
  scrollToRowViewPosition = viewPosition;
  scrollToRowViewOffset = viewOffset;
}

void Container::scrollToEnd() {
  pendingScrollToEnd = true;
}

void Container::scrollToStart() {
  pendingScrollToStart = true;
  pendingScrollToEnd = false;
  scrollToRowTarget = UNDEFINED_INDEX;
}

void Container::scrollToOffset(double offset) {
  std::size_t count = getRowCount();
  // A NaN offset matches no row and would carry NaN into the row offset. Treat it as the top.
  if (std::isnan(offset) || offset <= 0.0 || count == 0) {
    scrollToStart();
    return;
  }
  // The row reaching furthest back that still covers the offset. In a grid the first column wins.
  std::size_t best = UNDEFINED_INDEX;
  double bestLeading = 0.0;
  for (std::size_t index = 0; index < count; ++index) {
    double leading = getRowOffset(index);
    if (leading + getRowSize(index) <= offset) {
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
  scrollToRow(best, 0.0, offset - bestLeading);
}

void Container::requestScrollToRow(
  double commandIndex,
  double commandSequence,
  int propIndex,
  double commandViewPosition,
  double commandViewOffset) {
  /*
   * The command wins over the prop. Each call bumps a counter. The same index can scroll
   * twice, and a lower sequence is an older state that must not run the command again.
   */
  bool fired = false;
  if (commandSequence > previousScrollToRowSequence_) {
    SL_LOG("  scroll command: index=%.0f sequence=%.0f position=%.2f", commandIndex, commandSequence, commandViewPosition);
    previousScrollToRowSequence_ = commandSequence;
    // scrollToEnd comes through the same command with SCROLL_TO_END_INDEX as the index.
    if (commandIndex == SCROLL_TO_END_INDEX) {
      scrollToEnd();
      fired = true;
    } else if (commandIndex == SCROLL_TO_OFFSET_INDEX) {
      scrollToOffset(std::isfinite(commandViewOffset) ? commandViewOffset : 0.0);
      fired = true;
    } else if (commandIndex >= 0.0 && std::isfinite(commandIndex) &&
               commandIndex < static_cast<double>(std::numeric_limits<std::size_t>::max())) {
      // A missing position means the top. Anything else is clamped between 0 and 1 to keep the row on screen.
      double viewPosition = std::isfinite(commandViewPosition)
        ? std::min(1.0, std::max(0.0, commandViewPosition))
        : 0.0;
      double viewOffset = std::isfinite(commandViewOffset) ? commandViewOffset : 0.0;
      scrollToRow(static_cast<std::size_t>(commandIndex), viewPosition, viewOffset);
      fired = true;
    }
  }

  // The prop only scrolls when its value changes. A negative value turns it off.
  if (!fired && propIndex >= 0 && propIndex != previousScrollToRowProp_) {
    scrollToRow(static_cast<std::size_t>(propIndex));
  }
  previousScrollToRowProp_ = propIndex;
}

ContainerStateUpdate Container::resolveStateUpdate(
  double previousOffsetX,
  double previousOffsetY,
  double previousContentWidth,
  double previousContentHeight) const {
  ContainerStateUpdate update;

  bool corrected = offsetCorrected;
  bool sizeChanged =
    revision.contentWidth != previousContentWidth ||
    revision.contentHeight != previousContentHeight;

  update.contentWidth = revision.contentWidth;
  update.contentHeight = revision.contentHeight;

  // Use our offset only when we want to move the view. Otherwise keep the reported one so we don't fight the user.
  if (corrected) {
    update.offsetX = revision.offsetX;
    update.offsetY = revision.offsetY;
  } else {
    update.offsetX = previousOffsetX;
    update.offsetY = previousOffsetY;
  }

  update.applyOffset = corrected;
  update.changed = corrected || sizeChanged;

  /*
   * Send the commit token only when an operation moved the offset. The host sends it back.
   * Offset changes without an operation send 0.
   */
  update.commitToken = (corrected && operation) ? operation->commitToken : 0;

  return update;
}

double Container::getFooterStart(double footerSize) const {
  double totalSize = horizontal ? revision.contentWidth : revision.contentHeight;
  return totalSize - footerSize;
}

const std::vector<double>& Container::getSnapOffsets() const {
  double windowSize = getWindowSize();
  double totalSize = horizontal ? revision.contentWidth : revision.contentHeight;

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

  std::size_t rowCount = revision.rows.size();
  if (rowCount == 0) {
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
  snapOffsets.reserve(rowCount + 2);
  snapOffsets.push_back(0.0);
  snapOffsets.push_back(maxOffset);
  for (std::size_t nextRowIndex = 0; nextRowIndex < rowCount; ++nextRowIndex) {
    const Row& nextRow = revision.rows[nextRowIndex];
    double rowOffset = horizontal ? nextRow.offsetX : nextRow.offsetY;
    double rowSize = horizontal ? nextRow.width : nextRow.height;

    double target;
    if (snapAlignment == SnapAlignment::Center) {
      target = rowOffset - (windowSize - rowSize) / 2.0;
    } else if (snapAlignment == SnapAlignment::End) {
      target = rowOffset + rowSize - windowSize;
    } else {
      target = rowOffset;
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

std::size_t Container::indexOfKey(const std::string& key) const {
  if (key.empty()) {
    return UNDEFINED_INDEX;
  }

  return revision.indexOfKey(key);
}

bool Container::isAnchorable(const std::string& key) const {
  if (key.empty()) {
    return false;
  }
  // With no keys excluded, every row can be the anchor.
  return nonAnchorKeys.empty() ||
    nonAnchorKeys.find(key) == nonAnchorKeys.end();
}

const Anchor* Container::getCompensationAnchor() const {
  if (!operation) {
    return &anchor;
  }
  return operation->target.mode == AnchorMode::Row ? &operation->target : nullptr;
}

void Container::dispatchObservers() {
  /*
   * Before the host gives the viewport a size, the window holds only the row at the offset.
   * Reporting that would unmount the rows the first frame shows.
   */
  if (getWindowSize() <= 0.0) {
    return;
  }
  IndexRange measured = getMeasuredRange();
  if (onMeasuredRangeChangeCallback && (measured.low != previousMeasuredLow_ || measured.high != previousMeasuredHigh_)) {
    SL_LOG("  emit onMeasuredRangeChange(%zd, %zd) keys=[%s..%s]",
      static_cast<std::ptrdiff_t>(measured.low), static_cast<std::ptrdiff_t>(measured.high),
      debugKeyAt(revision.rows, measured.low),
      debugKeyAt(revision.rows, measured.high));
    onMeasuredRangeChangeCallback(measured.low, measured.high);
  }
  previousMeasuredLow_ = measured.low;
  previousMeasuredHigh_ = measured.high;

  // Only work out the viewable rows when someone listens, since it scans the window.
  if (onViewableIndicesChangeCallback) {
    std::vector<std::size_t> ranges;
    ranges.reserve(viewableRules.size() * 2);
    for (const auto& rule : viewableRules) {
      IndexRange viewable = getViewableIndices(rule);
      ranges.push_back(viewable.low);
      ranges.push_back(viewable.high);
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

  double offsetX = revision.offsetX;
  double offsetY = revision.offsetY;
  if (onScrollCallback &&
    (!previousOffsetValid_ || offsetX != previousOffsetX_ || offsetY != previousOffsetY_)) {
    onScrollCallback(offsetX, offsetY);
  }
  previousOffsetX_ = offsetX;
  previousOffsetY_ = offsetY;
  previousOffsetValid_ = true;
}

bool Container::hasPendingCommand() const {
  return operation.has_value() || pendingScrollToEnd || pendingScrollToStart ||
    scrollToRowTarget != UNDEFINED_INDEX;
}

OffsetBand Container::computeOffsetBand() const {
  const OffsetBand empty;
  const std::vector<Row>& rows = revision.rows;
  std::size_t rowCount = rows.size();

  // Before the first real frame the window fills from the edge, not from the offset.
  if (rowCount == 0 || revisionCount == REVISION_COUNT_FIRST) {
    return empty;
  }

  // Corrections and scroll commands settle over several frames.
  if (hasPendingCommand() || offsetCorrected) {
    return empty;
  }

  // Rows or sizes the next frame would still lay out. Positions are about to move.
  if (rowStructureDirty || rowSizeDirtyFromIndex != UNDEFINED_INDEX) {
    return empty;
  }
  auto [estimatedWidth, estimatedHeight] = estimatedRowSize;
  double fallbackWidth = revision.averageRowWidth > 0.0 ? revision.averageRowWidth : estimatedWidth;
  double fallbackHeight = revision.averageRowHeight > 0.0 ? revision.averageRowHeight : estimatedHeight;
  /*
   * Same inputs as the row reflow in layoutRows. The footer and the window size along the
   * scroll axis never move rows, and the band is recomputed on the frame that changes them.
   */
  bool crossWindowChanged = horizontal
    ? revision.windowHeight != previousLayoutWindowHeight
    : revision.windowWidth != previousLayoutWindowWidth;
  if (fallbackWidth != previousFallbackWidth || fallbackHeight != previousFallbackHeight ||
      headerSize != previousLayoutHeaderSize || crossWindowChanged ||
      numberOfColumns != previousLayoutNumberOfColumns || horizontal != previousLayoutHorizontal) {
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

  double offset = getOffset();
  double windowSize = getWindowSize();
  double totalSize = horizontal ? revision.contentWidth : revision.contentHeight;
  double maxOffset = std::max(0.0, totalSize - windowSize);
  std::size_t measuredLow = revision.measuredLow;
  std::size_t measuredHigh = revision.measuredHigh;
  if (!(windowSize > 0.0) || !std::isfinite(offset) || measuredLow == UNDEFINED_INDEX ||
      measuredHigh == UNDEFINED_INDEX) {
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
  std::size_t columnCount = numberOfColumns > 0 ? numberOfColumns : 1;
  std::size_t reach = 2 * columnCount + 2;
  std::size_t fromIndex = measuredLow > reach ? measuredLow - reach : 0;
  std::size_t toIndex = std::min(rowCount - 1, measuredHigh + reach);
  for (std::size_t index = fromIndex; index <= toIndex; ++index) {
    const Row& row = rows[index];
    double rowStart = horizontal ? row.offsetX : row.offsetY;
    double rowSize = horizontal ? row.width : row.height;
    addFlip(rowStart - windowSize - overscanSize);
    addFlip(rowStart + rowSize + overscanSize);
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

const Row& Container::getRowAtIndex(std::size_t index) const {
  if (index >= revision.rows.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  return revision.rows[index];
}

std::size_t Container::getRowCount() const {
  return revision.rows.size();
}

IndexRange Container::getMeasuredRange() const {
  if (revision.measuredLow == UNDEFINED_INDEX || revision.measuredHigh == UNDEFINED_INDEX) {
    return {};
  }
  return {revision.measuredLow, revision.measuredHigh};
}

IndexRange Container::getViewableIndices(const ViewableRule& rule) const {
  IndexRange measured = getMeasuredRange();
  if (measured.isEmpty()) {
    return {};
  }

  double viewportStart = getOffset();
  double windowSize = getWindowSize();
  double viewportEnd = viewportStart + windowSize;

  IndexRange viewable;
  for (std::size_t nextRowIndex = measured.low; nextRowIndex <= measured.high && nextRowIndex < revision.rows.size(); ++nextRowIndex) {
    const Row& nextRow = revision.rows[nextRowIndex];
    double rowStart = horizontal ? nextRow.offsetX : nextRow.offsetY;
    double rowSize = horizontal ? nextRow.width : nextRow.height;
    if (rowSize <= 0.0) {
      continue;
    }
    double rowEnd = rowStart + rowSize;

    double overlapStart = rowStart > viewportStart ? rowStart : viewportStart;
    double overlapEnd = rowEnd < viewportEnd ? rowEnd : viewportEnd;
    double visible = overlapEnd - overlapStart;

    /*
     * Compare against the smaller of row and viewport. A row taller than the screen
     * can still count as fully visible. Coverage compares against the viewport, and a row
     * fully on screen always counts.
     */
    double referenceSize = rule.coverage ? windowSize : (rowSize < windowSize ? rowSize : windowSize);
    bool fullyVisible = rule.coverage && visible >= rowSize;
    if (visible > 0.0 && referenceSize > 0.0 && (fullyVisible || (visible / referenceSize) >= rule.threshold)) {
      if (viewable.low == UNDEFINED_INDEX) {
        viewable.low = nextRowIndex;
      }
      viewable.high = nextRowIndex;
    }
  }
  return viewable;
}

void Container::setPredictedSize(const std::string& key, Size size) {
  if (key.empty()) {
    return;
  }
  predictedSizes[key] = size;
}

bool Container::hasTrustedSize(std::size_t index) const {
  if (index >= revision.rows.size()) {
    return false;
  }

  const Row& nextRow = revision.rows[index];
  return nextRow.measured || nextRow.predicted;
}

double Container::getRowOffset(std::size_t index) const {
  if (index >= revision.rows.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  const Row& nextRow = revision.rows[index];
  return horizontal ? nextRow.offsetX : nextRow.offsetY;
}

double Container::getRowSize(std::size_t index) const {
  if (index >= revision.rows.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  const Row& nextRow = revision.rows[index];
  return horizontal ? nextRow.width : nextRow.height;
}

double Container::getOffset() const {
  return horizontal ? revision.offsetX : revision.offsetY;
}

double Container::getWindowSize() const {
  return horizontal ? revision.windowWidth : revision.windowHeight;
}

void Container::setEndReachedEnabled(bool enabled) {
  endReachedEnabled = enabled;
}

void Container::setStartReachedEnabled(bool enabled) {
  startReachedEnabled = enabled;
}

}
