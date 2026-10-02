#include <shadowlist-core/host/MountedRange.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace azimgd::shadowlist {

MountedRange initialMountedRange(long size, long initial, bool inverted, long offsetIndex, long overscanRows,
  double viewPosition) {
  if (size <= 0) {
    return {-1, -1};
  }
  if (offsetIndex >= 0) {
    long target = std::min(offsetIndex, size - 1);
    // Math.round in JS rounds halves up.
    long before = static_cast<long>(std::floor(viewPosition * static_cast<double>(initial) + 0.5));
    return {std::max(0L, target - overscanRows - before), std::min(size - 1, target + (initial - before))};
  }
  if (inverted) {
    return {std::max(0L, size - initial), size - 1};
  }
  return {0, std::min(initial, size - 1)};
}

std::vector<long> rangeToIndices(const MountedRange& range) {
  std::vector<long> indices;
  if (range.low < 0 || range.high < 0 || range.low > range.high) {
    return indices;
  }
  indices.reserve(static_cast<std::size_t>(range.high - range.low + 1));
  for (long index = range.low; index <= range.high; ++index) {
    indices.push_back(index);
  }
  return indices;
}

std::vector<long> unionRangeIndices(const MountedRange& first, const MountedRange& second) {
  std::vector<long> firstIndices = rangeToIndices(first);
  std::vector<long> secondIndices = rangeToIndices(second);
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
  std::vector<long> indices = rangeToIndices(lower);
  std::vector<long> upperIndices = rangeToIndices(upper);
  indices.insert(indices.end(), upperIndices.begin(), upperIndices.end());
  return indices;
}

bool shouldReseedFromOffsetIndex(long previousOffsetIndex, long nextOffsetIndex) {
  return nextOffsetIndex != previousOffsetIndex && nextOffsetIndex >= 0;
}

MountedRange stepMountedRange(const MountedRange& current, const MountedRange& target, const MountedRange& window, long step) {
  bool disjoint = current.low < 0 || current.high < 0 || target.low > current.high || target.high < current.low;
  const MountedRange& base = disjoint ? window : current;
  long low = target.low >= base.low ? target.low : std::max(target.low, std::min(base.low - step, window.low));
  long high = target.high <= base.high ? target.high : std::min(target.high, std::max(base.high + step, window.high));
  return {low, high};
}

long mountStepForWindow(const MountedRange& window, long minimumStep) {
  long windowRows = window.high - window.low + 1;
  return std::max(minimumStep, (windowRows + 3) / 4);
}

MountedRange grownMountedRange(long lowIndex, long highIndex, bool lowAtStart, bool highAtEnd, long size,
  long overscanRowsLeading, bool followTail) {
  long low = std::min(lowIndex, highIndex);
  long high = std::max(lowIndex, highIndex);
  long grownLow = lowAtStart ? std::max(0L, low - overscanRowsLeading) : low;
  if (followTail && highAtEnd) {
    long tailHigh = size - 1;
    long tailLow = tailHigh - (high - low) - MAX_FOLLOWED_APPEND;
    return {std::max(grownLow, tailLow), tailHigh};
  }
  return {grownLow, highAtEnd ? std::min(size - 1, high + overscanRowsLeading) : high};
}

MountedRange visibleTargetRange(const MountedRange& window, const std::optional<MountedRange>& previousWindow, long size,
  long overscanRows, long overscanRowsLeading) {
  bool movingForward = previousWindow && window.low > previousWindow->low;
  bool movingBackward = previousWindow && window.low < previousWindow->low;
  long lowPad = movingBackward ? overscanRowsLeading : overscanRows;
  long highPad = movingForward ? overscanRowsLeading : overscanRows;
  return {std::max(0L, window.low - lowPad), std::min(size - 1, window.high + highPad)};
}

std::optional<ReportedRange> reportedMountedRange(const MountedRange& current, const MountedRange& window,
  const std::optional<MountedRange>& previousWindow, bool firstReport, long size, long overscanRows,
  long overscanRowsLeading, long minimumStep) {
  bool holdsWindow = current.low >= 0 && window.low >= current.low && window.high <= current.high;
  if (holdsWindow && !firstReport) {
    return std::nullopt;
  }
  MountedRange target = visibleTargetRange(window, previousWindow, size, overscanRows, overscanRowsLeading);
  MountedRange range = stepMountedRange(current, target, window, mountStepForWindow(window, minimumStep));
  return ReportedRange{range, target};
}

std::optional<MountedRange> viewableWindow(long startIndex, long endIndex) {
  if (startIndex == -1 || endIndex == -1) {
    return std::nullopt;
  }
  return MountedRange{std::min(startIndex, endIndex), std::max(startIndex, endIndex)};
}

long activeStickyIndexFor(const std::vector<long>& stickyHeaderIndices, long windowLow) {
  long active = -1;
  for (long stickyIndex : stickyHeaderIndices) {
    if (stickyIndex > windowLow) {
      break;
    }
    active = stickyIndex;
  }
  return active;
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
