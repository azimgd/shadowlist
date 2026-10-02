#pragma once

#include <optional>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Which rows a host keeps mounted, and how that range follows the visible rows. The React
 * Native host runs the same steps in TypeScript (src/virtualizer/mountedRange.ts), function
 * for function, and the tests of both use the same cases.
 * Indices are row indices, low to high. A range with a negative end is empty.
 */
struct MountedRange {
  long low = -1;
  long high = -1;

  bool operator==(const MountedRange& other) const {
    return low == other.low && high == other.high;
  }
};

// Default overscan rows on each side, matching SHADOWLIST_OVERSCAN.
constexpr long DEFAULT_OVERSCAN_ROWS = 4;

/*
 * How many appended rows an inverted list at its end mounts on top of its range. A bigger
 * burst moves the range to the tail instead.
 */
constexpr long MAX_FOLLOWED_APPEND = 50;

/*
 * The first range, before the host reports what is visible. With a target it mounts around it,
 * split by viewPosition, which wins over an inverted list's bottom start.
 */
MountedRange initialMountedRange(long size, long initial, bool inverted, long offsetIndex,
  long overscanRows = DEFAULT_OVERSCAN_ROWS, double viewPosition = 0.0);

/*
 * Indices of both ranges, sorted and without duplicates. While a scroll command is on its way
 * the rows around its target and the rows still on screen both stay mounted.
 */
std::vector<long> unionRangeIndices(const MountedRange& first, const MountedRange& second);

std::vector<long> rangeToIndices(const MountedRange& range);

/*
 * Whether a new start index should rebuild the range around it. A negative value means no
 * target, and must not pull a reader who scrolled away back to the start.
 */
bool shouldReseedFromOffsetIndex(long previousOffsetIndex, long nextOffsetIndex);

/*
 * One step from the mounted range toward the target: rows on screen (window) mount right away,
 * the overscan beyond them grows by at most step rows per end. Shrinking is never paced.
 * A target that doesn't overlap the mounted rows, like after a jump, grows from the window.
 */
MountedRange stepMountedRange(const MountedRange& current, const MountedRange& target, const MountedRange& window, long step);

/*
 * Overscan rows to add per step for a window this size: a quarter of the window, at least
 * minimumStep, so a pad forms within a few frames however tall the rows are.
 */
long mountStepForWindow(const MountedRange& window, long minimumStep);

/*
 * The range from its edge rows' current indices. A range at an edge of the data grows to take
 * rows added past it, up to the leading pad. followTail is an inverted list following appends.
 */
MountedRange grownMountedRange(long lowIndex, long highIndex, bool lowAtStart, bool highAtEnd, long size,
  long overscanRowsLeading, bool followTail);

/*
 * Where the range should end up for a visible window: overscan on both sides, and the leading
 * pad ahead of the direction the window moved.
 */
MountedRange visibleTargetRange(const MountedRange& window, const std::optional<MountedRange>& lastWindow, long size,
  long overscanRows, long overscanRowsLeading);

/*
 * The next step of the range after a visible rows report, and the target it steps toward.
 */
struct ReportedRange {
  MountedRange range;
  MountedRange target;
};

/*
 * What a visible rows report does to the range, or nothing to keep it. A range that holds the
 * window stays, except on the first report, which trims the initial guess to the window plus
 * overscan.
 */
std::optional<ReportedRange> reportedMountedRange(const MountedRange& current, const MountedRange& window,
  const std::optional<MountedRange>& lastWindow, bool firstReport, long size, long overscanRows,
  long overscanRowsLeading, long minimumStep);

/*
 * The viewable window from a host's start and end, low to high, or nothing when none is
 * viewable. Inverted lists report start after end.
 */
std::optional<MountedRange> viewableWindow(long startIndex, long endIndex);

/*
 * The section header the sticky overlay shows for a window starting at windowLow: the last
 * sticky index at or above it, or -1. Indices are ascending.
 */
long activeStickyIndexFor(const std::vector<long>& stickyHeaderIndices, long windowLow);

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
