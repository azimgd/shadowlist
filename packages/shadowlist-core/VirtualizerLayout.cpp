#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/VirtualizerInternal.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>

namespace azimgd::shadowlist {

namespace {
/*
 * Reflow a single column. Built once per axis so the loop skips the orientation check.
 */
template <double Row::*offset, double Row::*size, double Row::*crossOffset, double Row::*crossSize>
void reflowSingleTrack(
  Row* rows,
  std::size_t fromIndex,
  std::size_t rowCount,
  std::size_t changedThroughIndex,
  bool canStopEarly,
  double nextOffset,
  double& crossMax,
  bool& anyOffsetChanged) {
  for (std::size_t nextRowIndex = fromIndex; nextRowIndex < rowCount; ++nextRowIndex) {
    Row& nextRow = rows[nextRowIndex];
    if (nextRow.index != nextRowIndex) {
      nextRow.index = nextRowIndex;
    } else if (canStopEarly && nextRowIndex > changedThroughIndex &&
               nextRow.*offset == nextOffset) {
      break;
    }

    anyOffsetChanged = anyOffsetChanged || nextRow.*offset != nextOffset;
    nextRow.*offset = nextOffset;
    nextOffset += nextRow.*size;

    double crossExtent = nextRow.*crossOffset + nextRow.*crossSize;
    if (crossExtent > crossMax) {
      crossMax = crossExtent;
    }
  }
}

/*
 * Reflow several columns. Rows go to columns in turn and take the column's width.
 */
template <double Row::*offset, double Row::*size, double Row::*crossOffset, double Row::*crossSize>
void reflowTracks(
  Container& container,
  std::size_t fromIndex,
  double trackSize,
  double& crossMax,
  bool& anyOffsetChanged) {
  std::vector<Row>& rows = container.revision.rows;
  std::size_t columns = container.numberOfColumns;

  // Columns start below the header.
  std::vector<double> trackSizes(columns, container.headerSize);

  /*
   * Start each column at the end of its last row before fromIndex. Those rows are all
   * within one row per column back.
   */
  for (std::size_t seedIndex = fromIndex; seedIndex-- > 0 && seedIndex + columns >= fromIndex;) {
    const Row& seedRow = rows[seedIndex];
    trackSizes[seedIndex % columns] = seedRow.*offset + seedRow.*size;
  }

  for (std::size_t nextRowIndex = fromIndex; nextRowIndex < rows.size(); ++nextRowIndex) {
    Row& nextRow = rows[nextRowIndex];
    nextRow.index = nextRowIndex;

    std::size_t trackIndex = nextRowIndex % columns;

    // Set the width too. A reflow after the window size is known fixes it.
    anyOffsetChanged = anyOffsetChanged || nextRow.*offset != trackSizes[trackIndex] ||
      nextRow.*crossOffset != trackIndex * trackSize;
    nextRow.*crossOffset = trackIndex * trackSize;
    nextRow.*crossSize = trackSize;
    nextRow.*offset = trackSizes[trackIndex];
    trackSizes[trackIndex] += nextRow.*size;

    double crossExtent = nextRow.*crossOffset + nextRow.*crossSize;
    if (crossExtent > crossMax) {
      crossMax = crossExtent;
    }
  }
}
}

void Virtualizer::layoutRows(Container& container) {
  std::size_t rowCount = container.revision.rows.size();

  double trackSize = container.horizontal
    ? container.revision.windowHeight / (container.numberOfColumns > 0 ? container.numberOfColumns : 1)
    : container.revision.windowWidth / (container.numberOfColumns > 0 ? container.numberOfColumns : 1);

  // Unmeasured rows get the average size, or the estimate until there is an average.
  auto [fallbackWidth, fallbackHeight] = effectiveFallbackSize(container);

  /*
   * Only inputs that move rows count. The footer and the window size along the scroll axis
   * never do. A chat composer resizing the list doesn't walk every row. Columns take
   * their width from the window's cross size. That one counts.
   */
  bool layoutParamsChanged =
    container.headerSize != container.previousLayoutHeaderSize ||
    (container.horizontal
      ? container.revision.windowHeight != container.previousLayoutWindowHeight
      : container.revision.windowWidth != container.previousLayoutWindowWidth) ||
    container.numberOfColumns != container.previousLayoutNumberOfColumns ||
    container.horizontal != container.previousLayoutHorizontal;

  /*
   * The sizing loop visits every row. Only run it when it can change something:
   * a new fallback size, a new layout setting, or new rows. Otherwise every scroll frame
   * would walk the whole list for nothing.
   */
  bool fallbackDimensionsChanged =
    fallbackWidth != container.previousFallbackWidth ||
    fallbackHeight != container.previousFallbackHeight;

  /*
   * Rows before the first one a structure change touched kept their size and place. A
   * removed row may have been the widest one, and only a full pass finds the new widest.
   */
  std::size_t structureFrom = UNDEFINED_INDEX;
  if (container.rowStructureDirty) {
    double crossWindow = container.horizontal ? container.revision.windowHeight
                                              : container.revision.windowWidth;
    structureFrom = container.maxCrossAxisExtent > crossWindow ? 0 : container.rowStructureDirtyFromIndex;
  }
  bool fullSizing = fallbackDimensionsChanged || layoutParamsChanged;
  std::size_t sizingFrom = fullSizing ? 0 : std::min(structureFrom, rowCount);

  bool anyNewlyEstimated = false;

  if (fullSizing || container.rowStructureDirty) {
    if (container.numberOfColumns > 1) {
      double Row::*size = container.horizontal ? &Row::width : &Row::height;
      double Row::*crossSize = container.horizontal ? &Row::height : &Row::width;
      double fallbackSize = container.horizontal ? fallbackWidth : fallbackHeight;
      for (std::size_t nextRowIndex = sizingFrom; nextRowIndex < rowCount; ++nextRowIndex) {
        Row& nextRow = container.revision.rows[nextRowIndex];
        if (!nextRow.estimated && nextRow.*size != fallbackSize) {
          nextRow.*size = fallbackSize;
          anyNewlyEstimated = true;
        }
        if (nextRow.*crossSize != trackSize) {
          nextRow.*crossSize = trackSize;
          anyNewlyEstimated = true;
        }
      }
    } else {
      for (std::size_t nextRowIndex = sizingFrom; nextRowIndex < rowCount; ++nextRowIndex) {
        Row& nextRow = container.revision.rows[nextRowIndex];
        if (!nextRow.estimated &&
            (nextRow.width != fallbackWidth || nextRow.height != fallbackHeight)) {
          nextRow.width = fallbackWidth;
          nextRow.height = fallbackHeight;
          anyNewlyEstimated = true;
        }
      }
    }

    container.previousFallbackWidth = fallbackWidth;
    container.previousFallbackHeight = fallbackHeight;
  }

  // Recomputing offsets walks every row. Skip it unless a size, row or setting changed.
  bool sizesDirty = container.rowSizeDirtyFromIndex != UNDEFINED_INDEX;

  if (anyNewlyEstimated || layoutParamsChanged || container.rowStructureDirty || sizesDirty) {
    /*
     * Reflow from the first row that moved. Setting or fallback changes can move anything.
     * They start at 0. A structure or size change only moves the rows after it. After a
     * structure change every later row may have a new index. Walk them all.
     */
    std::size_t reflowFrom = std::min(structureFrom, container.rowSizeDirtyFromIndex);
    if (anyNewlyEstimated) {
      reflowFrom = std::min(reflowFrom, sizingFrom);
    }
    if (layoutParamsChanged) {
      reflowFrom = 0;
    }
    std::size_t changedThroughIndex =
      container.rowStructureDirty ? UNDEFINED_INDEX : container.rowSizeDirtyToIndex;

    recomputeRowOffsets(container, reflowFrom, changedThroughIndex);
    container.previousLayoutHeaderSize = container.headerSize;
    container.previousLayoutWindowWidth = container.revision.windowWidth;
    container.previousLayoutWindowHeight = container.revision.windowHeight;
    container.previousLayoutNumberOfColumns = container.numberOfColumns;
    container.previousLayoutHorizontal = container.horizontal;
    container.rowStructureDirty = false;
    container.rowSizeDirtyFromIndex = UNDEFINED_INDEX;
    container.rowSizeDirtyToIndex = 0;
  }
}

void Virtualizer::recomputeRowOffsets(
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

  std::size_t rowCount = container.revision.rows.size();

  // A full pass rebuilds the widest extent. A partial pass can only grow it.
  double crossMax = fromIndex == 0 ? 0.0 : container.maxCrossAxisExtent;

  if (fromIndex >= rowCount) {
    container.maxCrossAxisExtent = crossMax;
    return;
  }

  if (container.numberOfColumns > 1) {
    double trackSize = container.horizontal
      ? container.revision.windowHeight / container.numberOfColumns
      : container.revision.windowWidth / container.numberOfColumns;
    if (container.horizontal) {
      reflowTracks<&Row::offsetX, &Row::width, &Row::offsetY, &Row::height>(
        container, fromIndex, trackSize, crossMax, anyOffsetChanged);
    } else {
      reflowTracks<&Row::offsetY, &Row::height, &Row::offsetX, &Row::width>(
        container, fromIndex, trackSize, crossMax, anyOffsetChanged);
    }
  } else {
    // Rows start below the header, or right after the row before fromIndex.
    double nextOffset = container.headerSize;

    if (fromIndex > 0) {
      const Row& previousRow = container.revision.rows[fromIndex - 1];
      nextOffset = container.horizontal
        ? previousRow.offsetX + previousRow.width
        : previousRow.offsetY + previousRow.height;
    }

    /*
     * This is the hottest loop in the core. The orientation check is kept out of it, and
     * a row's index is only written when it is stale, to avoid needless memory writes.
     */
    Row* rows = container.revision.rows.data();

    /*
     * Past the last changed row, stop at the first row already at the right offset.
     * Every row after it is correct too. A reflow that moves nothing costs almost nothing.
     * Two guards: don't stop between two changed rows, and a full pass from 0 must visit
     * every row to rebuild the widest extent.
     */
    bool canStopEarly = fromIndex > 0 && changedThroughIndex != UNDEFINED_INDEX;

    if (container.horizontal) {
      reflowSingleTrack<&Row::offsetX, &Row::width, &Row::offsetY, &Row::height>(
        rows, fromIndex, rowCount, changedThroughIndex, canStopEarly, nextOffset, crossMax, anyOffsetChanged);
    } else {
      reflowSingleTrack<&Row::offsetY, &Row::height, &Row::offsetX, &Row::width>(
        rows, fromIndex, rowCount, changedThroughIndex, canStopEarly, nextOffset, crossMax, anyOffsetChanged);
    }
  }

  container.maxCrossAxisExtent = crossMax;

  if (anyOffsetChanged) {
    container.geometryVersion++;
  }
}

void Virtualizer::recomputeContentSize(Container& container) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  // The content size is the furthest row edge. Row offsets already include the header.
  Size extent = tailExtent(container);

  /*
   * Along the scroll axis, add the footer and never go below the header.
   * Across it, never go below the window to keep columns from collapsing to zero width,
   * and cover any row measured wider than the window.
   */
  if (container.horizontal) {
    container.revision.contentWidth = std::max(extent.width, container.headerSize) + container.footerSize;
    container.revision.contentHeight =
      std::max({extent.height, container.maxCrossAxisExtent, container.revision.windowHeight});
  } else {
    container.revision.contentHeight = std::max(extent.height, container.headerSize) + container.footerSize;
    container.revision.contentWidth =
      std::max({extent.width, container.maxCrossAxisExtent, container.revision.windowWidth});
  }

  /*
   * Freeze the average once, from the first real measurements. Unmeasured rows then keep
   * a stable size and the visible content can be held in place. The average is whole. A host
   * that measures in whole pixels then only ever sees whole corrections, which it can apply to
   * its scroll offset exactly.
   */
  if (container.revision.measuredRealCount > 0) {
    if (container.revision.averageRowWidth == 0.0) {
      container.revision.averageRowWidth =
        std::round(container.revision.measuredRealTotalWidth / container.revision.measuredRealCount);
    }
    if (container.revision.averageRowHeight == 0.0) {
      container.revision.averageRowHeight =
        std::round(container.revision.measuredRealTotalHeight / container.revision.measuredRealCount);
    }
  }
}

}
