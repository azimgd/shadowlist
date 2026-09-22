#pragma once

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <cstddef>
#include <set>
#include <string>
#include <vector>

// Fixture helpers shared by the core test files. Tests already use namespace slt.
namespace slt {

inline constexpr double WINDOW_WIDTH = 390.0;
inline constexpr double WINDOW_HEIGHT = 840.0;
// Size used for a row nobody has measured or predicted yet.
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
  input.windowContainerWidth = WINDOW_WIDTH;
  input.windowContainerHeight = WINDOW_HEIGHT;
  input.columns = 1;
  input.overscan = 1.0;
  input.estimatedElementSize = {WINDOW_WIDTH, ESTIMATED_ROW_HEIGHT};
  input.containerOffsetY = offset;
  return input;
}

/*
 * Position of a row along the scroll direction.
 */
inline double offsetOf(const azimgd::shadowlist::Container& container, std::size_t index) {
  const azimgd::shadowlist::Element& element = container.revision.elements[index];
  return container.horizontal ? element.offsetX : element.offsetY;
}

/*
 * Size of a row along the scroll direction.
 */
inline double sizeOf(const azimgd::shadowlist::Container& container, std::size_t index) {
  const azimgd::shadowlist::Element& element = container.revision.elements[index];
  return container.horizontal ? element.width : element.height;
}

/*
 * Every row that overlaps the screen plus overscanUnits screens on each side.
 * Done the slow and obvious way so window checks have something to compare against.
 */
inline std::set<std::size_t> overlappingIndices(const azimgd::shadowlist::Container& container, double overscanUnits) {
  double windowSize = container.horizontal
    ? container.revision.windowContainerWidth
    : container.revision.windowContainerHeight;
  double offset = container.horizontal
    ? container.revision.containerOffsetX
    : container.revision.containerOffsetY;
  double bandSize = windowSize * overscanUnits;
  double lowerBound = offset - bandSize;
  double upperBound = offset + windowSize + bandSize;

  std::set<std::size_t> overlapping;
  for (std::size_t index = 0; index < container.revision.elements.size(); ++index) {
    double elementOffset = offsetOf(container, index);
    double elementSize = sizeOf(container, index);
    /*
     * Count real overlap only. A row starting exactly on the far edge covers no pixels,
     * so the core may include it or not.
     */
    if (elementOffset >= upperBound) {
      continue;
    }
    if (elementOffset + elementSize <= lowerBound) {
      continue;
    }
    overlapping.insert(index);
  }
  return overlapping;
}

}
