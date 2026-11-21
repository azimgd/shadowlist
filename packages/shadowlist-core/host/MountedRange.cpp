#include <shadowlist-core/host/MountedRange.hpp>

#include <shadowlist-core/host/StickyLayout.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <unordered_set>

namespace azimgd::shadowlist {

namespace {

/*
 * value minus amount, stopping at 0.
 */
std::size_t subtractClamped(std::size_t value, std::size_t amount) {
  return value > amount ? value - amount : 0;
}

/*
 * A row count as a double for signed arithmetic, and a row index back from one. An index below
 * zero has no row.
 */
double asNumber(std::size_t count) {
  return static_cast<double>(count);
}

std::size_t asIndex(double index) {
  return index >= 0.0 ? static_cast<std::size_t>(index) : UNDEFINED_INDEX;
}

/*
 * value rounded to the nearest whole number, a half going up, as JS Math.round does.
 */
double roundHalfUp(double value) {
  double floor = std::floor(value);
  return value - floor >= 0.5 ? floor + 1.0 : floor;
}

void appendRange(std::vector<std::size_t>& indices, const MountedRange& range) {
  if (range.isEmpty()) {
    return;
  }
  for (std::size_t index = range.low; index <= range.high; ++index) {
    indices.push_back(index);
  }
}

}

MountedRange stepMountedRange(
  const MountedRange& current,
  const MountedRange& target,
  const MountedRange& measured,
  std::size_t step) {
  bool disjoint = current.isEmpty() || target.low > current.high || target.high < current.low;
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
  bool holdsMeasured = !current.isEmpty() && measured.low >= current.low && measured.high <= current.high;
  MountedRange target = visibleTargetRange(measured, previousMeasured, size, overscanRows, overscanRowsLeading);
  if (holdsMeasured && !firstReport) {
    std::size_t targetCount = target.high - target.low + 1;
    std::size_t currentCount = current.high - current.low + 1;
    bool oversized = currentCount > targetCount && currentCount - targetCount > std::max(targetCount, MAX_FOLLOWED_APPEND);
    if (!oversized) {
      return std::nullopt;
    }
  }
  MountedRange range = stepMountedRange(current, target, measured, mountStepForRange(measured, minimumStep));
  return ReportedRange{range, target};
}

std::optional<MountedRange> viewableRange(std::size_t low, std::size_t high) {
  MountedRange range{low, high};
  if (range.isEmpty()) {
    return std::nullopt;
  }
  return range;
}

std::size_t activeStickyIndexFor(const std::vector<std::size_t>& stickyIndices, std::size_t rangeLow) {
  std::size_t position = pinnedSectionIndex(stickyIndices.size(), static_cast<double>(rangeLow),
    [&](std::size_t at) { return static_cast<double>(stickyIndices[at]); });
  return position == UNDEFINED_INDEX ? UNDEFINED_INDEX : stickyIndices[position];
}

ViewableChanges viewableChanges(const std::vector<std::string>& previous, const std::vector<std::string>& current) {
  ViewableChanges changes;
  std::unordered_set<std::string_view> previousKeys(previous.begin(), previous.end());
  std::unordered_set<std::string_view> currentKeys(current.begin(), current.end());
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

MountedRange initialMountedRange(
  std::size_t size,
  std::size_t initialCount,
  bool inverted,
  std::size_t target,
  std::size_t overscanRows,
  double viewPosition) {
  if (size == 0) {
    return {};
  }
  double last = asNumber(size - 1);
  double initial = asNumber(initialCount);
  if (target != UNDEFINED_INDEX) {
    double row = std::min(asNumber(target), last);
    double before = roundHalfUp(viewPosition * initial);
    double low = std::max(0.0, row - asNumber(overscanRows) - before);
    double high = std::min(last, row + (initial - before));
    // A view position outside 0 to 1 can push the range off the rows.
    if (!(low <= high)) {
      return {};
    }
    return {asIndex(low), asIndex(high)};
  }
  if (inverted) {
    return {asIndex(std::max(0.0, asNumber(size) - initial)), size - 1};
  }
  return {0, std::min(initialCount, size - 1)};
}

MountedRange grownMountedRange(
  const MountedRange& edges,
  bool lowAtStart,
  bool highAtEnd,
  std::size_t size,
  std::size_t overscanRowsLeading,
  bool followTail) {
  if (edges.low == UNDEFINED_INDEX || edges.high == UNDEFINED_INDEX || size == 0) {
    return {};
  }
  std::size_t low = std::min(edges.low, edges.high);
  std::size_t high = std::max(edges.low, edges.high);
  std::size_t grownLow = lowAtStart ? low - std::min(low, overscanRowsLeading) : low;
  if (followTail && highAtEnd) {
    std::size_t tailHigh = size - 1;
    std::size_t tailLength = high - low + MAX_FOLLOWED_APPEND;
    std::size_t tailLow = tailHigh - std::min(tailHigh, tailLength);
    return {std::max(grownLow, tailLow), tailHigh};
  }
  if (!highAtEnd || high >= size - 1) {
    return {grownLow, highAtEnd ? size - 1 : high};
  }
  return {grownLow, high + std::min(size - 1 - high, overscanRowsLeading)};
}

std::vector<std::size_t> unionRangeIndices(const MountedRange& first, const MountedRange& second) {
  std::vector<std::size_t> indices;
  if (first.isEmpty() || second.isEmpty()) {
    appendRange(indices, first);
    appendRange(indices, second);
    return indices;
  }
  const MountedRange& lower = first.low <= second.low ? first : second;
  const MountedRange& upper = first.low <= second.low ? second : first;
  if (upper.low <= lower.high + 1) {
    appendRange(indices, {lower.low, std::max(lower.high, upper.high)});
    return indices;
  }
  appendRange(indices, lower);
  appendRange(indices, upper);
  return indices;
}

bool shouldReseedFromIndex(std::size_t previousIndex, std::size_t nextIndex) {
  return nextIndex != previousIndex && nextIndex != UNDEFINED_INDEX;
}

}
