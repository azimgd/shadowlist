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
 * Every pass uses this same value. If two passes disagreed, a row entering the measured range
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
  container.state.offsetCorrected = true;
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
 * True when an inverted list with followAppends rested at its bottom when this frame started.
 * It then keeps the bottom as any row is inserted or grows in place, not only the newest.
 */
inline bool followsBottom(const Container& container) {
  return container.inverted && container.followAppends && container.state.restingAtInvertedBottom;
}

/*
 * Furthest right and bottom edges of the content, read from the last row of each column.
 * That is enough along the scroll axis. Across it, callers also use maxCrossAxisExtent.
 */
inline Size tailExtent(const Container& container) {
  const std::vector<Row>& rows = container.revision.rows;
  if (container.state.hasRowSpans) {
    if (container.state.spanLayoutRowCount == rows.size()) {
      double end = container.state.spanLayout.getContentEnd();
      double cross = container.state.maxCrossAxisExtent;
      return container.horizontal ? Size{end, cross} : Size{cross, end};
    }
    // Removing the tail can leave the summary of the old suffix. Read actual remaining rows.
    Size extent{0.0, 0.0};
    for (const Row& row : rows) {
      extent.width = std::max(extent.width, row.offsetX + row.width);
      extent.height = std::max(extent.height, row.offsetY + row.height);
    }
    return extent;
  }
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
 * True when every row in the window that ends the content has a measured or predicted size. The
 * end is then more than a guess, and no row on screen there moves it once its size comes in.
 * In a grid the last row by index can end a shorter track, far above that window and never
 * measured. The walk back stops once a row of every track ends above the window.
 */
inline bool hasTrustedEnd(const Container& container) {
  const std::vector<Row>& rows = container.revision.rows;
  std::size_t columns = container.numberOfColumns > 0 ? container.numberOfColumns : 1;
  Size extent = tailExtent(container);
  double contentEnd = container.horizontal ? extent.width : extent.height;
  double windowStart = contentEnd + container.footerSize - container.getWindowSize();
  if (container.state.hasRowSpans) {
    for (std::size_t index = 0; index < rows.size(); ++index) {
      double end =
        container.horizontal ? rows[index].offsetX + rows[index].width : rows[index].offsetY + rows[index].height;
      if (end > windowStart && !container.hasTrustedSize(index)) {
        return false;
      }
    }
    return !rows.empty();
  }
  std::size_t tracksPast = 0;
  for (std::size_t nextRowIndex = rows.size(); nextRowIndex-- > 0 && tracksPast < columns;) {
    const Row& nextRow = rows[nextRowIndex];
    double rowEnd = container.horizontal ? nextRow.offsetX + nextRow.width : nextRow.offsetY + nextRow.height;
    if (rowEnd <= windowStart) {
      ++tracksPast;
      continue;
    }
    tracksPast = 0;
    if (!container.hasTrustedSize(nextRowIndex)) {
      return false;
    }
  }
  return !rows.empty();
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
