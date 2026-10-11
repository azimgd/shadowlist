#pragma once

#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <cstddef>
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
    fail(context + ": " + std::to_string(overlapping.size()) +
      " rows overlap the viewport but the core reported no measured range");
  }
  for (std::size_t index : overlapping) {
    if (index < measured.low || index > measured.high) {
      fail(context + ": row " + std::to_string(index) +
        " overlaps the viewport but is outside the measured range [" +
        std::to_string(measured.low) + ".." + std::to_string(measured.high) + "]");
    }
  }
}

/*
 * Within each column, every row must start exactly where the one before it ended.
 * A reflow that skipped a row fails here.
 */
inline void checkGeometryContiguous(const Container& container, const std::string& context) {
  std::size_t columns = container.numberOfColumns > 0 ? container.numberOfColumns : 1;
  double headerSize = container.headerSize;

  std::vector<double> trackEdges(columns, headerSize);
  for (std::size_t index = 0; index < container.revision.rows.size(); ++index) {
    std::size_t track = columns > 1 ? index % columns : 0;
    double expected = trackEdges[track];
    double actual = offsetOf(container, index);
    if (std::fabs(actual - expected) > 0.001) {
      fail(context + ": row " + std::to_string(index) + " sits at " + std::to_string(actual) +
        " but the running track edge is " + std::to_string(expected));
    }
    trackEdges[track] = actual + sizeOf(container, index);
  }
}

/*
 * Give every row a real, uneven size. Equal rows hide most window bugs.
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
