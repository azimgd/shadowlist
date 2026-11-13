#pragma once

#include <algorithm>
#include <cmath>

namespace azimgd::shadowlist {

/*
 * The height of a feed post's single image that has no size, and the aspects an image with a
 * size is kept between, in points.
 */
constexpr double FEED_SINGLE_IMAGE_HEIGHT = 200.0;
constexpr double FEED_IMAGE_MIN_ASPECT_RATIO = 3.0 / 4.0;
constexpr double FEED_IMAGE_MAX_ASPECT_RATIO = 16.0 / 9.0;

/*
 * How an image is framed: by aspectRatio, width over height, or by a fixed height. The one that
 * does not apply is 0.
 */
struct FeedImageFrame {
  double aspectRatio = 0.0;
  double height = 0.0;
};

/*
 * A feed post's single image: its own aspect kept between 3:4 and 16:9, or
 * FEED_SINGLE_IMAGE_HEIGHT tall when the image has no size.
 */
inline FeedImageFrame feedSingleImageFrame(double width, double height) {
  double ratio = width / height;
  // A side of 0 means the size is unknown. A ratio that is not a number frames nothing.
  if (width == 0.0 || height == 0.0 || std::isnan(ratio)) {
    return {0.0, FEED_SINGLE_IMAGE_HEIGHT};
  }
  return {std::min(FEED_IMAGE_MAX_ASPECT_RATIO, std::max(FEED_IMAGE_MIN_ASPECT_RATIO, ratio)), 0.0};
}

}
