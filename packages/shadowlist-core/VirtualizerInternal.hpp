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
  auto [estimatedWidth, estimatedHeight] = container.estimatedElementSize;
  return {
    container.revision.averageElementWidth > 0.0 ? container.revision.averageElementWidth : estimatedWidth,
    container.revision.averageElementHeight > 0.0 ? container.revision.averageElementHeight : estimatedHeight,
  };
}

/*
 * Writable scroll offset along the scroll axis.
 */
inline double& scrollAxisOffset(Container& container) {
  return container.horizontal ? container.revision.containerOffsetX : container.revision.containerOffsetY;
}

/*
 * Move the scroll offset and flag the frame so the host applies it.
 */
inline void correctOffset(Container& container, double offset) {
  scrollAxisOffset(container) = offset;
  container.containerOffsetCorrected = true;
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
  const std::vector<Element>& elements = container.revision.elements;
  if (index >= elements.size() || !container.isAnchorable(elements[index].key)) {
    return false;
  }
  /*
   * Walk back from the end. Only a few trailing decoration rows sit after the newest content
   * row. A forward walk from an anchor far up the list would visit every row, and this runs
   * on each measured row.
   */
  for (std::size_t nextElementIndex = elements.size() - 1; nextElementIndex > index; --nextElementIndex) {
    if (container.isAnchorable(elements[nextElementIndex].key)) {
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
  const std::vector<Element>& elements = container.revision.elements;
  std::size_t scanColumns = container.columns > 0 ? container.columns : 1;
  std::size_t scanFrom = elements.size() > scanColumns ? elements.size() - scanColumns : 0;
  Size extent{0.0, 0.0};
  for (std::size_t nextElementIndex = scanFrom; nextElementIndex < elements.size(); ++nextElementIndex) {
    const Element& nextElement = elements[nextElementIndex];
    extent.width = std::max(extent.width, nextElement.offsetX + nextElement.width);
    extent.height = std::max(extent.height, nextElement.offsetY + nextElement.height);
  }
  return extent;
}

/*
 * How far past the top of the viewport the anchored row should sit.
 * Keeping the visible content in place stores this in pixels. A scroll to a key works it
 * out again each frame from the free space around the row. It stays right while the
 * row's size or the window size is still settling.
 */
inline double resolveAnchorSubOffset(Container& container, const Operation& operation, std::size_t anchorIndex) {
  if (operation.type != OperationType::ScrollToKey) {
    return operation.target.subOffset;
  }
  double freeSpace = container.getWindowContainerSize() - container.getElementSize(anchorIndex);
  return (freeSpace > 0.0 ? -operation.viewPosition * freeSpace : 0.0) + operation.rowOffset;
}

}
