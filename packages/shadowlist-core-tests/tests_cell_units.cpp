/*
 * ListDriver in a terminal's units: a row is a cell tall, the window a few dozen cells, and
 * offsets whole cells. Nothing in the core may assume points.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/ListDriver.hpp>

#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

constexpr double WINDOW_ROWS = 20.0;
constexpr double WINDOW_COLUMNS = 80.0;

ListSettings cellSettings(bool inverted = false) {
  ListSettings settings;
  settings.estimatedRowSize = 1.0;
  settings.invertedFollowBand = 1.0;
  settings.inverted = inverted;
  settings.followAppends = inverted;
  return settings;
}

std::vector<std::string> keysOf(std::size_t count) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    keys.push_back("k" + std::to_string(index));
  }
  return keys;
}

/*
 * A host whose every row is one cell, or as tall as rowHeight says.
 */
struct CellHost {
  ListDriver driver;
  double offset = 0.0;
  double rowHeight = 1.0;
  std::size_t measures = 0;

  explicit CellHost(std::size_t count, ListSettings settings = cellSettings()) {
    driver.setSettings(settings);
    driver.setMeasureRow([this](std::size_t, const std::string&, double) {
      ++measures;
      return rowHeight;
    });
    driver.setKeys(keysOf(count));
  }

  PassResult layout(bool userScrolled = false, ScrollPhase phase = ScrollPhase::Idle) {
    PassInput input;
    input.offset = offset;
    input.windowAlong = WINDOW_ROWS;
    input.windowCross = WINDOW_COLUMNS;
    input.userScrolled = userScrolled;
    input.phase = phase;
    PassResult result = driver.runPasses(input);
    offset = result.offset;
    return result;
  }

  void settle() {
    PassResult result = layout();
    for (int frame = 0; frame < 10 && result.settling; ++frame) {
      result = layout();
    }
  }

  /*
   * A wheel notch, which is a gesture of one frame, then the frame at rest after it.
   */
  void wheel(double cells) {
    offset += cells;
    layout(true, ScrollPhase::Dragging);
    layout();
  }

  double maxOffset() const {
    return driver.getContentAlong() - WINDOW_ROWS;
  }
};

}

TEST(cell_units_default_follow_band_stays_in_points) {
  CHECK(ListSettings{}.invertedFollowBand == INVERTED_FOLLOW_BAND);
  CHECK(FrameInput{}.invertedFollowBand == INVERTED_FOLLOW_BAND);
}

TEST(cell_units_a_large_list_places_rows_on_whole_cells_and_measures_few) {
  CellHost host(100000);
  host.settle();
  CHECK(host.driver.getContentAlong() == 100000.0);
  CHECK(host.measures < 100);
  CHECK(host.driver.getRowRect(7).y == 7.0);
  CHECK(host.driver.getRowRect(7).height == 1.0);
  auto visible = host.driver.getVisibleRange();
  CHECK(visible.has_value());
  CHECK(visible->low == 0);
  CHECK(visible->high == 19);
}

TEST(cell_units_scroll_to_row_lands_exactly) {
  CellHost host(100000);
  host.settle();
  host.driver.scrollToRow(50000, 0.0);
  host.settle();
  CHECK(host.offset == 50000.0);
  host.driver.scrollToRow(50000, 1.0);
  host.settle();
  CHECK(host.offset == 50000.0 - WINDOW_ROWS + 1.0);
  host.driver.scrollToEnd();
  host.settle();
  CHECK(host.offset == host.maxOffset());
}

TEST(cell_units_rows_taller_than_a_cell_correct_the_estimate) {
  CellHost host(1000);
  host.rowHeight = 3.0;
  host.settle();
  CHECK(host.driver.getRowRect(2).y == 6.0);
  CHECK(host.driver.getExtentAt(2) == 3.0);
}

TEST(cell_units_inverted_list_opens_at_its_end_and_follows_appends) {
  CellHost host(100, cellSettings(true));
  host.settle();
  CHECK(host.offset == 80.0);
  host.driver.setKeys(keysOf(101));
  host.settle();
  CHECK(host.offset == 81.0);
}

TEST(cell_units_one_wheel_notch_away_releases_the_inverted_bottom) {
  CellHost host(100, cellSettings(true));
  host.settle();
  host.wheel(-3.0);
  CHECK(host.offset == 77.0);
  host.driver.setKeys(keysOf(101));
  host.settle();
  // A point sized band of 24 would have kept pinning this reader to the end.
  CHECK(host.offset == 77.0);
}

TEST(cell_units_one_cell_from_the_bottom_still_counts_as_at_it) {
  CellHost host(100, cellSettings(true));
  host.settle();
  host.wheel(-1.0);
  host.driver.setKeys(keysOf(101));
  host.settle();
  CHECK(host.offset == 81.0);
}
