/*
 * Drag to reorder in a grid: which cell the held one drops at, and where every other cell
 * slides for the preview, checked against the core's own layout after the move.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/host/DragReorder.hpp>

#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

/*
 * Twelve 100 by 100 cells in three columns keyed k0..k11. The held cell is left out.
 */
DragRow gridCell(long index) {
  return {index, "k" + std::to_string(index), (index / 3) * 100.0, 100.0, (index % 3) * 100.0, 100.0};
}

std::vector<DragRow> gridWithout(long held) {
  std::vector<DragRow> rows;
  for (long index = 0; index < 12; ++index) {
    if (index != held) {
      rows.push_back(gridCell(index));
    }
  }
  return rows;
}

/*
 * Pick a cell up by its middle and move the finger to a point in the content.
 */
DragReorder dragCell(long held, double touchContent, double touchCross) {
  DragRow resting = gridCell(held);
  DragReorder drag;
  drag.beginCell(resting, resting.leading + 50.0, resting.crossLeading + 50.0, 3);
  drag.placeCell(touchContent, touchCross, resting, 400.0, 300.0);
  drag.updateInsertion(gridWithout(held));
  return drag;
}

void checkOffset(const DragReorder& drag, long index, double leading, double cross) {
  DragOffset offset = drag.offsetFor(index);
  CHECK_EQ(offset.leading, leading);
  CHECK_EQ(offset.cross, cross);
}

}

TEST(grid_drag_forward_across_a_row_wraps_cells_back) {
  // Cell 1 over cell 5, the last cell of the second row.
  DragReorder drag = dragCell(1, 150.0, 250.0);
  CHECK(drag.isGrid());
  CHECK_EQ(drag.insertionIndex(), 5L);
  CHECK_EQ(drag.insertionKey(), std::string("k5"));
  checkOffset(drag, 0, 0.0, 0.0);
  checkOffset(drag, 2, 0.0, -100.0);
  // Cell 3 starts a row. It wraps up to the end of the row above.

  checkOffset(drag, 3, -100.0, 200.0);
  checkOffset(drag, 4, 0.0, -100.0);
  checkOffset(drag, 5, 0.0, -100.0);
  checkOffset(drag, 6, 0.0, 0.0);
}

TEST(grid_drag_backward_wraps_cells_forward) {
  // Cell 7 over cell 2.
  DragReorder drag = dragCell(7, 50.0, 250.0);
  CHECK_EQ(drag.insertionIndex(), 2L);
  checkOffset(drag, 1, 0.0, 0.0);
  checkOffset(drag, 2, 100.0, -200.0);
  checkOffset(drag, 3, 0.0, 100.0);
  checkOffset(drag, 5, 100.0, -200.0);
  checkOffset(drag, 6, 0.0, 100.0);
  checkOffset(drag, 8, 0.0, 0.0);
}

TEST(grid_drag_to_the_first_and_last_cells) {
  DragReorder first = dragCell(5, 10.0, 10.0);
  CHECK_EQ(first.insertionIndex(), 0L);
  checkOffset(first, 0, 0.0, 100.0);
  checkOffset(first, 2, 100.0, -200.0);
  checkOffset(first, 4, 0.0, 100.0);

  DragReorder last = dragCell(5, 390.0, 290.0);
  CHECK_EQ(last.insertionIndex(), 11L);
  checkOffset(last, 6, -100.0, 200.0);
  checkOffset(last, 11, 0.0, -100.0);
  checkOffset(last, 4, 0.0, 0.0);
}

TEST(grid_drag_over_its_own_slot_goes_back) {
  DragRow resting = gridCell(4);
  DragReorder drag;
  drag.beginCell(resting, 150.0, 150.0, 3);
  drag.placeCell(250.0, 150.0, resting, 400.0, 300.0);
  drag.updateInsertion(gridWithout(4));
  CHECK_EQ(drag.insertionIndex(), 7L);
  drag.placeCell(160.0, 140.0, resting, 400.0, 300.0);
  drag.updateInsertion(gridWithout(4));
  CHECK_EQ(drag.insertionIndex(), 4L);
  CHECK_EQ(drag.insertionKey(), std::string("k4"));
  checkOffset(drag, 7, 0.0, 0.0);
}

TEST(grid_drag_over_no_cell_keeps_the_drop_spot) {
  // Eleven cells leave the last slot empty. Over it the drop spot stays where it was.
  std::vector<DragRow> rows;
  for (long index = 0; index < 11; ++index) {
    if (index != 0) {
      rows.push_back(gridCell(index));
    }
  }
  DragRow resting = gridCell(0);
  DragReorder drag;
  drag.beginCell(resting, 50.0, 50.0, 3);
  drag.placeCell(350.0, 150.0, resting, 400.0, 300.0);
  drag.updateInsertion(rows);
  CHECK_EQ(drag.insertionIndex(), 10L);
  drag.placeCell(350.0, 250.0, resting, 400.0, 300.0);
  drag.updateInsertion(rows);
  CHECK_EQ(drag.insertionIndex(), 10L);
}

TEST(grid_held_cell_stays_inside_the_content) {
  DragRow resting = gridCell(4);
  DragReorder drag;
  drag.beginCell(resting, 150.0, 150.0, 3);
  DragOffset offset = drag.placeCell(-500.0, 900.0, resting, 400.0, 300.0);
  CHECK_EQ(drag.leading(), 0.0);
  CHECK_EQ(drag.crossLeading(), 200.0);
  CHECK_EQ(offset.leading, -100.0);
  CHECK_EQ(offset.cross, 100.0);
}

TEST(grid_single_column_session_is_unchanged) {
  DragReorder drag;
  drag.begin(3, "k3", 300.0, 100.0, 350.0);
  CHECK(!drag.isGrid());
  drag.place(560.0, 300.0, 100.0, 1000.0);
  std::vector<DragRow> rows;
  for (long index = 0; index < 10; ++index) {
    if (index != 3) {
      rows.push_back({index, "k" + std::to_string(index), index * 100.0, 100.0});
    }
  }
  drag.updateInsertion(rows);
  CHECK_EQ(drag.insertionIndex(), 5L);
  CHECK_EQ(drag.offsetFor(4).leading, -100.0);
  CHECK_EQ(drag.offsetFor(4).cross, 0.0);
  // A grid drag before it leaves nothing behind.
  DragReorder reused = dragCell(1, 150.0, 250.0);
  reused.begin(3, "k3", 300.0, 100.0, 350.0);
  CHECK(!reused.isGrid());
  CHECK_EQ(reused.offsetFor(3).cross, 0.0);
}

TEST(grid_shifts_of_cell_arrays) {
  // Cells 0..5 in two columns without cell 1, which is held.
  std::vector<long> indices{0, 2, 3, 4, 5};
  std::vector<double> leadings{0.0, 100.0, 100.0, 200.0, 200.0};
  std::vector<double> extents(5, 100.0);
  std::vector<double> crossLeadings{0.0, 0.0, 100.0, 0.0, 100.0};
  std::vector<double> crossExtents(5, 100.0);
  DragCells cells{indices.data(), leadings.data(), extents.data(), crossLeadings.data(), crossExtents.data(), 5};
  DragRow held{1, "", 0.0, 100.0, 100.0, 100.0};

  CHECK_EQ(dragGridInsertionPosition(cells, held, 1, 250.0, 50.0), 3L);
  CHECK_EQ(dragGridInsertionPosition(cells, held, 4, 50.0, 150.0), -1L);
  CHECK_EQ(dragGridInsertionPosition(cells, held, 4, 900.0, 50.0), 3L);

  std::vector<double> shifts(5, 7.0);
  std::vector<double> crossShifts(5, 7.0);
  dragGridShifts(cells, held, 4, 2, shifts.data(), crossShifts.data());
  CHECK_EQ(shifts[0], 0.0);
  CHECK_EQ(shifts[1], -100.0);
  CHECK_EQ(crossShifts[1], 100.0);
  CHECK_EQ(shifts[2], 0.0);
  CHECK_EQ(crossShifts[2], -100.0);
  CHECK_EQ(shifts[3], -100.0);
  CHECK_EQ(crossShifts[3], 100.0);
  CHECK_EQ(shifts[4], 0.0);
  CHECK_EQ(crossShifts[4], 0.0);
}

TEST(grid_masonry_preview_matches_the_core_layout_after_the_move) {
  // Three columns of cells with mixed heights, laid out by the core.
  std::vector<std::string> keys = keysFor(30);
  const double heights[] = {100.0, 60.0, 140.0, 80.0, 120.0, 90.0, 70.0};
  auto layout = [&](Container& container, const std::vector<std::string>& order) {
    FrameInput input = inputFor(order, 0.0);
    input.columns = 3;
    input.windowContainerHeight = 4000.0;
    Virtualizer::update(&container, input);
    for (std::size_t index = 0; index < order.size(); ++index) {
      int number = std::stoi(order[index].substr(1));
      Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH / 3.0, heights[number % 7]});
    }
    Virtualizer::update(&container, input);
  };

  const long moves[][2] = {{2, 13}, {13, 2}, {0, 29}, {29, 0}, {7, 8}, {10, 4}};
  for (const auto& move : moves) {
    long from = move[0];
    long to = move[1];
    Container before;
    layout(before, keys);
    std::vector<DragRow> rows;
    DragRow held;
    for (std::size_t index = 0; index < keys.size(); ++index) {
      const Element& element = before.revision.elements[index];
      DragRow row{static_cast<long>(index), keys[index], element.offsetY, element.height, element.offsetX, element.width};
      if (static_cast<long>(index) == from) {
        held = row;
      } else {
        rows.push_back(row);
      }
    }

    std::vector<long> indices;
    std::vector<double> leadings, extents, crossLeadings, crossExtents;
    for (const DragRow& row : rows) {
      indices.push_back(row.index);
      leadings.push_back(row.leading);
      extents.push_back(row.extent);
      crossLeadings.push_back(row.crossLeading);
      crossExtents.push_back(row.crossExtent);
    }
    DragCells cells{indices.data(), leadings.data(), extents.data(), crossLeadings.data(), crossExtents.data(), rows.size()};
    std::vector<double> shifts(rows.size());
    std::vector<double> crossShifts(rows.size());
    dragGridShifts(cells, held, to, 3, shifts.data(), crossShifts.data());

    std::vector<std::string> moved = keys;
    std::string key = moved[static_cast<std::size_t>(from)];
    moved.erase(moved.begin() + from);
    moved.insert(moved.begin() + to, key);
    Container after;
    layout(after, moved);

    for (std::size_t position = 0; position < rows.size(); ++position) {
      const Element& landed = after.revision.elements[after.findElementIndexByKey(rows[position].key)];
      CHECK_NEAR(rows[position].leading + shifts[position], landed.offsetY, 1e-9);
      CHECK_NEAR(rows[position].crossLeading + crossShifts[position], landed.offsetX, 1e-9);
    }
  }
}

TEST(grid_preview_restarts_a_column_after_an_unmounted_cell) {
  // Equal cells in two columns. Cell 1 is held and cells 3 to 5 are not mounted.
  std::vector<long> indices{0, 2, 6, 7, 8, 9};
  std::vector<double> leadings, extents, crossLeadings, crossExtents;
  for (long index : indices) {
    leadings.push_back((index / 2) * 100.0);
    extents.push_back(100.0);
    crossLeadings.push_back((index % 2) * 100.0);
    crossExtents.push_back(100.0);
  }
  DragCells cells{indices.data(), leadings.data(), extents.data(), crossLeadings.data(), crossExtents.data(), indices.size()};
  DragRow held{1, "", 0.0, 100.0, 100.0, 100.0};
  std::vector<double> shifts(indices.size());
  std::vector<double> crossShifts(indices.size());
  dragGridShifts(cells, held, 8, 2, shifts.data(), crossShifts.data());
  // Every cell from 2 to 8 moves back one slot, wrapping between the columns.
  CHECK_EQ(shifts[0], 0.0);
  CHECK_EQ(shifts[1], -100.0);
  CHECK_EQ(crossShifts[1], 100.0);
  CHECK_EQ(shifts[2], -100.0);
  CHECK_EQ(crossShifts[2], 100.0);
  CHECK_EQ(shifts[3], 0.0);
  CHECK_EQ(crossShifts[3], -100.0);
  CHECK_EQ(shifts[4], -100.0);
  CHECK_EQ(crossShifts[4], 100.0);
  CHECK_EQ(shifts[5], 0.0);
  CHECK_EQ(crossShifts[5], 0.0);
}
