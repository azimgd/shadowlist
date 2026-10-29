/*
 * Drag to reorder math: where the held row goes, where it drops, how the others slide, and
 * the auto scroll near the edges.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/DragReorder.hpp>

#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

/*
 * Ten 100 tall rows keyed k0..k9. The held row is left out, like the hosts do.
 */
std::vector<DragRow> rowsWithout(std::size_t held) {
  std::vector<DragRow> rows;
  for (std::size_t index = 0; index < 10; ++index) {
    if (index != held) {
      rows.push_back({index, "k" + std::to_string(index), index * 100.0, 100.0});
    }
  }
  return rows;
}

/*
 * Pick up row 3 by its middle and move the finger to touchContent.
 */
DragReorder dragRow3To(double touchContent) {
  DragReorder drag;
  drag.begin(3, "k3", 300.0, 100.0, 350.0);
  drag.place(touchContent, 300.0, 100.0, 1000.0);
  drag.updateInsertion(rowsWithout(3));
  return drag;
}

}

/*
 * A host that looks keys up by index leaves DragRow keys empty and hands updateInsertion a
 * lookup. The drop spot gets the same key either way.
 */
TEST(drag_insertion_reads_keys_through_the_lookup) {
  DragReorder drag;
  drag.begin(3, "k3", 300.0, 100.0, 350.0);
  drag.place(560.0, 300.0, 100.0, 1000.0);
  std::vector<DragRow> rows = rowsWithout(3);
  std::vector<std::string> keys;
  for (DragRow& row : rows) {
    keys.push_back(row.key);
    row.key.clear();
  }
  std::size_t looked = 0;
  drag.updateInsertion(rows, [&](const DragRow& row) -> const std::string& {
    ++looked;
    return keys[row.index < 3 ? row.index : row.index - 1];
  });
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{5});
  CHECK_EQ(drag.getInsertionKey(), std::string("k5"));
  CHECK_EQ(looked, std::size_t{1});

  // Back over its own place the drop names the held row with no lookup.
  drag.place(350.0, 300.0, 100.0, 1000.0);
  drag.updateInsertion(rows, [&](const DragRow&) -> const std::string& {
    ++looked;
    return keys[0];
  });
  CHECK_EQ(drag.getInsertionKey(), std::string("k3"));
  CHECK_EQ(looked, std::size_t{1});
}

TEST(drag_without_passing_a_midpoint_stays) {
  DragReorder drag = dragRow3To(390.0);
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{3});
  CHECK_EQ(drag.getInsertionKey(), std::string("k3"));
  CHECK_EQ(drag.shiftFor(4), 0.0);
}

TEST(drag_forward_drops_past_the_farthest_passed_row) {
  // Center at 560 passed the midpoints of rows 4 (450) and 5 (550).
  DragReorder drag = dragRow3To(560.0);
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{5});
  CHECK_EQ(drag.getInsertionKey(), std::string("k5"));
  CHECK_EQ(drag.shiftFor(2), 0.0);
  CHECK_EQ(drag.shiftFor(4), -100.0);
  CHECK_EQ(drag.shiftFor(5), -100.0);
  CHECK_EQ(drag.shiftFor(6), 0.0);
}

TEST(drag_backward_drops_before_the_farthest_passed_row) {
  // Center at 140 passed the midpoints of rows 2 (250) and 1 (150).
  DragReorder drag = dragRow3To(140.0);
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{1});
  CHECK_EQ(drag.getInsertionKey(), std::string("k1"));
  CHECK_EQ(drag.shiftFor(0), 0.0);
  CHECK_EQ(drag.shiftFor(1), 100.0);
  CHECK_EQ(drag.shiftFor(2), 100.0);
  CHECK_EQ(drag.shiftFor(3), 0.0);
}

TEST(drag_midpoint_leaves_half_a_row_of_slack) {
  // Exactly on row 4's midpoint is not past it.
  CHECK_EQ(dragRow3To(450.0).getInsertionIndex(), std::size_t{3});
  CHECK_EQ(dragRow3To(451.0).getInsertionIndex(), std::size_t{4});
}

TEST(drag_held_row_stays_inside_the_content) {
  DragReorder drag;
  drag.begin(3, "k3", 300.0, 100.0, 350.0);
  // The leading edge would go to -250.
  CHECK_EQ(drag.place(-200.0, 300.0, 100.0, 1000.0), -300.0);
  CHECK_EQ(drag.getLeading(), 0.0);
  // And past the end it stops with its trailing edge on the content end.
  drag.place(5000.0, 300.0, 100.0, 1000.0);
  CHECK_EQ(drag.getLeading(), 900.0);
  CHECK_EQ(dragHeldLeading(100.0, 50.0, 100.0, 60.0), 0.0);
}

TEST(drag_follows_a_moved_origin) {
  DragReorder drag;
  drag.begin(3, "k3", 300.0, 100.0, 350.0);
  // A prepend of two rows moved the held row to index 5.
  drag.updateOrigin(5, "k3");
  CHECK_EQ(drag.getOriginIndex(), std::size_t{5});
  drag.updateOrigin(UNDEFINED_INDEX, "");
  CHECK_EQ(drag.getOriginIndex(), std::size_t{5});
  CHECK_EQ(drag.getOriginKey(), std::string("k3"));
}

TEST(drag_insertion_position_of_rows_arrays) {
  std::vector<std::size_t> indices{0, 1, 2, 4, UNDEFINED_INDEX, 5};
  std::vector<double> leadings{0.0, 100.0, 200.0, 400.0, 0.0, 500.0};
  std::vector<double> extents{100.0, 100.0, 100.0, 100.0, 100.0, 100.0};
  CHECK_EQ(dragInsertionIndex(indices.data(), leadings.data(), extents.data(), 6, 3, 560.0), std::size_t{5});
  CHECK_EQ(dragInsertionIndex(indices.data(), leadings.data(), extents.data(), 6, 3, 350.0), UNDEFINED_INDEX);
  CHECK_EQ(dragInsertionIndex(indices.data(), leadings.data(), extents.data(), 6, 3, 140.0), std::size_t{1});
}

// One 60 Hz frame. The speeds are per second and a frame of it scrolls a sixtieth.
constexpr double FRAME = 1.0 / 60.0;

TEST(drag_auto_scroll_speeds_up_toward_the_edges) {
  DragAutoScrollConfig config = DRAG_AUTO_SCROLL_IOS;
  CHECK_EQ(dragAutoScrollDelta(config, 300.0, 600.0, FRAME), 0.0);
  CHECK_NEAR(dragAutoScrollDelta(config, 0.0, 600.0, FRAME), -16.0, 1e-9);
  CHECK_NEAR(dragAutoScrollDelta(config, 45.0, 600.0, FRAME), -8.0, 1e-9);
  CHECK_NEAR(dragAutoScrollDelta(config, 600.0, 600.0, FRAME), 16.0, 1e-9);
  CHECK_NEAR(dragAutoScrollDelta(config, 555.0, 600.0, FRAME), 8.0, 1e-9);
  CHECK_EQ(dragAutoScrollDelta(config, 90.0, 600.0, FRAME), 0.0);
  // A finger past the viewport edge does not scroll faster than maxSpeed.
  CHECK_NEAR(dragAutoScrollDelta(config, -200.0, 600.0, FRAME), -16.0, 1e-9);
  CHECK_NEAR(dragAutoScrollDelta(config, 900.0, 600.0, FRAME), 16.0, 1e-9);
}

TEST(drag_auto_scroll_moves_as_far_per_second_at_any_frame_rate) {
  DragAutoScrollConfig config = DRAG_AUTO_SCROLL_IOS;
  double at60 = 0.0;
  double at120 = 0.0;
  for (int frame = 0; frame < 60; ++frame) {
    at60 += dragAutoScrollDelta(config, 0.0, 600.0, 1.0 / 60.0);
  }
  for (int frame = 0; frame < 120; ++frame) {
    at120 += dragAutoScrollDelta(config, 0.0, 600.0, 1.0 / 120.0);
  }
  CHECK_NEAR(at60, -config.maxSpeed, 1e-6);
  CHECK_NEAR(at120, -config.maxSpeed, 1e-6);
  // A stalled frame scrolls no further than the longest frame allowed.
  CHECK_NEAR(dragAutoScrollDelta(config, 0.0, 600.0, 0.5),
    -config.maxSpeed * DRAG_AUTO_SCROLL_MAX_FRAME_SECONDS, 1e-9);
  CHECK_EQ(dragAutoScrollDelta(config, 0.0, 600.0, 0.0), 0.0);
}

TEST(drag_auto_scroll_stays_in_the_scroll_range) {
  DragAutoScrollConfig config = DRAG_AUTO_SCROLL_ANDROID;
  CHECK_EQ(dragAutoScrollOffset(config, 0.0, 600.0, 5.0, 1400.0, FRAME), 0.0);
  CHECK_EQ(dragAutoScrollOffset(config, 600.0, 600.0, 1395.0, 1400.0, FRAME), 1400.0);
  CHECK_EQ(dragAutoScrollOffset(config, 300.0, 600.0, 700.0, 1400.0, FRAME), 700.0);
  CHECK_NEAR(dragAutoScrollOffset(config, 30.0, 600.0, 700.0, 1400.0, FRAME), 694.0, 1e-9);
  // An inset at the start lets the offset go below 0 as far as the inset.
  CHECK_EQ(dragAutoScrollOffset(config, 0.0, 600.0, 5.0, 1400.0, FRAME, -20.0), -7.0);
  CHECK_EQ(dragAutoScrollOffset(config, 0.0, 600.0, -15.0, 1400.0, FRAME, -20.0), -20.0);
}

TEST(drag_press_fails_sooner_along_the_scroll_axis) {
  CHECK(dragPressHolds(0.0, 0.0));
  CHECK(dragPressHolds(DRAG_PRESS_SCROLL_SLOP, 0.0));
  CHECK(!dragPressHolds(DRAG_PRESS_SCROLL_SLOP + 1.0, 0.0));
  CHECK(!dragPressHolds(-DRAG_PRESS_SCROLL_SLOP - 1.0, 0.0));
  CHECK(dragPressHolds(0.0, DRAG_PRESS_SLOP));
  CHECK(!dragPressHolds(0.0, DRAG_PRESS_SLOP + 1.0));
  CHECK(!dragPressHolds(8.0, 19.0));
}

TEST(drag_feedback_ticks_on_a_new_drop_spot_at_most_every_interval) {
  DragFeedback feedback;
  feedback.reset(3);
  CHECK(!feedback.shouldTick(3, 0.0));
  CHECK(feedback.shouldTick(4, 100.0));
  CHECK(!feedback.shouldTick(4, 200.0));
  // A spot that flickers back right away does not buzz again.
  CHECK(!feedback.shouldTick(3, 100.0 + DRAG_FEEDBACK_TICK_INTERVAL_MS / 2.0));
  CHECK(feedback.shouldTick(4, 100.0 + DRAG_FEEDBACK_TICK_INTERVAL_MS + 1.0));
  // No spot never ticks, and a reset to none makes the next spot tick.
  CHECK(!feedback.shouldTick(UNDEFINED_INDEX, 1000.0));
  feedback.reset();
  CHECK(feedback.shouldTick(0, 2000.0));
}

TEST(drop_settle_waits_for_the_row_then_settles_or_gives_up) {
  DropSettle settle;
  CHECK(settle.tick(2, 0.0) == DropSettle::Status::Idle);
  settle.begin(5, 1000.0);
  CHECK(settle.isWaiting());
  CHECK(settle.tick(2, 1010.0) == DropSettle::Status::Waiting);
  CHECK(settle.tick(5, 1020.0) == DropSettle::Status::Settle);
  CHECK(!settle.isWaiting());
  CHECK(settle.tick(5, 1030.0) == DropSettle::Status::Idle);

  settle.begin(5, 1000.0);
  CHECK(settle.tick(2, 1000.0 + DRAG_DROP_SETTLE_TIMEOUT_MS) == DropSettle::Status::GiveUp);
  settle.begin(5, 1000.0);
  CHECK(settle.tick(UNDEFINED_INDEX, 1001.0) == DropSettle::Status::GiveUp);
  settle.begin(UNDEFINED_INDEX, 1000.0);
  CHECK(!settle.isWaiting());
}

TEST(drag_board_scroll_arms_only_toward_the_edge) {
  // Picked up at 300 in a 400 wide board, whose right edge zone starts at 336.
  CHECK(!dragBoardShouldArm(300.0, 310.0, 0.0, 1.0, DRAG_BOARD_SCROLL_ARM_IOS));
  CHECK(dragBoardShouldArm(300.0, 320.0, 0.0, 1.0, DRAG_BOARD_SCROLL_ARM_IOS));
  // Moving back away from the edge disarms, and drift toward the other side never arms it.
  CHECK(!dragBoardShouldArm(300.0, 320.0, -200.0, 1.0, DRAG_BOARD_SCROLL_ARM_IOS));
  CHECK(!dragBoardShouldArm(300.0, 280.0, 0.0, 1.0, DRAG_BOARD_SCROLL_ARM_IOS));
  CHECK(dragBoardShouldArm(300.0, 280.0, 0.0, -1.0, DRAG_BOARD_SCROLL_ARM_IOS));

  // A vertical drag near the right edge whose finger drifts 16 left never scrolls the board.
  DragBoardScroll drift;
  drift.begin(380.0);
  double scrolled = 0.0;
  for (int frame = 0; frame < 30; ++frame) {
    double finger = 380.0 - frame * 0.6;
    scrolled += drift.delta(DRAG_BOARD_SCROLL_IOS, DRAG_BOARD_SCROLL_ARM_IOS, finger, finger, 400.0, FRAME);
  }
  CHECK_EQ(scrolled, 0.0);

  // A finger heading for the edge scrolls once it has moved far enough, and keeps scrolling
  // while it rests there.
  DragBoardScroll toward;
  toward.begin(300.0);
  CHECK_EQ(toward.delta(DRAG_BOARD_SCROLL_IOS, DRAG_BOARD_SCROLL_ARM_IOS, 310.0, 310.0, 400.0, FRAME), 0.0);
  CHECK_EQ(toward.delta(DRAG_BOARD_SCROLL_IOS, DRAG_BOARD_SCROLL_ARM_IOS, 350.0, 350.0, 400.0, FRAME) > 0.0, true);
  double rest = 0.0;
  for (int frame = 0; frame < 30; ++frame) {
    rest = toward.delta(DRAG_BOARD_SCROLL_IOS, DRAG_BOARD_SCROLL_ARM_IOS, 390.0, 390.0, 400.0, FRAME);
  }
  CHECK(rest > 0.0);
}
