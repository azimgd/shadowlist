#pragma once

#include <shadowlist-core/Constants.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Which rows a host keeps mounted, and how that range follows the visible rows. MountDriver
 * runs these for Fabric, where React mounts the rows. Fabric's JS keeps only what must run in
 * the render that changes the data or the scroll target, the initial range and how stored edge
 * keys grow with appended rows. A range with an UNDEFINED_INDEX end is empty.
 */
struct MountedRange {
  std::size_t low = UNDEFINED_INDEX;
  std::size_t high = UNDEFINED_INDEX;

  bool isEmpty() const {
    return low == UNDEFINED_INDEX || high == UNDEFINED_INDEX || low > high;
  }

  bool operator==(const MountedRange& other) const {
    return low == other.low && high == other.high;
  }

  bool operator!=(const MountedRange& other) const {
    return !(*this == other);
  }
};

/*
 * Default overscan rows on each side, and ahead of a scroll, matching SHADOWLIST_OVERSCAN and
 * SHADOWLIST_OVERSCAN_LEADING.
 */
constexpr std::size_t DEFAULT_OVERSCAN_ROWS = 4;
constexpr std::size_t DEFAULT_OVERSCAN_ROWS_LEADING = 10;

/*
 * New overscan rows mounted per end per step, see stepMountedRange. Rows on screen always
 * mount in the same step.
 */
constexpr std::size_t MOUNT_STEP_ROWS = 2;

/*
 * How many appended rows an inverted list at its end mounts on top of its range. A bigger
 * burst moves the range to the tail instead. Fabric's JS grows its range by the same rule.
 */
constexpr std::size_t MAX_FOLLOWED_APPEND = 50;

/*
 * One step from the mounted range toward the target. Rows on screen mount right away and the
 * overscan beyond them grows by at most step rows per end. Mounting a whole overscan pad at once
 * puts ten or more new rows, each a fresh subtree, into one commit. Shrinking is never paced. A
 * target that doesn't overlap the mounted rows, like after a jump, grows from the measured range.
 */
MountedRange stepMountedRange(
  const MountedRange& current,
  const MountedRange& target,
  const MountedRange& measured,
  std::size_t step);

/*
 * Overscan rows to add per step, a quarter of the measured range and at least minimumStep. A
 * fling over short rows then still builds a leading pad within a few frames.
 */
std::size_t mountStepForRange(const MountedRange& measured, std::size_t minimumStep);

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
 * steps, which trims the initial guess. A range that holds the measured rows is kept unless rows
 * inserted inside it made it far bigger than the target, like a tree expanding every folder.
 * Then it shrinks to the target.
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
 * sticky index at or above it, or UNDEFINED_INDEX. Indices are ascending. Binary search.
 */
std::size_t activeStickyIndexFor(const std::vector<std::size_t>& stickyIndices, std::size_t rangeLow);

/*
 * What changed between two viewable key lists: keys that became viewable, in their order,
 * then keys that stopped, in theirs.
 */
struct ViewableChanges {
  std::vector<std::string> added;
  std::vector<std::string> removed;

  bool isEmpty() const {
    return added.empty() && removed.empty();
  }
};

ViewableChanges viewableChanges(const std::vector<std::string>& previous, const std::vector<std::string>& current);

}
