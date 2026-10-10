#pragma once

#include <shadowlist-core/Constants.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Which rows a host keeps mounted, and how that range follows the visible rows. It mirrors
 * src/virtualizer/mountedRange.ts function for function. A range with an UNDEFINED_INDEX end is empty.
 */
struct MountedRange {
  std::size_t low = UNDEFINED_INDEX;
  std::size_t high = UNDEFINED_INDEX;

  bool operator==(const MountedRange& other) const {
    return low == other.low && high == other.high;
  }
};

/*
 * Default overscan rows on each side, matching SHADOWLIST_OVERSCAN.
 */
constexpr std::size_t DEFAULT_OVERSCAN_ROWS = 4;

/*
 * How many appended rows an inverted list at its end mounts on top of its range. A bigger
 * burst moves the range to the tail instead.
 */
constexpr std::size_t MAX_FOLLOWED_APPEND = 50;

/*
 * The first range, before the host reports what is visible. With a target it mounts around it,
 * split by viewPosition, which wins over an inverted list's bottom start.
 */
MountedRange initialMountedRange(
  std::size_t size,
  std::size_t initial,
  bool inverted,
  std::size_t offsetIndex,
  std::size_t overscanRows = DEFAULT_OVERSCAN_ROWS,
  double viewPosition = 0.0);

/*
 * Indices of both ranges, sorted and without duplicates.
 */
std::vector<std::size_t> unionRangeIndices(const MountedRange& first, const MountedRange& second);

std::vector<std::size_t> rangeToIndices(const MountedRange& range);

/*
 * Whether a new start index should rebuild the range around it. UNDEFINED_INDEX means no
 * target, and must not pull a reader who scrolled away back to the start.
 */
bool shouldReseedFromOffsetIndex(std::size_t previousOffsetIndex, std::size_t nextOffsetIndex);

/*
 * One step from the mounted range toward the target. Rows on screen mount right away and the
 * overscan beyond them grows by at most step rows per end. Shrinking is never paced.
 */
MountedRange stepMountedRange(
  const MountedRange& current,
  const MountedRange& target,
  const MountedRange& measured,
  std::size_t step);

/*
 * Overscan rows to add per step, a quarter of the measured range and at least minimumStep.
 */
std::size_t mountStepForRange(const MountedRange& measured, std::size_t minimumStep);

/*
 * The range from its edge rows' current indices. A range at an edge of the data grows to take
 * rows added past it, up to the leading pad. followTail is an inverted list following appends.
 */
MountedRange grownMountedRange(
  std::size_t lowIndex,
  std::size_t highIndex,
  bool lowAtStart,
  bool highAtEnd,
  std::size_t size,
  std::size_t overscanRowsLeading,
  bool followTail);

/*
 * Where the range should end up for a measured range: overscan on both sides, and the leading
 * pad ahead of the direction the measured range moved.
 */
MountedRange visibleTargetRange(
  const MountedRange& measured,
  const std::optional<MountedRange>& previousMeasured,
  std::size_t size,
  std::size_t overscanRows,
  std::size_t overscanRowsLeading);

/*
 * The next step of the range after a visible rows report, and the target it steps toward.
 */
struct ReportedRange {
  MountedRange range;
  MountedRange target;
};

/*
 * What a visible rows report does to the range, or nothing to keep it. The first report always
 * trims the initial guess to the measured range plus overscan.
 */
std::optional<ReportedRange> reportedMountedRange(
  const MountedRange& current,
  const MountedRange& measured,
  const std::optional<MountedRange>& previousMeasured,
  bool firstReport,
  std::size_t size,
  std::size_t overscanRows,
  std::size_t overscanRowsLeading,
  std::size_t minimumStep);
/*
 * The viewable range from a host's low and high, or nothing when none is viewable.
 */
std::optional<MountedRange> viewableRange(std::size_t low, std::size_t high);

/*
 * The section header the sticky overlay shows for a range starting at rangeLow: the last
 * sticky index at or above it, or UNDEFINED_INDEX. Indices are ascending.
 */
std::size_t activeStickyIndexFor(const std::vector<std::size_t>& stickyIndices, std::size_t rangeLow);

/*
 * What changed between two viewable key lists: keys that became viewable, in their order,
 * then keys that stopped, in theirs.
 */
struct ViewableChanges {
  std::vector<std::string> added;
  std::vector<std::string> removed;
};

ViewableChanges viewableChanges(const std::vector<std::string>& previous, const std::vector<std::string>& current);

}
