#include "TestFramework.hpp"

#include <shadowlist-core/utils/WrapLayout.hpp>

#include <limits>

using namespace azimgd::shadowlist;
using namespace slt;

TEST(wrap_layout_places_exact_fits_and_uses_the_tallest_child_in_each_line) {
  auto layout = wrapLayout(100.0, {{40.0, 20.0}, {52.0, 34.0}, {30.0, 28.0}}, 8.0);
  CHECK_EQ(layout.frames.size(), std::size_t(3));
  CHECK_EQ(layout.frames[1].x, 48.0);
  CHECK_EQ(layout.frames[1].y, 0.0);
  CHECK_EQ(layout.frames[2].x, 0.0);
  CHECK_EQ(layout.frames[2].y, 42.0);
  CHECK_EQ(layout.height, 70.0);
}

TEST(wrap_layout_keeps_oversized_children_and_their_source_order) {
  auto layout = wrapLayout(100.0, {{120.0, 34.0}, {20.0, 28.0}, {20.0, 40.0}}, 8.0);
  CHECK_EQ(layout.frames[0].width, 120.0);
  CHECK_EQ(layout.frames[1].x, 0.0);
  CHECK_EQ(layout.frames[1].y, 42.0);
  CHECK_EQ(layout.frames[2].x, 28.0);
  CHECK_EQ(layout.height, 82.0);
}

TEST(wrap_layout_has_no_trailing_gap_and_empty_rows_take_no_height) {
  CHECK_EQ(wrapLayout(100.0, {}, 8.0).height, 0.0);
  CHECK_EQ(wrapLayout(100.0, {{20.0, 28.0}}, 8.0).height, 28.0);
  CHECK_EQ(wrapLayout(0.0, {{20.0, 28.0}}, 8.0).frames.size(), std::size_t(0));
  CHECK_EQ(wrapLayout(std::numeric_limits<double>::infinity(), {{20.0, 28.0}}, 8.0).height, 0.0);
}

TEST(wrap_layout_sanitizes_invalid_native_measurements_without_reordering) {
  auto layout = wrapLayout(100.0, {{-1.0, 20.0}, {std::numeric_limits<double>::quiet_NaN(), 28.0}}, -3.0);
  CHECK_EQ(layout.frames.size(), std::size_t(2));
  CHECK_EQ(layout.frames[0].width, 0.0);
  CHECK_EQ(layout.frames[1].x, 0.0);
  CHECK_EQ(layout.height, 28.0);
}
