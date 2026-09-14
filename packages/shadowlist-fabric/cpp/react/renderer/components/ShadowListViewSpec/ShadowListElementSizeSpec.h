#pragma once

#include <limits>
#include <string>

namespace facebook::react {

/*
 * One row's text, as the host can describe it before rendering: the string, the attributes
 * that affect how it wraps, and the fixed chrome around it. Parsed from the
 * `elementsSizeSpecs` prop and measured by ShadowListTextMeasurer.h; kept in its own header
 * so the shadow node can cache parsed specs without pulling in the text layout stack.
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
   * Everything in the row that is NOT the text, in points: padding, borders, an avatar
   * column, a timestamp row. The host knows these from its own styles; the core needs the
   * whole row's size, not the paragraph's.
   *
   * `insetWidth` is subtracted from the width the text gets to wrap in, and `insetHeight`
   * is added to the height it measures to. A row whose chrome does not reduce to two
   * scalars is one the host should leave unpredicted.
   */
  double insetWidth = 0.0;
  double insetHeight = 0.0;

  /*
   * Fraction of the list width the text may occupy before `insetWidth` is taken off it,
   * for the very common case of a percentage-width row element -- a chat bubble capped at
   * `maxWidth: '75%'`, say. A percentage cannot be expressed as a fixed inset, and rounding
   * it to one would mis-wrap every row at a width the author never tested.
   */
  double widthFraction = 1.0;

  /*
   * A height the host already knows exactly (a fixed-height row). When set, no text
   * measurement happens at all and this is published verbatim -- the cheapest possible
   * prediction, and the right one for rows that have no text to measure.
   */
  double fixedHeight = std::numeric_limits<double>::quiet_NaN();
};

}
