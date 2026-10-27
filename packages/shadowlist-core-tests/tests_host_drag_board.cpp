/*
 * Drag board: a row dragged out of one list into another. The home list closes the gap while
 * the held row is away, and the list under it opens a gap for a row it does not own. Grid
 * previews are checked against the core's own layout after the removal or insertion. DragBoard
 * decides when the row lifts out, which list it targets and how a release ends.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/host/DragBoard.hpp>
#include <shadowlist-core/host/DragReorder.hpp>

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
  for (std::size_t to : {std::size_t{0}, std::size_t{5}, std::size_t{13}, std::size_t{28}, std::size_t{29},
         std::size_t{30}}) {
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

TEST(drag_board_end_foreign_clears_the_gap) {
  DragReorder drag = foreignGrid(150.0, 250.0);
  drag.endForeign();
  CHECK(!drag.isForeign());
  CHECK_EQ(drag.getInsertionIndex(), UNDEFINED_INDEX);
  CHECK_EQ(drag.getInsertionKey(), std::string());
  checkOffset(drag, 5, 0.0, 0.0);
  checkOffset(drag, 11, 0.0, 0.0);

  DragReorder list = foreignList(560.0);
  list.endForeign();
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
  // Only the first landing counts, and a settling flight never expires.
  CHECK(!board.landed("b", true));
  CHECK(!board.expired(500.0 + DRAG_BOARD_LAND_TIMEOUT_MS * 2.0));
  board.endFlight();
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
