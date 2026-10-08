#include <shadowlist-core/host/ScrollTarget.hpp>

#include <algorithm>
#include <cmath>

namespace azimgd::shadowlist {

std::optional<ListAnchor> anchorAt(const Container& core, double offset) {
  auto visible = core.getVisibleIndices();
  const auto& elements = core.revision.elements;
  if (visible.first == UNDEFINED_INDEX || visible.second == UNDEFINED_INDEX || elements.empty()) {
    return std::nullopt;
  }
  std::size_t low = std::min(visible.first, visible.second);
  std::size_t high = std::min(std::max(visible.first, visible.second), elements.size() - 1);
  std::size_t best = UNDEFINED_INDEX;
  for (std::size_t index = low; index <= high; ++index) {
    double leading = core.getElementOffset(index);
    if (leading + core.getElementSize(index) <= offset) {
      continue;
    }
    if (best == UNDEFINED_INDEX || leading < core.getElementOffset(best)) {
      best = index;
    }
  }
  if (best == UNDEFINED_INDEX) {
    return std::nullopt;
  }
  return ListAnchor{elements[best].key, offset - core.getElementOffset(best)};
}

double rowTargetOffset(
  const Container& core,
  std::size_t index,
  double viewPosition,
  double rowOffset,
  double windowAlong,
  double maxOffset) {
  if (index >= core.getElementsSize()) {
    return 0.0;
  }
  double target =
    core.getElementOffset(index) - viewPosition * std::max(0.0, windowAlong - core.getElementSize(index)) + rowOffset;
  return std::min(std::max(target, 0.0), std::max(0.0, maxOffset));
}

std::optional<double> commandTargetOffset(
  const Container& core,
  double commandIndex,
  double viewPosition,
  double rowOffset) {
  double windowAlong = core.getWindowContainerSize();
  double totalAlong = core.horizontal ? core.revision.totalContainerWidth : core.revision.totalContainerHeight;
  double maxOffset = std::max(0.0, totalAlong - windowAlong);
  if (commandIndex == SCROLL_TO_END_INDEX) {
    return maxOffset;
  }
  if (commandIndex == SCROLL_TO_OFFSET_INDEX) {
    return std::min(std::max(std::isfinite(rowOffset) ? rowOffset : 0.0, 0.0), maxOffset);
  }
  if (!(commandIndex >= 0.0) || !std::isfinite(commandIndex) ||
      commandIndex >= static_cast<double>(core.getElementsSize())) {
    return std::nullopt;
  }
  double position = std::isfinite(viewPosition) ? std::min(1.0, std::max(0.0, viewPosition)) : 0.0;
  double offset = std::isfinite(rowOffset) ? rowOffset : 0.0;
  return rowTargetOffset(core, static_cast<std::size_t>(commandIndex), position, offset, windowAlong, maxOffset);
}

double pageScrollTarget(double offset, double windowAlong, double maxOffset, int direction) {
  double target = offset + (direction > 0 ? windowAlong : direction < 0 ? -windowAlong : 0.0);
  return std::min(std::max(target, 0.0), std::max(maxOffset, 0.0));
}

}
