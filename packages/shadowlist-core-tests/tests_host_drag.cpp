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
std::vector<DragRow> rowsWithout(long held) {
  std::vector<DragRow> rows;
  for (long index = 0; index < 10; ++index) {
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

TEST(drag_without_passing_a_midpoint_stays) {
  DragReorder drag = dragRow3To(390.0);
  CHECK_EQ(drag.insertionIndex(), 3L);
  CHECK_EQ(drag.insertionKey(), std::string("k3"));
  CHECK_EQ(drag.shiftFor(4), 0.0);
}

TEST(drag_forward_drops_past_the_farthest_passed_row) {
  // Center at 560 passed the midpoints of rows 4 (450) and 5 (550).
  DragReorder drag = dragRow3To(560.0);
  CHECK_EQ(drag.insertionIndex(), 5L);
  CHECK_EQ(drag.insertionKey(), std::string("k5"));
  CHECK_EQ(drag.shiftFor(2), 0.0);
  CHECK_EQ(drag.shiftFor(4), -100.0);
  CHECK_EQ(drag.shiftFor(5), -100.0);
  CHECK_EQ(drag.shiftFor(6), 0.0);
}

TEST(drag_backward_drops_before_the_farthest_passed_row) {
  // Center at 140 passed the midpoints of rows 2 (250) and 1 (150).
  DragReorder drag = dragRow3To(140.0);
  CHECK_EQ(drag.insertionIndex(), 1L);
  CHECK_EQ(drag.insertionKey(), std::string("k1"));
  CHECK_EQ(drag.shiftFor(0), 0.0);
  CHECK_EQ(drag.shiftFor(1), 100.0);
  CHECK_EQ(drag.shiftFor(2), 100.0);
  CHECK_EQ(drag.shiftFor(3), 0.0);
}

TEST(drag_midpoint_leaves_half_a_row_of_slack) {
  // Exactly on row 4's midpoint is not past it.
  CHECK_EQ(dragRow3To(450.0).insertionIndex(), 3L);
  CHECK_EQ(dragRow3To(451.0).insertionIndex(), 4L);
}

TEST(drag_held_row_stays_inside_the_content) {
  DragReorder drag;
  drag.begin(3, "k3", 300.0, 100.0, 350.0);
  // The leading edge would go to -250.
  CHECK_EQ(drag.place(-200.0, 300.0, 100.0, 1000.0), -300.0);
  CHECK_EQ(drag.leading(), 0.0);
  // And past the end it stops with its trailing edge on the content end.
  drag.place(5000.0, 300.0, 100.0, 1000.0);
  CHECK_EQ(drag.leading(), 900.0);
  CHECK_EQ(dragHeldLeading(100.0, 50.0, 100.0, 60.0), 0.0);
}

TEST(drag_follows_a_moved_origin) {
  DragReorder drag;
  drag.begin(3, "k3", 300.0, 100.0, 350.0);
  // A prepend of two rows moved the held row to index 5.
  drag.updateOrigin(5, "k3");
  CHECK_EQ(drag.originIndex(), 5L);
  drag.updateOrigin(-1, "");
  CHECK_EQ(drag.originIndex(), 5L);
  CHECK_EQ(drag.originKey(), std::string("k3"));
}

TEST(drag_insertion_position_of_rows_arrays) {
  std::vector<long> indices{0, 1, 2, 4, -1, 5};
  std::vector<double> leadings{0.0, 100.0, 200.0, 400.0, 0.0, 500.0};
  std::vector<double> extents{100.0, 100.0, 100.0, 100.0, 100.0, 100.0};
  CHECK_EQ(dragInsertionPosition(indices.data(), leadings.data(), extents.data(), 6, 3, 560.0), 5L);
  CHECK_EQ(dragInsertionPosition(indices.data(), leadings.data(), extents.data(), 6, 3, 350.0), -1L);
  CHECK_EQ(dragInsertionPosition(indices.data(), leadings.data(), extents.data(), 6, 3, 140.0), 1L);
}

TEST(drag_auto_scroll_speeds_up_toward_the_edges) {
  DragAutoScrollConfig config = DRAG_AUTO_SCROLL_IOS;
  CHECK_EQ(dragAutoScrollDelta(config, 300.0, 600.0), 0.0);
  CHECK_NEAR(dragAutoScrollDelta(config, 0.0, 600.0), -16.0, 1e-9);
  CHECK_NEAR(dragAutoScrollDelta(config, 45.0, 600.0), -8.0, 1e-9);
  CHECK_NEAR(dragAutoScrollDelta(config, 600.0, 600.0), 16.0, 1e-9);
  CHECK_NEAR(dragAutoScrollDelta(config, 555.0, 600.0), 8.0, 1e-9);
  CHECK_EQ(dragAutoScrollDelta(config, 90.0, 600.0), 0.0);
}

TEST(drag_auto_scroll_stays_in_the_scroll_range) {
  DragAutoScrollConfig config = DRAG_AUTO_SCROLL_ANDROID;
  CHECK_EQ(dragAutoScrollOffset(config, 0.0, 600.0, 5.0, 1400.0), 0.0);
  CHECK_EQ(dragAutoScrollOffset(config, 600.0, 600.0, 1395.0, 1400.0), 1400.0);
  CHECK_EQ(dragAutoScrollOffset(config, 300.0, 600.0, 700.0, 1400.0), 700.0);
  CHECK_NEAR(dragAutoScrollOffset(config, 30.0, 600.0, 700.0, 1400.0), 694.0, 1e-9);
}
