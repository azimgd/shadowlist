/*
 * Feed layout tests: the frame of a post's single image.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/utils/FeedImageFrame.hpp>

#include <limits>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(an_image_inside_the_range_keeps_its_aspect) {
  FeedImageFrame frame = feedSingleImageFrame(1000.0, 1000.0);
  CHECK_EQ(frame.aspectRatio, 1.0);
  CHECK_EQ(frame.height, 0.0);
  CHECK_EQ(feedSingleImageFrame(400.0, 300.0).aspectRatio, 400.0 / 300.0);
}

TEST(a_tall_image_is_kept_at_three_to_four) {
  CHECK_EQ(feedSingleImageFrame(600.0, 1600.0).aspectRatio, FEED_IMAGE_MIN_ASPECT_RATIO);
  CHECK_EQ(feedSingleImageFrame(300.0, 400.0).aspectRatio, 3.0 / 4.0);
}

TEST(a_wide_image_is_kept_at_sixteen_to_nine) {
  CHECK_EQ(feedSingleImageFrame(3000.0, 500.0).aspectRatio, FEED_IMAGE_MAX_ASPECT_RATIO);
  CHECK_EQ(feedSingleImageFrame(1600.0, 900.0).aspectRatio, 16.0 / 9.0);
}

TEST(an_image_without_a_size_takes_the_fixed_height) {
  for (FeedImageFrame frame : {feedSingleImageFrame(0.0, 0.0), feedSingleImageFrame(800.0, 0.0),
         feedSingleImageFrame(0.0, 600.0)}) {
    CHECK_EQ(frame.aspectRatio, 0.0);
    CHECK_EQ(frame.height, FEED_SINGLE_IMAGE_HEIGHT);
  }
}

TEST(an_image_size_that_is_not_a_number_takes_the_fixed_height) {
  double nan = std::numeric_limits<double>::quiet_NaN();
  double infinity = std::numeric_limits<double>::infinity();
  CHECK_EQ(feedSingleImageFrame(nan, 600.0).height, FEED_SINGLE_IMAGE_HEIGHT);
  CHECK_EQ(feedSingleImageFrame(800.0, nan).height, FEED_SINGLE_IMAGE_HEIGHT);
  CHECK_EQ(feedSingleImageFrame(infinity, infinity).height, FEED_SINGLE_IMAGE_HEIGHT);
}
