#pragma once

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <cstddef>
#include <set>
#include <string>
#include <vector>

/*
 * Fixture helpers shared by the core test files. Tests already use namespace slt.
 */
namespace slt {

inline constexpr double WINDOW_WIDTH = 390.0;
inline constexpr double WINDOW_HEIGHT = 840.0;

/*
 * Size used for a row nobody has measured or predicted yet.
 */
inline constexpr double ESTIMATED_ROW_HEIGHT = 120.0;

inline std::vector<std::string> keysFor(std::size_t count, const std::string& prefix = "k") {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    keys.push_back(prefix + std::to_string(index));
  }
  return keys;
}

/*
 * A one column vertical frame scrolled to offset, with every row estimated.
 */
inline azimgd::shadowlist::FrameInput inputFor(const std::vector<std::string>& keys, double offset) {
  azimgd::shadowlist::FrameInput input;
  input.keys = keys;
  input.windowWidth = WINDOW_WIDTH;
  input.windowHeight = WINDOW_HEIGHT;
  input.numberOfColumns = 1;
  input.overscan = 1.0;
  input.estimatedRowSize = {WINDOW_WIDTH, ESTIMATED_ROW_HEIGHT};
  input.offsetY = offset;
  return input;
}

/*
 * Position of a row along the scroll direction.
 */
inline double offsetOf(const azimgd::shadowlist::Container& container, std::size_t index) {
  const azimgd::shadowlist::Row& row = container.revision.rows[index];
  return container.horizontal ? row.offsetX : row.offsetY;
}

/*
 * Size of a row along the scroll direction.
 */
inline double sizeOf(const azimgd::shadowlist::Container& container, std::size_t index) {
  const azimgd::shadowlist::Row& row = container.revision.rows[index];
  return container.horizontal ? row.width : row.height;
}

/*
 * Every row that overlaps the screen plus overscanUnits screens on each side.
 * Done the slow and obvious way so window checks have something to compare against.
 */
inline std::set<std::size_t> overlappingIndices(const azimgd::shadowlist::Container& container, double overscanUnits) {
  double windowSize = container.horizontal
    ? container.revision.windowWidth
    : container.revision.windowHeight;
  double offset = container.horizontal
    ? container.revision.offsetX
    : container.revision.offsetY;
  double bandSize = windowSize * overscanUnits;
  double lowerBound = offset - bandSize;
  double upperBound = offset + windowSize + bandSize;

  std::set<std::size_t> overlapping;
  for (std::size_t index = 0; index < container.revision.rows.size(); ++index) {
    double rowOffset = offsetOf(container, index);
    double rowSize = sizeOf(container, index);
    /*
     * Count real overlap only. A row starting exactly on the far edge covers no pixels.
     * The core may include it or not.
     */
    if (rowOffset >= upperBound) {
      continue;
    }
    if (rowOffset + rowSize <= lowerBound) {
      continue;
    }
    overlapping.insert(index);
  }
  return overlapping;
}

}
