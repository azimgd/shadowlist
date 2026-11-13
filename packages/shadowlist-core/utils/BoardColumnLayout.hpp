#pragma once

#include <shadowlist-core/utils/Rounding.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace azimgd::shadowlist {

/*
 * A board's horizontal layout, in points. A column takes BOARD_COLUMN_SHARE of the board on a
 * phone, capped at BOARD_COLUMN_MAX_WIDTH for wide windows where several columns fit, with the
 * next column's edge in view as a hint to scroll sideways. BOARD_COLUMN_GAP sits between
 * columns, and BOARD_SIDE_PADDING before the first and after the last.
 */
constexpr double BOARD_COLUMN_GAP = 12.0;
constexpr double BOARD_COLUMN_SHARE = 0.78;
constexpr double BOARD_COLUMN_MAX_WIDTH = 360.0;
constexpr double BOARD_SIDE_PADDING = 24.0;

/*
 * snapInterval is a column and its gap. The board then always comes to rest on a column's
 * leading edge.
 */
struct BoardColumnLayout {
  double columnWidth = 0.0;
  double sidePadding = 0.0;
  double snapInterval = 0.0;
};

/*
 * The columns of a board boardWidth wide. A columnWidth that is positive and finite is used as
 * it is. Anything else, a NaN included, takes the default share of the board. A column narrower
 * than the share keeps BOARD_SIDE_PADDING on each side, and one as wide or wider is centered,
 * never with a negative padding. A sidePadding that is finite and 0 or more is used as it is,
 * whatever the width.
 */
inline BoardColumnLayout boardColumnLayout(double boardWidth, double columnWidth, double sidePadding) {
  double share = roundHalfUp(boardWidth * BOARD_COLUMN_SHARE);
  bool hasWidth = std::isfinite(columnWidth) && columnWidth > 0.0;
  double width = hasWidth ? columnWidth : std::min(share, BOARD_COLUMN_MAX_WIDTH);
  double padding = BOARD_SIDE_PADDING;
  if (std::isfinite(sidePadding) && sidePadding >= 0.0) {
    padding = sidePadding;
  } else if (!(width < share)) {
    padding = std::max(0.0, roundHalfUp((boardWidth - width) / 2.0));
  }
  return {width, padding, width + BOARD_COLUMN_GAP};
}

/*
 * The height every column of a board takes in content mode: the tallest of columnHeights, never
 * below minimum or above maximum. The maximum wins when the two cross, and the result is never
 * negative.
 */
inline double boardColumnHeight(const std::vector<double>& columnHeights, double minimum, double maximum) {
  double tallest = 0.0;
  for (double height : columnHeights) {
    tallest = std::max(tallest, height);
  }
  return std::max(0.0, std::min(maximum, std::max(minimum, tallest)));
}

}
