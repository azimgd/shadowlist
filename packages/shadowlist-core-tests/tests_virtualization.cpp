/*
 * Virtualization tests. Every row on screen stays mounted, rows stay back to back after any
 * change, measured sizes survive data changes, and a prepend keeps the visible content in place,
 * in every layout. Mounted rows are checked against a slow scan of every row. A search that
 * skips a row fails here. Only the public core API is used.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"
#include "VirtualizationHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

// Window selection

TEST(window_covers_every_overlapping_row_single_column) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(600);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, unevenHeights(keys.size()));

  double total = container.revision.contentHeight;
  for (double offset = 0.0; offset <= total; offset += 137.0) {
    Virtualizer::update(container, inputFor(keys, offset, fixture));
    checkNoRowLost(container, "single column @" + std::to_string(offset));
  }
}

TEST(window_covers_every_overlapping_row_horizontal) {
  Fixture fixture;
  fixture.horizontal = true;
  fixture.estimatedWidth = 140.0;
  fixture.estimatedHeight = WINDOW_HEIGHT;

  std::vector<std::string> keys = keysFor(400);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  for (std::size_t index = 0; index < keys.size(); ++index) {
    double width = (index % 4 == 0) ? 300.0 : 120.0 + static_cast<double>(index % 5) * 9.0;
    Virtualizer::updateRowAtIndex(container, index, {width, WINDOW_HEIGHT});
  }

  double total = container.revision.contentWidth;
  for (double offset = 0.0; offset <= total; offset += 91.0) {
    Virtualizer::update(container, inputFor(keys, offset, fixture));
    checkNoRowLost(container, "horizontal @" + std::to_string(offset));
  }
}

TEST(window_covers_every_overlapping_row_inverted) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(500);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, unevenHeights(keys.size()));

  double total = container.revision.contentHeight;
  for (double offset = total; offset >= 0.0; offset -= 149.0) {
    Virtualizer::update(container, inputFor(keys, offset < 0.0 ? 0.0 : offset, fixture));
    checkNoRowLost(container, "inverted @" + std::to_string(offset));
  }
}

/*
 * The hard case for a search: grid columns that grow at very different rates. Rows near
 * each other on screen have indices far apart.
 */
TEST(window_covers_every_overlapping_row_skewed_columns) {
  Fixture fixture;
  fixture.numberOfColumns = 3;

  std::vector<std::string> keys = keysFor(600);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  for (std::size_t index = 0; index < keys.size(); ++index) {
    // Column 0 grows fast, column 1 slowly, column 2 in between.
    double height = (index % 3 == 0) ? 400.0 : (index % 3 == 1 ? 40.0 : 150.0);
    Virtualizer::updateRowAtIndex(container, index, {WINDOW_WIDTH / 3.0, height});
  }

  double total = container.revision.contentHeight;
  for (double offset = 0.0; offset <= total; offset += 113.0) {
    Virtualizer::update(container, inputFor(keys, offset, fixture));
    checkNoRowLost(container, "3 skewed columns @" + std::to_string(offset));
  }
}

/*
 * A row that starts above the window but still covers it must stay mounted. Checking only
 * where rows start would drop it and leave a gap.
 */
TEST(window_keeps_row_straddling_the_lower_bound) {
  Fixture fixture;
  fixture.overscan = 0.0;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  std::vector<double> heights(keys.size(), 100.0);
  heights[10] = 5000.0;  // one row far taller than the screen
  measureRows(container, heights);

  // Scroll into the middle of the very tall row.
  double offsetInsideTallRow = offsetOf(container, 10) + 2500.0;
  Virtualizer::update(container, inputFor(keys, offsetInsideTallRow, fixture));

  IndexRange measured = container.getMeasuredRange();
  CHECK(measured.low != UNDEFINED_INDEX);
  CHECK(measured.low <= 10 && 10 <= measured.high);
  checkNoRowLost(container, "straddling row");
}

TEST(window_is_correct_on_the_frame_a_reorder_lands) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(300);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, unevenHeights(keys.size()));

  Virtualizer::update(container, inputFor(keys, 4000.0, fixture));

  /*
   * Move a row from deep in the list to the front. Its old position would break a search
   * that trusted the layout from before the reflow.
   */
  std::vector<std::string> reordered = keys;
  std::string moved = reordered[250];
  reordered.erase(reordered.begin() + 250);
  reordered.insert(reordered.begin(), moved);

  Virtualizer::update(container, inputFor(reordered, 4000.0, fixture));
  checkNoRowLost(container, "reorder frame");
  checkGeometryContiguous(container, "reorder frame");
}

// Geometry integrity

TEST(geometry_stays_contiguous_across_scrolling_and_measurement) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(400);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  std::vector<double> heights = unevenHeights(keys.size());
  for (std::size_t index = 0; index < heights.size(); ++index) {
    Virtualizer::updateRowAtIndex(container, index, {WINDOW_WIDTH, heights[index]});
    if (index % 25 == 0) {
      Virtualizer::update(container, inputFor(keys, static_cast<double>(index) * 13.0, fixture));
    }
  }

  checkGeometryContiguous(container, "after interleaved measure/scroll");
  for (std::size_t index = 0; index < heights.size(); ++index) {
    CHECK_NEAR(sizeOf(container, index), heights[index], 0.001);
  }
}

/*
 * Reporting a size a row already has must not move anything. Unchanged sizes are the common
 * case because every layout reports every mounted row.
 */
TEST(repeated_identical_measurements_do_not_move_geometry) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  std::vector<double> heights = unevenHeights(keys.size());
  measureRows(container, heights);
  Virtualizer::update(container, inputFor(keys, 900.0, fixture));

  std::vector<double> offsetsBefore;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    offsetsBefore.push_back(offsetOf(container, index));
  }
  double offsetBefore = container.revision.offsetY;
  double totalBefore = container.revision.contentHeight;

  for (int repeat = 0; repeat < 3; ++repeat) {
    for (std::size_t index = 0; index < keys.size(); ++index) {
      Virtualizer::updateRowAtIndex(container, index, {WINDOW_WIDTH, heights[index]});
    }
  }
  Virtualizer::recomputeContentSize(container);

  for (std::size_t index = 0; index < keys.size(); ++index) {
    CHECK_NEAR(offsetOf(container, index), offsetsBefore[index], 0.001);
  }
  CHECK_NEAR(container.revision.offsetY, offsetBefore, 0.001);
  CHECK_NEAR(container.revision.contentHeight, totalBefore, 0.001);
}

TEST(a_genuine_resize_shifts_only_the_rows_after_it) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  std::vector<double> before;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    before.push_back(offsetOf(container, index));
  }

  Virtualizer::updateRowAtIndex(container, 40, {WINDOW_WIDTH, 300.0});

  for (std::size_t index = 0; index <= 40; ++index) {
    CHECK_NEAR(offsetOf(container, index), before[index], 0.001);
  }
  for (std::size_t index = 41; index < keys.size(); ++index) {
    CHECK_NEAR(offsetOf(container, index), before[index] + 200.0, 0.001);
  }
  checkGeometryContiguous(container, "after single resize");
}

TEST(header_size_shifts_every_row_and_the_total) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(50);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  FrameInput withHeader = inputFor(keys, 0.0, fixture);
  withHeader.headerSize = 250.0;
  withHeader.footerSize = 60.0;
  Virtualizer::update(container, withHeader);

  CHECK_NEAR(offsetOf(container, 0), 250.0, 0.001);
  CHECK_NEAR(offsetOf(container, 49), 250.0 + 49.0 * 100.0, 0.001);
  CHECK_NEAR(container.revision.contentHeight, 250.0 + 50.0 * 100.0 + 60.0, 0.001);
  checkGeometryContiguous(container, "with header");
}

// Reconciliation

TEST(reconcile_preserves_measured_sizes_of_surviving_rows) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  std::vector<double> heights = unevenHeights(keys.size());
  measureRows(container, heights);

  // Prepend a page, drop two from the middle, append one.
  std::vector<std::string> next = keysFor(20, "older");
  for (std::size_t index = 0; index < keys.size(); ++index) {
    if (index == 30 || index == 31) {
      continue;
    }
    next.push_back(keys[index]);
  }
  next.push_back("appended");

  Virtualizer::update(container, inputFor(next, 0.0, fixture));

  for (std::size_t index = 0; index < keys.size(); ++index) {
    if (index == 30 || index == 31) {
      CHECK_EQ(container.indexOfKey(keys[index]), UNDEFINED_INDEX);
      continue;
    }
    std::size_t liveIndex = container.indexOfKey(keys[index]);
    CHECK(liveIndex != UNDEFINED_INDEX);
    CHECK(container.revision.rows[liveIndex].measured);
    CHECK_NEAR(sizeOf(container, liveIndex), heights[index], 0.001);
  }
  checkGeometryContiguous(container, "after mixed reconcile");
}

TEST(key_index_map_matches_the_row_list_after_every_mutation) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  auto checkMap = [&](const std::string& context) {
    for (std::size_t index = 0; index < container.revision.rows.size(); ++index) {
      const std::string& key = container.revision.rows[index].key;
      std::size_t found = container.indexOfKey(key);
      // A duplicate key resolves to its first occurrence. Only that direction is checked.
      if (found > index) {
        fail(context + ": key '" + key + "' at " + std::to_string(index) +
          " resolves to " + std::to_string(found));
      }
      CHECK_EQ(container.revision.rows[found].key, key);
    }
  };
  checkMap("initial");

  std::vector<std::string> reversed(keys.rbegin(), keys.rend());
  Virtualizer::update(container, inputFor(reversed, 0.0, fixture));
  checkMap("after full reverse");

  std::vector<std::string> trimmed(keys.begin(), keys.begin() + 10);
  Virtualizer::update(container, inputFor(trimmed, 0.0, fixture));
  checkMap("after trim");
  CHECK_EQ(container.getRowCount(), static_cast<std::size_t>(10));

  std::vector<std::string> replaced = keysFor(30, "fresh");
  Virtualizer::update(container, inputFor(replaced, 0.0, fixture));
  checkMap("after full replacement");

  Virtualizer::update(container, inputFor({}, 0.0, fixture));
  CHECK_EQ(container.getRowCount(), static_cast<std::size_t>(0));
}

/*
 * Two rows with the same key must not share one measured row. The first keeps its size
 * and the second starts fresh.
 */
TEST(duplicate_keys_do_not_share_one_row) {
  Fixture fixture;
  std::vector<std::string> keys = {"a", "b", "c"};
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, {100.0, 200.0, 300.0});

  std::vector<std::string> withDuplicate = {"a", "b", "b", "c"};
  Virtualizer::update(container, inputFor(withDuplicate, 0.0, fixture));

  CHECK_EQ(container.getRowCount(), static_cast<std::size_t>(4));
  CHECK_EQ(container.revision.rows[0].key, std::string("a"));
  CHECK_EQ(container.revision.rows[1].key, std::string("b"));
  CHECK_EQ(container.revision.rows[2].key, std::string("b"));
  CHECK_EQ(container.revision.rows[3].key, std::string("c"));
  // The old row keeps its size and the new one does not take it.
  CHECK(container.revision.rows[1].measured);
  CHECK(!container.revision.rows[2].measured);
  CHECK_EQ(container.indexOfKey("b"), static_cast<std::size_t>(1));
  checkGeometryContiguous(container, "with duplicate key");
}

TEST(full_replacement_resets_the_frozen_average) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 500.0));
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  CHECK(container.revision.averageRowHeight > 0.0);

  std::vector<std::string> replaced = keysFor(40, "fresh");
  Virtualizer::update(container, inputFor(replaced, 0.0, fixture));
  CHECK_EQ(container.revision.measuredRealCount, static_cast<std::size_t>(0));
  CHECK_NEAR(container.revision.averageRowHeight, 0.0, 0.001);
}
