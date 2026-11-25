/*
 * Drag board: a row dragged out of one list into another. The home list closes the gap while
 * the held row is away, and the list under it opens a gap for a row it does not own. Grid
 * previews are checked against the core's own layout after the removal or insertion. DragBoard
 * decides when the row lifts out, which list it targets and how a release ends. The fuzz test
 * runs a board against what its header says of every call. See tests_fuzz.cpp for how the
 * fuzz tests run.
 */

#include "FuzzHelpers.hpp"
#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/host/DragBoard.hpp>
#include <shadowlist-core/host/DragReorder.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

/*
 * Ten 100 tall rows keyed k0..k9, with held left out when it names one.
 */
std::vector<DragRow> listRows(std::size_t held = UNDEFINED_INDEX) {
  std::vector<DragRow> rows;
  for (std::size_t index = 0; index < 10; ++index) {
    if (index != held) {
      rows.push_back({index, "k" + std::to_string(index), index * 100.0, 100.0});
    }
  }
  return rows;
}

/*
 * A 100 by 100 cell keyed k<index> in a grid of three columns.
 */
DragRow gridCell(std::size_t index) {
  return {index, "k" + std::to_string(index), (index / 3) * 100.0, 100.0, (index % 3) * 100.0, 100.0};
}

/*
 * The first count cells of that grid, with held left out when it names one.
 */
std::vector<DragRow> gridRows(std::size_t count, std::size_t held = UNDEFINED_INDEX) {
  std::vector<DragRow> rows;
  for (std::size_t index = 0; index < count; ++index) {
    if (index != held) {
      rows.push_back(gridCell(index));
    }
  }
  return rows;
}

DragReorder foreignList(double center) {
  DragReorder drag;
  drag.beginForeign(80.0, 0.0, 1);
  drag.placeForeignCenter(center, 0.0);
  drag.updateInsertion(listRows());
  return drag;
}

DragReorder foreignGrid(double center, double crossCenter) {
  DragReorder drag;
  drag.beginForeign(100.0, 100.0, 3);
  drag.placeForeignCenter(center, crossCenter);
  drag.updateInsertion(gridRows(12));
  return drag;
}

/*
 * Rows as the parallel arrays of DragCells. The arrays live as long as this does.
 */
struct CellArrays {
  explicit CellArrays(const std::vector<DragRow>& rows) {
    for (const DragRow& row : rows) {
      indices.push_back(row.index);
      leadings.push_back(row.leading);
      extents.push_back(row.extent);
      crossLeadings.push_back(row.crossLeading);
      crossExtents.push_back(row.crossExtent);
    }
  }

  DragCells cells() const {
    return {indices.data(), leadings.data(), extents.data(), crossLeadings.data(), crossExtents.data(), indices.size()};
  }

  std::vector<std::size_t> indices;
  std::vector<double> leadings, extents, crossLeadings, crossExtents;
};

void checkOffset(const DragReorder& drag, std::size_t index, double leading, double cross) {
  DragOffset offset = drag.offsetFor(index);
  CHECK_EQ(offset.leading, leading);
  CHECK_EQ(offset.cross, cross);
}

/*
 * Thirty cells of mixed heights in three columns, laid out by the core.
 */
const double MASONRY_HEIGHTS[] = {100.0, 60.0, 140.0, 80.0, 120.0, 90.0, 70.0};

void masonryLayout(Container& container, const std::vector<std::string>& order) {
  FrameInput input = inputFor(order, 0.0);
  input.numberOfColumns = 3;
  input.windowHeight = 6000.0;
  Virtualizer::update(container, input);
  for (std::size_t index = 0; index < order.size(); ++index) {
    int number = std::stoi(order[index].substr(1));
    Virtualizer::updateRowAtIndex(container, index, {WINDOW_WIDTH / 3.0, MASONRY_HEIGHTS[number % 7]});
  }
  Virtualizer::update(container, input);
}

std::vector<DragRow> masonryRows(const Container& container, const std::vector<std::string>& keys) {
  std::vector<DragRow> rows;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const Row& placed = container.revision.rows[index];
    rows.push_back({index, keys[index], placed.offsetY, placed.height, placed.offsetX, placed.width});
  }
  return rows;
}

}

TEST(drag_board_held_away_closes_the_gap_in_a_list) {
  DragReorder drag;
  drag.begin(3, "k3", 300.0, 100.0, 350.0);
  drag.place(560.0, 300.0, 100.0, 1000.0);
  drag.updateInsertion(listRows(3));
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{5});

  drag.setHeldAway(true);
  CHECK(drag.isHeldAway());
  CHECK_EQ(drag.getInsertionIndex(), UNDEFINED_INDEX);
  CHECK_EQ(drag.getInsertionKey(), std::string());
  CHECK_EQ(drag.shiftFor(2), 0.0);
  CHECK_EQ(drag.shiftFor(4), -100.0);
  CHECK_EQ(drag.shiftFor(9), -100.0);
  checkOffset(drag, 6, -100.0, 0.0);

  // The finger keeps moving over the other list. The home list stays closed.
  drag.place(900.0, 300.0, 100.0, 1000.0);
  drag.updateInsertion(listRows(3));
  CHECK_EQ(drag.getInsertionIndex(), UNDEFINED_INDEX);
  CHECK_EQ(drag.shiftFor(7), -100.0);
}

TEST(drag_board_held_row_coming_back_restores_the_drag) {
  DragReorder drag;
  drag.begin(3, "k3", 300.0, 100.0, 350.0);
  drag.setHeldAway(true);
  drag.setHeldAway(false);
  CHECK(!drag.isHeldAway());
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{3});
  CHECK_EQ(drag.getInsertionKey(), std::string("k3"));
  CHECK_EQ(drag.shiftFor(4), 0.0);

  drag.place(560.0, 300.0, 100.0, 1000.0);
  drag.updateInsertion(listRows(3));
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{5});
  CHECK_EQ(drag.getInsertionKey(), std::string("k5"));
  CHECK_EQ(drag.shiftFor(4), -100.0);
  CHECK_EQ(drag.shiftFor(6), 0.0);
}

TEST(drag_board_removal_shift_free_function) {
  CHECK_EQ(dragRemovalShift(3, 100.0, 2), 0.0);
  CHECK_EQ(dragRemovalShift(3, 100.0, 3), 0.0);
  CHECK_EQ(dragRemovalShift(3, 100.0, 4), -100.0);
  CHECK_EQ(dragRemovalShift(UNDEFINED_INDEX, 100.0, 4), 0.0);
}

TEST(drag_board_foreign_list_insertion_at_start_middle_and_end) {
  DragReorder start = foreignList(10.0);
  CHECK_EQ(start.getOriginIndex(), UNDEFINED_INDEX);
  CHECK_EQ(start.getInsertionIndex(), std::size_t{0});
  CHECK_EQ(start.getInsertionKey(), std::string("k0"));

  // Exactly on row 0's midpoint is not past it.
  CHECK_EQ(foreignList(50.0).getInsertionIndex(), std::size_t{0});
  CHECK_EQ(foreignList(51.0).getInsertionIndex(), std::size_t{1});

  // Center at 560 passed the midpoints of rows 0 to 5.
  DragReorder middle = foreignList(560.0);
  CHECK_EQ(middle.getInsertionIndex(), std::size_t{6});
  CHECK_EQ(middle.getInsertionKey(), std::string("k6"));

  // Before row 9's midpoint the incoming row still drops before it.
  DragReorder last = foreignList(940.0);
  CHECK_EQ(last.getInsertionIndex(), std::size_t{9});
  CHECK_EQ(last.getInsertionKey(), std::string("k9"));
}

TEST(drag_board_foreign_list_past_the_end_appends_with_an_empty_key) {
  DragReorder end = foreignList(951.0);
  CHECK_EQ(end.getInsertionIndex(), std::size_t{10});
  CHECK_EQ(end.getInsertionKey(), std::string());
  CHECK_EQ(end.shiftFor(9), 0.0);

  DragReorder empty;
  empty.beginForeign(80.0, 0.0, 1);
  empty.placeForeignCenter(200.0, 0.0);
  empty.updateInsertion({});
  CHECK_EQ(empty.getInsertionIndex(), std::size_t{0});
  CHECK_EQ(empty.getInsertionKey(), std::string());
}

TEST(drag_board_foreign_list_counts_from_the_first_mounted_row) {
  // Rows 4 to 7 are mounted. Above them all the incoming row drops before row 4.
  std::vector<DragRow> all = listRows();
  std::vector<DragRow> rows(all.begin() + 4, all.begin() + 8);
  DragReorder drag;
  drag.beginForeign(80.0, 0.0, 1);
  drag.placeForeignCenter(0.0, 0.0);
  drag.updateInsertion(rows);
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{4});
  CHECK_EQ(drag.getInsertionKey(), std::string("k4"));

  drag.placeForeignCenter(760.0, 0.0);
  drag.updateInsertion(rows);
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{8});
  CHECK_EQ(drag.getInsertionKey(), std::string());
}

TEST(drag_board_foreign_list_opens_the_gap) {
  DragReorder drag = foreignList(560.0);
  CHECK(drag.isForeign());
  CHECK_EQ(drag.shiftFor(0), 0.0);
  CHECK_EQ(drag.shiftFor(5), 0.0);
  CHECK_EQ(drag.shiftFor(6), 80.0);
  checkOffset(drag, 9, 80.0, 0.0);

  // Moving up moves the gap with it.
  drag.placeForeignCenter(140.0, 0.0);
  drag.updateInsertion(listRows());
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{1});
  CHECK_EQ(drag.shiftFor(0), 0.0);
  CHECK_EQ(drag.shiftFor(1), 80.0);
}

TEST(drag_board_foreign_free_functions) {
  std::vector<std::size_t> indices{2, 3, 4};
  std::vector<double> leadings{200.0, 300.0, 400.0};
  std::vector<double> extents{100.0, 100.0, 100.0};
  CHECK_EQ(dragForeignInsertionIndex(indices.data(), leadings.data(), extents.data(), 3, 0.0), std::size_t{0});
  CHECK_EQ(dragForeignInsertionIndex(indices.data(), leadings.data(), extents.data(), 3, 360.0), std::size_t{2});
  CHECK_EQ(dragForeignInsertionIndex(indices.data(), leadings.data(), extents.data(), 3, 460.0), std::size_t{3});
  CHECK_EQ(dragForeignInsertionIndex(indices.data(), leadings.data(), extents.data(), 0, 0.0), std::size_t{0});
  CHECK_EQ(dragForeignShift(3, 80.0, 2), 0.0);
  CHECK_EQ(dragForeignShift(3, 80.0, 3), 80.0);
  CHECK_EQ(dragForeignShift(UNDEFINED_INDEX, 80.0, 3), 0.0);
}

TEST(grid_drag_board_held_away_relayouts_without_the_cell) {
  DragRow resting = gridCell(1);
  DragReorder drag;
  drag.beginCell(resting, 50.0, 150.0, 3);
  drag.setHeldAway(true);
  drag.updateInsertion(gridRows(12, 1));
  CHECK_EQ(drag.getInsertionIndex(), UNDEFINED_INDEX);
  checkOffset(drag, 0, 0.0, 0.0);
  checkOffset(drag, 2, 0.0, -100.0);
  // Cell 3 moves up a row into the last column.
  checkOffset(drag, 3, -100.0, 200.0);
  checkOffset(drag, 4, 0.0, -100.0);
  checkOffset(drag, 11, 0.0, -100.0);

  drag.setHeldAway(false);
  checkOffset(drag, 2, 0.0, 0.0);
  drag.placeCell(50.0, 150.0, resting, 400.0, 300.0);
  drag.updateInsertion(gridRows(12, 1));
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{1});
  checkOffset(drag, 2, 0.0, 0.0);
  checkOffset(drag, 3, 0.0, 0.0);
}

TEST(grid_drag_board_removal_matches_the_core_layout) {
  std::vector<std::string> keys = keysFor(30);
  Container before;
  masonryLayout(before, keys);
  std::vector<DragRow> all = masonryRows(before, keys);

  for (std::size_t from : {std::size_t{0}, std::size_t{2}, std::size_t{13}, std::size_t{29}}) {
    std::vector<DragRow> rows;
    for (const DragRow& row : all) {
      if (row.index != from) {
        rows.push_back(row);
      }
    }
    CellArrays arrays(rows);
    DragCells cells = arrays.cells();
    std::vector<double> shifts(rows.size());
    std::vector<double> crossShifts(rows.size());
    dragGridRemovalShifts(cells, all[from], 3, shifts.data(), crossShifts.data());

    std::vector<std::string> removed = keys;
    removed.erase(removed.begin() + from);
    Container after;
    masonryLayout(after, removed);
    for (std::size_t position = 0; position < rows.size(); ++position) {
      const Row& landed = after.revision.rows[after.indexOfKey(rows[position].key)];
      CHECK_NEAR(rows[position].leading + shifts[position], landed.offsetY, 1e-9);
      CHECK_NEAR(rows[position].crossLeading + crossShifts[position], landed.offsetX, 1e-9);
    }
  }
}

TEST(grid_drag_board_foreign_insertion_and_gap) {
  // Over cell 5, the last cell of the second row.
  DragReorder drag = foreignGrid(150.0, 250.0);
  CHECK(drag.isGrid());
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{5});
  CHECK_EQ(drag.getInsertionKey(), std::string("k5"));
  checkOffset(drag, 4, 0.0, 0.0);
  // Cell 5 wraps to the first column of the next row.
  checkOffset(drag, 5, 100.0, -200.0);
  checkOffset(drag, 6, 0.0, 100.0);
  checkOffset(drag, 8, 100.0, -200.0);
  checkOffset(drag, 11, 100.0, -200.0);

  // Over no cell inside the content the drop spot stays.
  drag.placeForeignCenter(150.0, 350.0);
  drag.updateInsertion(gridRows(12));
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{5});
  checkOffset(drag, 5, 100.0, -200.0);
}

TEST(grid_drag_board_foreign_past_the_end) {
  DragReorder below = foreignGrid(450.0, 50.0);
  CHECK_EQ(below.getInsertionIndex(), std::size_t{12});
  CHECK_EQ(below.getInsertionKey(), std::string());
  checkOffset(below, 11, 0.0, 0.0);

  // The empty slot beside the last cell of a partial row appends.
  DragReorder beside;
  beside.beginForeign(100.0, 100.0, 3);
  beside.placeForeignCenter(350.0, 250.0);
  beside.updateInsertion(gridRows(11));
  CHECK_EQ(beside.getInsertionIndex(), std::size_t{11});
  CHECK_EQ(beside.getInsertionKey(), std::string());

  DragReorder empty;
  empty.beginForeign(100.0, 100.0, 3);
  empty.placeForeignCenter(50.0, 50.0);
  empty.updateInsertion({});
  CHECK_EQ(empty.getInsertionIndex(), std::size_t{0});
}

TEST(grid_drag_board_foreign_matches_the_core_layout) {
  std::vector<std::string> keys = keysFor(30);
  Container before;
  masonryLayout(before, keys);
  std::vector<DragRow> rows = masonryRows(before, keys);
  CellArrays arrays(rows);
  DragCells cells = arrays.cells();

  // The incoming cell is k30, laid out at MASONRY_HEIGHTS[30 % 7].
  std::string incoming = "k30";
  double extent = MASONRY_HEIGHTS[30 % 7];
  for (std::size_t to :
       {std::size_t{0}, std::size_t{5}, std::size_t{13}, std::size_t{28}, std::size_t{29}, std::size_t{30}}) {
    std::vector<double> shifts(rows.size());
    std::vector<double> crossShifts(rows.size());
    dragGridForeignShifts(cells, extent, WINDOW_WIDTH / 3.0, to, 3, shifts.data(), crossShifts.data());

    std::vector<std::string> inserted = keys;
    inserted.insert(inserted.begin() + to, incoming);
    Container after;
    masonryLayout(after, inserted);
    for (std::size_t position = 0; position < rows.size(); ++position) {
      const Row& landed = after.revision.rows[after.indexOfKey(rows[position].key)];
      CHECK_NEAR(rows[position].leading + shifts[position], landed.offsetY, 1e-9);
      CHECK_NEAR(rows[position].crossLeading + crossShifts[position], landed.offsetX, 1e-9);
    }
  }
}

TEST(grid_drag_board_foreign_insertion_free_function) {
  std::vector<DragRow> rows = gridRows(12);
  CellArrays arrays(rows);
  DragCells cells = arrays.cells();
  CHECK_EQ(dragGridForeignInsertionIndex(cells, UNDEFINED_INDEX, 150.0, 250.0), std::size_t{5});
  CHECK_EQ(dragGridForeignInsertionIndex(cells, 5, 150.0, 350.0), std::size_t{5});
  CHECK_EQ(dragGridForeignInsertionIndex(cells, UNDEFINED_INDEX, 150.0, 350.0), std::size_t{12});
  CHECK_EQ(dragGridForeignInsertionIndex(cells, 40, 150.0, 350.0), UNDEFINED_INDEX);
  CHECK_EQ(dragGridForeignInsertionIndex(cells, 5, 450.0, 50.0), std::size_t{12});
}

TEST(drag_board_begin_foreign_after_a_drag_resets_it) {
  DragReorder drag;
  drag.begin(3, "k3", 300.0, 100.0, 350.0);
  drag.place(560.0, 300.0, 100.0, 1000.0);
  drag.updateInsertion(listRows(3));
  drag.setHeldAway(true);

  drag.beginForeign(80.0, 0.0, 1);
  CHECK(drag.isForeign());
  CHECK(!drag.isHeldAway());
  CHECK_EQ(drag.getOriginIndex(), UNDEFINED_INDEX);
  CHECK_EQ(drag.getOriginKey(), std::string());
  CHECK_EQ(drag.getInsertionIndex(), UNDEFINED_INDEX);
  CHECK_EQ(drag.getInsertionKey(), std::string());
  CHECK_EQ(drag.shiftFor(4), 0.0);

  // Held away means nothing in a foreign session.
  drag.setHeldAway(true);
  CHECK(!drag.isHeldAway());

  drag.placeForeignCenter(560.0, 0.0);
  drag.updateInsertion(listRows());
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{6});
  CHECK_EQ(drag.shiftFor(6), 80.0);
}

TEST(drag_board_begin_after_a_foreign_session_resets_it) {
  DragReorder drag = foreignGrid(150.0, 250.0);
  checkOffset(drag, 5, 100.0, -200.0);

  drag.begin(3, "k3", 300.0, 100.0, 350.0);
  CHECK(!drag.isForeign());
  CHECK(!drag.isGrid());
  CHECK_EQ(drag.getOriginIndex(), std::size_t{3});
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{3});
  CHECK_EQ(drag.getInsertionKey(), std::string("k3"));
  checkOffset(drag, 5, 0.0, 0.0);
  drag.place(560.0, 300.0, 100.0, 1000.0);
  drag.updateInsertion(listRows(3));
  CHECK_EQ(drag.getInsertionIndex(), std::size_t{5});
  CHECK_EQ(drag.shiftFor(4), -100.0);
}

TEST(drag_board_ending_a_foreign_session_clears_the_gap) {
  DragReorder drag = foreignGrid(150.0, 250.0);
  drag.end();
  CHECK(!drag.isForeign());
  CHECK_EQ(drag.getInsertionIndex(), UNDEFINED_INDEX);
  CHECK_EQ(drag.getInsertionKey(), std::string());
  checkOffset(drag, 5, 0.0, 0.0);
  checkOffset(drag, 11, 0.0, 0.0);

  DragReorder list = foreignList(560.0);
  list.end();
  CHECK_EQ(list.shiftFor(6), 0.0);
}

TEST(drag_board_drop_index_caps_and_appends) {
  CHECK_EQ(dragBoardDropIndex(3, "k3", 10), std::size_t{3});
  // A stale mount can report an index past the data.
  CHECK_EQ(dragBoardDropIndex(12, "k12", 10), std::size_t{10});
  // An empty key appends after every row, mounted or not.
  CHECK_EQ(dragBoardDropIndex(6, "", 10), std::size_t{10});
  CHECK_EQ(dragBoardDropIndex(0, "", 0), std::size_t{0});
  // An unknown row count keeps the index.
  CHECK_EQ(dragBoardDropIndex(12, "k12", UNDEFINED_INDEX), std::size_t{12});
  CHECK_EQ(dragBoardDropIndex(6, "", UNDEFINED_INDEX), std::size_t{6});
  CHECK_EQ(dragBoardDropIndex(UNDEFINED_INDEX, "k3", 10), UNDEFINED_INDEX);
}

TEST(drag_board_lifts_out_only_over_another_list) {
  DragBoard board;
  CHECK(board.getPhase() == DragBoardPhase::Idle);
  CHECK(board.hover({"b"}).kind == DragBoardStep::Kind::Stay);

  board.begin("a", "k3");
  CHECK(board.getPhase() == DragBoardPhase::AtHome);
  CHECK_EQ(board.getHomeId(), std::string("a"));
  CHECK_EQ(board.getHeldKey(), std::string("k3"));
  CHECK(board.hover({"a"}).kind == DragBoardStep::Kind::Stay);
  CHECK(board.hover({}).kind == DragBoardStep::Kind::Stay);
  CHECK(board.hover({""}).kind == DragBoardStep::Kind::Stay);
  CHECK(!board.isFloating());
  CHECK_EQ(board.getTargetId(), std::string());

  DragBoardStep step = board.hover({"b"});
  CHECK(step.kind == DragBoardStep::Kind::LiftOut);
  CHECK_EQ(step.previous, std::string());
  CHECK_EQ(step.next, std::string("b"));
  CHECK(board.isFloating());
  CHECK_EQ(board.getTargetId(), std::string("b"));
}

TEST(drag_board_home_wins_where_lists_overlap) {
  DragBoard board;
  board.begin("a", "k3");
  CHECK(board.hover({"b", "a"}).kind == DragBoardStep::Kind::Stay);
  CHECK(!board.isFloating());

  // Over two other lists the first one is hovered.
  DragBoardStep lift = board.hover({"c", "b"});
  CHECK(lift.kind == DragBoardStep::Kind::LiftOut);
  CHECK_EQ(lift.next, std::string("c"));

  DragBoardStep back = board.hover({"c", "a"});
  CHECK(back.kind == DragBoardStep::Kind::ReturnHome);
  CHECK_EQ(back.previous, std::string("c"));
}

TEST(drag_board_target_stays_between_lists) {
  DragBoard board;
  board.begin("a", "k3");
  board.hover({"b"});
  CHECK(board.hover({}).kind == DragBoardStep::Kind::Stay);
  CHECK(board.hover({"b"}).kind == DragBoardStep::Kind::Stay);
  CHECK(board.hover({}).kind == DragBoardStep::Kind::Stay);
  CHECK(board.isFloating());
  CHECK_EQ(board.getTargetId(), std::string("b"));
}

TEST(drag_board_returns_home) {
  DragBoard board;
  board.begin("a", "k3");
  board.hover({"b"});
  board.hover({});
  DragBoardStep step = board.hover({"a"});
  CHECK(step.kind == DragBoardStep::Kind::ReturnHome);
  CHECK_EQ(step.previous, std::string("b"));
  CHECK_EQ(step.next, std::string());
  CHECK(board.getPhase() == DragBoardPhase::AtHome);
  CHECK_EQ(board.getTargetId(), std::string());

  // Back home the release is the normal drop.
  DragBoardRelease release = board.release(UNDEFINED_INDEX, "", 5);
  CHECK(release.kind == DragBoardRelease::Kind::Home);
  CHECK(board.getPhase() == DragBoardPhase::Idle);
}

TEST(drag_board_retargets_between_two_other_lists) {
  DragBoard board;
  board.begin("a", "k3");
  board.hover({"b"});
  DragBoardStep step = board.hover({"c"});
  CHECK(step.kind == DragBoardStep::Kind::Retarget);
  CHECK_EQ(step.previous, std::string("b"));
  CHECK_EQ(step.next, std::string("c"));
  CHECK_EQ(board.getTargetId(), std::string("c"));
  CHECK(board.isFloating());
}

TEST(drag_board_release_between_lists_drops_into_the_last_target) {
  DragBoard board;
  board.begin("a", "k3");
  board.hover({"b"});
  board.hover({});
  DragBoardRelease release = board.release(2, "k2", 5);
  CHECK(release.kind == DragBoardRelease::Kind::Drop);
  CHECK_EQ(release.listId, std::string("b"));
  CHECK_EQ(release.index, std::size_t{2});
  CHECK_EQ(release.key, std::string("k2"));
  // The drop waits for the row to land in b.
  CHECK(board.getPhase() == DragBoardPhase::Landing);
  CHECK_EQ(board.getTargetId(), std::string("b"));

  // Past every row the drop appends.
  board.begin("a", "k3");
  board.hover({"b"});
  DragBoardRelease append = board.release(4, "", 5);
  CHECK(append.kind == DragBoardRelease::Kind::Drop);
  CHECK_EQ(append.index, std::size_t{5});
  CHECK_EQ(append.key, std::string());
}

TEST(drag_board_release_without_a_target_cancels) {
  // The target left the board and nothing was hovered since.
  DragBoard board;
  board.begin("a", "k3");
  board.hover({"b"});
  board.leave("b");
  CHECK(board.isFloating());
  CHECK_EQ(board.getTargetId(), std::string());
  CHECK(board.release(2, "k2", 5).kind == DragBoardRelease::Kind::Cancel);

  // A target without a drop spot.
  board.begin("a", "k3");
  board.hover({"b"});
  CHECK(board.release(UNDEFINED_INDEX, "", 5).kind == DragBoardRelease::Kind::Cancel);

  // A held row without a key.
  board.begin("a", "");
  board.hover({"b"});
  CHECK(board.release(2, "k2", 5).kind == DragBoardRelease::Kind::Cancel);
}

TEST(drag_board_after_the_target_leaves_the_next_list_retargets) {
  DragBoard board;
  board.begin("a", "k3");
  board.hover({"b"});
  board.leave("b");
  DragBoardStep step = board.hover({"c"});
  CHECK(step.kind == DragBoardStep::Kind::Retarget);
  CHECK_EQ(step.previous, std::string());
  CHECK_EQ(step.next, std::string("c"));

  // The home list leaving ends the drag.
  board.leave("a");
  CHECK(board.getPhase() == DragBoardPhase::Idle);
  CHECK(board.hover({"c"}).kind == DragBoardStep::Kind::Stay);
}

TEST(drag_board_cancel_returns_the_target) {
  DragBoard board;
  board.begin("a", "k3");
  board.hover({"b"});
  CHECK_EQ(board.cancel(), std::string("b"));
  CHECK(board.getPhase() == DragBoardPhase::Idle);
  CHECK_EQ(board.getHomeId(), std::string());

  board.begin("a", "k3");
  CHECK_EQ(board.cancel(), std::string());
}

/*
 * The first frame of a foreign session in a grid has no drop spot to keep. Over a cell it
 * takes that cell, and in a gap inside the content, like the margin beside the last column,
 * it appends until the finger reaches a cell.
 */
TEST(grid_drag_board_foreign_first_frame) {
  DragReorder overCell;
  overCell.beginForeign(100.0, 100.0, 3);
  CHECK_EQ(overCell.getInsertionIndex(), UNDEFINED_INDEX);
  overCell.placeForeignCenter(150.0, 150.0);
  overCell.updateInsertion(gridRows(12));
  CHECK_EQ(overCell.getInsertionIndex(), std::size_t{4});
  CHECK_EQ(overCell.getInsertionKey(), std::string("k4"));

  DragReorder inGap;
  inGap.beginForeign(100.0, 100.0, 3);
  inGap.placeForeignCenter(150.0, 350.0);
  inGap.updateInsertion(gridRows(12));
  CHECK_EQ(inGap.getInsertionIndex(), std::size_t{12});
  CHECK_EQ(inGap.getInsertionKey(), std::string());
  inGap.placeForeignCenter(150.0, 250.0);
  inGap.updateInsertion(gridRows(12));
  CHECK_EQ(inGap.getInsertionIndex(), std::size_t{5});
}

/*
 * The picture lifts out from where the row showed and reaches its place under the finger,
 * with the grab from pickup, after DRAG_BOARD_LIFT_OUT_MS.
 */
TEST(drag_board_frame_moves_the_picture_under_the_finger_on_lift_out) {
  DragBoard board;
  // Picked up 30 left of the row's center and 10 above it.
  board.begin("a", "k3", {70.0, 200.0}, {100.0, 210.0});
  CHECK_NEAR(board.getGrab().x, 30.0, 1e-9);
  CHECK_NEAR(board.getGrab().y, 10.0, 1e-9);

  DragBoardFrame home = board.frame({150.0, 205.0}, {100.0, 215.0}, {"a"}, 0.0);
  CHECK(home.step.kind == DragBoardStep::Kind::Stay);
  CHECK(!home.floating);
  CHECK(!home.hideHeld);

  // The finger moved 230 sideways while the row stayed in its column.
  DragBoardFrame lift = board.frame({300.0, 205.0}, {100.0, 215.0}, {"b"}, 1000.0);
  CHECK(lift.step.kind == DragBoardStep::Kind::LiftOut);
  CHECK(lift.floating);
  CHECK(lift.hideHeld);
  CHECK_NEAR(lift.overlayCenter.x, 100.0, 1e-9);
  CHECK_NEAR(lift.overlayCenter.y, 215.0, 1e-9);

  DragBoardFrame halfway = board.frame({300.0, 205.0}, {100.0, 215.0}, {"b"}, 1000.0 + DRAG_BOARD_LIFT_OUT_MS / 2.0);
  CHECK(halfway.overlayCenter.x > 100.0 && halfway.overlayCenter.x < 330.0);

  DragBoardFrame caught = board.frame({310.0, 240.0}, {100.0, 215.0}, {"b"}, 1000.0 + DRAG_BOARD_LIFT_OUT_MS);
  CHECK_NEAR(caught.overlayCenter.x, 340.0, 1e-9);
  CHECK_NEAR(caught.overlayCenter.y, 250.0, 1e-9);
}

TEST(drag_board_drop_lands_once_the_target_keys_hold_the_row) {
  DragBoard board;
  board.begin("a", "k3");
  std::uint64_t flight = board.getFlight();
  board.hover({"b"});
  DragBoardRelease release = board.release(1, "b1", 4, 500.0);
  CHECK(release.kind == DragBoardRelease::Kind::Drop);
  CHECK(board.isLanding());
  CHECK(!board.isDeciding());
  // Hovers no longer decide anything once dropped.
  CHECK(board.hover({"c"}).kind == DragBoardStep::Kind::Stay);

  CHECK(!board.landed("c", std::vector<std::string>{"k3"}));
  CHECK(!board.landed("b", std::vector<std::string>{"b0", "b1"}));
  CHECK(!board.expired(500.0 + DRAG_BOARD_LAND_TIMEOUT_MS / 2.0));
  CHECK(board.landed("b", std::vector<std::string>{"b0", "k3", "b1"}));
  CHECK(board.getPhase() == DragBoardPhase::Settling);
  // Only the first landing counts, and a picture flying onto the row never expires.
  CHECK(!board.landed("b", true));
  CHECK(board.beginOverlaySettle());
  CHECK(board.isOverlaySettling());
  CHECK(!board.beginOverlaySettle());
  CHECK(!board.expired(500.0 + DRAG_BOARD_LAND_TIMEOUT_MS * 2.0));
  board.endFlight();
  CHECK(!board.isOverlaySettling());
  CHECK(board.getPhase() == DragBoardPhase::Idle);

  // The next pickup is a new flight.
  board.begin("a", "k4");
  CHECK(board.getFlight() != flight);
}

TEST(drag_board_landing_expires_after_its_timeout) {
  DragBoard board;
  board.begin("a", "k3");
  board.hover({"b"});
  board.release(1, "b1", 4, 500.0);
  CHECK(!board.expired(499.0 + DRAG_BOARD_LAND_TIMEOUT_MS));
  CHECK(board.expired(500.0 + DRAG_BOARD_LAND_TIMEOUT_MS));
  CHECK(board.getPhase() == DragBoardPhase::Idle);
  CHECK(!board.landed("b", true));
}

TEST(drag_board_a_landed_row_that_never_mounts_expires_after_the_landing_timeout) {
  DragBoard board;
  board.begin("a", "k3");
  board.hover({"b"});
  board.release(1, "b1", 4, 500.0);
  CHECK(board.landed("b", true));
  CHECK(!board.expired(499.0 + DRAG_BOARD_LAND_TIMEOUT_MS));
  CHECK(board.expired(500.0 + DRAG_BOARD_LAND_TIMEOUT_MS));
  CHECK(board.getPhase() == DragBoardPhase::Idle);
  CHECK(!board.beginOverlaySettle());
}

TEST(drag_board_landing_ends_when_its_list_leaves) {
  DragBoard board;
  board.begin("a", "k3");
  board.hover({"b"});
  board.release(1, "b1", 4, 0.0);
  // The home list's part is over. Its leaving keeps the landing.
  board.leave("a");
  CHECK(board.isLanding());
  board.leave("b");
  CHECK(board.getPhase() == DragBoardPhase::Idle);
}

/*
 * A row coming back over its home list starts where its floating picture was and eases into
 * the place the home list's drag gives it over DRAG_BOARD_RETURN_HOME_MS.
 */
TEST(drag_board_return_home_hands_off_from_the_picture) {
  DragBoard board;
  board.begin("a", "k3", {100.0, 200.0}, {100.0, 200.0});
  board.frame({300.0, 200.0}, {100.0, 200.0}, {"b"}, 0.0);
  DragBoardFrame floating = board.frame({300.0, 200.0}, {100.0, 200.0}, {"b"}, DRAG_BOARD_LIFT_OUT_MS);
  CHECK_NEAR(floating.overlayCenter.x, 300.0, 1e-9);
  // Nothing to hand off before a return.
  CHECK_NEAR(board.returnOffset({100.0, 200.0}, DRAG_BOARD_LIFT_OUT_MS).x, 0.0, 1e-9);

  DragBoardFrame back = board.frame({180.0, 220.0}, {100.0, 200.0}, {"a"}, 1000.0);
  CHECK(back.step.kind == DragBoardStep::Kind::ReturnHome);
  CHECK(!back.floating);
  // The home list places the row at 100, 220. It starts 200 to the right, where the picture was.
  DragPoint start = board.returnOffset({100.0, 220.0}, 1000.0);
  CHECK_NEAR(start.x, 200.0, 1e-9);
  CHECK_NEAR(start.y, -20.0, 1e-9);
  // The offset was measured once. The row follows the finger and the offset shrinks.
  DragPoint halfway = board.returnOffset({100.0, 260.0}, 1000.0 + DRAG_BOARD_RETURN_HOME_MS / 2.0);
  CHECK_NEAR(halfway.x, 50.0, 1e-9);
  CHECK_NEAR(halfway.y, -5.0, 1e-9);
  DragPoint done = board.returnOffset({100.0, 260.0}, 1000.0 + DRAG_BOARD_RETURN_HOME_MS);
  CHECK_NEAR(done.x, 0.0, 1e-9);
  CHECK_NEAR(done.y, 0.0, 1e-9);
  CHECK_NEAR(board.returnOffset({100.0, 260.0}, 1000.0).x, 0.0, 1e-9);
}

TEST(drag_board_return_offset_ends_with_a_lift_out_or_a_new_pickup) {
  DragBoard board;
  board.begin("a", "k3", {100.0, 200.0}, {100.0, 200.0});
  board.frame({300.0, 200.0}, {100.0, 200.0}, {"b"}, 0.0);
  board.frame({300.0, 200.0}, {100.0, 200.0}, {"b"}, DRAG_BOARD_LIFT_OUT_MS);
  board.frame({180.0, 200.0}, {100.0, 200.0}, {"a"}, 1000.0);
  // Out again before the hand-off was measured.
  board.frame({300.0, 200.0}, {100.0, 200.0}, {"b"}, 1010.0);
  CHECK_NEAR(board.returnOffset({100.0, 200.0}, 1020.0).x, 0.0, 1e-9);

  board.frame({180.0, 200.0}, {100.0, 200.0}, {"a"}, 2000.0);
  board.begin("a", "k4", {100.0, 200.0}, {100.0, 200.0});
  CHECK_NEAR(board.returnOffset({100.0, 200.0}, 2010.0).x, 0.0, 1e-9);
}

TEST(drag_board_pickup_over_a_settling_flight_settles_its_own_picture) {
  DragBoard board;
  board.begin("a", "k3");
  board.hover({"b"});
  board.release(1, "b1", 4);
  CHECK(board.landed("b", true));
  CHECK(board.beginOverlaySettle());

  // The next row is picked up while the picture still flies onto the first.
  board.begin("a", "k4");
  CHECK(!board.isOverlaySettling());
  board.hover({"b"});
  board.release(1, "b1", 5, 500.0);
  CHECK(board.landed("b", true));
  CHECK(!board.expired(500.0 + DRAG_BOARD_LAND_TIMEOUT_MS / 2.0));
  CHECK(board.beginOverlaySettle());
}

namespace {

/*
 * What DragBoard.hpp says a board is after each call, kept as plain state.
 */
struct BoardModel {
  DragBoardPhase phase = DragBoardPhase::Idle;
  std::string home;
  std::string held;
  std::string target;
  std::uint64_t flight = 0;
  bool overlaySettling = false;
  double landDeadline = 0.0;
  DragPoint grab;

  /*
   * The lift out: where the picture started, the finger then, when, and how far the picture
   * had come at the frame before.
   */
  bool lifting = false;
  DragPoint liftFrom;
  DragPoint liftFinger;
  double liftAt = 0.0;
  double liftShare = 0.0;

  /*
   * The way back home: the picture's last center, when the row returned, what the first
   * returnOffset measured and the share of it the call before gave.
   */
  DragPoint picture;
  bool returning = false;
  bool returnMeasured = false;
  double returnAt = 0.0;
  DragPoint returnDelta;
  double returnShare = 1.0;

  bool isDeciding() const { return phase == DragBoardPhase::AtHome || phase == DragBoardPhase::Floating; }

  bool isLanding() const { return phase == DragBoardPhase::Landing || phase == DragBoardPhase::Settling; }

  void goIdle() {
    phase = DragBoardPhase::Idle;
    home.clear();
    held.clear();
    target.clear();
    overlaySettling = false;
    lifting = false;
    returning = false;
  }

  /*
   * A hover: the home list wins among the candidates, otherwise the first one that has a name.
   */
  DragBoardStep hover(const std::vector<std::string>& candidates) {
    DragBoardStep step;
    if (!isDeciding()) {
      return step;
    }
    std::string hovered;
    for (const std::string& id : candidates) {
      if (id == home) {
        hovered = id;
        break;
      }
      if (hovered.empty()) {
        hovered = id;
      }
    }
    if (phase == DragBoardPhase::AtHome) {
      if (!hovered.empty() && hovered != home) {
        step = {DragBoardStep::Kind::LiftOut, "", hovered};
        phase = DragBoardPhase::Floating;
        target = hovered;
      }
    } else if (hovered == home) {
      step = {DragBoardStep::Kind::ReturnHome, target, ""};
      phase = DragBoardPhase::AtHome;
      target.clear();
    } else if (!hovered.empty() && hovered != target) {
      step = {DragBoardStep::Kind::Retarget, target, hovered};
      target = hovered;
    }
    return step;
  }
};

const char* nameOf(DragBoardPhase phase) {
  switch (phase) {
    case DragBoardPhase::Idle:
      return "idle";
    case DragBoardPhase::AtHome:
      return "at home";
    case DragBoardPhase::Floating:
      return "floating";
    case DragBoardPhase::Landing:
      return "landing";
    case DragBoardPhase::Settling:
      return "settling";
  }
  return "";
}

const char* nameOf(DragBoardStep::Kind kind) {
  switch (kind) {
    case DragBoardStep::Kind::Stay:
      return "stay";
    case DragBoardStep::Kind::LiftOut:
      return "lift out";
    case DragBoardStep::Kind::ReturnHome:
      return "return home";
    case DragBoardStep::Kind::Retarget:
      return "retarget";
  }
  return "";
}

bool samePoint(DragPoint a, DragPoint b) {
  return std::fabs(a.x - b.x) <= 1e-6 && std::fabs(a.y - b.y) <= 1e-6;
}

std::string describe(DragPoint point) {
  return "(" + std::to_string(point.x) + ", " + std::to_string(point.y) + ")";
}

/*
 * How far point is along the way from start to end, by the axis the way is longer on. Fails
 * when it is off the line between them.
 */
double shareAlong(const Trail& trail, DragPoint start, DragPoint end, DragPoint point, const std::string& what) {
  double wayX = end.x - start.x;
  double wayY = end.y - start.y;
  bool byX = std::fabs(wayX) >= std::fabs(wayY);
  double way = byX ? wayX : wayY;
  if (std::fabs(way) <= 1e-6) {
    if (!samePoint(point, end)) {
      trail.failWith(what + " is at " + describe(point) + " and has nowhere to go from " + describe(end));
    }
    return 1.0;
  }
  double share = ((byX ? point.x : point.y) - (byX ? start.x : start.y)) / way;
  if (!samePoint(point, {start.x + wayX * share, start.y + wayY * share})) {
    trail.failWith(what + " at " + describe(point) + " left the line from " + describe(start) + " to " + describe(end));
  }
  return share;
}

}

/*
 * A board through pickups, drag frames over one list, several or none, releases, drops that
 * land, time out or lose their list, pictures that settle, cancels and lists that leave. Every
 * answer and the board's state after it are what DragBoard.hpp says, a pickup starts a new
 * flight from whatever the flight before left, the picture lifts out along the line to its place
 * under the finger and the row comes back home along the line from the picture.
 */
TEST(fuzz_drag_board_is_what_its_calls_say_through_pickups_hovers_drops_and_leaves) {
  const FuzzKinds kinds = {
    {"pickup", "pickup over a flight", "stay", "lift out", "return home", "retarget", "return offset",
     "release at home", "release without a spot", "drop", "landed", "not landed", "settle", "settle refused", "expired",
     "not expired", "end flight", "cancel", "leave"},
    {},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    Trail trail(seed, episode);
    std::mt19937 rng(seed);
    DragBoard board;
    BoardModel model;
    const std::vector<std::string> lists = {"a", "b", "c", "d"};
    // A host calls frame for a picture or hover without one, never both.
    bool pictures = chance(rng, 75);
    double now = 0.0;
    std::size_t nextKey = 0;
    auto pickPoint = [&] {
      return DragPoint{static_cast<double>(pickInt(rng, 0, 800)), static_cast<double>(pickInt(rng, 0, 1200))};
    };
    auto pickList = [&] {
      // The target comes up as often as every other list together.
      if (!model.target.empty() && chance(rng, 50)) {
        return model.target;
      }
      return lists[pickSize(rng, 0, lists.size() - 1)];
    };

    for (int step = 0; step < 40; ++step) {
      int roll = pickPercent(rng);
      // A dropped row mostly gets the calls of a landing.
      if (model.isLanding() && chance(rng, 60)) {
        roll = pickInt(rng, 70, 91);
      }
      if (roll < 12) {
        episode.reach(model.phase == DragBoardPhase::Idle ? "pickup" : "pickup over a flight");
        std::string home = lists[pickSize(rng, 0, lists.size() - 1)];
        std::string key = chance(rng, 90) ? "k" + std::to_string(nextKey++) : std::string();
        DragPoint finger = pickPoint();
        DragPoint center = pickPoint();
        trail.note("begin in " + home + " with '" + key + "'");
        board.begin(home, key, finger, center);
        model.goIdle();
        model.phase = DragBoardPhase::AtHome;
        model.home = home;
        model.held = key;
        model.flight += 1;
        model.grab = {center.x - finger.x, center.y - finger.y};
        if (!samePoint(board.getGrab(), model.grab)) {
          trail.failWith("the grab is " + describe(board.getGrab()) + " expected " + describe(model.grab));
        }
      } else if (roll < 52) {
        std::vector<std::string> candidates;
        int count = pickInt(rng, 0, 3);
        std::string named;
        for (int candidate = 0; candidate < count; ++candidate) {
          candidates.push_back(chance(rng, 12) ? std::string() : lists[pickSize(rng, 0, lists.size() - 1)]);
          named += (candidate > 0 ? "," : "") + candidates.back();
        }
        now += pickInt(rng, 0, 60);
        DragPoint finger = pickPoint();
        DragPoint shown = pickPoint();
        trail.note((pictures ? "frame over [" : "hover over [") + named + "] at " + std::to_string(now));
        DragBoardStep expected = model.hover(candidates);
        DragBoardFrame frame;
        if (pictures) {
          frame = board.frame(finger, shown, candidates, now);
        } else {
          frame.step = board.hover(candidates);
        }
        episode.reach(nameOf(expected.kind));
        if (
          frame.step.kind != expected.kind || frame.step.previous != expected.previous ||
          frame.step.next != expected.next) {
          trail.failWith(
            std::string("the hover answered ") + nameOf(frame.step.kind) + " from '" + frame.step.previous + "' to '" +
            frame.step.next + "' expected " + nameOf(expected.kind) + " from '" + expected.previous + "' to '" +
            expected.next + "'");
        }
        if (!pictures) {
          continue;
        }
        if (expected.kind == DragBoardStep::Kind::LiftOut) {
          model.lifting = true;
          model.liftFrom = shown;
          model.liftFinger = finger;
          model.liftAt = now;
          model.liftShare = 0.0;
          model.returning = false;
        } else if (expected.kind == DragBoardStep::Kind::ReturnHome) {
          model.returning = true;
          model.returnMeasured = false;
          model.returnAt = now;
          model.returnShare = 1.0;
          model.lifting = false;
        }
        bool floating = model.phase == DragBoardPhase::Floating;
        bool hides = model.phase != DragBoardPhase::Idle && model.phase != DragBoardPhase::AtHome;
        if (frame.floating != floating || frame.hideHeld != hides) {
          trail.failWith(
            "the frame says floating " + std::to_string(frame.floating) + " and hidden " +
            std::to_string(frame.hideHeld) + " in phase " + nameOf(model.phase));
        }
        if (!floating) {
          continue;
        }
        /*
         * The picture starts where the row showed, has moved along with the finger since, and
         * closes in on its place under the finger without ever falling back. After
         * DRAG_BOARD_LIFT_OUT_MS it is there and stays.
         */
        DragPoint under{finger.x + model.grab.x, finger.y + model.grab.y};
        if (model.lifting && now - model.liftAt >= DRAG_BOARD_LIFT_OUT_MS) {
          model.lifting = false;
        }
        if (!model.lifting) {
          if (!samePoint(frame.overlayCenter, under)) {
            trail.failWith(
              "the picture is at " + describe(frame.overlayCenter) + " expected " + describe(under) +
              " under the finger");
          }
        } else {
          DragPoint from{
            model.liftFrom.x + finger.x - model.liftFinger.x, model.liftFrom.y + finger.y - model.liftFinger.y};
          double share = shareAlong(trail, from, under, frame.overlayCenter, "the picture lifting out");
          bool startsOff = now == model.liftAt && !samePoint(frame.overlayCenter, from);
          if (share < model.liftShare - 1e-9 || share > 1.0 + 1e-9 || startsOff) {
            trail.failWith(
              "the picture is " + std::to_string(share) + " of its way out after " + std::to_string(model.liftShare));
          }
          model.liftShare = share;
        }
        model.picture = frame.overlayCenter;
      } else if (roll < 60) {
        episode.reach("return offset");
        now += pickInt(rng, 0, 60);
        DragPoint placed = pickPoint();
        trail.note("return offset at " + std::to_string(now));
        DragPoint offset = board.returnOffset(placed, now);
        bool returns = model.returning && model.phase == DragBoardPhase::AtHome;
        if (returns && !model.returnMeasured) {
          model.returnMeasured = true;
          model.returnDelta = {model.picture.x - placed.x, model.picture.y - placed.y};
        }
        if (returns && now - model.returnAt >= DRAG_BOARD_RETURN_HOME_MS) {
          model.returning = false;
          returns = false;
        }
        if (!returns) {
          if (!samePoint(offset, {})) {
            trail.failWith("a return offset of " + describe(offset) + " with no way home left");
          }
          continue;
        }
        // The whole way from the picture at first, then less of it at every call.
        double share = shareAlong(trail, {}, model.returnDelta, offset, "the return offset");
        bool startsOff = now == model.returnAt && !samePoint(offset, model.returnDelta);
        if (share > model.returnShare + 1e-9 || share < -1e-9 || startsOff) {
          trail.failWith(
            "the return offset is " + std::to_string(share) + " of the way after " + std::to_string(model.returnShare));
        }
        model.returnShare = share;
      } else if (roll < 70) {
        std::size_t insertion = chance(rng, 15) ? UNDEFINED_INDEX : pickSize(rng, 0, 12);
        std::string key = chance(rng, 30) ? std::string() : "t" + std::to_string(pickInt(rng, 0, 12));
        std::size_t rowCount = chance(rng, 15) ? UNDEFINED_INDEX : pickSize(rng, 0, 10);
        now += pickInt(rng, 0, 60);
        trail.note(
          "release at " + std::to_string(static_cast<std::ptrdiff_t>(insertion)) + " before '" + key + "' of " +
          std::to_string(static_cast<std::ptrdiff_t>(rowCount)) + " rows");
        DragBoardRelease release = board.release(insertion, key, rowCount, now);
        DragBoardRelease expected;
        if (model.phase != DragBoardPhase::Floating) {
          episode.reach("release at home");
          model.goIdle();
        } else {
          std::size_t index = insertion;
          if (insertion != UNDEFINED_INDEX && rowCount != UNDEFINED_INDEX) {
            index = key.empty() ? rowCount : std::min(insertion, rowCount);
          }
          if (model.target.empty() || model.held.empty() || index == UNDEFINED_INDEX) {
            episode.reach("release without a spot");
            expected.kind = DragBoardRelease::Kind::Cancel;
            model.goIdle();
          } else {
            episode.reach("drop");
            expected = {DragBoardRelease::Kind::Drop, model.target, index, key};
            model.phase = DragBoardPhase::Landing;
            model.landDeadline = now + DRAG_BOARD_LAND_TIMEOUT_MS;
            model.lifting = false;
          }
        }
        if (
          release.kind != expected.kind || release.listId != expected.listId || release.index != expected.index ||
          release.key != expected.key) {
          trail.failWith(
            "the release answered kind " + std::to_string(static_cast<int>(release.kind)) + " into '" + release.listId +
            "' at " + std::to_string(static_cast<std::ptrdiff_t>(release.index)) + " expected kind " +
            std::to_string(static_cast<int>(expected.kind)) + " into '" + expected.listId + "' at " +
            std::to_string(static_cast<std::ptrdiff_t>(expected.index)));
        }
      } else if (roll < 80) {
        std::string list = pickList();
        bool holds = chance(rng, 60);
        trail.note("landed in " + list + (holds ? " with the row" : " without the row"));
        bool expected = model.phase == DragBoardPhase::Landing && list == model.target && holds;
        bool landed = false;
        if (chance(rng, 50)) {
          landed = board.landed(list, holds);
        } else {
          std::vector<std::string> keys = {"t0", "t1"};
          if (holds) {
            keys.insert(keys.begin() + 1, board.getHeldKey());
          }
          // A board without a held key finds no row by it.
          expected = expected && !model.held.empty();
          landed = board.landed(list, keys);
        }
        episode.reach(expected ? "landed" : "not landed");
        if (landed != expected) {
          trail.failWith("landed answered " + std::to_string(landed) + " in phase " + nameOf(model.phase));
        }
        if (expected) {
          model.phase = DragBoardPhase::Settling;
        }
      } else if (roll < 86) {
        bool expected = model.phase == DragBoardPhase::Settling && !model.overlaySettling;
        episode.reach(expected ? "settle" : "settle refused");
        trail.note("begin overlay settle");
        if (board.beginOverlaySettle() != expected) {
          trail.failWith(
            std::string("the picture ") + (expected ? "did not start" : "started") + " to settle in phase " +
            nameOf(model.phase));
        }
        model.overlaySettling = model.overlaySettling || expected;
      } else if (roll < 92) {
        now += chance(rng, 40) ? pickInt(rng, 400, 1400) : pickInt(rng, 0, 60);
        bool waiting =
          model.phase == DragBoardPhase::Landing || (model.phase == DragBoardPhase::Settling && !model.overlaySettling);
        bool expected = waiting && now >= model.landDeadline;
        episode.reach(expected ? "expired" : "not expired");
        trail.note("expired at " + std::to_string(now));
        if (board.expired(now) != expected) {
          trail.failWith(
            std::string("the wait ") + (expected ? "did not run out" : "ran out") + " at " + std::to_string(now) +
            " for a deadline of " + std::to_string(model.landDeadline) + " in phase " + nameOf(model.phase));
        }
        if (expected) {
          model.goIdle();
        }
      } else if (roll < 94) {
        episode.reach("end flight");
        trail.note("end flight");
        board.endFlight();
        model.goIdle();
      } else if (roll < 96) {
        episode.reach("cancel");
        trail.note("cancel");
        std::string target = board.cancel();
        if (target != model.target) {
          trail.failWith("cancel returned '" + target + "' for the target '" + model.target + "'");
        }
        model.goIdle();
      } else {
        episode.reach("leave");
        std::string list = chance(rng, 10) ? std::string() : pickList();
        if (chance(rng, 30) && !model.home.empty()) {
          list = model.home;
        }
        trail.note("leave '" + list + "'");
        board.leave(list);
        // After a drop only the list the row lands in matters. Before it the home list ends the drag.
        bool named = model.phase != DragBoardPhase::Idle && !list.empty();
        bool ends = model.isLanding() ? list == model.target : list == model.home;
        if (named && ends) {
          model.goIdle();
        } else if (named && !model.isLanding() && list == model.target) {
          model.target.clear();
        }
      }

      if (board.getPhase() != model.phase) {
        trail.failWith(std::string("the board is ") + nameOf(board.getPhase()) + " expected " + nameOf(model.phase));
      }
      if (board.getHomeId() != model.home || board.getHeldKey() != model.held || board.getTargetId() != model.target) {
        trail.failWith(
          "the board holds '" + board.getHeldKey() + "' of '" + board.getHomeId() + "' over '" + board.getTargetId() +
          "' expected '" + model.held + "' of '" + model.home + "' over '" + model.target + "'");
      }
      if (board.getFlight() != model.flight) {
        trail.failWith("flight " + std::to_string(board.getFlight()) + " expected " + std::to_string(model.flight));
      }
      if (board.isOverlaySettling() != model.overlaySettling) {
        trail.failWith(
          std::string("the picture ") + (board.isOverlaySettling() ? "flies" : "does not fly") +
          " onto the row in phase " + nameOf(model.phase));
      }
      if (
        board.isFloating() != (model.phase == DragBoardPhase::Floating) || board.isDeciding() != model.isDeciding() ||
        board.isLanding() != model.isLanding()) {
        trail.failWith(std::string("the board's questions disagree with phase ") + nameOf(model.phase));
      }
      episode.observe(static_cast<int>(board.getPhase()));
      episode.observe(board.getTargetId());
      episode.observe(board.getFlight());
    }
  });
}
