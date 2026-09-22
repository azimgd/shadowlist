/*
 * Virtualization tests. Every row on screen stays mounted, rows stay back to back after any
 * change, measured sizes survive data changes, and a prepend keeps the visible content in place,
 * in every layout. Mounted rows are checked against a slow scan of every row, so a search that
 * skips a row fails here. Only the public core API is used.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

struct Fixture {
  std::size_t columns = 1;
  bool horizontal = false;
  bool inverted = false;
  bool followAppends = false;
  double overscan = 1.0;
  double estimatedWidth = WINDOW_WIDTH;
  double estimatedHeight = ESTIMATED_ROW_HEIGHT;
};

FrameInput inputFor(const std::vector<std::string>& keys, double offset, const Fixture& fixture) {
  FrameInput input;
  input.keys = keys;
  input.windowContainerWidth = WINDOW_WIDTH;
  input.windowContainerHeight = WINDOW_HEIGHT;
  input.columns = fixture.columns;
  input.horizontal = fixture.horizontal;
  input.inverted = fixture.inverted;
  input.followAppends = fixture.followAppends;
  input.overscan = fixture.overscan;
  input.estimatedElementSize = {fixture.estimatedWidth, fixture.estimatedHeight};
  if (fixture.horizontal) {
    input.containerOffsetX = offset;
  } else {
    input.containerOffsetY = offset;
  }
  return input;
}

/*
 * The core reports its window as a first and last index, reversed for inverted lists.
 * Turn it into the ascending range the host would mount.
 */
std::pair<std::size_t, std::size_t> reportedWindow(const Container& container) {
  auto visible = container.getVisibleIndices();
  if (visible.first == UNDEFINED_INDEX || visible.second == UNDEFINED_INDEX) {
    return {UNDEFINED_INDEX, UNDEFINED_INDEX};
  }
  return {std::min(visible.first, visible.second), std::max(visible.first, visible.second)};
}

/*
 * No row on screen or in the overscan may fall outside the range the host mounts.
 */
void checkNoRowLost(const Container& container, const std::string& context) {
  std::set<std::size_t> overlapping = overlappingIndices(container, container.overscan);
  auto window = reportedWindow(container);

  if (overlapping.empty()) {
    return;
  }

  if (window.first == UNDEFINED_INDEX) {
    fail(context + ": " + std::to_string(overlapping.size()) +
      " rows overlap the viewport but the core reported no window");
  }
  for (std::size_t index : overlapping) {
    if (index < window.first || index > window.second) {
      fail(context + ": row " + std::to_string(index) +
        " overlaps the viewport but is outside the reported window [" +
        std::to_string(window.first) + ".." + std::to_string(window.second) + "]");
    }
  }
}

/*
 * Within each column, every row must start exactly where the one before it ended.
 * A reflow that skipped a row fails here.
 */
void checkGeometryContiguous(const Container& container, const std::string& context) {
  std::size_t columns = container.columns > 0 ? container.columns : 1;
  double headerSize = container.headerSize;

  std::vector<double> trackEdges(columns, headerSize);
  for (std::size_t index = 0; index < container.revision.elements.size(); ++index) {
    std::size_t track = columns > 1 ? index % columns : 0;
    double expected = trackEdges[track];
    double actual = offsetOf(container, index);
    if (std::fabs(actual - expected) > 0.001) {
      fail(context + ": row " + std::to_string(index) + " sits at " + std::to_string(actual) +
        " but the running track edge is " + std::to_string(expected));
    }
    trackEdges[track] = actual + sizeOf(container, index);
  }
}

/*
 * Give every row a real, uneven size. Equal rows hide most window bugs.
 */
void measureRows(Container& container, const std::vector<double>& heights) {
  for (std::size_t index = 0; index < heights.size(); ++index) {
    Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, heights[index]});
  }
}

std::vector<double> unevenHeights(std::size_t count) {
  std::vector<double> heights;
  heights.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    // Mix tall rows, short rows and a few with zero height.
    if (index % 17 == 0) {
      heights.push_back(0.0);
    } else if (index % 5 == 0) {
      heights.push_back(420.0);
    } else if (index % 3 == 0) {
      heights.push_back(24.0);
    } else {
      heights.push_back(96.0 + static_cast<double>(index % 7) * 11.0);
    }
  }
  return heights;
}

}

// Window selection

TEST(window_covers_every_overlapping_row_single_column) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(600);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, unevenHeights(keys.size()));

  double total = container.revision.totalContainerHeight;
  for (double offset = 0.0; offset <= total; offset += 137.0) {
    Virtualizer::update(&container, inputFor(keys, offset, fixture));
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
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  for (std::size_t index = 0; index < keys.size(); ++index) {
    double width = (index % 4 == 0) ? 300.0 : 120.0 + static_cast<double>(index % 5) * 9.0;
    Virtualizer::updateElementAtIndex(&container, index, {width, WINDOW_HEIGHT});
  }

  double total = container.revision.totalContainerWidth;
  for (double offset = 0.0; offset <= total; offset += 91.0) {
    Virtualizer::update(&container, inputFor(keys, offset, fixture));
    checkNoRowLost(container, "horizontal @" + std::to_string(offset));
  }
}

TEST(window_covers_every_overlapping_row_inverted) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(500);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, unevenHeights(keys.size()));

  double total = container.revision.totalContainerHeight;
  for (double offset = total; offset >= 0.0; offset -= 149.0) {
    Virtualizer::update(&container, inputFor(keys, offset < 0.0 ? 0.0 : offset, fixture));
    checkNoRowLost(container, "inverted @" + std::to_string(offset));
  }
}

/*
 * The hard case for a search: grid columns that grow at very different rates, so rows
 * near each other on screen have indices far apart.
 */
TEST(window_covers_every_overlapping_row_skewed_columns) {
  Fixture fixture;
  fixture.columns = 3;

  std::vector<std::string> keys = keysFor(600);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  for (std::size_t index = 0; index < keys.size(); ++index) {
    // Column 0 grows fast, column 1 slowly, column 2 in between.
    double height = (index % 3 == 0) ? 400.0 : (index % 3 == 1 ? 40.0 : 150.0);
    Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH / 3.0, height});
  }

  double total = container.revision.totalContainerHeight;
  for (double offset = 0.0; offset <= total; offset += 113.0) {
    Virtualizer::update(&container, inputFor(keys, offset, fixture));
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
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  std::vector<double> heights(keys.size(), 100.0);
  heights[10] = 5000.0;  // one row far taller than the screen
  measureRows(container, heights);

  // Scroll into the middle of the very tall row.
  double offsetInsideTallRow = offsetOf(container, 10) + 2500.0;
  Virtualizer::update(&container, inputFor(keys, offsetInsideTallRow, fixture));

  auto window = reportedWindow(container);
  CHECK(window.first != UNDEFINED_INDEX);
  CHECK(window.first <= 10 && 10 <= window.second);
  checkNoRowLost(container, "straddling row");
}

TEST(window_is_correct_on_the_frame_a_reorder_lands) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(300);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, unevenHeights(keys.size()));

  Virtualizer::update(&container, inputFor(keys, 4000.0, fixture));

  /*
   * Move a row from deep in the list to the front. Its old position would break a search
   * that trusted the layout from before the reflow.
   */
  std::vector<std::string> reordered = keys;
  std::string moved = reordered[250];
  reordered.erase(reordered.begin() + 250);
  reordered.insert(reordered.begin(), moved);

  Virtualizer::update(&container, inputFor(reordered, 4000.0, fixture));
  checkNoRowLost(container, "reorder frame");
  checkGeometryContiguous(container, "reorder frame");
}

// Geometry integrity

TEST(geometry_stays_contiguous_across_scrolling_and_measurement) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(400);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  std::vector<double> heights = unevenHeights(keys.size());
  for (std::size_t index = 0; index < heights.size(); ++index) {
    Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, heights[index]});
    if (index % 25 == 0) {
      Virtualizer::update(&container, inputFor(keys, static_cast<double>(index) * 13.0, fixture));
    }
  }

  checkGeometryContiguous(container, "after interleaved measure/scroll");
  for (std::size_t index = 0; index < heights.size(); ++index) {
    CHECK_NEAR(sizeOf(container, index), heights[index], 0.001);
  }
}

/*
 * Reporting a size a row already has must not move anything. Every layout reports every
 * mounted row, so unchanged sizes are the common case.
 */
TEST(repeated_identical_measurements_do_not_move_geometry) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  std::vector<double> heights = unevenHeights(keys.size());
  measureRows(container, heights);
  Virtualizer::update(&container, inputFor(keys, 900.0, fixture));

  std::vector<double> offsetsBefore;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    offsetsBefore.push_back(offsetOf(container, index));
  }
  double offsetBefore = container.revision.containerOffsetY;
  double totalBefore = container.revision.totalContainerHeight;

  for (int repeat = 0; repeat < 3; ++repeat) {
    for (std::size_t index = 0; index < keys.size(); ++index) {
      Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, heights[index]});
    }
  }
  Virtualizer::recomputeTotalSize(&container);

  for (std::size_t index = 0; index < keys.size(); ++index) {
    CHECK_NEAR(offsetOf(container, index), offsetsBefore[index], 0.001);
  }
  CHECK_NEAR(container.revision.containerOffsetY, offsetBefore, 0.001);
  CHECK_NEAR(container.revision.totalContainerHeight, totalBefore, 0.001);
}

TEST(a_genuine_resize_shifts_only_the_rows_after_it) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  std::vector<double> before;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    before.push_back(offsetOf(container, index));
  }

  Virtualizer::updateElementAtIndex(&container, 40, {WINDOW_WIDTH, 300.0});

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
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  FrameInput withHeader = inputFor(keys, 0.0, fixture);
  withHeader.headerSize = 250.0;
  withHeader.footerSize = 60.0;
  Virtualizer::update(&container, withHeader);

  CHECK_NEAR(offsetOf(container, 0), 250.0, 0.001);
  CHECK_NEAR(offsetOf(container, 49), 250.0 + 49.0 * 100.0, 0.001);
  CHECK_NEAR(container.revision.totalContainerHeight, 250.0 + 50.0 * 100.0 + 60.0, 0.001);
  checkGeometryContiguous(container, "with header");
}

// Reconciliation

TEST(reconcile_preserves_measured_sizes_of_surviving_rows) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

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

  Virtualizer::update(&container, inputFor(next, 0.0, fixture));

  for (std::size_t index = 0; index < keys.size(); ++index) {
    if (index == 30 || index == 31) {
      CHECK_EQ(container.findElementIndexByKey(keys[index]), UNDEFINED_INDEX);
      continue;
    }
    std::size_t liveIndex = container.findElementIndexByKey(keys[index]);
    CHECK(liveIndex != UNDEFINED_INDEX);
    CHECK(container.revision.elements[liveIndex].measured);
    CHECK_NEAR(sizeOf(container, liveIndex), heights[index], 0.001);
  }
  checkGeometryContiguous(container, "after mixed reconcile");
}

TEST(key_index_map_matches_the_element_list_after_every_mutation) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  auto checkMap = [&](const std::string& context) {
    for (std::size_t index = 0; index < container.revision.elements.size(); ++index) {
      const std::string& key = container.revision.elements[index].key;
      std::size_t found = container.findElementIndexByKey(key);
      // A duplicate key resolves to its first occurrence, so only check that direction.
      if (found > index) {
        fail(context + ": key '" + key + "' at " + std::to_string(index) +
          " resolves to " + std::to_string(found));
      }
      CHECK_EQ(container.revision.elements[found].key, key);
    }
  };
  checkMap("initial");

  std::vector<std::string> reversed(keys.rbegin(), keys.rend());
  Virtualizer::update(&container, inputFor(reversed, 0.0, fixture));
  checkMap("after full reverse");

  std::vector<std::string> trimmed(keys.begin(), keys.begin() + 10);
  Virtualizer::update(&container, inputFor(trimmed, 0.0, fixture));
  checkMap("after trim");
  CHECK_EQ(container.getElementsSize(), static_cast<std::size_t>(10));

  std::vector<std::string> replaced = keysFor(30, "fresh");
  Virtualizer::update(&container, inputFor(replaced, 0.0, fixture));
  checkMap("after full replacement");

  Virtualizer::update(&container, inputFor({}, 0.0, fixture));
  CHECK_EQ(container.getElementsSize(), static_cast<std::size_t>(0));
}

/*
 * Two rows with the same key must not share one measured row. The first keeps its size
 * and the second starts fresh.
 */
TEST(duplicate_keys_do_not_share_one_element) {
  Fixture fixture;
  std::vector<std::string> keys = {"a", "b", "c"};
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, {100.0, 200.0, 300.0});

  std::vector<std::string> withDuplicate = {"a", "b", "b", "c"};
  Virtualizer::update(&container, inputFor(withDuplicate, 0.0, fixture));

  CHECK_EQ(container.getElementsSize(), static_cast<std::size_t>(4));
  CHECK_EQ(container.revision.elements[0].key, std::string("a"));
  CHECK_EQ(container.revision.elements[1].key, std::string("b"));
  CHECK_EQ(container.revision.elements[2].key, std::string("b"));
  CHECK_EQ(container.revision.elements[3].key, std::string("c"));
  // The old row keeps its size and the new one does not take it.
  CHECK(container.revision.elements[1].measured);
  CHECK(!container.revision.elements[2].measured);
  CHECK_EQ(container.findElementIndexByKey("b"), static_cast<std::size_t>(1));
  checkGeometryContiguous(container, "with duplicate key");
}

TEST(full_replacement_resets_the_frozen_average) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 500.0));
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  CHECK(container.revision.averageElementHeight > 0.0);

  std::vector<std::string> replaced = keysFor(40, "fresh");
  Virtualizer::update(&container, inputFor(replaced, 0.0, fixture));
  CHECK_EQ(container.revision.measuredRealCount, static_cast<std::size_t>(0));
  CHECK_NEAR(container.revision.averageElementHeight, 0.0, 0.001);
}

// Scroll position

TEST(prepend_keeps_the_visible_row_in_place) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  // Rest on row 80.
  double offset = offsetOf(container, 80);
  Virtualizer::update(&container, inputFor(keys, offset, fixture));
  CHECK_NEAR(offsetOf(container, 80), container.revision.containerOffsetY, 1.0);

  std::vector<std::string> prepended = keysFor(30, "older");
  prepended.insert(prepended.end(), keys.begin(), keys.end());
  Virtualizer::update(&container, inputFor(prepended, offset, fixture));

  std::size_t movedIndex = container.findElementIndexByKey("k80");
  CHECK(movedIndex != UNDEFINED_INDEX);
  CHECK_EQ(movedIndex, static_cast<std::size_t>(110));
  // The same row must still be at the top of the screen.
  CHECK_NEAR(offsetOf(container, movedIndex), container.revision.containerOffsetY, 1.0);
}

namespace {

/*
 * Prepend while resting at offset 0 with a header, like the Feed screen. The first row sits
 * below the top by the header size, but it must hold just like it does mid list: the new
 * rows land above the screen and the offset grows by their height.
 */
void checkPrependHoldsFirstVisibleRow(const Fixture& fixture, double headerSize, std::size_t restIndex) {
  std::vector<std::string> keys = keysFor(200);
  Container container;
  auto frame = [&](const std::vector<std::string>& frameKeys, double offset) {
    FrameInput input = inputFor(frameKeys, offset, fixture);
    input.headerSize = headerSize;
    Virtualizer::update(&container, input);
  };
  auto currentOffset = [&]() {
    return fixture.horizontal ? container.revision.containerOffsetX : container.revision.containerOffsetY;
  };

  auto measureAll = [&](std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
      Size size = fixture.horizontal ? Size{100.0, WINDOW_HEIGHT} : Size{WINDOW_WIDTH, 100.0};
      Virtualizer::updateElementAtIndex(&container, index, size);
    }
  };

  frame(keys, 0.0);
  measureAll(keys.size());
  frame(keys, 0.0);

  double offset = restIndex == 0 ? 0.0 : offsetOf(container, restIndex);
  frame(keys, offset);
  CHECK_NEAR(currentOffset(), offset, 1.0);
  double restKeyScreenPosition = offsetOf(container, restIndex) - offset;

  std::vector<std::string> prepended = keysFor(10, "older");
  prepended.insert(prepended.end(), keys.begin(), keys.end());
  frame(prepended, offset);

  std::size_t movedIndex = container.findElementIndexByKey("k" + std::to_string(restIndex));
  CHECK_EQ(movedIndex, restIndex + 10);
  // The row stays where it was on screen, and the offset grew by the new rows.
  CHECK_NEAR(offsetOf(container, movedIndex) - currentOffset(), restKeyScreenPosition, 1.0);
  CHECK(container.containerOffsetCorrected);

  // The host applies the new offset and measures the new rows, and nothing moves.
  double corrected = currentOffset();
  FrameInput confirm = inputFor(prepended, corrected, fixture);
  confirm.headerSize = headerSize;
  confirm.containerOffsetEnabled = true;
  Virtualizer::update(&container, confirm);
  measureAll(prepended.size());
  CHECK_NEAR(offsetOf(container, movedIndex) - currentOffset(), restKeyScreenPosition, 1.0);
}

}

TEST(prepend_at_the_top_keeps_the_first_row_in_place) {
  checkPrependHoldsFirstVisibleRow(Fixture{}, 0.0, 0);
}

TEST(prepend_at_the_top_below_a_header_keeps_the_first_row_in_place) {
  checkPrependHoldsFirstVisibleRow(Fixture{}, 150.0, 0);
}

TEST(prepend_mid_list_below_a_header_keeps_the_visible_row_in_place) {
  checkPrependHoldsFirstVisibleRow(Fixture{}, 150.0, 80);
}

TEST(prepend_at_the_start_of_a_horizontal_list_keeps_the_first_column_in_place) {
  Fixture fixture;
  fixture.horizontal = true;
  fixture.estimatedWidth = 120.0;
  fixture.estimatedHeight = WINDOW_HEIGHT;
  checkPrependHoldsFirstVisibleRow(fixture, 150.0, 0);
}

TEST(inverted_list_opens_pinned_to_the_bottom) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(150);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  // Report the offset back each frame, like the host does, until it settles at the bottom.
  double offset = 0.0;
  for (int frame = 0; frame < 6; ++frame) {
    FrameInput input = inputFor(keys, offset, fixture);
    input.containerOffsetEnabled = false;
    Virtualizer::update(&container, input);
    offset = container.revision.containerOffsetY;
  }

  double maxOffset = container.revision.totalContainerHeight - WINDOW_HEIGHT;
  CHECK_NEAR(offset, maxOffset, 1.0);
  checkNoRowLost(container, "inverted at rest");
}

/*
 * Tapping the status bar on an inverted list jumps from the bottom to 0 in one step.
 * The mounted rows must follow the new offset.
 */
TEST(inverted_list_jump_to_top_reports_the_first_rows) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(400);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  double offset = 0.0;
  for (int frame = 0; frame < 6; ++frame) {
    Virtualizer::update(&container, inputFor(keys, offset, fixture));
    offset = container.revision.containerOffsetY;
  }

  FrameInput jump = inputFor(keys, 0.0, fixture);
  jump.userScrolled = true;
  jump.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(&container, jump);

  auto window = reportedWindow(container);
  CHECK(window.first != UNDEFINED_INDEX);
  CHECK_EQ(window.first, static_cast<std::size_t>(0));
  checkNoRowLost(container, "inverted after jump to top");

  // The jump must stick and not be pulled back to the bottom.
  FrameInput settle = inputFor(keys, 0.0, fixture);
  Virtualizer::update(&container, settle);
  CHECK_NEAR(container.revision.containerOffsetY, 0.0, 1.0);
}

namespace {

/*
 * Settle an inverted list at its bottom by reporting the offset back each frame, like the
 * host does. Returns the resting offset.
 */
double settleAtBottom(
  Container& container,
  const std::vector<std::string>& keys,
  const Fixture& fixture,
  const std::vector<std::string>& nonAnchorable = {}) {
  double offset = 0.0;
  for (int frame = 0; frame < 8; ++frame) {
    FrameInput input = inputFor(keys, offset, fixture);
    input.nonAnchorableKeys = nonAnchorable;
    Virtualizer::update(&container, input);
    offset = container.revision.containerOffsetY;
  }
  return offset;
}

}

/*
 * A streaming reply taller than the screen sits at the bottom of an inverted list. When the
 * user drags up into it, the list must stop sticking to the bottom and stay that way while
 * the row grows, or the view snaps back to the bottom.
 */
TEST(inverted_bottom_pin_releases_when_the_user_scrolls_up_a_tall_last_row) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  std::vector<double> heights(keys.size(), 100.0);
  heights.back() = 3000.0;
  measureRows(container, heights);

  double bottom = settleAtBottom(container, keys, fixture);
  CHECK_NEAR(bottom, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);

  // The finger drags 400 points up, still inside the tall last row.
  double dragged = bottom - 400.0;
  FrameInput drag = inputFor(keys, dragged, fixture);
  drag.userScrolled = true;
  Virtualizer::update(&container, drag);
  CHECK_NEAR(container.revision.containerOffsetY, dragged, 1.0);

  // The reply keeps streaming while the finger rests, and the view must not move.
  for (int flush = 1; flush <= 5; ++flush) {
    Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, 3000.0 + flush * 60.0});
    Virtualizer::update(&container, inputFor(keys, container.revision.containerOffsetY, fixture));
  }
  CHECK_NEAR(container.revision.containerOffsetY, dragged, 1.0);
}

/*
 * The chat setup where only the newest row can be an anchor. A user drag must still stop
 * the list sticking to the bottom, even though that row is the only anchor on screen.
 */
TEST(inverted_bottom_pin_releases_when_only_the_last_row_is_anchorable) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(40);
  std::vector<std::string> allButLast(keys.begin(), keys.end() - 1);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  double bottom = settleAtBottom(container, keys, fixture, allButLast);

  double dragged = bottom - 300.0;
  FrameInput drag = inputFor(keys, dragged, fixture);
  drag.userScrolled = true;
  drag.nonAnchorableKeys = allButLast;
  Virtualizer::update(&container, drag);
  CHECK_NEAR(container.revision.containerOffsetY, dragged, 1.0);

  for (int flush = 1; flush <= 5; ++flush) {
    Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, 100.0 + flush * 80.0});
    FrameInput rest = inputFor(keys, container.revision.containerOffsetY, fixture);
    rest.nonAnchorableKeys = allButLast;
    Virtualizer::update(&container, rest);
  }
  CHECK_NEAR(container.revision.containerOffsetY, dragged, 1.0);
}

/*
 * Letting go of the bottom is not permanent. Once the user scrolls back down, growth of the
 * last row is followed again.
 */
TEST(inverted_bottom_pin_reengages_once_the_user_returns_to_the_bottom) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  std::vector<double> heights(keys.size(), 100.0);
  heights.back() = 3000.0;
  measureRows(container, heights);

  double bottom = settleAtBottom(container, keys, fixture);

  FrameInput away = inputFor(keys, bottom - 400.0, fixture);
  away.userScrolled = true;
  Virtualizer::update(&container, away);
  // Without this check the test would pass even if the bottom was never let go.
  CHECK(container.invertedBottomReleased);

  FrameInput back = inputFor(keys, bottom, fixture);
  back.userScrolled = true;
  Virtualizer::update(&container, back);
  CHECK(!container.invertedBottomReleased);

  Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, 3300.0});
  double offset = container.revision.containerOffsetY;
  for (int frame = 0; frame < 4; ++frame) {
    Virtualizer::update(&container, inputFor(keys, offset, fixture));
    offset = container.revision.containerOffsetY;
  }
  CHECK_NEAR(offset, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);
}

/*
 * A 3 point nudge, like a stray touch or the scroll view settling after a bounce, does not
 * mean the reader left the bottom. Treating it that way would strand them for the whole reply.
 */
TEST(inverted_bottom_pin_ignores_a_nudge_off_the_bottom) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  std::vector<std::string> allButLast(keys.begin(), keys.end() - 1);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  double bottom = settleAtBottom(container, keys, fixture, allButLast);

  FrameInput nudge = inputFor(keys, bottom - 3.0, fixture);
  nudge.userScrolled = true;
  nudge.nonAnchorableKeys = allButLast;
  Virtualizer::update(&container, nudge);
  CHECK(!container.invertedBottomReleased);

  // Still following, so the view moves with the growing last row.
  for (int flush = 1; flush <= 4; ++flush) {
    Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, 100.0 + flush * 120.0});
    FrameInput rest = inputFor(keys, container.revision.containerOffsetY, fixture);
    rest.nonAnchorableKeys = allButLast;
    Virtualizer::update(&container, rest);
  }
  CHECK_NEAR(container.revision.containerOffsetY, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);
}

/*
 * A conversation shorter than the screen has its bottom at offset 0, so an overscroll bounce
 * looks far above it. Letting go there would stop following before the reply even fills the screen.
 */
TEST(inverted_bottom_pin_survives_a_bounce_on_a_list_shorter_than_the_viewport) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(3);
  std::vector<std::string> allButLast(keys.begin(), keys.end() - 1);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  settleAtBottom(container, keys, fixture, allButLast);

  FrameInput bounce = inputFor(keys, -40.0, fixture);
  bounce.userScrolled = true;
  bounce.nonAnchorableKeys = allButLast;
  Virtualizer::update(&container, bounce);
  CHECK(!container.invertedBottomReleased);

  // The reply then grows past the screen, and its new bottom must be followed.
  for (int flush = 1; flush <= 6; ++flush) {
    Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, 100.0 + flush * 300.0});
    FrameInput rest = inputFor(keys, container.revision.containerOffsetY, fixture);
    rest.nonAnchorableKeys = allButLast;
    Virtualizer::update(&container, rest);
  }
  CHECK_NEAR(container.revision.containerOffsetY, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);
}

/*
 * A reader scrolled up into a reply, which then shrinks until its bottom reaches them, like a
 * code block closing. The reader did not go back to the bottom, so the list must keep not
 * following, or the next token pulls them to the end of the reply they were reading.
 */
TEST(inverted_bottom_pin_stays_released_when_the_reply_shrinks_onto_the_reader) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  std::vector<double> heights(keys.size(), 100.0);
  heights.back() = 3000.0;
  measureRows(container, heights);
  // Height of everything above the reply, so the shrink can stop exactly at the reader.
  double headRows = 100.0 * static_cast<double>(keys.size() - 1);

  double bottom = settleAtBottom(container, keys, fixture);
  double parked = bottom - 400.0;
  FrameInput drag = inputFor(keys, parked, fixture);
  drag.userScrolled = true;
  Virtualizer::update(&container, drag);
  CHECK(container.invertedBottomReleased);

  // Shrink the last row until the bottom lands exactly on the reader's offset.
  double shrunk = parked + WINDOW_HEIGHT - headRows;
  Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, shrunk});
  Virtualizer::update(&container, inputFor(keys, container.revision.containerOffsetY, fixture));
  CHECK(container.invertedBottomReleased);

  for (int flush = 1; flush <= 4; ++flush) {
    Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, shrunk + flush * 150.0});
    Virtualizer::update(&container, inputFor(keys, container.revision.containerOffsetY, fixture));
  }
  CHECK_NEAR(container.revision.containerOffsetY, parked, 1.0);
}

/*
 * Regenerating a reply the reader scrolled past. The reply empties, the bottom rises above the
 * reader, the host clamps the offset to it as a user scroll, then the core's correction moves
 * a few points toward the bottom. That move is the core's own write coming back, not the reader
 * returning, so the list must keep not following while the reply streams back in.
 */
TEST(inverted_bottom_pin_stays_released_when_a_correction_echo_nears_the_bottom) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  std::vector<double> heights(keys.size(), 100.0);
  heights.back() = 3000.0;
  measureRows(container, heights);

  double bottom = settleAtBottom(container, keys, fixture);
  double parked = bottom - 400.0;
  FrameInput drag = inputFor(keys, parked, fixture);
  drag.userScrolled = true;
  Virtualizer::update(&container, drag);
  CHECK(container.invertedBottomReleased);

  /*
   * The reply loses 1400 points, so the new bottom is 1000 points above the reader. Computed
   * here because the stored total only updates on the next frame.
   */
  const double emptied = 1600.0;
  Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, emptied});
  double shrunkBottom = bottom - (3000.0 - emptied);
  CHECK(shrunkBottom < parked);

  // The host clamps just short of the new bottom, then reports the core's nudge onto it.
  FrameInput clamp = inputFor(keys, shrunkBottom - 20.0, fixture);
  clamp.userScrolled = true;
  Virtualizer::update(&container, clamp);
  FrameInput echo = inputFor(keys, shrunkBottom, fixture);
  Virtualizer::update(&container, echo);
  CHECK(container.invertedBottomReleased);

  // The reply streams back in and the reader stays where the clamp left them.
  double rested = container.revision.containerOffsetY;
  for (int flush = 1; flush <= 4; ++flush) {
    Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, emptied + flush * 150.0});
    Virtualizer::update(&container, inputFor(keys, container.revision.containerOffsetY, fixture));
  }
  CHECK_NEAR(container.revision.containerOffsetY, rested, 1.0);
}

/*
 * Edge callbacks follow the data order in every layout. Inverted rests at the end but does not
 * swap the edges. An inverted chat at its bottom is at the end of the data, so onStartReached
 * must not fire there, or each load of older rows would fire it again every frame.
 */
TEST(inverted_list_reports_end_at_the_bottom_and_start_at_the_top) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  int startReached = 0;
  int endReached = 0;
  container.onStartReachedCallback = [&]() { startReached++; };
  container.onEndReachedCallback = [&]() { endReached++; };

  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);
  CHECK_NEAR(bottom, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);

  int startAtBottom = startReached;
  CHECK(endReached >= 1);

  // Rows prepended while resting at the bottom do not fire the start edge.
  for (int round = 0; round < 3; ++round) {
    std::vector<std::string> grown = keysFor(6, "older" + std::to_string(round) + "_");
    grown.insert(grown.end(), keys.begin(), keys.end());
    keys = grown;
    FrameInput input = inputFor(keys, container.revision.containerOffsetY, fixture);
    Virtualizer::update(&container, input);
    Virtualizer::update(&container, inputFor(keys, container.revision.containerOffsetY, fixture));
  }
  CHECK_EQ(startReached, startAtBottom);

  // Scrolling to the top fires the start edge.
  FrameInput top = inputFor(keys, 0.0, fixture);
  top.userScrolled = true;
  Virtualizer::update(&container, top);
  CHECK(startReached > startAtBottom);
}

/*
 * A slow drag away from the bottom starts inside INVERTED_FOLLOW_BAND, where the list still
 * follows the bottom. While the finger is down, including commits between touch frames, the
 * list must not pull the view back. Lifting inside the band hands the view back to the bottom.
 */
TEST(inverted_bottom_pin_yields_while_a_finger_is_down_inside_the_band) {
  Fixture fixture;
  fixture.inverted = true;

  // Like the chat, only the newest row can be an anchor.
  std::vector<std::string> keys = keysFor(20);
  std::vector<std::string> allButLast(keys.begin(), keys.end() - 1);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture, allButLast);

  double offset = bottom;
  for (int frame = 0; frame < 8; ++frame) {
    offset -= 2.0;
    FrameInput drag = inputFor(keys, offset, fixture);
    drag.nonAnchorableKeys = allButLast;
    drag.userScrolled = true;
    drag.scrollPhase = ScrollPhase::Dragging;
    Virtualizer::update(&container, drag);
    CHECK_NEAR(container.revision.containerOffsetY, offset, 0.01);

    // A commit between two touch frames reports the same offset with the finger still down.
    FrameInput commit = inputFor(keys, offset, fixture);
    commit.nonAnchorableKeys = allButLast;
    commit.userScrolled = true;
    commit.scrollPhase = ScrollPhase::Dragging;
    Virtualizer::update(&container, commit);
    CHECK_NEAR(container.revision.containerOffsetY, offset, 0.01);
  }
  CHECK(!container.invertedBottomReleased);

  // The finger lifts 16 points above the bottom, inside the band, so the view goes back to the bottom.
  FrameInput lift = inputFor(keys, offset, fixture);
  lift.nonAnchorableKeys = allButLast;
  Virtualizer::update(&container, lift);
  for (int frame = 0; frame < 4; ++frame) {
    FrameInput rest = inputFor(keys, container.revision.containerOffsetY, fixture);
    rest.nonAnchorableKeys = allButLast;
    Virtualizer::update(&container, rest);
  }
  CHECK_NEAR(container.revision.containerOffsetY, bottom, 1.0);
}

/*
 * Growth measured between frames is followed right away. The reply's footer mounts when the
 * stream ends and is measured after the last commit. With no frame after that, the measuring
 * step itself must move the view to the new bottom, or the last row stays cut off.
 */
TEST(inverted_bottom_pin_follows_growth_measured_between_frames) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  std::vector<std::string> allButLast(keys.begin(), keys.end() - 1);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture, allButLast);
  CHECK_NEAR(bottom, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);

  // The newest row grows by 40 points with no frame, and the view is already at the new bottom.
  Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, 140.0});
  CHECK_NEAR(container.revision.containerOffsetY, bottom + 40.0, 0.01);
  CHECK(container.containerOffsetCorrected);

  // A reader who scrolled away is left alone.
  FrameInput away = inputFor(keys, bottom - 300.0, fixture);
  away.nonAnchorableKeys = allButLast;
  away.userScrolled = true;
  Virtualizer::update(&container, away);
  CHECK(container.invertedBottomReleased);
  double parked = container.revision.containerOffsetY;
  Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, 200.0});
  CHECK_NEAR(container.revision.containerOffsetY, parked, 0.01);
}

TEST(scroll_to_index_lands_on_the_requested_row) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(500);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  container.scrollToIndex(321);

  double offset = 0.0;
  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(&container, inputFor(keys, offset, fixture));
    offset = container.revision.containerOffsetY;
  }

  std::size_t targetIndex = container.findElementIndexByKey("k321");
  CHECK_NEAR(offset, offsetOf(container, targetIndex), 1.0);
  checkNoRowLost(container, "after scrollToIndex");
}

TEST(content_shrinking_below_the_offset_pulls_the_view_back) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(300);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  double deepOffset = 20000.0;
  Virtualizer::update(&container, inputFor(keys, deepOffset, fixture));

  std::vector<std::string> collapsed(keys.begin(), keys.begin() + 20);
  double offset = deepOffset;
  for (int frame = 0; frame < 6; ++frame) {
    Virtualizer::update(&container, inputFor(collapsed, offset, fixture));
    offset = container.revision.containerOffsetY;
  }

  double maxOffset = std::max(0.0, container.revision.totalContainerHeight - WINDOW_HEIGHT);
  CHECK(offset <= maxOffset + 1.0);
  checkNoRowLost(container, "after collapse");
}

// Derived geometry

TEST(snap_offsets_track_geometry_changes) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(120);
  Container container;
  FrameInput input = inputFor(keys, 0.0, fixture);
  input.snapToItem = true;
  Virtualizer::update(&container, input);
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  Virtualizer::update(&container, input);

  std::vector<double> first = container.getSnapOffsets();
  CHECK(!first.empty());
  // Snap points go up and start at the first row.
  for (std::size_t index = 1; index < first.size(); ++index) {
    CHECK(first[index] > first[index - 1]);
  }
  CHECK_NEAR(first.front(), 0.0, 0.001);

  // The same frame again gives the same snap points.
  Virtualizer::update(&container, input);
  CHECK(container.getSnapOffsets() == first);

  // A real resize changes them.
  Virtualizer::updateElementAtIndex(&container, 3, {WINDOW_WIDTH, 700.0});
  Virtualizer::recomputeTotalSize(&container);
  std::vector<double> afterResize = container.getSnapOffsets();
  CHECK(afterResize != first);
  CHECK_NEAR(afterResize[4], offsetOf(container, 4), 0.001);

  // So does turning snapping off.
  FrameInput noSnap = inputFor(keys, 0.0, fixture);
  Virtualizer::update(&container, noSnap);
  CHECK(container.getSnapOffsets().empty());
}

TEST(snap_offsets_survive_a_pure_scroll) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(80);
  Container container;
  FrameInput input = inputFor(keys, 0.0, fixture);
  input.snapToItem = true;
  Virtualizer::update(&container, input);
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  Virtualizer::update(&container, input);

  std::vector<double> before = container.getSnapOffsets();
  FrameInput scrolled = inputFor(keys, 2500.0, fixture);
  scrolled.snapToItem = true;
  scrolled.userScrolled = true;
  Virtualizer::update(&container, scrolled);
  CHECK(container.getSnapOffsets() == before);
}

TEST(viewable_indices_stay_inside_the_viewport) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  container.onViewableIndicesChangeCallback = [](std::size_t, std::size_t) {};

  FrameInput input = inputFor(keys, 0.0, fixture);
  input.viewablePercentThreshold = 0.5;
  Virtualizer::update(&container, input);
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  FrameInput scrolled = inputFor(keys, 1000.0, fixture);
  scrolled.viewablePercentThreshold = 0.5;
  Virtualizer::update(&container, scrolled);

  auto viewable = container.getViewableIndices();
  CHECK(viewable.first != UNDEFINED_INDEX);
  double viewportStart = container.revision.containerOffsetY;
  double viewportEnd = viewportStart + WINDOW_HEIGHT;
  for (std::size_t index = viewable.first; index <= viewable.second; ++index) {
    double start = offsetOf(container, index);
    double end = start + sizeOf(container, index);
    CHECK(end > viewportStart);
    CHECK(start < viewportEnd);
  }
}

// Estimation

/*
 * Unmeasured rows use the current fallback size and pick up a new one when the average or
 * the estimate changes. This is what lets the sizing step be skipped when nothing changed.
 */
TEST(unmeasured_rows_track_the_current_fallback_size) {
  Fixture fixture;
  fixture.estimatedHeight = 120.0;

  std::vector<std::string> keys = keysFor(500);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  // Nothing is measured yet, so every far row uses the estimate.
  for (std::size_t index = 200; index < 500; ++index) {
    CHECK_NEAR(sizeOf(container, index), 120.0, 0.001);
  }

  // Measure the first screen of rows much taller, and the average becomes the fallback.
  for (std::size_t index = 0; index < 20; ++index) {
    Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, 400.0});
  }
  /*
   * Two frames on purpose. Sizing runs before recomputeTotalSize fixes the average, so the
   * new average only reaches unmeasured rows on the second frame.
   */
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  CHECK_NEAR(container.revision.averageElementHeight, 400.0, 0.001);
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  for (std::size_t index = 200; index < 500; ++index) {
    CHECK(!container.revision.elements[index].measured);
    CHECK_NEAR(sizeOf(container, index), 400.0, 0.001);
  }
  checkGeometryContiguous(container, "after average froze");

  // Repeating the settled frame leaves everything exactly where it is.
  double totalBefore = container.revision.totalContainerHeight;
  double lastOffsetBefore = offsetOf(container, 499);
  for (int repeat = 0; repeat < 3; ++repeat) {
    Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  }
  CHECK_NEAR(container.revision.totalContainerHeight, totalBefore, 0.001);
  CHECK_NEAR(offsetOf(container, 499), lastOffsetBefore, 0.001);
}

TEST(newly_inserted_rows_get_a_fallback_size_immediately) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 250.0));
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  std::vector<std::string> grown = keys;
  for (std::size_t index = 0; index < 50; ++index) {
    grown.push_back("new" + std::to_string(index));
  }
  Virtualizer::update(&container, inputFor(grown, 0.0, fixture));

  for (std::size_t index = 100; index < 150; ++index) {
    CHECK(sizeOf(container, index) > 0.0);
  }
  checkGeometryContiguous(container, "after insert");
  CHECK(container.revision.totalContainerHeight > 100.0 * 250.0);
}

TEST(multi_column_rows_span_their_track) {
  Fixture fixture;
  fixture.columns = 4;

  std::vector<std::string> keys = keysFor(80);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  double trackSize = WINDOW_WIDTH / 4.0;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    CHECK_NEAR(container.revision.elements[index].width, trackSize, 0.001);
    CHECK_NEAR(container.revision.elements[index].offsetX,
      static_cast<double>(index % 4) * trackSize, 0.001);
  }
  checkGeometryContiguous(container, "4 columns");

  // The content width must cover all columns and never collapse.
  CHECK(container.revision.totalContainerWidth >= WINDOW_WIDTH - 0.001);
}

TEST(empty_then_refilled_list_recovers) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  Virtualizer::update(&container, inputFor({}, 0.0, fixture));
  CHECK_EQ(container.getElementsSize(), static_cast<std::size_t>(0));
  CHECK_NEAR(container.revision.totalContainerHeight, 0.0, 0.001);

  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  CHECK_EQ(container.getElementsSize(), static_cast<std::size_t>(100));
  checkNoRowLost(container, "after refill");
  checkGeometryContiguous(container, "after refill");
  CHECK(container.revision.totalContainerHeight > 0.0);
}

/*
 * A long random session of scrolls, measures, inserts, removes and moves, in a fixed order
 * from a seed, checking everything after each step. It catches mixes the tests above miss.
 */
TEST(randomized_session_never_loses_a_row) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(250);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  std::uint64_t seed = 0x5DEECE66Dull;
  auto nextRandom = [&]() {
    seed = seed * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::size_t>((seed >> 33) & 0xFFFFFFFFull);
  };

  double offset = 0.0;
  std::size_t freshCounter = 0;
  /*
   * Whether the mounted range matches the current layout. Sizes arrive after the commit
   * that made the range, so after a resize it is one frame behind, just like on device.
   * Only check the range when the host would have a fresh one.
   */
  bool windowFresh = true;

  for (int step = 0; step < 400; ++step) {
    switch (nextRandom() % 5) {
      case 0: {  // scroll somewhere
        double total = container.revision.totalContainerHeight;
        offset = total > 0.0 ? static_cast<double>(nextRandom() % 100000) / 100000.0 * total : 0.0;
        FrameInput input = inputFor(keys, offset, fixture);
        input.userScrolled = true;
        input.scrollPhase = ScrollPhase::Dragging;
        Virtualizer::update(&container, input);
        windowFresh = true;
        break;
      }
      case 1: {  // measure a mounted row
        if (container.getElementsSize() == 0) {
          break;
        }
        std::size_t index = nextRandom() % container.getElementsSize();
        double height = static_cast<double>(nextRandom() % 400);
        Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, height});
        Virtualizer::recomputeTotalSize(&container);
        windowFresh = false;
        break;
      }
      case 2: {  // insert a page somewhere
        std::size_t at = keys.empty() ? 0 : nextRandom() % keys.size();
        std::vector<std::string> inserted;
        for (std::size_t index = 0; index < 5; ++index) {
          inserted.push_back("fresh" + std::to_string(freshCounter++));
        }
        keys.insert(keys.begin() + static_cast<long>(at), inserted.begin(), inserted.end());
        Virtualizer::update(&container, inputFor(keys, offset, fixture));
        windowFresh = true;
        break;
      }
      case 3: {  // remove a page
        if (keys.size() < 20) {
          break;
        }
        std::size_t at = nextRandom() % (keys.size() - 10);
        keys.erase(keys.begin() + static_cast<long>(at), keys.begin() + static_cast<long>(at) + 8);
        Virtualizer::update(&container, inputFor(keys, offset, fixture));
        windowFresh = true;
        break;
      }
      default: {  // move a row
        if (keys.size() < 4) {
          break;
        }
        std::size_t from = nextRandom() % keys.size();
        std::size_t to = nextRandom() % keys.size();
        std::string moved = keys[from];
        keys.erase(keys.begin() + static_cast<long>(from));
        keys.insert(keys.begin() + static_cast<long>(std::min(to, keys.size())), moved);
        Virtualizer::update(&container, inputFor(keys, offset, fixture));
        windowFresh = true;
        break;
      }
    }

    std::string context = "randomized step " + std::to_string(step);
    CHECK_EQ(container.getElementsSize(), keys.size());
    checkGeometryContiguous(container, context);
    if (windowFresh) {
      checkNoRowLost(container, context);
    }
  }
}

/*
 * A prepend that lands while a fling is still settling must keep the visible content in place.
 * Every commit runs update() twice on the same old scroll report. If the second run took the
 * settling phase for a new gesture, it would drop the correction the first run started and the
 * new rows would push the content down.
 * The correction must survive the second run, move along with momentum frames reported before
 * the host applied it, and give the offset back to the gesture once the host reports its token.
 */
TEST(prepend_while_settling_keeps_visible_content_in_place) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  for (int frame = 0; frame < 3; ++frame) {
    Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  }

  // The fling reaches the top area and is still slowing down.
  FrameInput coasting = inputFor(keys, 37.0, fixture);
  coasting.userScrolled = true;
  coasting.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, coasting);
  CHECK_NEAR(container.revision.containerOffsetY, 37.0, 0.5);

  // Ten unmeasured rows are prepended, and both runs of the commit see the old report.
  std::vector<std::string> grown = keysFor(10, "fresh");
  grown.insert(grown.end(), keys.begin(), keys.end());
  // Unmeasured rows use the fallback size from the measured rows.
  double inserted = 10 * 100.0;

  FrameInput prepend = inputFor(grown, 37.0, fixture);
  prepend.userScrolled = true;
  prepend.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, prepend);
  CHECK_NEAR(container.revision.containerOffsetY, 37.0 + inserted, 1.0);

  Virtualizer::update(&container, prepend);
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, 37.0 + inserted, 1.0);
  CHECK(container.operation.has_value());

  // A momentum frame reported before the host applied the correction moves the target along.
  FrameInput momentum = inputFor(grown, 21.0, fixture);
  momentum.userScrolled = true;
  momentum.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, momentum);
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, 21.0 + inserted, 1.0);
  CHECK(container.operation.has_value());

  // The host applies it and reports the token back, so the gesture owns the offset again.
  std::uint64_t token = container.operation ? container.operation->id : 0;
  FrameInput echo = inputFor(grown, 21.0 + inserted, fixture);
  echo.scrollPhase = ScrollPhase::Settling;
  echo.commitToken = token;
  Virtualizer::update(&container, echo);
  CHECK(!container.containerOffsetCorrected);
  CHECK(!container.operation.has_value());
  CHECK_NEAR(container.revision.containerOffsetY, 21.0 + inserted, 1.0);

  // Momentum carries on from there and is not pulled back.
  FrameInput carryOn = inputFor(grown, 5.0 + inserted, fixture);
  carryOn.userScrolled = true;
  carryOn.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, carryOn);
  CHECK(!container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, 5.0 + inserted, 1.0);

  /*
   * The old first row was 37 pixels past the top, and two momentum frames moved the view up
   * 16 pixels each, so it is now 5 pixels past it. The prepend moved nothing.
   */
  std::size_t firstOld = container.findElementIndexByKey("k0");
  CHECK_NEAR(container.revision.containerOffsetY - offsetOf(container, firstOld), 5.0, 1.0);
  checkNoRowLost(container, "prepend while settling");
}

/*
 * With followAppends, an inverted list resting at its bottom follows appended rows. The new
 * rows measure taller than the estimate after they land, and the view still ends at the real bottom.
 */
TEST(inverted_list_at_the_bottom_follows_appended_rows) {
  Fixture fixture;
  fixture.inverted = true;
  fixture.followAppends = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);
  CHECK_NEAR(bottom, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);

  std::vector<std::string> grown = keys;
  for (std::size_t index = 0; index < 3; ++index) {
    grown.push_back("appended" + std::to_string(index));
  }

  double offset = bottom;
  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(&container, inputFor(grown, offset, fixture));
    if (frame == 1) {
      for (std::size_t index = keys.size(); index < grown.size(); ++index) {
        Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, 180.0});
      }
    }
    offset = container.revision.containerOffsetY;
  }

  double newBottom = container.revision.totalContainerHeight - WINDOW_HEIGHT;
  CHECK_NEAR(newBottom, bottom + 3 * 180.0, 1.0);
  CHECK_NEAR(offset, newBottom, 1.0);
  CHECK(!container.operation.has_value());
}

/*
 * By default an append keeps what the reader at the bottom is looking at, like any insert.
 * The new rows land below the screen and nothing on screen moves.
 */
TEST(inverted_list_at_the_bottom_holds_appended_rows_by_default) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);

  std::vector<std::string> grown = keys;
  for (std::size_t index = 0; index < 3; ++index) {
    grown.push_back("appended" + std::to_string(index));
  }
  double offset = bottom;
  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(&container, inputFor(grown, offset, fixture));
    if (frame == 1) {
      for (std::size_t index = keys.size(); index < grown.size(); ++index) {
        Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, 180.0});
      }
    }
    offset = container.revision.containerOffsetY;
    CHECK_NEAR(offset, bottom, 0.01);
  }
  CHECK(!container.pendingScrollToEnd);
  CHECK(!container.operation.has_value());
}

/*
 * The same append while the reader has scrolled up leaves them where they are. The rows
 * land off screen below.
 */
TEST(inverted_list_scrolled_up_holds_when_rows_are_appended) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);

  double parked = bottom - 500.0;
  FrameInput drag = inputFor(keys, parked, fixture);
  drag.userScrolled = true;
  drag.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(&container, drag);
  Virtualizer::update(&container, inputFor(keys, parked, fixture));
  CHECK(container.invertedBottomReleased);

  std::vector<std::string> grown = keys;
  grown.push_back("appended");
  for (int frame = 0; frame < 4; ++frame) {
    Virtualizer::update(&container, inputFor(grown, parked, fixture));
    CHECK_NEAR(container.revision.containerOffsetY, parked, 0.01);
  }
  CHECK(!container.pendingScrollToEnd);
}

/*
 * A chat composer grows a line at a time as the message wraps, shrinking the list from the
 * bottom. A reader at the bottom stays at the bottom. Otherwise the newest rows slide under the
 * composer and the next sent message lands off screen. The composer shrinking back after the
 * send keeps the bottom too.
 */
TEST(inverted_list_at_the_bottom_keeps_it_when_the_viewport_resizes) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double offset = settleAtBottom(container, keys, fixture);

  auto frame = [&](const std::vector<std::string>& frameKeys, double windowHeight) {
    FrameInput input = inputFor(frameKeys, offset, fixture);
    input.windowContainerHeight = windowHeight;
    Virtualizer::update(&container, input);
    offset = container.revision.containerOffsetY;
  };

  // A page of history lands at the end of the opening settle, as it does on device.
  keys.insert(keys.begin(), "earlier");
  for (int settle = 0; settle < 6; ++settle) {
    frame(keys, WINDOW_HEIGHT);
    if (settle == 1) {
      Virtualizer::updateElementAtIndex(&container, 0, {WINDOW_WIDTH, 100.0});
    }
  }
  CHECK(!container.invertedOpeningPin);
  CHECK(!container.pendingScrollToEnd);

  double windowHeight = WINDOW_HEIGHT;
  for (int line = 0; line < 3; ++line) {
    windowHeight -= 22.0;
    frame(keys, windowHeight);
    frame(keys, windowHeight);
    CHECK_NEAR(offset, container.revision.totalContainerHeight - windowHeight, 1.0);
  }

  fixture.followAppends = true;
  std::vector<std::string> grown = keys;
  grown.push_back("sent");
  for (int settle = 0; settle < 6; ++settle) {
    frame(grown, windowHeight);
    if (settle == 1) {
      Virtualizer::updateElementAtIndex(&container, grown.size() - 1, {WINDOW_WIDTH, 100.0});
    }
  }
  CHECK_NEAR(offset, container.revision.totalContainerHeight - windowHeight, 1.0);

  for (int settle = 0; settle < 4; ++settle) {
    frame(grown, WINDOW_HEIGHT);
  }
  CHECK_NEAR(offset, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);
  CHECK(!container.operation.has_value());
  CHECK(!container.pendingScrollToEnd);
}

// The same resize for a reader who scrolled up moves nothing on screen.
TEST(inverted_list_scrolled_up_holds_when_the_viewport_resizes) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);

  double parked = bottom - 500.0;
  FrameInput drag = inputFor(keys, parked, fixture);
  drag.userScrolled = true;
  drag.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(&container, drag);
  Virtualizer::update(&container, inputFor(keys, parked, fixture));

  for (int frame = 0; frame < 4; ++frame) {
    FrameInput input = inputFor(keys, parked, fixture);
    input.windowContainerHeight = WINDOW_HEIGHT - 66.0;
    Virtualizer::update(&container, input);
    CHECK_NEAR(container.revision.containerOffsetY, parked, 0.01);
  }
  CHECK(!container.pendingScrollToEnd);
}

/*
 * A scrollToEnd sent while a fling is still coasting must still run. Momentum is not the
 * reader taking over, so even with momentum frames moving the offset the view lands on the
 * bottom. Only a finger cancels it.
 */
TEST(scroll_to_end_requested_during_momentum_lands_on_the_bottom) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  double maxOffset = container.revision.totalContainerHeight - WINDOW_HEIGHT;

  FrameInput coasting = inputFor(keys, 300.0, fixture);
  coasting.userScrolled = true;
  coasting.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, coasting);

  // The command arrives on top of the last momentum report.
  container.requestScrollToIndex(SCROLL_TO_END_INDEX, 1.0, -2);
  FrameInput command = inputFor(keys, 300.0, fixture);
  command.userScrolled = true;
  command.scrollPhase = ScrollPhase::Settling;
  command.containerOffsetEnabled = true;
  Virtualizer::update(&container, command);
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, maxOffset, 1.0);

  FrameInput momentum = inputFor(keys, 340.0, fixture);
  momentum.userScrolled = true;
  momentum.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, momentum);
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, maxOffset, 1.0);

  std::uint64_t token = container.operation ? container.operation->id : 0;
  CHECK(token != 0);
  FrameInput echo = inputFor(keys, maxOffset, fixture);
  echo.commitToken = token;
  Virtualizer::update(&container, echo);
  Virtualizer::update(&container, inputFor(keys, maxOffset, fixture));
  CHECK(!container.operation.has_value());
  CHECK(!container.pendingScrollToEnd);
  CHECK_NEAR(container.revision.containerOffsetY, maxOffset, 1.0);
}

TEST(a_drag_cancels_a_scroll_command_but_momentum_does_not) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  container.requestScrollToIndex(120.0, 1.0, -2);
  FrameInput command = inputFor(keys, 500.0, fixture);
  command.userScrolled = true;
  command.scrollPhase = ScrollPhase::Settling;
  command.containerOffsetEnabled = true;
  Virtualizer::update(&container, command);
  double target = offsetOf(container, 120);
  CHECK_NEAR(container.revision.containerOffsetY, target, 1.0);

  FrameInput momentum = inputFor(keys, 520.0, fixture);
  momentum.userScrolled = true;
  momentum.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, momentum);
  CHECK(container.operation.has_value());
  CHECK_NEAR(container.revision.containerOffsetY, target, 1.0);

  FrameInput drag = inputFor(keys, 540.0, fixture);
  drag.userScrolled = true;
  drag.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(&container, drag);
  CHECK(!container.operation.has_value());
  CHECK(!container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, 540.0, 0.01);
}

namespace {

/*
 * Report the core's offset back like the host does, clamped to the scrollable range, until
 * it stops moving. Returns the resting offset.
 */
double settleReportingClamped(
  Container& container,
  const std::vector<std::string>& keys,
  const Fixture& fixture,
  double offset) {
  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(&container, inputFor(keys, offset, fixture));
    double maxOffset = std::max(0.0, container.revision.totalContainerHeight - WINDOW_HEIGHT);
    offset = std::min(std::max(container.revision.containerOffsetY, 0.0), maxOffset);
  }
  return offset;
}

}

/*
 * Queue predicted sizes for rows that were still estimated when the list settled. Rows above
 * the top row shrink and one below it grows. The core applies them in the next update after
 * picking that frame's anchor, which is how size specs arrive on a list that just opened.
 */
void predictOpeningRemeasure(Container& container, const std::vector<std::string>& keys) {
  for (std::size_t index = 20; index < 30; ++index) {
    container.setPredictedSize(keys[index], {WINDOW_WIDTH, 100.0});
  }
  container.setPredictedSize(keys[38], {WINDOW_WIDTH, 160.0});
}

/*
 * An inverted list stays at its real bottom while rows are measured again after opening.
 * Holding the top row instead would leave it short of the bottom, and then it stops following
 * new messages.
 */
TEST(inverted_list_stays_on_the_bottom_while_opening_rows_are_remeasured) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  double bottom = settleAtBottom(container, keys, fixture);
  CHECK_NEAR(bottom, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);

  predictOpeningRemeasure(container, keys);
  double offset = settleReportingClamped(container, keys, fixture, bottom);
  CHECK_NEAR(offset, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);
}

/*
 * Once the reader has touched the list, the same remeasure holds the top row still instead.
 * A screen that stops following on purpose, like an assistant rewriting an older reply, relies on it.
 */
TEST(inverted_list_after_a_gesture_holds_the_viewport_top_row_through_a_remeasure) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  double bottom = settleAtBottom(container, keys, fixture);

  FrameInput touch = inputFor(keys, bottom, fixture);
  touch.userScrolled = true;
  touch.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(&container, touch);
  Virtualizer::update(&container, inputFor(keys, bottom, fixture));

  std::size_t anchorRow = static_cast<std::size_t>(bottom / ESTIMATED_ROW_HEIGHT);
  double anchorDelta = bottom - offsetOf(container, anchorRow);

  predictOpeningRemeasure(container, keys);
  double offset = settleReportingClamped(container, keys, fixture, bottom);
  CHECK_NEAR(offset, offsetOf(container, anchorRow) + anchorDelta, 1.0);
  CHECK(offset < container.revision.totalContainerHeight - WINDOW_HEIGHT - INVERTED_FOLLOW_BAND);
}

/*
 * The top row is often cut by the top of the screen, with only its lower part showing. When it
 * is first measured, the rows below it hold still and the change is absorbed above the screen.
 * Holding its top edge instead would move every visible row by the estimate's error.
 */
TEST(first_measurement_of_a_straddling_anchor_row_keeps_the_rows_below_in_place) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  double straddling = 5 * ESTIMATED_ROW_HEIGHT + 100.0;
  FrameInput scroll = inputFor(keys, straddling, fixture);
  scroll.userScrolled = true;
  scroll.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(&container, scroll);
  Virtualizer::update(&container, inputFor(keys, straddling, fixture));
  CHECK(!container.operation.has_value());

  double belowOnScreen = offsetOf(container, 6) - straddling;
  Virtualizer::updateElementAtIndex(&container, 5, {WINDOW_WIDTH, 60.0});
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(offsetOf(container, 6) - container.revision.containerOffsetY, belowOnScreen, 0.5);
}

// Prepend while a fling settles at the top edge

namespace {

/*
 * A header like the Feed screen's. It is what puts the top of the screen inside the last
 * prepended row once the correction lands.
 */
constexpr double TOP_EDGE_HEADER = 79.0;
constexpr double TOP_EDGE_ROW_HEIGHT = 100.0;

/*
 * A host scroll report with its offset, gesture phase and the last token it reported back.
 */
FrameInput topEdgeReport(
  const std::vector<std::string>& keys,
  double offset,
  const Fixture& fixture,
  ScrollPhase phase,
  std::uint64_t echoedToken) {
  FrameInput input = inputFor(keys, offset, fixture);
  input.headerSize = TOP_EDGE_HEADER;
  input.userScrolled = phase != ScrollPhase::Idle;
  input.scrollPhase = phase;
  input.commitToken = echoedToken;
  return input;
}

/*
 * The next commit after a correction: the core's own offset and token, plus the gesture
 * fields of the report it came from.
 */
FrameInput publishedCorrection(const Container& container, const FrameInput& report) {
  FrameInput input = report;
  input.containerOffsetY = container.revision.containerOffsetY;
  input.containerOffsetEnabled = true;
  input.commitToken = container.operation ? container.operation->id : 0;
  return input;
}

/*
 * A list resting at its top with every row measured.
 */
void openAtTheTop(Container& container, const std::vector<std::string>& keys, const Fixture& fixture) {
  Virtualizer::update(&container, topEdgeReport(keys, 0.0, fixture, ScrollPhase::Idle, 0));
  measureRows(container, std::vector<double>(keys.size(), TOP_EDGE_ROW_HEIGHT));
  for (int frame = 0; frame < 3; ++frame) {
    Virtualizer::update(&container, topEdgeReport(keys, 0.0, fixture, ScrollPhase::Idle, 0));
  }
}

std::vector<std::string> prependedTo(const std::vector<std::string>& keys, const std::string& prefix) {
  std::vector<std::string> grown = keysFor(10, prefix);
  grown.insert(grown.end(), keys.begin(), keys.end());
  return grown;
}

/*
 * How far below the top of the screen the row with this key starts.
 */
double belowViewportTop(const Container& container, const std::string& key) {
  return offsetOf(container, container.findElementIndexByKey(key)) - container.revision.containerOffsetY;
}

/*
 * Mount rows like Fabric does after a prepend. Each row first reports zero height before its
 * content lays out, then the layout pass reports the real sizes in one batch.
 */
void mountRows(Container& container, const std::vector<std::string>& keys, double height) {
  for (const std::string& key : keys) {
    Virtualizer::updateElementAtIndex(&container, container.findElementIndexByKey(key), {WINDOW_WIDTH, 0.0});
  }
  std::size_t lowestChangedIndex = UNDEFINED_INDEX;
  for (const std::string& key : keys) {
    std::size_t index = container.findElementIndexByKey(key);
    if (Virtualizer::applyElementSize(&container, index, {WINDOW_WIDTH, height}) && index < lowestChangedIndex) {
      lowestChangedIndex = index;
    }
  }
  if (lowestChangedIndex != UNDEFINED_INDEX) {
    Virtualizer::commitElementSizes(&container, lowestChangedIndex);
  }
  Virtualizer::recomputeTotalSize(&container);
}

std::vector<std::string> keyRange(const std::vector<std::string>& keys, std::size_t from, std::size_t to) {
  return std::vector<std::string>(keys.begin() + static_cast<std::ptrdiff_t>(from), keys.begin() + static_cast<std::ptrdiff_t>(to));
}

}

/*
 * A prepend while a fling is still bouncing at the top edge. The commit that applies the
 * correction runs update() before the host has moved anything, and it carries the correction's
 * own token. Taking that for the host's report back would end the correction too early, and
 * the bounce back would no longer move the target along.
 */
TEST(prepend_while_bouncing_at_the_top_edge_survives_its_own_offset_write) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  openAtTheTop(container, keys, fixture);

  for (double offset : {40.0, 0.0, -12.0}) {
    Virtualizer::update(&container, topEdgeReport(keys, offset, fixture, ScrollPhase::Settling, 0));
  }
  double firstRowBelowTop = belowViewportTop(container, "k0");
  CHECK_NEAR(firstRowBelowTop, TOP_EDGE_HEADER + 12.0, 0.5);

  std::vector<std::string> grown = prependedTo(keys, "fresh");
  FrameInput prepend = topEdgeReport(grown, -12.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(&container, prepend);
  Virtualizer::update(&container, prepend);
  CHECK(container.operation.has_value());
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  std::uint64_t token = container.operation ? container.operation->id : 0;

  Virtualizer::update(&container, publishedCorrection(container, prepend));
  CHECK(container.operation.has_value());
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);

  // The bounce moves the view 12 pixels down to the edge before the host applies the offset.
  FrameInput spring = topEdgeReport(grown, 0.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(&container, spring);
  CHECK(container.operation.has_value());
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop - 12.0, 0.5);
  Virtualizer::update(&container, publishedCorrection(container, spring));
  CHECK_NEAR(belowViewportTop(container, "k0"), TOP_EDGE_HEADER, 0.5);

  double applied = container.revision.containerOffsetY;
  Virtualizer::update(&container, topEdgeReport(grown, applied, fixture, ScrollPhase::Settling, token));
  CHECK(!container.operation.has_value());
  Virtualizer::update(&container, topEdgeReport(grown, applied, fixture, ScrollPhase::Idle, token));
  CHECK(!container.containerOffsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), TOP_EDGE_HEADER, 0.5);
  checkNoRowLost(container, "prepend while bouncing at the top edge");
}

/*
 * The prepended rows mount while the list still settles and measure far from their estimate,
 * some before the host applies the correction and the rest after. Each first lays out at zero.
 * The row the reader was looking at holds its place through all of it, even once nothing is
 * running and the top of the screen is inside an unmeasured new row.
 */
TEST(prepend_while_bouncing_holds_while_the_new_rows_measure_unlike_their_estimate) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  openAtTheTop(container, keys, fixture);

  Virtualizer::update(&container, topEdgeReport(keys, 30.0, fixture, ScrollPhase::Settling, 0));
  Virtualizer::update(&container, topEdgeReport(keys, 0.0, fixture, ScrollPhase::Settling, 0));
  double firstRowBelowTop = belowViewportTop(container, "k0");

  std::vector<std::string> grown = prependedTo(keys, "fresh");
  std::vector<std::string> fresh = keysFor(10, "fresh");
  FrameInput prepend = topEdgeReport(grown, 0.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(&container, prepend);
  Virtualizer::update(&container, prepend);
  std::uint64_t token = container.operation ? container.operation->id : 0;
  CHECK(token != 0);
  Virtualizer::update(&container, publishedCorrection(container, prepend));

  mountRows(container, keyRange(fresh, 0, 5), 160.0);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  Virtualizer::update(&container, publishedCorrection(container, prepend));
  CHECK(container.operation.has_value());
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);

  double applied = container.revision.containerOffsetY;
  Virtualizer::update(&container, topEdgeReport(grown, applied, fixture, ScrollPhase::Settling, token));
  CHECK(!container.operation.has_value());

  mountRows(container, keyRange(fresh, 5, 10), 60.0);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  Virtualizer::update(&container, publishedCorrection(container, prepend));
  double rest = container.revision.containerOffsetY;
  Virtualizer::update(&container, topEdgeReport(grown, rest, fixture, ScrollPhase::Idle, token));
  Virtualizer::update(&container, topEdgeReport(grown, rest, fixture, ScrollPhase::Idle, token));
  CHECK(!container.containerOffsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  checkNoRowLost(container, "prepend while bouncing with remeasured rows");
}

/*
 * Two quick prepends while the list bounces at the top edge, the second before the host applied
 * the first correction. Both share one correction, and the reader's row holds as both batches mount.
 */
TEST(two_prepends_in_quick_succession_while_bouncing_hold_the_first_row) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  openAtTheTop(container, keys, fixture);

  Virtualizer::update(&container, topEdgeReport(keys, 40.0, fixture, ScrollPhase::Settling, 0));
  Virtualizer::update(&container, topEdgeReport(keys, 0.0, fixture, ScrollPhase::Settling, 0));
  double firstRowBelowTop = belowViewportTop(container, "k0");

  std::vector<std::string> first = prependedTo(keys, "fresh");
  FrameInput firstPrepend = topEdgeReport(first, 0.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(&container, firstPrepend);
  Virtualizer::update(&container, firstPrepend);
  std::uint64_t token = container.operation ? container.operation->id : 0;
  CHECK(token != 0);
  Virtualizer::update(&container, publishedCorrection(container, firstPrepend));

  std::vector<std::string> second = prependedTo(first, "older");
  FrameInput secondPrepend = topEdgeReport(second, 0.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(&container, secondPrepend);
  Virtualizer::update(&container, secondPrepend);
  CHECK(container.operation.has_value());
  CHECK(container.operation && container.operation->id == token);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  Virtualizer::update(&container, publishedCorrection(container, secondPrepend));

  mountRows(container, keysFor(10, "older"), 140.0);
  mountRows(container, keysFor(10, "fresh"), 90.0);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  Virtualizer::update(&container, publishedCorrection(container, secondPrepend));
  CHECK(container.operation.has_value());

  double applied = container.revision.containerOffsetY;
  Virtualizer::update(&container, topEdgeReport(second, applied, fixture, ScrollPhase::Settling, token));
  CHECK(!container.operation.has_value());
  Virtualizer::update(&container, topEdgeReport(second, applied, fixture, ScrollPhase::Idle, token));
  CHECK(!container.containerOffsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  checkNoRowLost(container, "two prepends in quick succession while bouncing");
}

/*
 * The bounce ends after the prepend but before the host applies the correction, and the host
 * never reports that last bit of movement. The host shifts its current offset by the correction,
 * so its report back lands short of the core's target by that movement. That report confirms the
 * correction. Pushing on to the exact target would undo the movement.
 */
TEST(prepend_whose_bounce_ends_before_the_correction_lands_is_confirmed_by_its_echo) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  openAtTheTop(container, keys, fixture);

  Virtualizer::update(&container, topEdgeReport(keys, 40.0, fixture, ScrollPhase::Settling, 0));
  Virtualizer::update(&container, topEdgeReport(keys, 16.0, fixture, ScrollPhase::Settling, 0));

  std::vector<std::string> grown = prependedTo(keys, "fresh");
  FrameInput prepend = topEdgeReport(grown, 16.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(&container, prepend);
  Virtualizer::update(&container, prepend);
  std::uint64_t token = container.operation ? container.operation->id : 0;
  CHECK(token != 0);
  double firstTarget = container.revision.containerOffsetY;
  Virtualizer::update(&container, publishedCorrection(container, prepend));

  mountRows(container, keysFor(10, "fresh"), 150.0);
  Virtualizer::update(&container, publishedCorrection(container, prepend));
  double retarget = container.revision.containerOffsetY;

  // The host came to rest at the edge, 16 pixels past the report, and shifted by the correction.
  double echoed = 0.0 + (firstTarget - 16.0);
  Virtualizer::update(&container, topEdgeReport(grown, echoed, fixture, ScrollPhase::Idle, token));
  CHECK(!container.operation.has_value());
  CHECK(!container.containerOffsetCorrected);

  // The updated target applies next and moves the view by the difference.
  double rest = echoed + (retarget - firstTarget);
  Virtualizer::update(&container, topEdgeReport(grown, rest, fixture, ScrollPhase::Idle, token));
  Virtualizer::update(&container, topEdgeReport(grown, rest, fixture, ScrollPhase::Idle, token));
  CHECK(!container.containerOffsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), TOP_EDGE_HEADER, 0.5);
  checkNoRowLost(container, "prepend whose bounce ends before the correction lands");
}

/*
 * The bounce ends after the core worked out the correction but before the host applies it, and
 * this time the host reports it as an idle report at the edge. That moves the target like a
 * momentum frame, so the row holds where the reader saw it stop.
 */
TEST(prepend_while_bouncing_follows_the_idle_report_of_the_bounce_ending) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  openAtTheTop(container, keys, fixture);

  Virtualizer::update(&container, topEdgeReport(keys, 50.0, fixture, ScrollPhase::Settling, 0));
  Virtualizer::update(&container, topEdgeReport(keys, 24.0, fixture, ScrollPhase::Settling, 0));

  std::vector<std::string> grown = prependedTo(keys, "fresh");
  FrameInput prepend = topEdgeReport(grown, 24.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(&container, prepend);
  Virtualizer::update(&container, prepend);
  std::uint64_t token = container.operation ? container.operation->id : 0;
  CHECK(token != 0);
  Virtualizer::update(&container, publishedCorrection(container, prepend));

  FrameInput rested = topEdgeReport(grown, 0.0, fixture, ScrollPhase::Idle, 0);
  Virtualizer::update(&container, rested);
  CHECK(container.operation.has_value());
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), TOP_EDGE_HEADER, 0.5);

  double applied = container.revision.containerOffsetY;
  Virtualizer::update(&container, topEdgeReport(grown, applied, fixture, ScrollPhase::Idle, token));
  CHECK(!container.operation.has_value());
  Virtualizer::update(&container, topEdgeReport(grown, applied, fixture, ScrollPhase::Idle, token));
  CHECK(!container.containerOffsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), TOP_EDGE_HEADER, 0.5);
  checkNoRowLost(container, "prepend while bouncing with the bounce ending reported idle");
}

/*
 * A pull to refresh batch lands while the gesture flag is still set and the view rests at the
 * top, then the new rows measure far from their estimate. The host reports back the first offset
 * after the core already moved the target. That report is not movement, and it does not confirm
 * the old target, so the new target stands. Reaching it ends the correction with the reader's row in place.
 */
TEST(prepend_after_a_pull_to_refresh_keeps_its_retarget_through_the_echo_of_the_first_write) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  openAtTheTop(container, keys, fixture);

  FrameInput released = topEdgeReport(keys, 0.0, fixture, ScrollPhase::Idle, 0);
  released.userScrolled = true;
  Virtualizer::update(&container, released);
  double firstRowBelowTop = belowViewportTop(container, "k0");

  std::vector<std::string> grown = prependedTo(keys, "fresh");
  FrameInput refresh = topEdgeReport(grown, 0.0, fixture, ScrollPhase::Idle, 0);
  refresh.userScrolled = true;
  Virtualizer::update(&container, refresh);
  Virtualizer::update(&container, refresh);
  std::uint64_t token = container.operation ? container.operation->id : 0;
  CHECK(token != 0);
  double firstTarget = container.revision.containerOffsetY;
  Virtualizer::update(&container, publishedCorrection(container, refresh));

  mountRows(container, keysFor(10, "fresh"), 60.0);
  Virtualizer::update(&container, publishedCorrection(container, refresh));
  double retarget = container.revision.containerOffsetY;
  CHECK(std::fabs(retarget - firstTarget) > 100.0);

  Virtualizer::update(&container, topEdgeReport(grown, firstTarget, fixture, ScrollPhase::Idle, token));
  CHECK(container.operation.has_value());
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, retarget, 0.5);

  Virtualizer::update(&container, topEdgeReport(grown, retarget, fixture, ScrollPhase::Idle, token));
  Virtualizer::update(&container, topEdgeReport(grown, retarget, fixture, ScrollPhase::Idle, token));
  CHECK(!container.operation.has_value());
  CHECK(!container.containerOffsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  checkNoRowLost(container, "prepend after a pull-to-refresh with a retarget before the echo");
}
