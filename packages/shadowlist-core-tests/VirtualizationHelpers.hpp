#pragma once

#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <cmath>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

/*
 * Fixture and checks shared by the virtualization test files.
 */
namespace slt {

using namespace azimgd::shadowlist;

struct Fixture {
  std::size_t numberOfColumns = 1;
  bool horizontal = false;
  bool inverted = false;
  bool followAppends = false;
  double overscan = 1.0;
  double estimatedWidth = WINDOW_WIDTH;
  double estimatedHeight = ESTIMATED_ROW_HEIGHT;
};

inline FrameInput inputFor(const std::vector<std::string>& keys, double offset, const Fixture& fixture) {
  FrameInput input;
  input.keys = keys;
  input.windowWidth = WINDOW_WIDTH;
  input.windowHeight = WINDOW_HEIGHT;
  input.numberOfColumns = fixture.numberOfColumns;
  input.horizontal = fixture.horizontal;
  input.inverted = fixture.inverted;
  input.followAppends = fixture.followAppends;
  input.overscan = fixture.overscan;
  input.estimatedRowSize = {fixture.estimatedWidth, fixture.estimatedHeight};
  if (fixture.horizontal) {
    input.offsetX = offset;
  } else {
    input.offsetY = offset;
  }
  return input;
}

/*
 * No row on screen or in the overscan may fall outside the range the host mounts.
 */
inline void checkNoRowLost(const Container& container, const std::string& context) {
  std::set<std::size_t> overlapping = overlappingIndices(container, container.overscan);
  IndexRange measured = container.getMeasuredRange();

  if (overlapping.empty()) {
    return;
  }

  if (measured.low == UNDEFINED_INDEX) {
    fail(
      context + ": " + std::to_string(overlapping.size()) +
      " rows overlap the viewport but the core reported no measured range");
  }
  for (std::size_t index : overlapping) {
    if (index < measured.low || index > measured.high) {
      fail(
        context + ": row " + std::to_string(index) + " overlaps the viewport but is outside the measured range [" +
        std::to_string(measured.low) + ".." + std::to_string(measured.high) + "]");
    }
  }
}

/*
 * A scroll report from the host with its offset and gesture phase. A view that is not idle
 * reports a user scroll.
 */
inline FrameInput
scrollReport(const std::vector<std::string>& keys, double offset, const Fixture& fixture, ScrollPhase phase) {
  FrameInput input = inputFor(keys, offset, fixture);
  input.scrollPhase = phase;
  input.userScrolled = phase != ScrollPhase::Idle;
  return input;
}

/*
 * The first row that ends after position, or the row count when none does. The rows of one
 * column are in order and searched. With more columns every row is a candidate and the answer is 0.
 */
inline std::size_t firstRowEndingAfter(const Container& container, double position) {
  if (container.numberOfColumns > 1) {
    return 0;
  }
  std::size_t low = 0;
  std::size_t high = container.revision.rows.size();
  while (low < high) {
    std::size_t middle = low + (high - low) / 2;
    if (offsetOf(container, middle) + sizeOf(container, middle) <= position) {
      low = middle + 1;
    } else {
      high = middle;
    }
  }
  return low;
}

/*
 * The first row overlapping the viewport at offset that the measured range, grown by pad rows
 * at each end, leaves out. A host mounts that many rows around the range. UNDEFINED_INDEX when
 * every row on screen is mounted.
 */
inline std::size_t firstRowNotMounted(const Container& container, double offset, std::size_t pad) {
  IndexRange measured = container.getMeasuredRange();
  bool mountsNothing = measured.low == UNDEFINED_INDEX;
  std::size_t low = mountsNothing ? 0 : measured.low - std::min(measured.low, pad);
  std::size_t high = mountsNothing ? 0 : measured.high + pad;
  double end = offset + container.getWindowSize();
  bool ordered = container.numberOfColumns <= 1;
  for (std::size_t index = firstRowEndingAfter(container, offset); index < container.revision.rows.size(); ++index) {
    double start = offsetOf(container, index);
    if (start >= end) {
      if (ordered) {
        break;
      }
      continue;
    }
    if (start + sizeOf(container, index) <= offset) {
      continue;
    }
    if (mountsNothing || index < low || index > high) {
      return index;
    }
  }
  return UNDEFINED_INDEX;
}

/*
 * Within each column, every row must start exactly where the one before it ended. Returns
 * the first row that does not, in words, or nothing when the rows are contiguous.
 */
inline std::string describeGeometryGap(const Container& container) {
  std::size_t columns = container.numberOfColumns > 0 ? container.numberOfColumns : 1;
  double headerSize = container.headerSize;

  bool horizontal = container.horizontal;
  const std::vector<Row>& rows = container.revision.rows;
  std::vector<double> trackEdges(columns, headerSize);
  double* edges = trackEdges.data();
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const Row& row = rows[index];
    std::size_t track = columns > 1 ? index % columns : 0;
    double expected = edges[track];
    double actual = horizontal ? row.offsetX : row.offsetY;
    double size = horizontal ? row.width : row.height;
    // A sum with a value that is not finite is not finite.
    if (!std::isfinite(actual + size)) {
      return "row " + std::to_string(index) + " has no finite place or size";
    }
    if (std::fabs(actual - expected) > 0.001) {
      return "row " + std::to_string(index) + " sits at " + std::to_string(actual) + " but the running track edge is " +
             std::to_string(expected);
    }
    edges[track] = actual + size;
  }
  return "";
}

/*
 * A reflow that skipped a row fails here.
 */
inline void checkGeometryContiguous(const Container& container, const std::string& context) {
  std::string gap = describeGeometryGap(container);
  if (!gap.empty()) {
    fail(context + ": " + gap);
  }
}

/*
 * A layout pass over the rows low to high: every size lands in one batch, then one reflow from
 * the lowest row that changed, then the content size. sizeFor is called as sizeFor(index) and
 * returns the row's Size, or an empty optional for a row the pass leaves alone. Returns whether
 * a row changed.
 */
template <typename SizeFor>
bool layoutRows(Container& container, std::size_t low, std::size_t high, const SizeFor& sizeFor) {
  std::size_t lowestChangedIndex = UNDEFINED_INDEX;
  for (std::size_t index = low; index <= high; ++index) {
    std::optional<Size> size = sizeFor(index);
    if (size && Virtualizer::applyRowSize(container, index, *size) && index < lowestChangedIndex) {
      lowestChangedIndex = index;
    }
  }
  if (lowestChangedIndex != UNDEFINED_INDEX) {
    Virtualizer::commitRowSizes(container, lowestChangedIndex);
  }
  Virtualizer::recomputeContentSize(container);
  return lowestChangedIndex != UNDEFINED_INDEX;
}

/*
 * The same pass with a height for each row from low on, at the window's width.
 */
inline bool layoutRows(Container& container, std::size_t low, const std::vector<double>& heights) {
  if (heights.empty()) {
    return false;
  }
  return layoutRows(container, low, low + heights.size() - 1, [&](std::size_t index) {
    return Size{WINDOW_WIDTH, heights[index - low]};
  });
}

/*
 * Give every row a real, uneven size. Equal rows hide most measured range bugs.
 */
inline void measureRows(Container& container, const std::vector<double>& heights) {
  for (std::size_t index = 0; index < heights.size(); ++index) {
    Virtualizer::updateRowAtIndex(container, index, {WINDOW_WIDTH, heights[index]});
  }
}

inline std::vector<double> unevenHeights(std::size_t count) {
  std::vector<double> heights;
  heights.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    // Mix tall rows, short rows and a few with zero height.
    if (index % 17 == 0) {
      heights.push_back(0.0);
    } else if (index % 5 == 0) {
      heights.push_back(420.0);
    } else if (index % 3 == 0) {
      heights.push_back(24.0);
    } else {
      heights.push_back(96.0 + static_cast<double>(index % 7) * 11.0);
    }
  }
  return heights;
}

/*
 * Settle an inverted list at its bottom by reporting the offset back each frame, like the
 * host does. Returns the resting offset.
 */
inline double settleAtBottom(
  Container& container,
  const std::vector<std::string>& keys,
  const Fixture& fixture,
  const std::vector<std::string>& nonAnchorKeys = {}) {
  double offset = 0.0;
  for (int frame = 0; frame < 8; ++frame) {
    FrameInput input = inputFor(keys, offset, fixture);
    input.nonAnchorKeys = nonAnchorKeys;
    Virtualizer::update(container, input);
    offset = container.revision.offsetY;
  }
  return offset;
}

}
