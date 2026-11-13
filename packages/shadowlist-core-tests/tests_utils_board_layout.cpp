/*
 * Board layout tests: a board's column width, side padding and snap interval, and the height
 * its columns share in content mode.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/utils/BoardColumnLayout.hpp>

#include <limits>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

constexpr double UNSET = std::numeric_limits<double>::quiet_NaN();

void checkLayout(const BoardColumnLayout& layout, double columnWidth, double sidePadding, double snapInterval) {
  CHECK_EQ(layout.columnWidth, columnWidth);
  CHECK_EQ(layout.sidePadding, sidePadding);
  CHECK_EQ(layout.snapInterval, snapInterval);
}

}

TEST(a_phone_wide_board_gives_a_column_its_share_centered) {
  checkLayout(boardColumnLayout(400.0, UNSET, UNSET), 312.0, 44.0, 324.0);
}

TEST(a_wide_board_caps_the_column_and_keeps_the_side_padding) {
  checkLayout(boardColumnLayout(1200.0, UNSET, UNSET), BOARD_COLUMN_MAX_WIDTH, BOARD_SIDE_PADDING, 372.0);
}

TEST(a_narrow_board_keeps_the_side_padding) {
  checkLayout(boardColumnLayout(400.0, 280.0, UNSET), 280.0, BOARD_SIDE_PADDING, 292.0);
  checkLayout(boardColumnLayout(1200.0, 500.0, UNSET), 500.0, BOARD_SIDE_PADDING, 512.0);
  CHECK_EQ(boardColumnLayout(400.0, 300.5, UNSET).columnWidth, 300.5);
}

TEST(a_column_as_wide_as_the_share_or_wider_is_centered_never_below_zero) {
  CHECK_EQ(boardColumnLayout(400.0, 340.0, UNSET).sidePadding, 30.0);
  CHECK_EQ(boardColumnLayout(400.0, 312.0, UNSET).sidePadding, 44.0);
  CHECK_EQ(boardColumnLayout(400.0, 600.0, UNSET).sidePadding, 0.0);
}

TEST(a_given_side_padding_is_used_whatever_the_width) {
  checkLayout(boardColumnLayout(400.0, UNSET, 16.0), 312.0, 16.0, 324.0);
  CHECK_EQ(boardColumnLayout(400.0, 280.0, 16.0).sidePadding, 16.0);
  CHECK_EQ(boardColumnLayout(400.0, 600.0, 20.0).sidePadding, 20.0);
  CHECK_EQ(boardColumnLayout(1200.0, UNSET, 0.0).sidePadding, 0.0);
  CHECK_EQ(boardColumnLayout(400.0, 280.0, 12.5).sidePadding, 12.5);
}

TEST(a_side_padding_that_is_negative_or_not_finite_takes_the_default) {
  for (double padding : {-1.0, UNSET, std::numeric_limits<double>::infinity()}) {
    CHECK_EQ(boardColumnLayout(400.0, UNSET, padding).sidePadding, 44.0);
    CHECK_EQ(boardColumnLayout(400.0, 280.0, padding).sidePadding, BOARD_SIDE_PADDING);
  }
}

TEST(a_column_width_that_is_not_positive_and_finite_takes_the_default) {
  for (double width : {0.0, -280.0, UNSET, std::numeric_limits<double>::infinity()}) {
    checkLayout(boardColumnLayout(400.0, width, UNSET), 312.0, 44.0, 324.0);
  }
}

TEST(a_board_share_of_a_half_rounds_up) {
  // 25 * 0.78 is 19.5.
  CHECK_EQ(boardColumnLayout(25.0, UNSET, UNSET).columnWidth, 20.0);
}

TEST(board_columns_take_the_height_of_the_tallest) {
  CHECK_EQ(boardColumnHeight({120.0, 480.0, 300.0}, 0.0, 1000.0), 480.0);
  CHECK_EQ(boardColumnHeight({90.0}, 0.0, 1000.0), 90.0);
}

TEST(a_board_column_height_stays_between_the_minimum_and_the_maximum) {
  CHECK_EQ(boardColumnHeight({10.0}, 120.0, 1000.0), 120.0);
  CHECK_EQ(boardColumnHeight({5000.0}, 120.0, 600.0), 600.0);
  CHECK_EQ(boardColumnHeight({}, 120.0, 600.0), 120.0);
}

TEST(a_maximum_below_the_minimum_wins) {
  CHECK_EQ(boardColumnHeight({180.0}, 300.0, 200.0), 200.0);
  // Never negative.
  CHECK_EQ(boardColumnHeight({180.0}, 0.0, -50.0), 0.0);
}
