#pragma once

#include <limits>
#include <string>

namespace facebook::react {

/*
 * Describes a row's text before it renders, so its height can be predicted.
 * Comes from the elementsSizeSpecs prop and is measured by ShadowListTextMeasurer.h.
 * Lives in its own header so the shadow node can cache specs without the text layout code.
 */
struct ShadowListElementSizeSpec {
  static constexpr double DEFAULT_FONT_SIZE = 14.0;

  std::string key;
  std::string text;

  std::string fontFamily;
  double fontSize = DEFAULT_FONT_SIZE;
  std::string fontWeight;
  std::string fontStyle;
  double lineHeight = std::numeric_limits<double>::quiet_NaN();
  double letterSpacing = std::numeric_limits<double>::quiet_NaN();
  int numberOfLines = 0;

  /*
   * Space in the row around the text, in points, like padding or an avatar column.
   * Width is taken off the wrap width and height is added to the measured text.
   * Rows whose extra space can't be described by these two numbers should not be predicted.
   */
  double insetWidth = 0.0;
  double insetHeight = 0.0;

  /*
   * Share of the list width the text may use before insetWidth, for rows with a percent width
   * such as a chat bubble capped at 75 percent. A fixed inset can't express that without wrapping wrong.
   */
  double widthFraction = 1.0;

  // A known row height. When set, the text is not measured and this height is used as is.
  double fixedHeight = std::numeric_limits<double>::quiet_NaN();
};

}
