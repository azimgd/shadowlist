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
  originIndex_ = index;
  insertionIndex_ = index;
  originKey_ = std::move(key);
  insertionKey_ = originKey_;
  draggedExtent_ = extent;
  grabOffset_ = touchContent - restingLeading;
  leading_ = restingLeading;
  center_ = restingLeading + extent / 2.0;
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

void DragReorder::updateInsertion(const std::vector<DragRow>& rows) {
  long position = insertionPosition(rows.size(), originIndex_, center_, [&](std::size_t current, long& index, double& leading, double& extent) {
    index = rows[current].index;
    leading = rows[current].leading;
    extent = rows[current].extent;
  });
  if (position < 0) {
    // Nothing passed, so the drop names the held row itself and nothing moves.
    insertionIndex_ = originIndex_;
    insertionKey_ = originKey_;
    return;
  }
  const DragRow& row = rows[static_cast<std::size_t>(position)];
  insertionIndex_ = row.index;
  insertionKey_ = row.key.empty() ? originKey_ : row.key;
}

}
