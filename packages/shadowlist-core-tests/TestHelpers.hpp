#pragma once

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <cstddef>
#include <set>
#include <string>
#include <vector>

/*
 * Fixture helpers shared by the core test files. They live in namespace slt, so a test
 * file picks them up through the `using namespace slt;` it already has.
 */
namespace slt {

inline constexpr double WINDOW_WIDTH = 390.0;
inline constexpr double WINDOW_HEIGHT = 840.0;
// Estimated main-axis size of a row nobody has measured or predicted.
inline constexpr double ESTIMATED_ROW_HEIGHT = 120.0;

inline std::vector<std::string> keysFor(std::size_t count, const std::string& prefix = "k") {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    keys.push_back(prefix + std::to_string(index));
  }
  return keys;
}

// A single-column vertical frame scrolled to `offset`, estimating rows at ESTIMATED_ROW_HEIGHT.
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

// Element offset along the scroll axis.
inline double offsetOf(const azimgd::shadowlist::Container& container, std::size_t index) {
  const azimgd::shadowlist::Element& element = container.revision.elements[index];
  return container.horizontal ? element.offsetX : element.offsetY;
}

// Element size along the scroll axis.
inline double sizeOf(const azimgd::shadowlist::Container& container, std::size_t index) {
  const azimgd::shadowlist::Element& element = container.revision.elements[index];
  return container.horizontal ? element.width : element.height;
}

/*
 * Every index that overlaps the viewport widened by `overscanUnits` viewports on each side,
 * decided the slow, obvious way. This is the oracle window and band assertions check against.
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
     * Genuine overlap only. A row whose leading edge sits exactly on the far bound covers
     * zero pixels of the region, so whether a given pass includes it is a tie-break, not a
     * lost row; the core is free to report a superset.
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
