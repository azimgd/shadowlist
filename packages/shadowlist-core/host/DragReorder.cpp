#include <shadowlist-core/host/DragReorder.hpp>

#include <algorithm>
#include <utility>

namespace azimgd::shadowlist {

double dragAutoScrollDelta(const DragAutoScrollConfig& config, double touch, double windowSize) {
  if (config.edge <= 0.0) {
    return 0.0;
  }
  if (touch < config.edge) {
    return -config.maxSpeed * (1.0 - touch / config.edge);
  }
  if (touch > windowSize - config.edge) {
    return config.maxSpeed * (1.0 - (windowSize - touch) / config.edge);
  }
  return 0.0;
}

double dragAutoScrollOffset(const DragAutoScrollConfig& config, double touch, double windowSize, double offset, double maxOffset) {
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
long insertionPosition(std::size_t count, long originIndex, double center, RowAt row) {
  long insertion = originIndex;
  long position = -1;
  for (std::size_t current = 0; current < count; ++current) {
    long index = 0;
    double leading = 0.0;
    double extent = 0.0;
    row(current, index, leading, extent);
    if (index < 0) {
      continue;
    }
    double midpoint = leading + extent / 2.0;
    if ((index > originIndex && center > midpoint && index > insertion) ||
        (index < originIndex && center < midpoint && index < insertion)) {
      insertion = index;
      position = static_cast<long>(current);
    }
  }
  return position;
}

/*
 * A grid cell's resting frame.
 */
struct CellFrame {
  long index = -1;
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
long gridInsertionPosition(std::size_t count, CellAt cellAt, const DragRow& held, long insertionIndex, double center, double crossCenter) {
  long kept = -1;
  for (std::size_t position = 0; position < count; ++position) {
    CellFrame cell = cellAt(position);
    if (cell.index < 0) {
      continue;
    }
    if (frameContains(cell, center, crossCenter)) {
      return static_cast<long>(position);
    }
    if (cell.index == insertionIndex) {
      kept = static_cast<long>(position);
    }
  }
  if (frameContains(frameOf(held), center, crossCenter) || insertionIndex == held.index) {
    return -1;
  }
  return kept;
}

constexpr long NO_CELL = -1;
constexpr long HELD_CELL = -2;

/*
 * Lay the cells from the lower of origin and drop spot onward out again in their new order.
 * write(position, offset) gets every cell's shift, zero for the ones that stay.
 */
template <typename CellAt, typename Write>
void gridShifts(std::size_t count, CellAt cellAt, const DragRow& held, long insertionIndex, std::size_t columns, Write write) {
  long maxIndex = held.index;
  for (std::size_t position = 0; position < count; ++position) {
    write(position, DragOffset{});
    maxIndex = std::max(maxIndex, cellAt(position).index);
  }
  long origin = held.index;
  if (columns == 0 || origin < 0 || insertionIndex < 0 || origin == insertionIndex) {
    return;
  }
  long low = std::min(origin, insertionIndex);
  if (insertionIndex > maxIndex) {
    return;
  }

  // Position of each index from low in the cells, or the held cell.
  static thread_local std::vector<long> slots;
  slots.assign(static_cast<std::size_t>(maxIndex - low + 1), NO_CELL);
  for (std::size_t position = 0; position < count; ++position) {
    long index = cellAt(position).index;
    if (index >= low) {
      slots[static_cast<std::size_t>(index - low)] = static_cast<long>(position);
    }
  }
  slots[static_cast<std::size_t>(origin - low)] = HELD_CELL;
  auto slotOf = [&](long index) {
    return index < low || index > maxIndex ? NO_CELL : slots[static_cast<std::size_t>(index - low)];
  };
  auto frameAt = [&](long slot) {
    return slot == HELD_CELL ? frameOf(held) : cellAt(static_cast<std::size_t>(slot));
  };
  // The cell that sits at index after the move.
  auto sourceOf = [&](long index) {
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

  for (long index = low; index <= maxIndex; ++index) {
    std::size_t lane = static_cast<std::size_t>(index) % columns;
    long restingSlot = slotOf(index);
    if (!known[lane] && restingSlot != NO_CELL) {
      CellFrame resting = frameAt(restingSlot);
      lanes[lane] = {resting.leading, resting.crossLeading};
      known[lane] = 1;
    }
    long sourceSlot = slotOf(sourceOf(index));
    if (sourceSlot == NO_CELL) {
      known[lane] = 0;
      continue;
    }
    CellFrame source = frameAt(sourceSlot);
    if (!known[lane]) {
      long belowSlot = slotOf(index + static_cast<long>(columns));
      if (belowSlot == NO_CELL) {
        continue;
      }
      CellFrame below = frameAt(belowSlot);
      lanes[lane] = {below.leading - source.extent, below.crossLeading};
      known[lane] = 1;
    }
    if (sourceSlot != HELD_CELL) {
      write(static_cast<std::size_t>(sourceSlot),
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

long dragGridInsertionPosition(const DragCells& cells, const DragRow& held, long insertionIndex, double center, double crossCenter) {
  return gridInsertionPosition(cells.count, [&](std::size_t position) { return cellFrameAt(cells, position); },
    held, insertionIndex, center, crossCenter);
}

void dragGridShifts(const DragCells& cells, const DragRow& held, long insertionIndex, std::size_t columns, double* shifts, double* crossShifts) {
  gridShifts(cells.count, [&](std::size_t position) { return cellFrameAt(cells, position); },
    held, insertionIndex, columns, [&](std::size_t position, DragOffset offset) {
      shifts[position] = offset.leading;
      crossShifts[position] = offset.cross;
    });
}

long dragInsertionPosition(const long* indices, const double* leadings, const double* extents, std::size_t count, long originIndex, double center) {
  return insertionPosition(count, originIndex, center, [&](std::size_t current, long& index, double& leading, double& extent) {
    index = indices[current];
    leading = leadings[current];
    extent = extents[current];
  });
}

double dragShift(long originIndex, long insertionIndex, double draggedExtent, long index) {
  if (originIndex < insertionIndex && index > originIndex && index <= insertionIndex) {
    return -draggedExtent;
  }
  if (insertionIndex < originIndex && index >= insertionIndex && index < originIndex) {
    return draggedExtent;
  }
  return 0.0;
}

void DragReorder::begin(long index, std::string key, double restingLeading, double extent, double touchContent) {
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

void DragReorder::updateOrigin(long index, const std::string& key) {
  if (index >= 0) {
    originIndex_ = index;
  }
  if (!key.empty()) {
    originKey_ = key;
  }
}

double DragReorder::place(double touchContent, double restingLeading, double extent, double contentExtent) {
  leading_ = dragHeldLeading(touchContent, grabOffset_, extent, contentExtent);
  center_ = leading_ + extent / 2.0;
  return leading_ - restingLeading;
}

DragOffset DragReorder::placeCell(double touchContent, double touchCross, const DragRow& resting, double contentExtent, double crossContentExtent) {
  heldResting_.leading = resting.leading;
  heldResting_.extent = resting.extent;
  heldResting_.crossLeading = resting.crossLeading;
  heldResting_.crossExtent = resting.crossExtent;
  double translation = place(touchContent, resting.leading, resting.extent, contentExtent);
  crossLeading_ = dragHeldLeading(touchCross, crossGrabOffset_, resting.crossExtent, crossContentExtent);
  crossCenter_ = crossLeading_ + resting.crossExtent / 2.0;
  return {translation, crossLeading_ - resting.crossLeading};
}

DragOffset DragReorder::offsetFor(long index) const {
  if (!isGrid()) {
    return {shiftFor(index), 0.0};
  }
  if (index < offsetsBase_ || index - offsetsBase_ >= static_cast<long>(offsets_.size())) {
    return {};
  }
  return offsets_[static_cast<std::size_t>(index - offsetsBase_)];
}

void DragReorder::updateInsertion(const std::vector<DragRow>& rows) {
  if (isGrid()) {
    updateGridInsertion(rows);
    return;
  }
  long position = insertionPosition(rows.size(), originIndex_, center_, [&](std::size_t current, long& index, double& leading, double& extent) {
    index = rows[current].index;
    leading = rows[current].leading;
    extent = rows[current].extent;
  });
  if (position < 0) {
    // Nothing passed. The drop names the held row itself and nothing moves.
    insertionIndex_ = originIndex_;
    insertionKey_ = originKey_;
    return;
  }
  const DragRow& row = rows[static_cast<std::size_t>(position)];
  insertionIndex_ = row.index;
  insertionKey_ = row.key.empty() ? originKey_ : row.key;
}

void DragReorder::updateGridInsertion(const std::vector<DragRow>& rows) {
  heldResting_.index = originIndex_;
  auto cellAt = [&](std::size_t position) { return frameOf(rows[position]); };
  long position = gridInsertionPosition(rows.size(), cellAt, heldResting_, insertionIndex_, center_, crossCenter_);
  if (position < 0) {
    insertionIndex_ = originIndex_;
    insertionKey_ = originKey_;
  } else {
    const DragRow& row = rows[static_cast<std::size_t>(position)];
    insertionIndex_ = row.index;
    insertionKey_ = row.key.empty() ? originKey_ : row.key;
  }

  long first = -1;
  long last = -1;
  for (const DragRow& row : rows) {
    if (row.index >= 0) {
      first = first < 0 ? row.index : std::min(first, row.index);
      last = std::max(last, row.index);
    }
  }
  offsets_.clear();
  if (first < 0) {
    return;
  }
  offsetsBase_ = first;
  offsets_.assign(static_cast<std::size_t>(last - first + 1), DragOffset{});
  gridShifts(rows.size(), cellAt, heldResting_, insertionIndex_, columns_, [&](std::size_t current, DragOffset offset) {
    long index = rows[current].index;
    if (index >= 0) {
      offsets_[static_cast<std::size_t>(index - offsetsBase_)] = offset;
    }
  });
}

}
