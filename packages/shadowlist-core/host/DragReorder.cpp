#include <shadowlist-core/host/DragReorder.hpp>

#include <algorithm>
#include <utility>

namespace azimgd::shadowlist {

double dragAutoScrollDelta(const DragAutoScrollConfig& config, double touch, double windowSize) {
  if (config.edge <= 0.0) {
    return 0.0;
  }
  // A finger past the viewport edge scrolls at maxSpeed, not faster.
  if (touch < config.edge) {
    return -config.maxSpeed * std::min(1.0, 1.0 - touch / config.edge);
  }
  if (touch > windowSize - config.edge) {
    return config.maxSpeed * std::min(1.0, 1.0 - (windowSize - touch) / config.edge);
  }
  return 0.0;
}

double dragAutoScrollOffset(
  const DragAutoScrollConfig& config,
  double touch,
  double windowSize,
  double offset,
  double maxOffset) {
  double delta = dragAutoScrollDelta(config, touch, windowSize);
  if (delta == 0.0) {
    return offset;
  }
  return std::min(std::max(offset + delta, 0.0), std::max(0.0, maxOffset));
}

double dragHeldLeading(double touchContent, double grabOffset, double extent, double contentExtent) {
  double leading = touchContent - grabOffset;
  return std::max(0.0, std::min(leading, std::max(0.0, contentExtent - extent)));
}

namespace {

/*
 * Shared by the array form and the session. row(position) gives index, leading and extent.
 */
template <typename RowAt>
std::size_t insertionPosition(std::size_t count, std::size_t originIndex, double center, RowAt row) {
  std::size_t insertion = originIndex;
  std::size_t position = UNDEFINED_INDEX;
  for (std::size_t current = 0; current < count; ++current) {
    std::size_t index = 0;
    double leading = 0.0;
    double extent = 0.0;
    row(current, index, leading, extent);
    if (index == UNDEFINED_INDEX) {
      continue;
    }
    double midpoint = leading + extent / 2.0;
    if ((index > originIndex && center > midpoint && index > insertion) ||
        (index < originIndex && center < midpoint && index < insertion)) {
      insertion = index;
      position = current;
    }
  }
  return position;
}

/*
 * A grid cell's resting frame.
 */
struct CellFrame {
  std::size_t index = UNDEFINED_INDEX;
  double leading = 0.0;
  double extent = 0.0;
  double crossLeading = 0.0;
  double crossExtent = 0.0;
};

CellFrame frameOf(const DragRow& row) {
  return {row.index, row.leading, row.extent, row.crossLeading, row.crossExtent};
}

bool frameContains(const CellFrame& frame, double center, double crossCenter) {
  return center >= frame.leading && center < frame.leading + frame.extent &&
    crossCenter >= frame.crossLeading && crossCenter < frame.crossLeading + frame.crossExtent;
}

/*
 * Shared by the array form and the session. cellAt(position) gives a cell's frame.
 */
template <typename CellAt>
std::size_t gridInsertionPosition(
  std::size_t count,
  CellAt cellAt,
  const DragRow& held,
  std::size_t insertionIndex,
  double center,
  double crossCenter) {
  std::size_t kept = UNDEFINED_INDEX;
  for (std::size_t position = 0; position < count; ++position) {
    CellFrame cell = cellAt(position);
    if (cell.index == UNDEFINED_INDEX) {
      continue;
    }
    if (frameContains(cell, center, crossCenter)) {
      return position;
    }
    if (cell.index == insertionIndex) {
      kept = position;
    }
  }
  if (frameContains(frameOf(held), center, crossCenter) || insertionIndex == held.index) {
    return UNDEFINED_INDEX;
  }
  return kept;
}

/*
 * Slot values for an index with no mounted cell and for the held cell.
 */
constexpr std::size_t NO_CELL = UNDEFINED_INDEX;
constexpr std::size_t HELD_CELL = UNDEFINED_INDEX - 1;

/*
 * Lay the cells from the lower of origin and drop spot onward out again in their new order.
 * write(position, offset) gets every cell's shift, zero for the ones that stay.
 */
template <typename CellAt, typename Write>
void gridShifts(
  std::size_t count,
  CellAt cellAt,
  const DragRow& held,
  std::size_t insertionIndex,
  std::size_t columns,
  Write write) {
  std::size_t origin = held.index;
  if (origin == UNDEFINED_INDEX) {
    for (std::size_t position = 0; position < count; ++position) {
      write(position, DragOffset{});
    }
    return;
  }
  std::size_t maxIndex = origin;
  for (std::size_t position = 0; position < count; ++position) {
    write(position, DragOffset{});
    std::size_t index = cellAt(position).index;
    if (index != UNDEFINED_INDEX) {
      maxIndex = std::max(maxIndex, index);
    }
  }
  if (columns == 0 || insertionIndex == UNDEFINED_INDEX || origin == insertionIndex) {
    return;
  }
  std::size_t low = std::min(origin, insertionIndex);
  if (insertionIndex > maxIndex) {
    return;
  }

  // Position of each index from low in the cells, or the held cell.
  static thread_local std::vector<std::size_t> slots;
  slots.assign(maxIndex - low + 1, NO_CELL);
  for (std::size_t position = 0; position < count; ++position) {
    std::size_t index = cellAt(position).index;
    if (index != UNDEFINED_INDEX && index >= low) {
      slots[index - low] = position;
    }
  }
  slots[origin - low] = HELD_CELL;
  auto slotOf = [&](std::size_t index) {
    return index < low || index > maxIndex ? NO_CELL : slots[index - low];
  };
  auto frameAt = [&](std::size_t slot) {
    return slot == HELD_CELL ? frameOf(held) : cellAt(slot);
  };
  // The cell that sits at index after the move.
  auto sourceOf = [&](std::size_t index) {
    if (index == insertionIndex) {
      return origin;
    }
    if (origin < insertionIndex && index >= origin && index < insertionIndex) {
      return index + 1;
    }
    if (insertionIndex < origin && index > insertionIndex && index <= origin) {
      return index - 1;
    }
    return index;
  };

  // Where each column's stack ends, restarted after a cell that isn't mounted.
  static thread_local std::vector<DragOffset> lanes;
  static thread_local std::vector<char> known;
  lanes.assign(columns, DragOffset{});
  known.assign(columns, 0);

  for (std::size_t index = low; index <= maxIndex; ++index) {
    std::size_t lane = index % columns;
    std::size_t restingSlot = slotOf(index);
    if (!known[lane] && restingSlot != NO_CELL) {
      CellFrame resting = frameAt(restingSlot);
      lanes[lane] = {resting.leading, resting.crossLeading};
      known[lane] = 1;
    }
    std::size_t sourceSlot = slotOf(sourceOf(index));
    if (sourceSlot == NO_CELL) {
      known[lane] = 0;
      continue;
    }
    CellFrame source = frameAt(sourceSlot);
    if (!known[lane]) {
      std::size_t belowSlot = slotOf(index + columns);
      if (belowSlot == NO_CELL) {
        continue;
      }
      CellFrame below = frameAt(belowSlot);
      lanes[lane] = {below.leading - source.extent, below.crossLeading};
      known[lane] = 1;
    }
    if (sourceSlot != HELD_CELL) {
      write(sourceSlot,
        DragOffset{lanes[lane].leading - source.leading, lanes[lane].cross - source.crossLeading});
    }
    lanes[lane].leading += source.extent;
  }
}

CellFrame cellFrameAt(const DragCells& cells, std::size_t position) {
  return {cells.indices[position], cells.leadings[position], cells.extents[position],
    cells.crossLeadings[position], cells.crossExtents[position]};
}

}

std::size_t dragGridInsertionPosition(
  const DragCells& cells,
  const DragRow& held,
  std::size_t insertionIndex,
  double center,
  double crossCenter) {
  return gridInsertionPosition(cells.count, [&](std::size_t position) { return cellFrameAt(cells, position); },
    held, insertionIndex, center, crossCenter);
}

void dragGridShifts(
  const DragCells& cells,
  const DragRow& held,
  std::size_t insertionIndex,
  std::size_t columns,
  double* shifts,
  double* crossShifts) {
  gridShifts(cells.count, [&](std::size_t position) { return cellFrameAt(cells, position); },
    held, insertionIndex, columns, [&](std::size_t position, DragOffset offset) {
      shifts[position] = offset.leading;
      crossShifts[position] = offset.cross;
    });
}

std::size_t dragInsertionPosition(
  const std::size_t* indices,
  const double* leadings,
  const double* extents,
  std::size_t count,
  std::size_t originIndex,
  double center) {
  auto rowAt = [&](std::size_t current, std::size_t& index, double& leading, double& extent) {
    index = indices[current];
    leading = leadings[current];
    extent = extents[current];
  };
  return insertionPosition(count, originIndex, center, rowAt);
}

double dragShift(std::size_t originIndex, std::size_t insertionIndex, double draggedExtent, std::size_t index) {
  if (originIndex == UNDEFINED_INDEX || insertionIndex == UNDEFINED_INDEX) {
    return 0.0;
  }
  if (originIndex < insertionIndex && index > originIndex && index <= insertionIndex) {
    return -draggedExtent;
  }
  if (insertionIndex < originIndex && index >= insertionIndex && index < originIndex) {
    return draggedExtent;
  }
  return 0.0;
}

void DragReorder::begin(std::size_t index, std::string key, double restingLeading, double extent, double touchContent) {
  columns_ = 1;
  crossGrabOffset_ = 0.0;
  crossLeading_ = 0.0;
  crossCenter_ = 0.0;
  heldResting_ = DragRow{};
  offsets_.clear();
  originIndex_ = index;
  insertionIndex_ = index;
  originKey_ = std::move(key);
  insertionKey_ = originKey_;
  draggedExtent_ = extent;
  grabOffset_ = touchContent - restingLeading;
  leading_ = restingLeading;
  center_ = restingLeading + extent / 2.0;
}

void DragReorder::beginCell(const DragRow& resting, double touchContent, double touchCross, std::size_t columns) {
  begin(resting.index, resting.key, resting.leading, resting.extent, touchContent);
  columns_ = std::max<std::size_t>(1, columns);
  heldResting_ = resting;
  crossGrabOffset_ = touchCross - resting.crossLeading;
  crossLeading_ = resting.crossLeading;
  crossCenter_ = resting.crossLeading + resting.crossExtent / 2.0;
}

void DragReorder::updateOrigin(std::size_t index, const std::string& key) {
  if (index != UNDEFINED_INDEX) {
    originIndex_ = index;
  }
  if (!key.empty()) {
    originKey_ = key;
  }
}

void DragReorder::begin(const DragRow& resting, double touchAlong, double touchCross, std::size_t columns) {
  if (columns > 1) {
    beginCell(resting, touchAlong, touchCross, columns);
  } else {
    begin(resting.index, resting.key, resting.leading, resting.extent, touchAlong);
  }
}

DragOffset DragReorder::placeRow(
  double touchAlong,
  double touchCross,
  const DragRow& resting,
  double contentExtent,
  double crossExtent) {
  if (isGrid()) {
    return placeCell(touchAlong, touchCross, resting, contentExtent, crossExtent);
  }
  DragOffset offset;
  offset.leading = place(touchAlong, resting.leading, resting.extent, contentExtent);
  return offset;
}

double DragReorder::place(double touchContent, double restingLeading, double extent, double contentExtent) {
  leading_ = dragHeldLeading(touchContent, grabOffset_, extent, contentExtent);
  center_ = leading_ + extent / 2.0;
  return leading_ - restingLeading;
}

DragOffset DragReorder::placeCell(
  double touchContent,
  double touchCross,
  const DragRow& resting,
  double contentExtent,
  double crossContentExtent) {
  heldResting_.leading = resting.leading;
  heldResting_.extent = resting.extent;
  heldResting_.crossLeading = resting.crossLeading;
  heldResting_.crossExtent = resting.crossExtent;
  double translation = place(touchContent, resting.leading, resting.extent, contentExtent);
  crossLeading_ = dragHeldLeading(touchCross, crossGrabOffset_, resting.crossExtent, crossContentExtent);
  crossCenter_ = crossLeading_ + resting.crossExtent / 2.0;
  return {translation, crossLeading_ - resting.crossLeading};
}

DragOffset DragReorder::offsetFor(std::size_t index) const {
  if (!isGrid()) {
    return {shiftFor(index), 0.0};
  }
  if (index < offsetsBase_ || index - offsetsBase_ >= offsets_.size()) {
    return {};
  }
  return offsets_[index - offsetsBase_];
}

void DragReorder::updateInsertion(const std::vector<DragRow>& rows) {
  if (isGrid()) {
    updateGridInsertion(rows);
    return;
  }
  auto rowAt = [&](std::size_t current, std::size_t& index, double& leading, double& extent) {
    index = rows[current].index;
    leading = rows[current].leading;
    extent = rows[current].extent;
  };
  std::size_t position = insertionPosition(rows.size(), originIndex_, center_, rowAt);
  if (position == UNDEFINED_INDEX) {
    // Nothing passed. The drop names the held row itself and nothing moves.
    insertionIndex_ = originIndex_;
    insertionKey_ = originKey_;
    return;
  }
  const DragRow& row = rows[position];
  insertionIndex_ = row.index;
  insertionKey_ = row.key.empty() ? originKey_ : row.key;
}

void DragReorder::updateGridInsertion(const std::vector<DragRow>& rows) {
  heldResting_.index = originIndex_;
  auto cellAt = [&](std::size_t position) { return frameOf(rows[position]); };
  std::size_t position =
    gridInsertionPosition(rows.size(), cellAt, heldResting_, insertionIndex_, center_, crossCenter_);
  if (position == UNDEFINED_INDEX) {
    insertionIndex_ = originIndex_;
    insertionKey_ = originKey_;
  } else {
    const DragRow& row = rows[position];
    insertionIndex_ = row.index;
    insertionKey_ = row.key.empty() ? originKey_ : row.key;
  }

  std::size_t first = UNDEFINED_INDEX;
  std::size_t last = 0;
  for (const DragRow& row : rows) {
    if (row.index != UNDEFINED_INDEX) {
      first = std::min(first, row.index);
      last = std::max(last, row.index);
    }
  }
  offsets_.clear();
  if (first == UNDEFINED_INDEX) {
    return;
  }
  offsetsBase_ = first;
  offsets_.assign(last - first + 1, DragOffset{});
  gridShifts(rows.size(), cellAt, heldResting_, insertionIndex_, columns_, [&](std::size_t current, DragOffset offset) {
    std::size_t index = rows[current].index;
    if (index != UNDEFINED_INDEX) {
      offsets_[index - offsetsBase_] = offset;
    }
  });
}

}
