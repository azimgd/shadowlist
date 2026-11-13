#pragma once

#include <shadowlist-core/Row.hpp>

#include <vector>

namespace azimgd::shadowlist {

/*
 * Leading-aligned, wrapping child frames. The host measures each child and mirrors the frames
 * for its native layout direction. Oversized children keep their measured width on one line.
 */
struct WrapLayout {
  struct Frame {
    double x;
    double y;
    double width;
    double height;
  };

  std::vector<Frame> frames;
  double height = 0.0;
};

/*
 * Wrap measured sizes at width, with gap between both columns and lines. Empty inputs or a
 * nonpositive/nonfinite width yield an empty layout. Invalid sizes and gaps become zero.
 */
WrapLayout wrapLayout(double width, const std::vector<Size>& sizes, double gap);

}
