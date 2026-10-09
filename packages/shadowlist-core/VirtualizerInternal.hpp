#pragma once

#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <cmath>
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

}
