#include <shadowlist-core/host/MountedRange.hpp>

#include <shadowlist-core/host/StickyLayout.hpp>

#include <algorithm>
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

}
