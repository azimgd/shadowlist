#pragma once

#include <cstddef>

namespace azimgd::shadowlist {

/*
 * The section index along a vertical list's trailing edge in both kits: one title per row of
 * SECTION_INDEX_TITLE_HEIGHT, centered in the area, in a strip SECTION_INDEX_WIDTH wide. Sizes are
 * in points, or in dp that the Android kit scales to pixels.
 */
constexpr double SECTION_INDEX_TITLE_HEIGHT = 16.0;
constexpr double SECTION_INDEX_WIDTH = 24.0;

/*
 * Where the first title starts in an area of areaHeight holding count titles.
 */
inline double sectionIndexTitlesTop(double areaHeight, std::size_t count, double scale) {
  double used = SECTION_INDEX_TITLE_HEIGHT * scale * static_cast<double>(count);
  return areaHeight > used ? (areaHeight - used) / 2.0 : 0.0;
}

/*
 * The title under y, measured from the area's top, clamped to the titles. 0 without titles.
 */
inline std::size_t sectionIndexTitleAt(double y, double areaHeight, std::size_t count, double scale) {
  if (count == 0) {
    return 0;
  }
  double position = (y - sectionIndexTitlesTop(areaHeight, count, scale)) / (SECTION_INDEX_TITLE_HEIGHT * scale);
  if (position < 0.0) {
    return 0;
  }
  std::size_t index = static_cast<std::size_t>(position);
  return index < count ? index : count - 1;
}

}
