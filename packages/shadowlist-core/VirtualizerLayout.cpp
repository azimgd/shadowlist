#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/VirtualizerInternal.hpp>

#include <algorithm>
#include <mutex>

namespace azimgd::shadowlist {

namespace {
/*
 * Reflow a single column. Built once per axis so the loop skips the orientation check.
 */
template <double Element::*offset, double Element::*size, double Element::*crossOffset, double Element::*crossSize>
void reflowSingleTrack(
  Element* elements,
  std::size_t fromIndex,
  std::size_t elementsSize,
  std::size_t changedThroughIndex,
  bool canStopEarly,
  double nextOffset,
  double& crossMax,
  bool& anyOffsetChanged) {
  for (std::size_t nextElementIndex = fromIndex; nextElementIndex < elementsSize; ++nextElementIndex) {
    Element& nextElement = elements[nextElementIndex];
    if (nextElement.index != nextElementIndex) {
      nextElement.index = nextElementIndex;
    } else if (canStopEarly && nextElementIndex > changedThroughIndex &&
               nextElement.*offset == nextOffset) {
      break;
    }

    anyOffsetChanged = anyOffsetChanged || nextElement.*offset != nextOffset;
    nextElement.*offset = nextOffset;
    nextOffset += nextElement.*size;

    double crossExtent = nextElement.*crossOffset + nextElement.*crossSize;
    if (crossExtent > crossMax) {
      crossMax = crossExtent;
    }
  }
}

/*
 * Reflow several columns. Rows go to columns in turn and take the column's width.
 */
template <double Element::*offset, double Element::*size, double Element::*crossOffset, double Element::*crossSize>
void reflowTracks(
  Container& container,
  std::size_t fromIndex,
  double trackSize,
  double& crossMax,
  bool& anyOffsetChanged) {
  std::vector<Element>& elements = container.revision.elements;
  std::size_t columns = container.columns;

  // Columns start below the header.
  std::vector<double> trackSizes(columns, container.headerSize);

  /*
   * Start each column at the end of its last row before fromIndex. Those rows are all
   * within one row per column back.
   */
  for (std::size_t seedIndex = fromIndex; seedIndex-- > 0 && seedIndex + columns >= fromIndex;) {
    const Element& seedElement = elements[seedIndex];
    trackSizes[seedIndex % columns] = seedElement.*offset + seedElement.*size;
  }

  for (std::size_t nextElementIndex = fromIndex; nextElementIndex < elements.size(); ++nextElementIndex) {
    Element& nextElement = elements[nextElementIndex];
    nextElement.index = nextElementIndex;

    std::size_t trackIndex = nextElementIndex % columns;

    // Set the width too. A reflow after the window size is known fixes it.
    anyOffsetChanged = anyOffsetChanged || nextElement.*offset != trackSizes[trackIndex] ||
      nextElement.*crossOffset != trackIndex * trackSize;
    nextElement.*crossOffset = trackIndex * trackSize;
    nextElement.*crossSize = trackSize;
    nextElement.*offset = trackSizes[trackIndex];
    trackSizes[trackIndex] += nextElement.*size;

    double crossExtent = nextElement.*crossOffset + nextElement.*crossSize;
    if (crossExtent > crossMax) {
      crossMax = crossExtent;
    }
  }
}
}

void Virtualizer::layoutElements(Container& container) {
  std::size_t elementsSize = container.revision.elements.size();

  double trackSize = container.horizontal
    ? container.revision.windowContainerHeight / (container.columns > 0 ? container.columns : 1)
    : container.revision.windowContainerWidth / (container.columns > 0 ? container.columns : 1);

  // Unmeasured rows get the average size, or the estimate until there is an average.
  auto [fallbackWidth, fallbackHeight] = effectiveFallbackSize(container);

  /*
   * Only inputs that move rows count. The footer and the window size along the scroll axis
   * never do. A chat composer resizing the list doesn't walk every row. Columns take
   * their width from the window's cross size. That one counts.
   */
  bool layoutParamsChanged =
    container.headerSize != container.lastLayoutHeaderSize ||
    (container.horizontal
      ? container.revision.windowContainerHeight != container.lastLayoutWindowHeight
      : container.revision.windowContainerWidth != container.lastLayoutWindowWidth) ||
    container.columns != container.lastLayoutColumns ||
    container.horizontal != container.lastLayoutHorizontal;

  /*
   * The sizing loop visits every row. Only run it when it can change something:
   * a new fallback size, a new layout setting, or new rows. Otherwise every scroll frame
   * would walk the whole list for nothing.
   */
  bool fallbackDimensionsChanged =
    fallbackWidth != container.lastFallbackWidth ||
    fallbackHeight != container.lastFallbackHeight;

  bool anyNewlyEstimated = false;

  if (fallbackDimensionsChanged || layoutParamsChanged || container.elementsStructureDirty) {
    if (container.columns > 1) {
      double Element::*size = container.horizontal ? &Element::width : &Element::height;
      double Element::*crossSize = container.horizontal ? &Element::height : &Element::width;
      double fallbackSize = container.horizontal ? fallbackWidth : fallbackHeight;
      for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
        Element& nextElement = container.revision.elements[nextElementIndex];
        if (!nextElement.estimated && nextElement.*size != fallbackSize) {
          nextElement.*size = fallbackSize;
          anyNewlyEstimated = true;
        }
        if (nextElement.*crossSize != trackSize) {
          nextElement.*crossSize = trackSize;
          anyNewlyEstimated = true;
        }
      }
    } else {
      for (std::size_t nextElementIndex = 0; nextElementIndex < elementsSize; ++nextElementIndex) {
        Element& nextElement = container.revision.elements[nextElementIndex];
        if (!nextElement.estimated &&
            (nextElement.width != fallbackWidth || nextElement.height != fallbackHeight)) {
          nextElement.width = fallbackWidth;
          nextElement.height = fallbackHeight;
          anyNewlyEstimated = true;
        }
      }
    }

    container.lastFallbackWidth = fallbackWidth;
    container.lastFallbackHeight = fallbackHeight;
  }

  // Recomputing offsets walks every row. Skip it unless a size, row or setting changed.
  bool sizesDirty = container.elementsSizeDirtyFromIndex != UNDEFINED_INDEX;

  if (anyNewlyEstimated || layoutParamsChanged || container.elementsStructureDirty || sizesDirty) {
    /*
     * Reflow from the first row that moved. Row, setting or fallback changes can move
     * anything. They start at 0. A size change only moves the rows after it.
     */
    std::size_t reflowFrom =
      (anyNewlyEstimated || layoutParamsChanged || container.elementsStructureDirty)
        ? 0
        : container.elementsSizeDirtyFromIndex;

    recomputeElementOffsets(container, reflowFrom, container.elementsSizeDirtyToIndex);
    container.lastLayoutHeaderSize = container.headerSize;
    container.lastLayoutWindowWidth = container.revision.windowContainerWidth;
    container.lastLayoutWindowHeight = container.revision.windowContainerHeight;
    container.lastLayoutColumns = container.columns;
    container.lastLayoutHorizontal = container.horizontal;
    container.elementsStructureDirty = false;
    container.elementsSizeDirtyFromIndex = UNDEFINED_INDEX;
    container.elementsSizeDirtyToIndex = 0;
  }
}

void Virtualizer::recomputeElementOffsets(
  Container& container,
  std::size_t fromIndex,
  std::size_t changedThroughIndex) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  /*
   * All row positions are written here. This is where snap and sticky caches go stale.
   * Only bump the geometry version when an offset really changed, or those caches get
   * thrown away for nothing.
   */
  bool anyOffsetChanged = false;

  std::size_t elementsSize = container.revision.elements.size();

  // A full pass rebuilds the widest extent. A partial pass can only grow it.
  double crossMax = fromIndex == 0 ? 0.0 : container.maxCrossAxisExtent;

  if (fromIndex >= elementsSize) {
    container.maxCrossAxisExtent = crossMax;
    return;
  }

  if (container.columns > 1) {
    double trackSize = container.horizontal
      ? container.revision.windowContainerHeight / container.columns
      : container.revision.windowContainerWidth / container.columns;
    if (container.horizontal) {
      reflowTracks<&Element::offsetX, &Element::width, &Element::offsetY, &Element::height>(
        container, fromIndex, trackSize, crossMax, anyOffsetChanged);
    } else {
      reflowTracks<&Element::offsetY, &Element::height, &Element::offsetX, &Element::width>(
        container, fromIndex, trackSize, crossMax, anyOffsetChanged);
    }
  } else {
    // Rows start below the header, or right after the row before fromIndex.
    double nextOffset = container.headerSize;

    if (fromIndex > 0) {
      const Element& previousElement = container.revision.elements[fromIndex - 1];
      nextOffset = container.horizontal
        ? previousElement.offsetX + previousElement.width
        : previousElement.offsetY + previousElement.height;
    }

    /*
     * This is the hottest loop in the core. The orientation check is kept out of it, and
     * a row's index is only written when it is stale, to avoid needless memory writes.
     */
    Element* elements = container.revision.elements.data();

    /*
     * Past the last changed row, stop at the first row already at the right offset.
     * Every row after it is correct too. A reflow that moves nothing costs almost nothing.
     * Two guards: don't stop between two changed rows, and a full pass from 0 must visit
     * every row to rebuild the widest extent.
     */
    bool canStopEarly = fromIndex > 0 && changedThroughIndex != UNDEFINED_INDEX;

    if (container.horizontal) {
      reflowSingleTrack<&Element::offsetX, &Element::width, &Element::offsetY, &Element::height>(
        elements, fromIndex, elementsSize, changedThroughIndex, canStopEarly, nextOffset, crossMax, anyOffsetChanged);
    } else {
      reflowSingleTrack<&Element::offsetY, &Element::height, &Element::offsetX, &Element::width>(
        elements, fromIndex, elementsSize, changedThroughIndex, canStopEarly, nextOffset, crossMax, anyOffsetChanged);
    }
  }

  container.maxCrossAxisExtent = crossMax;

  if (anyOffsetChanged) {
    container.geometryVersion++;
  }
}

void Virtualizer::recomputeTotalSize(Container& container) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  // The content size is the furthest row edge. Row offsets already include the header.
  Size extent = tailExtent(container);

  /*
   * Along the scroll axis, add the footer and never go below the header.
   * Across it, never go below the window to keep columns from collapsing to zero width,
   * and cover any row measured wider than the window.
   */
  if (container.horizontal) {
    container.revision.totalContainerWidth = std::max(extent.width, container.headerSize) + container.footerSize;
    container.revision.totalContainerHeight =
      std::max({extent.height, container.maxCrossAxisExtent, container.revision.windowContainerHeight});
  } else {
    container.revision.totalContainerHeight = std::max(extent.height, container.headerSize) + container.footerSize;
    container.revision.totalContainerWidth =
      std::max({extent.width, container.maxCrossAxisExtent, container.revision.windowContainerWidth});
  }

  /*
   * Freeze the average once, from the first real measurements. Unmeasured rows then keep
   * a stable size and the visible content can be held in place.
   */
  if (container.revision.measuredRealCount > 0) {
    if (container.revision.averageElementWidth == 0.0) {
      container.revision.averageElementWidth =
        container.revision.measuredRealTotalWidth / container.revision.measuredRealCount;
    }
    if (container.revision.averageElementHeight == 0.0) {
      container.revision.averageElementHeight =
        container.revision.measuredRealTotalHeight / container.revision.measuredRealCount;
    }
  }
}

}
