#pragma once

#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <utility>

/*
 * Helpers shared by the Virtualizer source files. Callers hold coreMutex.
 */
namespace azimgd::shadowlist {

/*
 * Size for a row that was never measured: the frozen average of real measurements once
 * there is one, otherwise the configured estimate.
 * Every pass uses this same value. If two passes disagreed, a row entering the window
 * would resize, reflow the rows after it, and a row pulled into range would miss a frame.
 */
inline std::pair<double, double> effectiveFallbackSize(const Container& container) {
  auto [estimatedWidth, estimatedHeight] = container.estimatedRowSize;
  return {
    container.revision.averageRowWidth > 0.0 ? container.revision.averageRowWidth : estimatedWidth,
    container.revision.averageRowHeight > 0.0 ? container.revision.averageRowHeight : estimatedHeight,
  };
}

/*
 * Writable scroll offset along the scroll axis.
 */
inline double& scrollAxisOffset(Container& container) {
  return container.horizontal ? container.revision.offsetX : container.revision.offsetY;
}

/*
 * Move the scroll offset and flag the frame so the host applies it.
 */
inline void correctOffset(Container& container, double offset) {
  scrollAxisOffset(container) = offset;
  container.offsetCorrected = true;
}

/*
 * True when an inverted list sits within INVERTED_FOLLOW_BAND of its bottom.
 */
inline bool atInvertedBottom(double offset, double total, double window) {
  return offset >= std::max(0.0, total - window) - INVERTED_FOLLOW_BAND;
}

/*
 * True when this is the newest content row, ignoring trailing decoration rows.
 */
inline bool isLastAnchorable(const Container& container, std::size_t index) {
  const std::vector<Row>& rows = container.revision.rows;
  if (index >= rows.size() || !container.isAnchorable(rows[index].key)) {
    return false;
  }
  /*
   * Walk back from the end. Only a few trailing decoration rows sit after the newest content
   * row. A forward walk from an anchor far up the list would visit every row, and this runs
   * on each measured row.
   */
  for (std::size_t nextRowIndex = rows.size() - 1; nextRowIndex > index; --nextRowIndex) {
    if (container.isAnchorable(rows[nextRowIndex].key)) {
      return false;
    }
  }
  return true;
}

/*
 * Furthest right and bottom edges of the content, read from the last row of each column.
 * That is enough along the scroll axis. Across it, callers also use maxCrossAxisExtent.
 */
inline Size tailExtent(const Container& container) {
  const std::vector<Row>& rows = container.revision.rows;
  std::size_t scanColumns = container.numberOfColumns > 0 ? container.numberOfColumns : 1;
  std::size_t scanFrom = rows.size() > scanColumns ? rows.size() - scanColumns : 0;
  Size extent{0.0, 0.0};
  for (std::size_t nextRowIndex = scanFrom; nextRowIndex < rows.size(); ++nextRowIndex) {
    const Row& nextRow = rows[nextRowIndex];
    extent.width = std::max(extent.width, nextRow.offsetX + nextRow.width);
    extent.height = std::max(extent.height, nextRow.offsetY + nextRow.height);
  }
  return extent;
}

/*
 * How far past the top of the viewport the anchored row should sit.
 * Keeping the visible content in place stores this in pixels. A scroll to a key works it
 * out again each frame from the free space around the row. It stays right while the
 * row's size or the window size is still settling.
 */
inline double resolveAnchorOffset(Container& container, const Operation& operation, std::size_t anchorIndex) {
  if (operation.type != OperationType::ScrollToKey) {
    return operation.target.offset;
  }
  double freeSpace = container.getWindowSize() - container.getRowSize(anchorIndex);
  return (freeSpace > 0.0 ? -operation.viewPosition * freeSpace : 0.0) + operation.viewOffset;
}

}
