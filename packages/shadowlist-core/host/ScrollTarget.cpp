#include <shadowlist-core/host/ScrollTarget.hpp>

#include <shadowlist-core/Constants.hpp>

#include <algorithm>
#include <cmath>

namespace azimgd::shadowlist {

std::optional<AnchorState> anchorStateAt(const Container& core, double offset) {
  IndexRange measured = core.getMeasuredRange();
  const auto& rows = core.revision.rows;
  if (measured.isEmpty() || rows.empty()) {
    return std::nullopt;
  }
  std::size_t low = measured.low;
  std::size_t high = std::min(measured.high, rows.size() - 1);
  std::size_t best = UNDEFINED_INDEX;
  for (std::size_t index = low; index <= high; ++index) {
    double leading = core.getRowOffset(index);
    if (leading + core.getRowSize(index) <= offset) {
      continue;
    }
    if (best == UNDEFINED_INDEX || leading < core.getRowOffset(best)) {
      best = index;
    }
  }
  if (best == UNDEFINED_INDEX) {
    return std::nullopt;
  }
  return AnchorState{rows[best].key, offset - core.getRowOffset(best)};
}

double rowTargetOffset(
  const Container& core,
  std::size_t index,
  double viewPosition,
  double viewOffset,
  double windowAlong,
  double maxOffset) {
  if (index >= core.getRowCount()) {
    return 0.0;
  }
  double target =
    core.getRowOffset(index) - viewPosition * std::max(0.0, windowAlong - core.getRowSize(index)) + viewOffset;
  return std::min(std::max(target, 0.0), std::max(0.0, maxOffset));
}

std::optional<double> commandTargetOffset(
  const Container& core,
  double commandIndex,
  double viewPosition,
  double viewOffset) {
  double windowAlong = core.getWindowSize();
  double totalAlong = core.horizontal ? core.revision.contentWidth : core.revision.contentHeight;
  double maxOffset = std::max(0.0, totalAlong - windowAlong);
  if (commandIndex == SCROLL_TO_END_INDEX) {
    return maxOffset;
  }
  if (commandIndex == SCROLL_TO_OFFSET_INDEX) {
    return std::min(std::max(std::isfinite(viewOffset) ? viewOffset : 0.0, 0.0), maxOffset);
  }
  if (!(commandIndex >= 0.0) || !std::isfinite(commandIndex) ||
      commandIndex >= static_cast<double>(core.getRowCount())) {
    return std::nullopt;
  }
  double position = std::isfinite(viewPosition) ? std::min(1.0, std::max(0.0, viewPosition)) : 0.0;
  double offset = std::isfinite(viewOffset) ? viewOffset : 0.0;
  return rowTargetOffset(core, static_cast<std::size_t>(commandIndex), position, offset, windowAlong, maxOffset);
}

double pageScrollTarget(double offset, double windowAlong, double maxOffset, int direction) {
  double target = offset + (direction > 0 ? windowAlong : direction < 0 ? -windowAlong : 0.0);
  return std::min(std::max(target, 0.0), std::max(maxOffset, 0.0));
}

}
