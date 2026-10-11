/*
 * SwipeReveal tests: the swipe action offsets both native lists use.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/SwipeReveal.hpp>

#include <cmath>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

SwipeSpec spec() {
  SwipeSpec value;
  value.leadingWidth = 80.0;
  value.trailingWidth = 160.0;
  value.trailingFullSwipe = true;
  value.rowSize = 400.0;
  return value;
}

}

TEST(swipe_follows_the_finger_then_rubber_bands_past_the_actions) {
  SwipeReveal swipe;
  swipe.begin(spec(), 0.0);
  CHECK_EQ(swipe.drag(40.0), 40.0);
  CHECK_EQ(swipe.drag(120.0), 80.0 + 40.0 * SWIPE_RUBBER_BAND);
  // The trailing side allows a full swipe and follows all the way.
  CHECK_EQ(swipe.drag(-300.0), -300.0);
  CHECK_EQ(swipe.drag(-900.0), -400.0);
}

TEST(swipe_side_without_actions_stays_closed) {
  SwipeSpec only = spec();
  only.leadingWidth = 0.0;
  SwipeReveal swipe;
  swipe.begin(only, 0.0);
  CHECK_EQ(swipe.drag(50.0), 0.0);
}

TEST(swipe_settles_open_closed_or_full) {
  SwipeReveal swipe;
  swipe.begin(spec(), 0.0);
  SwipeRest open = swipe.settle(50.0, 0.0, 500.0);
  CHECK(open.side == SwipeSide::Leading);
  CHECK_EQ(open.offset, 80.0);
  SwipeRest closed = swipe.settle(30.0, 0.0, 500.0);
  CHECK(closed.side == SwipeSide::None);
  CHECK_EQ(closed.offset, 0.0);
  // A short flick toward the side opens it, one away from it closes it.
  CHECK(swipe.settle(-20.0, -900.0, 500.0).side == SwipeSide::Trailing);
  CHECK(swipe.settle(-120.0, 900.0, 500.0).side == SwipeSide::None);
  SwipeRest full = swipe.settle(-250.0, 0.0, 500.0);
  CHECK(full.full);
  CHECK(full.side == SwipeSide::Trailing);
  CHECK_EQ(full.offset, -400.0);
  CHECK(!swipe.isPastFullSwipe(200.0));
}

TEST(swipe_starting_open_continues_from_the_open_offset) {
  SwipeReveal swipe;
  swipe.begin(spec(), -160.0);
  CHECK_EQ(swipe.drag(100.0), -60.0);
  CHECK(swipe.settle(-60.0, 0.0, 500.0).side == SwipeSide::None);
}

TEST(swipe_button_size_pads_the_title_and_never_goes_below_the_minimum) {
  CHECK_EQ(swipeButtonSize(20.0, 1.0), SWIPE_BUTTON_MIN);
  CHECK_EQ(swipeButtonSize(80.0, 1.0), 80.0 + SWIPE_BUTTON_PADDING);
  // Android scales the dp constants by the density.
  CHECK_EQ(swipeButtonSize(30.0, 3.0), SWIPE_BUTTON_MIN * 3.0);
  CHECK_EQ(swipeButtonSize(300.0, 3.0), 300.0 + SWIPE_BUTTON_PADDING * 3.0);
}

TEST(swipe_buttons_stretch_over_the_gap_from_the_outer_edge) {
  std::vector<SwipeSpan> spans;
  // Leading side, opened to twice the buttons' width.
  swipeButtonSpans({100.0, 50.0}, 300.0, false, 400.0, spans);
  CHECK_EQ(spans.size(), std::size_t{2});
  CHECK_NEAR(spans[0].start, 0.0, 1e-9);
  CHECK_NEAR(spans[0].size, 200.0, 1e-9);
  CHECK_NEAR(spans[1].start, 200.0, 1e-9);
  CHECK_NEAR(spans[1].size, 100.0, 1e-9);

  // Trailing side: the first action sits at the trailing edge.
  swipeButtonSpans({100.0, 50.0}, -150.0, false, 400.0, spans);
  CHECK_NEAR(spans[0].start, 300.0, 1e-9);
  CHECK_NEAR(spans[0].size, 100.0, 1e-9);
  CHECK_NEAR(spans[1].start, 250.0, 1e-9);
  CHECK_NEAR(spans[1].size, 50.0, 1e-9);
}

TEST(swipe_buttons_past_a_full_swipe_leave_the_gap_to_the_first) {
  std::vector<SwipeSpan> spans;
  swipeButtonSpans({100.0, 50.0}, -320.0, true, 400.0, spans);
  CHECK_NEAR(spans[0].start, 80.0, 1e-9);
  CHECK_NEAR(spans[0].size, 320.0, 1e-9);
  CHECK_NEAR(spans[1].size, 0.0, 1e-9);
}

TEST(swipe_button_edges_round_without_drifting) {
  /*
   * Three buttons over 100 pixels: rounding each size alone gives 33 + 33 + 33. Rounding the
   * edges keeps them touching and ending at the gap.
   */
  std::vector<SwipeSpan> spans;
  swipeButtonSpans({1.0, 1.0, 1.0}, 100.0, false, 300.0, spans);
  long previousEnd = 0;
  for (const SwipeSpan& span : spans) {
    long start = std::lround(span.start);
    long end = std::lround(span.start + span.size);
    CHECK_EQ(start, previousEnd);
    previousEnd = end;
  }
  CHECK_EQ(previousEnd, 100L);
}

TEST(swipe_revealed_span_is_the_gap_at_the_revealed_side) {
  SwipeSpan leading = swipeRevealedSpan(120.0, 400.0);
  CHECK_NEAR(leading.start, 0.0, 1e-9);
  CHECK_NEAR(leading.size, 120.0, 1e-9);
  SwipeSpan trailing = swipeRevealedSpan(-120.0, 400.0);
  CHECK_NEAR(trailing.start, 280.0, 1e-9);
  CHECK_NEAR(trailing.size, 120.0, 1e-9);
}

TEST(swipe_row_slid_all_the_way_counts_as_out) {
  SwipeReveal swipe;
  swipe.begin(spec(), 0.0);
  CHECK(swipe.isSwipedOut(-400.0));
  CHECK(swipe.isSwipedOut(399.5));
  CHECK(!swipe.isSwipedOut(-160.0));
  SwipeReveal closed;
  CHECK(!closed.isSwipedOut(0.0));
}
