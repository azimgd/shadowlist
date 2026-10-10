#include <shadowlist-core/host/MountedRange.hpp>

#include <shadowlist-core/host/StickyLayout.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace azimgd::shadowlist {

namespace {

/*
 * value minus amount, stopping at 0.
 */
std::size_t subtractClamped(std::size_t value, std::size_t amount) {
  return value > amount ? value - amount : 0;
}

}

MountedRange initialMountedRange(
  std::size_t size,
  std::size_t initial,
  bool inverted,
  std::size_t offsetIndex,
  std::size_t overscanRows,
  double viewPosition) {
  if (size == 0) {
    return {};
  }
  if (offsetIndex != UNDEFINED_INDEX) {
    std::size_t target = std::min(offsetIndex, size - 1);
    // Math.round in JS rounds halves up.
    double rounded = std::floor(viewPosition * static_cast<double>(initial) + 0.5);
    std::size_t before = std::min(initial, static_cast<std::size_t>(std::max(0.0, rounded)));
    return {subtractClamped(target, overscanRows + before), std::min(size - 1, target + (initial - before))};
  }
  if (inverted) {
    return {subtractClamped(size, initial), size - 1};
  }
  return {0, std::min(initial, size - 1)};
}

std::vector<std::size_t> rangeToIndices(const MountedRange& range) {
  std::vector<std::size_t> indices;
  if (range.low == UNDEFINED_INDEX || range.high == UNDEFINED_INDEX || range.low > range.high) {
    return indices;
  }
  indices.reserve(range.high - range.low + 1);
  for (std::size_t index = range.low; index <= range.high; ++index) {
    indices.push_back(index);
  }
  return indices;
}

std::vector<std::size_t> unionRangeIndices(const MountedRange& first, const MountedRange& second) {
  std::vector<std::size_t> firstIndices = rangeToIndices(first);
  std::vector<std::size_t> secondIndices = rangeToIndices(second);
  if (firstIndices.empty()) {
    return secondIndices;
  }
  if (secondIndices.empty()) {
    return firstIndices;
  }
  const MountedRange& lower = first.low <= second.low ? first : second;
  const MountedRange& upper = first.low <= second.low ? second : first;
  if (upper.low <= lower.high + 1) {
    return rangeToIndices({lower.low, std::max(lower.high, upper.high)});
  }
  std::vector<std::size_t> indices = rangeToIndices(lower);
  std::vector<std::size_t> upperIndices = rangeToIndices(upper);
  indices.insert(indices.end(), upperIndices.begin(), upperIndices.end());
  return indices;
}

bool shouldReseedFromOffsetIndex(std::size_t previousOffsetIndex, std::size_t nextOffsetIndex) {
  return nextOffsetIndex != previousOffsetIndex && nextOffsetIndex != UNDEFINED_INDEX;
}

MountedRange stepMountedRange(
  const MountedRange& current,
  const MountedRange& target,
  const MountedRange& measured,
  std::size_t step) {
  bool disjoint = current.low == UNDEFINED_INDEX || current.high == UNDEFINED_INDEX || target.low > current.high ||
    target.high < current.low;
  const MountedRange& base = disjoint ? measured : current;
  std::size_t low = target.low >= base.low
    ? target.low
    : std::max(target.low, std::min(subtractClamped(base.low, step), measured.low));
  std::size_t high = target.high <= base.high
    ? target.high
    : std::min(target.high, std::max(base.high + step, measured.high));
  return {low, high};
}

std::size_t mountStepForRange(const MountedRange& measured, std::size_t minimumStep) {
  std::size_t measuredRows = measured.high - measured.low + 1;
  return std::max(minimumStep, (measuredRows + 3) / 4);
}

MountedRange grownMountedRange(
  std::size_t lowIndex,
  std::size_t highIndex,
  bool lowAtStart,
  bool highAtEnd,
  std::size_t size,
  std::size_t overscanRowsLeading,
  bool followTail) {
  if (size == 0) {
    return {};
  }
  std::size_t low = std::min(lowIndex, highIndex);
  std::size_t high = std::max(lowIndex, highIndex);
  std::size_t grownLow = lowAtStart ? subtractClamped(low, overscanRowsLeading) : low;
  if (followTail && highAtEnd) {
    std::size_t tailHigh = size - 1;
    std::size_t tailLow = subtractClamped(tailHigh, (high - low) + MAX_FOLLOWED_APPEND);
    return {std::max(grownLow, tailLow), tailHigh};
  }
  return {grownLow, highAtEnd ? std::min(size - 1, high + overscanRowsLeading) : high};
}

MountedRange visibleTargetRange(
  const MountedRange& measured,
  const std::optional<MountedRange>& previousMeasured,
  std::size_t size,
  std::size_t overscanRows,
  std::size_t overscanRowsLeading) {
  if (size == 0) {
    return {};
  }
  bool movingForward = previousMeasured && measured.low > previousMeasured->low;
  bool movingBackward = previousMeasured && measured.low < previousMeasured->low;
  std::size_t lowPad = movingBackward ? overscanRowsLeading : overscanRows;
  std::size_t highPad = movingForward ? overscanRowsLeading : overscanRows;
  return {subtractClamped(measured.low, lowPad), std::min(size - 1, measured.high + highPad)};
}

std::optional<ReportedRange> reportedMountedRange(
  const MountedRange& current,
  const MountedRange& measured,
  const std::optional<MountedRange>& previousMeasured,
  bool firstReport,
  std::size_t size,
  std::size_t overscanRows,
  std::size_t overscanRowsLeading,
  std::size_t minimumStep) {
  bool holdsMeasured = current.low != UNDEFINED_INDEX && measured.low >= current.low && measured.high <= current.high;
  if (holdsMeasured && !firstReport) {
    return std::nullopt;
  }
  MountedRange target = visibleTargetRange(measured, previousMeasured, size, overscanRows, overscanRowsLeading);
  MountedRange range = stepMountedRange(current, target, measured, mountStepForRange(measured, minimumStep));
  return ReportedRange{range, target};
}

std::optional<MountedRange> viewableRange(std::size_t low, std::size_t high) {
  if (low == UNDEFINED_INDEX || high == UNDEFINED_INDEX) {
    return std::nullopt;
  }
  return MountedRange{low, high};
}

std::size_t activeStickyIndexFor(const std::vector<std::size_t>& stickyIndices, std::size_t rangeLow) {
  std::size_t position = pinnedSectionIndex(stickyIndices.size(), static_cast<double>(rangeLow),
    [&](std::size_t at) { return static_cast<double>(stickyIndices[at]); });
  return position == UNDEFINED_INDEX ? UNDEFINED_INDEX : stickyIndices[position];
}

ViewableChanges viewableChanges(const std::vector<std::string>& previous, const std::vector<std::string>& current) {
  ViewableChanges changes;
  std::unordered_set<std::string> previousKeys(previous.begin(), previous.end());
  std::unordered_set<std::string> currentKeys(current.begin(), current.end());
  for (const std::string& key : current) {
    if (previousKeys.count(key) == 0) {
      changes.added.push_back(key);
    }
  }
  for (const std::string& key : previous) {
    if (currentKeys.count(key) == 0) {
      changes.removed.push_back(key);
    }
  }
  return changes;
}

}
