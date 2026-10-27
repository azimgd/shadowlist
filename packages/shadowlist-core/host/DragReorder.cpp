#include <shadowlist-core/host/DragReorder.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace azimgd::shadowlist {

bool dragPressHolds(double along, double cross) {
  return std::abs(along) <= DRAG_PRESS_SCROLL_SLOP && std::hypot(along, cross) <= DRAG_PRESS_SLOP;
}

double dragAutoScrollDelta(const DragAutoScrollConfig& config, double touch, double windowSize, double dtSeconds) {
  if (config.edge <= 0.0 || !(dtSeconds > 0.0)) {
    return 0.0;
  }
  double step = config.maxSpeed * std::min(dtSeconds, DRAG_AUTO_SCROLL_MAX_FRAME_SECONDS);
  // A finger past the viewport edge scrolls at maxSpeed, not faster.
  if (touch < config.edge) {
    return -step * std::min(1.0, 1.0 - touch / config.edge);
  }
  if (touch > windowSize - config.edge) {
    return step * std::min(1.0, 1.0 - (windowSize - touch) / config.edge);
  }
  return 0.0;
}

double dragAutoScrollOffset(
  const DragAutoScrollConfig& config,
  double touch,
  double windowSize,
  double offset,
  double maxOffset,
  double dtSeconds,
  double minOffset) {
  double delta = dragAutoScrollDelta(config, touch, windowSize, dtSeconds);
  if (delta == 0.0) {
    return offset;
  }
  double low = std::min(minOffset, 0.0);
  double high = std::max(low, maxOffset);
  return std::min(std::max(offset + delta, low), high);
}

bool dragBoardShouldArm(double pickup, double finger, double velocity, double direction, double armDistance) {
  if (direction == 0.0) {
    return false;
  }
  double moved = (finger - pickup) * (direction > 0.0 ? 1.0 : -1.0);
  if (moved < armDistance) {
    return false;
  }
  // Heading back away from the edge, faster than a finger's tremble, disarms.
  constexpr double AWAY_SPEED = 30.0;
  return velocity * (direction > 0.0 ? 1.0 : -1.0) > -AWAY_SPEED;
}

void DragBoardScroll::begin(double pickup) {
  pickup_ = pickup;
  previous_ = pickup;
  velocity_ = 0.0;
}

double DragBoardScroll::delta(
  const DragAutoScrollConfig& config,
  double armDistance,
  double finger,
  double touch,
  double windowSize,
  double dtSeconds) {
  if (dtSeconds > 0.0) {
    // Smoothed over a few frames. One still frame mid move does not count as stopping.
    double instant = (finger - previous_) / dtSeconds;
    velocity_ += (instant - velocity_) * 0.5;
  }
  previous_ = finger;
  double scroll = dragAutoScrollDelta(config, touch, windowSize, dtSeconds);
  if (scroll == 0.0 || !dragBoardShouldArm(pickup_, finger, velocity_, scroll, armDistance)) {
    return 0.0;
  }
  return scroll;
}

void DragFeedback::reset(std::size_t insertionIndex) {
  insertionIndex_ = insertionIndex;
}

bool DragFeedback::shouldTick(std::size_t insertionIndex, double nowMs) {
  if (insertionIndex == insertionIndex_) {
    return false;
  }
  insertionIndex_ = insertionIndex;
  if (insertionIndex == UNDEFINED_INDEX || nowMs - lastTickMs_ < DRAG_FEEDBACK_TICK_INTERVAL_MS) {
    return false;
  }
  lastTickMs_ = nowMs;
  return true;
}

void DropSettle::begin(std::size_t insertionIndex, double nowMs, double timeoutMs) {
  waiting_ = insertionIndex != UNDEFINED_INDEX;
  insertionIndex_ = insertionIndex;
  deadlineMs_ = nowMs + timeoutMs;
}

DropSettle::Status DropSettle::tick(std::size_t currentIndex, double nowMs) {
  if (!waiting_) {
    return Status::Idle;
  }
  if (currentIndex == insertionIndex_) {
    waiting_ = false;
    return Status::Settle;
  }
  if (currentIndex == UNDEFINED_INDEX || nowMs >= deadlineMs_) {
    waiting_ = false;
    return Status::GiveUp;
  }
  return Status::Waiting;
}

double dragHeldLeading(double touchContent, double grabOffset, double extent, double contentSize) {
  double leading = touchContent - grabOffset;
  return std::max(0.0, std::min(leading, std::max(0.0, contentSize - extent)));
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
 * Slot values for an index with no mounted cell and for the extra cell, which is the held cell
 * or an incoming one from another list. sourceOf also returns EXTRA_CELL for the index the
 * incoming cell takes.
 */
constexpr std::size_t NO_CELL = UNDEFINED_INDEX;
constexpr std::size_t EXTRA_CELL = UNDEFINED_INDEX - 1;

/*
 * The highest index among the mounted cells and extraIndex, or UNDEFINED_INDEX for none.
 */
template <typename CellAt>
std::size_t maxCellIndex(std::size_t count, CellAt cellAt, std::size_t extraIndex) {
  std::size_t maxIndex = extraIndex;
  for (std::size_t position = 0; position < count; ++position) {
    std::size_t index = cellAt(position).index;
    if (index != UNDEFINED_INDEX) {
      maxIndex = maxIndex == UNDEFINED_INDEX ? index : std::max(maxIndex, index);
    }
  }
  return maxIndex;
}

/*
 * Lay the cells at indices low to high out again in a new order, the way the core stacks each
 * column. sourceOf(index) names the index whose cell sits at index after the change, or
 * EXTRA_CELL for the extra cell. The extra cell rests at extraIndex, or nowhere when that is
 * UNDEFINED_INDEX. write(position, offset) gets every mounted cell's shift, zero for the ones
 * that stay. A column whose first cell in the range is not mounted starts below the cell above
 * it when that one stays in place, without any spacing between the two.
 */
template <typename CellAt, typename SourceOf, typename Write>
void relayoutCells(
  std::size_t count,
  CellAt cellAt,
  const CellFrame& extra,
  std::size_t extraIndex,
  std::size_t low,
  std::size_t high,
  std::size_t columns,
  SourceOf sourceOf,
  Write write) {
  for (std::size_t position = 0; position < count; ++position) {
    write(position, DragOffset{});
  }
  std::size_t maxIndex = maxCellIndex(count, cellAt, extraIndex);
  if (maxIndex == UNDEFINED_INDEX || columns == 0 || low == UNDEFINED_INDEX || low > high) {
    return;
  }
  std::size_t base = low >= columns ? low - columns : 0;
  if (base > maxIndex) {
    return;
  }

  // Position of each index from base in the cells, or the extra cell.
  static thread_local std::vector<std::size_t> slots;
  slots.assign(maxIndex - base + 1, NO_CELL);
  for (std::size_t position = 0; position < count; ++position) {
    std::size_t index = cellAt(position).index;
    if (index != UNDEFINED_INDEX && index >= base) {
      slots[index - base] = position;
    }
  }
  if (extraIndex != UNDEFINED_INDEX && extraIndex >= base) {
    slots[extraIndex - base] = EXTRA_CELL;
  }
  auto slotOf = [&](std::size_t index) {
    if (index == EXTRA_CELL) {
      return EXTRA_CELL;
    }
    return index < base || index > maxIndex ? NO_CELL : slots[index - base];
  };
  auto frameAt = [&](std::size_t slot) {
    return slot == EXTRA_CELL ? extra : cellAt(slot);
  };

  // Where each column's stack ends, restarted after a cell that isn't mounted.
  static thread_local std::vector<DragOffset> lanes;
  static thread_local std::vector<char> known;
  lanes.assign(columns, DragOffset{});
  known.assign(columns, 0);

  for (std::size_t index = low; index <= high; ++index) {
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
      std::size_t aboveSlot = index >= columns && index - columns < low ? slotOf(index - columns) : NO_CELL;
      if (belowSlot != NO_CELL) {
        CellFrame below = frameAt(belowSlot);
        lanes[lane] = {below.leading - source.extent, below.crossLeading};
      } else if (aboveSlot != NO_CELL) {
        CellFrame above = frameAt(aboveSlot);
        lanes[lane] = {above.leading + above.extent, above.crossLeading};
      } else {
        continue;
      }
      known[lane] = 1;
    }
    if (sourceSlot != EXTRA_CELL) {
      write(sourceSlot,
        DragOffset{lanes[lane].leading - source.leading, lanes[lane].cross - source.crossLeading});
    }
    lanes[lane].leading += source.extent;
  }
}

/*
 * Lay the cells from the lower of origin and drop spot onward out again in their new order.
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
  std::size_t maxIndex = maxCellIndex(count, cellAt, origin);
  bool moves = origin != UNDEFINED_INDEX && insertionIndex != UNDEFINED_INDEX && origin != insertionIndex &&
    insertionIndex <= maxIndex;
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
  std::size_t low = moves ? std::min(origin, insertionIndex) : UNDEFINED_INDEX;
  relayoutCells(count, cellAt, frameOf(held), origin, low, maxIndex, columns, sourceOf, write);
}

/*
 * Lay the cells after the held one out again as if it were removed.
 */
template <typename CellAt, typename Write>
void gridRemovalShifts(std::size_t count, CellAt cellAt, const DragRow& held, std::size_t columns, Write write) {
  std::size_t origin = held.index;
  std::size_t maxIndex = maxCellIndex(count, cellAt, origin);
  auto sourceOf = [&](std::size_t index) { return index >= origin ? index + 1 : index; };
  relayoutCells(count, cellAt, frameOf(held), origin, origin, maxIndex, columns, sourceOf, write);
}

/*
 * Lay the cells from insertionIndex onward out again as if a cell of extent and crossExtent
 * were inserted there.
 */
template <typename CellAt, typename Write>
void gridForeignShifts(
  std::size_t count,
  CellAt cellAt,
  double extent,
  double crossExtent,
  std::size_t insertionIndex,
  std::size_t columns,
  Write write) {
  std::size_t maxIndex = maxCellIndex(count, cellAt, UNDEFINED_INDEX);
  bool opens = maxIndex != UNDEFINED_INDEX && insertionIndex != UNDEFINED_INDEX && insertionIndex <= maxIndex;
  auto sourceOf = [&](std::size_t index) {
    if (index == insertionIndex) {
      return EXTRA_CELL;
    }
    return index > insertionIndex ? index - 1 : index;
  };
  CellFrame extra{UNDEFINED_INDEX, 0.0, extent, 0.0, crossExtent};
  relayoutCells(count, cellAt, extra, UNDEFINED_INDEX, opens ? insertionIndex : UNDEFINED_INDEX,
    opens ? maxIndex + 1 : 0, columns, sourceOf, write);
}

/*
 * Shared by the array form and the session. Returns the position of the first row, by index,
 * whose midpoint the center has not passed, or count when it passed them all.
 */
template <typename RowAt>
std::size_t foreignInsertionPosition(std::size_t count, double center, RowAt row) {
  std::size_t position = count;
  std::size_t best = UNDEFINED_INDEX;
  for (std::size_t current = 0; current < count; ++current) {
    std::size_t index = 0;
    double leading = 0.0;
    double extent = 0.0;
    row(current, index, leading, extent);
    if (index == UNDEFINED_INDEX) {
      continue;
    }
    if (center <= leading + extent / 2.0 && (best == UNDEFINED_INDEX || index < best)) {
      best = index;
      position = current;
    }
  }
  return position;
}

/*
 * Shared by the array form and the session. See dragGridForeignInsertionIndex.
 */
template <typename CellAt>
std::size_t gridForeignInsertionPosition(
  std::size_t count,
  CellAt cellAt,
  std::size_t insertionIndex,
  double center,
  double crossCenter) {
  std::size_t kept = UNDEFINED_INDEX;
  bool any = false;
  double trailing = 0.0;
  CellFrame last;
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
    trailing = any ? std::max(trailing, cell.leading + cell.extent) : cell.leading + cell.extent;
    if (!any || cell.index > last.index) {
      last = cell;
    }
    any = true;
  }
  bool pastLast = center >= last.leading && center < last.leading + last.extent &&
    crossCenter >= last.crossLeading + last.crossExtent;
  if (!any || center >= trailing || pastLast || insertionIndex == UNDEFINED_INDEX) {
    return count;
  }
  return kept;
}

CellFrame cellFrameAt(const DragCells& cells, std::size_t position) {
  return {cells.indices[position], cells.leadings[position], cells.extents[position],
    cells.crossLeadings[position], cells.crossExtents[position]};
}

}

std::size_t dragGridInsertionIndex(
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

void dragGridRemovalShifts(
  const DragCells& cells,
  const DragRow& held,
  std::size_t columns,
  double* shifts,
  double* crossShifts) {
  gridRemovalShifts(cells.count, [&](std::size_t position) { return cellFrameAt(cells, position); },
    held, columns, [&](std::size_t position, DragOffset offset) {
      shifts[position] = offset.leading;
      crossShifts[position] = offset.cross;
    });
}

std::size_t dragGridForeignInsertionIndex(
  const DragCells& cells,
  std::size_t insertionIndex,
  double center,
  double crossCenter) {
  return gridForeignInsertionPosition(cells.count, [&](std::size_t position) { return cellFrameAt(cells, position); },
    insertionIndex, center, crossCenter);
}

void dragGridForeignShifts(
  const DragCells& cells,
  double extent,
  double crossExtent,
  std::size_t insertionIndex,
  std::size_t columns,
  double* shifts,
  double* crossShifts) {
  gridForeignShifts(cells.count, [&](std::size_t position) { return cellFrameAt(cells, position); },
    extent, crossExtent, insertionIndex, columns, [&](std::size_t position, DragOffset offset) {
      shifts[position] = offset.leading;
      crossShifts[position] = offset.cross;
    });
}

std::size_t dragForeignInsertionIndex(
  const std::size_t* indices,
  const double* leadings,
  const double* extents,
  std::size_t count,
  double center) {
  auto rowAt = [&](std::size_t current, std::size_t& index, double& leading, double& extent) {
    index = indices[current];
    leading = leadings[current];
    extent = extents[current];
  };
  return foreignInsertionPosition(count, center, rowAt);
}

double dragRemovalShift(std::size_t originIndex, double draggedExtent, std::size_t index) {
  if (originIndex == UNDEFINED_INDEX || index == UNDEFINED_INDEX || index <= originIndex) {
    return 0.0;
  }
  return -draggedExtent;
}

double dragForeignShift(std::size_t insertionIndex, double extent, std::size_t index) {
  if (insertionIndex == UNDEFINED_INDEX || index == UNDEFINED_INDEX || index < insertionIndex) {
    return 0.0;
  }
  return extent;
}

std::size_t dragInsertionIndex(
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
  foreign_ = false;
  heldAway_ = false;
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
  double contentSize,
  double crossExtent) {
  if (isGrid()) {
    return placeCell(touchAlong, touchCross, resting, contentSize, crossExtent);
  }
  DragOffset offset;
  offset.leading = place(touchAlong, resting.leading, resting.extent, contentSize);
  return offset;
}

double DragReorder::place(double touchContent, double restingLeading, double extent, double contentSize) {
  leading_ = dragHeldLeading(touchContent, grabOffset_, extent, contentSize);
  center_ = leading_ + extent / 2.0;
  return leading_ - restingLeading;
}

DragOffset DragReorder::placeCell(
  double touchContent,
  double touchCross,
  const DragRow& resting,
  double contentSize,
  double crossContentSize) {
  heldResting_.leading = resting.leading;
  heldResting_.extent = resting.extent;
  heldResting_.crossLeading = resting.crossLeading;
  heldResting_.crossExtent = resting.crossExtent;
  double translation = place(touchContent, resting.leading, resting.extent, contentSize);
  crossLeading_ = dragHeldLeading(touchCross, crossGrabOffset_, resting.crossExtent, crossContentSize);
  crossCenter_ = crossLeading_ + resting.crossExtent / 2.0;
  return {translation, crossLeading_ - resting.crossLeading};
}

DragOffset DragReorder::offsetFor(std::size_t index) const {
  if (!isGrid()) {
    if (foreign_) {
      return {dragForeignShift(insertionIndex_, draggedExtent_, index), 0.0};
    }
    if (heldAway_) {
      return {dragRemovalShift(originIndex_, draggedExtent_, index), 0.0};
    }
    return {dragShift(originIndex_, insertionIndex_, draggedExtent_, index), 0.0};
  }
  if (index < offsetsBase_ || index - offsetsBase_ >= offsets_.size()) {
    return {};
  }
  return offsets_[index - offsetsBase_];
}

void DragReorder::updateInsertion(const std::vector<DragRow>& rows) {
  if (foreign_) {
    updateForeignInsertion(rows);
    return;
  }
  if (heldAway_) {
    insertionIndex_ = UNDEFINED_INDEX;
    insertionKey_.clear();
    if (isGrid()) {
      heldResting_.index = originIndex_;
      auto cellAt = [&](std::size_t position) { return frameOf(rows[position]); };
      storeOffsets(rows, [&](auto write) { gridRemovalShifts(rows.size(), cellAt, heldResting_, columns_, write); });
    }
    return;
  }
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

  storeOffsets(rows, [&](auto write) {
    gridShifts(rows.size(), cellAt, heldResting_, insertionIndex_, columns_, write);
  });
}

template <typename Relayout>
void DragReorder::storeOffsets(const std::vector<DragRow>& rows, Relayout relayout) {
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
  relayout([&](std::size_t current, DragOffset offset) {
    std::size_t index = rows[current].index;
    if (index != UNDEFINED_INDEX) {
      offsets_[index - offsetsBase_] = offset;
    }
  });
}

void DragReorder::setHeldAway(bool away) {
  if (foreign_ || away == heldAway_) {
    return;
  }
  heldAway_ = away;
  offsets_.clear();
  if (away) {
    insertionIndex_ = UNDEFINED_INDEX;
    insertionKey_.clear();
  } else {
    insertionIndex_ = originIndex_;
    insertionKey_ = originKey_;
  }
}

void DragReorder::beginForeign(double extent, double crossExtent, std::size_t columns) {
  begin(UNDEFINED_INDEX, std::string(), 0.0, extent, 0.0);
  foreign_ = true;
  columns_ = std::max<std::size_t>(1, columns);
  heldResting_.extent = extent;
  heldResting_.crossExtent = crossExtent;
}

void DragReorder::placeForeignCenter(double center, double crossCenter) {
  center_ = center;
  crossCenter_ = crossCenter;
  leading_ = center - draggedExtent_ / 2.0;
  crossLeading_ = crossCenter - heldResting_.crossExtent / 2.0;
}

void DragReorder::endForeign() {
  begin(UNDEFINED_INDEX, std::string(), 0.0, 0.0, 0.0);
}

void DragReorder::updateForeignInsertion(const std::vector<DragRow>& rows) {
  auto cellAt = [&](std::size_t current) { return frameOf(rows[current]); };
  std::size_t position = UNDEFINED_INDEX;
  if (isGrid()) {
    position = gridForeignInsertionPosition(rows.size(), cellAt, insertionIndex_, center_, crossCenter_);
  } else {
    auto rowAt = [&](std::size_t current, std::size_t& index, double& leading, double& extent) {
      index = rows[current].index;
      leading = rows[current].leading;
      extent = rows[current].extent;
    };
    position = foreignInsertionPosition(rows.size(), center_, rowAt);
  }
  if (position == rows.size()) {
    // Past every mounted row. The incoming row goes after the last one.
    std::size_t last = maxCellIndex(rows.size(), cellAt, UNDEFINED_INDEX);
    insertionIndex_ = last == UNDEFINED_INDEX ? 0 : last + 1;
    insertionKey_.clear();
  } else if (position != UNDEFINED_INDEX) {
    insertionIndex_ = rows[position].index;
    insertionKey_ = rows[position].key;
  }
  if (!isGrid()) {
    return;
  }
  storeOffsets(rows, [&](auto write) {
    gridForeignShifts(rows.size(), cellAt, heldResting_.extent, heldResting_.crossExtent, insertionIndex_, columns_,
      write);
  });
}

}
