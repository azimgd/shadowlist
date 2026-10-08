/*
 * SwipeReveal tests: the swipe action offsets both native lists use.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/SwipeReveal.hpp>

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
