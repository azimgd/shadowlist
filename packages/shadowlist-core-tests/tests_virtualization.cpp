/*
 * Virtualization contract tests.
 *
 * These pin the properties a list must never lose, independently of how the core
 * arrives at them:
 *
 *   * the reported window covers EVERY row that overlaps the viewport plus overscan
 *     (nothing is virtualized away while it is on screen),
 *   * element geometry stays contiguous and ordered after any mutation,
 *   * measured sizes survive reconciliation and the key->index map stays truthful,
 *   * maintain-visible-content-position holds content still across a prepend,
 *   * inverted, horizontal and multi-column layouts obey all of the above.
 *
 * The window assertions are written as an exhaustive cross-check: an independent brute
 * force pass over every element decides which rows overlap, and the core's answer must
 * contain exactly those. That is what makes the fast index seek safe to change --
 * a search that skips a row shows up here as a lost row, not as a subtle visual bug.
 *
 * They are implementation-agnostic (public core API only), so they hold regardless of
 * which search or reflow strategy the core uses internally.
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
 * The core reports its window as a single [start, end] index span (inverted lists report
 * it reversed). Normalise it to the inclusive ascending range the host would mount.
 */
std::pair<std::size_t, std::size_t> reportedWindow(const Container& container) {
  auto visible = container.getVisibleIndices();
  if (visible.first == UNDEFINED_INDEX || visible.second == UNDEFINED_INDEX) {
    return {UNDEFINED_INDEX, UNDEFINED_INDEX};
  }
  return {std::min(visible.first, visible.second), std::max(visible.first, visible.second)};
}

/*
 * The whole point of the window: no row that is on screen (or within the overscan the
 * list promised) may fall outside what the host is told to mount.
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
 * Positions must stay contiguous along the scroll axis: within one track, each row starts
 * exactly where the previous one ended. A reflow that skipped a row shows up here.
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
 * Drive one measured row per index so the list has real, uneven geometry rather than a
 * uniform estimate -- uniform rows hide almost every window-selection bug.
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
    // A deliberately lumpy profile: tall rows, short rows and a few zero-height rows.
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

/* ------------------------------------------------------------------ *
 * Window selection
 * ------------------------------------------------------------------ */

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
 * The hard case for any ordered search: round-robin tracks whose cumulative heights drift
 * far apart, so nearby screen positions belong to distant indices.
 */
TEST(window_covers_every_overlapping_row_skewed_columns) {
  Fixture fixture;
  fixture.columns = 3;

  std::vector<std::string> keys = keysFor(600);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  for (std::size_t index = 0; index < keys.size(); ++index) {
    // Track 0 grows fast, track 1 slowly, track 2 in between: maximally skewed.
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
 * A row whose leading edge is above the window but whose body still covers it must stay
 * in the window. This is the row a leading-edge-only test drops, leaving a visible gap.
 */
TEST(window_keeps_row_straddling_the_lower_bound) {
  Fixture fixture;
  fixture.overscan = 0.0;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  std::vector<double> heights(keys.size(), 100.0);
  heights[10] = 5000.0;  // one row far taller than the viewport
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
   * Move a row from deep in the list to the very front: its stale offset would break any
   * ordered walk that trusted the pre-reflow geometry.
   */
  std::vector<std::string> reordered = keys;
  std::string moved = reordered[250];
  reordered.erase(reordered.begin() + 250);
  reordered.insert(reordered.begin(), moved);

  Virtualizer::update(&container, inputFor(reordered, 4000.0, fixture));
  checkNoRowLost(container, "reorder frame");
  checkGeometryContiguous(container, "reorder frame");
}

/* ------------------------------------------------------------------ *
 * Geometry integrity
 * ------------------------------------------------------------------ */

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
 * Re-reporting a size a row already has must not move anything. The measurement feedback
 * path runs for every mounted row on every layout, so an unchanged report is the common
 * case, not the exception.
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

/* ------------------------------------------------------------------ *
 * Reconciliation
 * ------------------------------------------------------------------ */

TEST(reconcile_preserves_measured_sizes_of_surviving_rows) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  std::vector<double> heights = unevenHeights(keys.size());
  measureRows(container, heights);

  // Prepend a page, drop a couple from the middle, append one.
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
      // First occurrence wins on a duplicate key, so only assert that direction.
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
 * A duplicate key must not let two rows share one measured element: the first occurrence
 * keeps the measured state, the second starts fresh.
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
  // The survivor keeps its measurement; the newcomer must not claim it.
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

/* ------------------------------------------------------------------ *
 * Scroll position
 * ------------------------------------------------------------------ */

TEST(prepend_keeps_the_visible_row_in_place) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  // Sit on row 80.
  double offset = offsetOf(container, 80);
  Virtualizer::update(&container, inputFor(keys, offset, fixture));
  CHECK_NEAR(offsetOf(container, 80), container.revision.containerOffsetY, 1.0);

  std::vector<std::string> prepended = keysFor(30, "older");
  prepended.insert(prepended.end(), keys.begin(), keys.end());
  Virtualizer::update(&container, inputFor(prepended, offset, fixture));

  std::size_t movedIndex = container.findElementIndexByKey("k80");
  CHECK(movedIndex != UNDEFINED_INDEX);
  CHECK_EQ(movedIndex, static_cast<std::size_t>(110));
  // The same row must still sit at the top of the viewport.
  CHECK_NEAR(offsetOf(container, movedIndex), container.revision.containerOffsetY, 1.0);
}

namespace {

/*
 * Prepend while resting at the very top (offset 0) with a list header, the Feed screen's
 * shape. The anchor there is the first row, which sits below the viewport top by the header
 * size, so its captured sub-offset is negative. MVCP must hold it exactly as it does
 * mid-list: the new rows land above the viewport and the offset grows by their height.
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
  // The row keeps its on-screen position: the offset absorbed the inserted rows.
  CHECK_NEAR(offsetOf(container, movedIndex) - currentOffset(), restKeyScreenPosition, 1.0);
  CHECK(container.containerOffsetCorrected);

  // The host confirms the corrected offset; the prepended rows get measured; nothing moves.
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

  // Let the pin converge the way the host does: report the offset back each frame.
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
 * "Tap the status bar" on an inverted list: a single jump from the resting bottom to 0.
 * The window must follow the offset, not stay where the content used to be.
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

  // And the jump must stick: the core must not drag the view back to the bottom.
  FrameInput settle = inputFor(keys, 0.0, fixture);
  Virtualizer::update(&container, settle);
  CHECK_NEAR(container.revision.containerOffsetY, 0.0, 1.0);
}

namespace {

/*
 * Settle an inverted list at its bottom the way the host does: report the offset back each
 * frame until the pin converges. Returns the resting offset.
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
 * A streaming reply taller than the viewport at the bottom of an inverted list. The user
 * drags up into it: the bottom pin must stand down on that frame and stay down while the row
 * keeps growing below, or the drag fights the pin and the view snaps back to the bottom.
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

  // The finger drags 400pt up, still inside the tall last row.
  double dragged = bottom - 400.0;
  FrameInput drag = inputFor(keys, dragged, fixture);
  drag.userScrolled = true;
  Virtualizer::update(&container, drag);
  CHECK_NEAR(container.revision.containerOffsetY, dragged, 1.0);

  // The reply keeps streaming while the finger rests: the view must not move.
  for (int flush = 1; flush <= 5; ++flush) {
    Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, 3000.0 + flush * 60.0});
    Virtualizer::update(&container, inputFor(keys, container.revision.containerOffsetY, fixture));
  }
  CHECK_NEAR(container.revision.containerOffsetY, dragged, 1.0);
}

/*
 * The chat template's following policy: every row but the newest is non-anchorable, so the
 * pin tracks the newest row. A user drag must still release the pin even though the newest
 * row is the only anchor candidate on screen.
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
 * Releasing must not be permanent: once the user scrolls back down to the bottom, growth of
 * the last row is followed again.
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
  // Without this the test passes whether or not the pin was ever released.
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
 * The release must survive noise. A 3pt nudge -- a stray touch while reading, the scroll
 * view settling after a bounce -- is not the reader leaving the stream, and treating it as
 * one strands them off the bottom for the rest of the reply.
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

  // Still following: the last row growing still carries the view with it.
  for (int flush = 1; flush <= 4; ++flush) {
    Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, 100.0 + flush * 120.0});
    FrameInput rest = inputFor(keys, container.revision.containerOffsetY, fixture);
    rest.nonAnchorableKeys = allButLast;
    Virtualizer::update(&container, rest);
  }
  CHECK_NEAR(container.revision.containerOffsetY, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);
}

/*
 * A conversation that does not fill the viewport has no bottom to leave: the bottom offset
 * is clamped to 0, so an overscroll bounce reports an offset "far above" it. Releasing
 * there would latch following off before the reply has even grown past the window.
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

  // The reply then grows past the viewport; the newly created bottom must be followed.
  for (int flush = 1; flush <= 6; ++flush) {
    Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, 100.0 + flush * 300.0});
    FrameInput rest = inputFor(keys, container.revision.containerOffsetY, fixture);
    rest.nonAnchorableKeys = allButLast;
    Virtualizer::update(&container, rest);
  }
  CHECK_NEAR(container.revision.containerOffsetY, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);
}

/*
 * A released reader parked mid-reply, under which the reply SHRINKS (a fence closing, a
 * preview line collapsing) until the bottom is level with them. The bottom came to the
 * reader, the reader did not go to the bottom, so the pin must stay down -- otherwise the
 * next token yanks them to the end of a reply they were still reading.
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
  // Everything above the streaming reply, so the shrink can be aimed at the reader exactly.
  double headRows = 100.0 * static_cast<double>(keys.size() - 1);

  double bottom = settleAtBottom(container, keys, fixture);
  double parked = bottom - 400.0;
  FrameInput drag = inputFor(keys, parked, fixture);
  drag.userScrolled = true;
  Virtualizer::update(&container, drag);
  CHECK(container.invertedBottomReleased);

  // Shrink the last row until maxOffset lands exactly on the parked offset.
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
 * Regenerate on a reply the reader has scrolled past: the reply empties, the bottom rises
 * above the reader, the host clamps the offset down to it (a report the host flags as a user
 * scroll), and the core's correction then moves the view a few points back toward the
 * bottom. That last move is the echo of the core's own write, not the reader returning, so
 * the pin must stay released -- or the reply is chased to the bottom as it streams back in.
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
   * The reply empties by 1400pt: the new bottom lands 1000pt above the reader. Derived rather
   * than read back, because the stored total only refreshes on the next update.
   */
  const double emptied = 1600.0;
  Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, emptied});
  double shrunkBottom = bottom - (3000.0 - emptied);
  CHECK(shrunkBottom < parked);

  // The host clamps short of the new bottom, then echoes the core's nudge onto it.
  FrameInput clamp = inputFor(keys, shrunkBottom - 20.0, fixture);
  clamp.userScrolled = true;
  Virtualizer::update(&container, clamp);
  FrameInput echo = inputFor(keys, shrunkBottom, fixture);
  Virtualizer::update(&container, echo);
  CHECK(container.invertedBottomReleased);

  // The reply streams back in: the reader stays where the clamp left them.
  double rested = container.revision.containerOffsetY;
  for (int flush = 1; flush <= 4; ++flush) {
    Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, emptied + flush * 150.0});
    Virtualizer::update(&container, inputFor(keys, container.revision.containerOffsetY, fixture));
  }
  CHECK_NEAR(container.revision.containerOffsetY, rested, 1.0);
}

/*
 * The edge callbacks follow the data order in every orientation: `inverted` pins the
 * resting position to the end but does not flip which edge is which. An inverted chat
 * resting at its bottom is at the END of the data; onStartReached must not fire there,
 * or the "load earlier" it triggers prepends rows, the new element count re-arms the
 * edge, and the callback fires again every frame for as long as the reader stays put.
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

  // Resting at the bottom while rows are prepended keeps the start edge quiet.
  for (int round = 0; round < 3; ++round) {
    std::vector<std::string> grown = keysFor(6, "older" + std::to_string(round) + "_");
    grown.insert(grown.end(), keys.begin(), keys.end());
    keys = grown;
    FrameInput input = inputFor(keys, container.revision.containerOffsetY, fixture);
    Virtualizer::update(&container, input);
    Virtualizer::update(&container, inputFor(keys, container.revision.containerOffsetY, fixture));
  }
  CHECK_EQ(startReached, startAtBottom);

  // Scrolling to the top reaches the start.
  FrameInput top = inputFor(keys, 0.0, fixture);
  top.userScrolled = true;
  Virtualizer::update(&container, top);
  CHECK(startReached > startAtBottom);
}

/*
 * A slow drag away from the bottom starts inside INVERTED_FOLLOW_BAND, where the pin is
 * not yet released. The host reports the finger as Dragging on those frames, and on the
 * commits that land between touch frames (a stream flush): the pin must yield to all of
 * them, or every frame snaps the content back and the finger fights the pin across the
 * whole band. Lifting inside the band hands the view back to the pin.
 */
TEST(inverted_bottom_pin_yields_while_a_finger_is_down_inside_the_band) {
  Fixture fixture;
  fixture.inverted = true;

  // The chat's following policy: only the newest row may anchor, so the pin tracks it.
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

    // A commit between two touch frames reports the same offset, finger still down.
    FrameInput commit = inputFor(keys, offset, fixture);
    commit.nonAnchorableKeys = allButLast;
    commit.userScrolled = true;
    commit.scrollPhase = ScrollPhase::Dragging;
    Virtualizer::update(&container, commit);
    CHECK_NEAR(container.revision.containerOffsetY, offset, 0.01);
  }
  CHECK(!container.invertedBottomReleased);

  // The finger lifts 16pt above the bottom, inside the band: the pin takes the view back.
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
 * Growth measured between frames is followed at once. The reply's footer mounts when the
 * stream ends and is measured after the final commit; with no further frame, the pin in
 * resolveScroll never runs, so the measurement path itself must move the view to the new
 * bottom rather than leave the reader looking at a cut-off row.
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

  // The newest row grows by 40pt with no frame in between: the view is already at the new bottom.
  Virtualizer::updateElementAtIndex(&container, keys.size() - 1, {WINDOW_WIDTH, 140.0});
  CHECK_NEAR(container.revision.containerOffsetY, bottom + 40.0, 0.01);
  CHECK(container.containerOffsetCorrected);

  // Released readers are left alone by the same path.
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

/* ------------------------------------------------------------------ *
 * Derived geometry
 * ------------------------------------------------------------------ */

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
  // Ascending and starting at the first row's resting position.
  for (std::size_t index = 1; index < first.size(); ++index) {
    CHECK(first[index] > first[index - 1]);
  }
  CHECK_NEAR(first.front(), 0.0, 0.001);

  // Repeating an identical frame must not change the answer.
  Virtualizer::update(&container, input);
  CHECK(container.getSnapOffsets() == first);

  // A real resize must.
  Virtualizer::updateElementAtIndex(&container, 3, {WINDOW_WIDTH, 700.0});
  Virtualizer::recomputeTotalSize(&container);
  std::vector<double> afterResize = container.getSnapOffsets();
  CHECK(afterResize != first);
  CHECK_NEAR(afterResize[4], offsetOf(container, 4), 0.001);

  // So must turning snapping off.
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

/* ------------------------------------------------------------------ *
 * Estimation
 * ------------------------------------------------------------------ */

/*
 * Rows nobody has measured yet must keep carrying the current fallback size, and must
 * pick up a new one when the frozen average or the estimate changes. This is the property
 * that lets the sizing pass be skipped when nothing changed.
 */
TEST(unmeasured_rows_track_the_current_fallback_size) {
  Fixture fixture;
  fixture.estimatedHeight = 120.0;

  std::vector<std::string> keys = keysFor(500);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  // Nothing measured yet: every far-away row carries the estimate.
  for (std::size_t index = 200; index < 500; ++index) {
    CHECK_NEAR(sizeOf(container, index), 120.0, 0.001);
  }

  // Measure the first screenful much taller; the frozen average takes over as fallback.
  for (std::size_t index = 0; index < 20; ++index) {
    Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, 400.0});
  }
  /*
   * Two frames, deliberately: within one measure pass the sizing runs BEFORE
   * recomputeTotalSize freezes the average, so the frame that first sees real
   * measurements still sizes unmeasured rows from the estimate and the new average only
   * reaches them on the next one.
   */
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));
  CHECK_NEAR(container.revision.averageElementHeight, 400.0, 0.001);
  Virtualizer::update(&container, inputFor(keys, 0.0, fixture));

  for (std::size_t index = 200; index < 500; ++index) {
    CHECK(!container.revision.elements[index].measured);
    CHECK_NEAR(sizeOf(container, index), 400.0, 0.001);
  }
  checkGeometryContiguous(container, "after average froze");

  // Repeating the settled frame must leave everything exactly where it is.
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

  // The published content width must cover the tracks, never collapse.
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
 * A long randomized session: scroll, measure, insert, remove and reorder in a fixed
 * pseudorandom order, asserting the whole contract after every single step. This is the
 * net that catches interactions the individual cases above do not think of.
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
   * Whether the reported window still describes the current geometry. Measurement
   * feedback arrives during layout, AFTER the commit that produced the window, so between
   * a resize and the next commit the window is expected to be one frame behind -- exactly
   * as it is in the real pipeline. Only assert the window where the host would have a
   * fresh one.
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
 * A prepend that lands while a fling is still settling (the list has just been flung to the
 * top and is bouncing, or is coasting mid-list) must still hold the visible content in place.
 * Every commit runs update() twice on the same, not-yet-refreshed scroll report. If the
 * second pass read the Settling phase as a fresh gesture takeover, it would drop the MVCP
 * correction the first pass had just started and re-capture the anchor against the
 * already-prepended rows, so the view would stay where it was and the new rows would push
 * the content down.
 *
 * The correction must survive the unmoved re-run, ride along with momentum frames reported
 * before the host applied it, and hand the offset back to the gesture once the host echoes
 * its commit token.
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

  // The fling arrives near the top and is still decelerating.
  FrameInput coasting = inputFor(keys, 37.0, fixture);
  coasting.userScrolled = true;
  coasting.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, coasting);
  CHECK_NEAR(container.revision.containerOffsetY, 37.0, 0.5);

  // Ten unmeasured rows are prepended; both passes of the commit see the stale report.
  std::vector<std::string> grown = keysFor(10, "fresh");
  grown.insert(grown.end(), keys.begin(), keys.end());
  // Unmeasured rows take the measured fallback size (see unmeasured_rows_track_the_current_fallback_size).
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

  // A momentum frame reported before the host applied the correction moves the target with it.
  FrameInput momentum = inputFor(grown, 21.0, fixture);
  momentum.userScrolled = true;
  momentum.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, momentum);
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, 21.0 + inserted, 1.0);
  CHECK(container.operation.has_value());

  // The host applies it and echoes the token: the gesture owns the offset again.
  std::uint64_t token = container.operation ? container.operation->id : 0;
  FrameInput echo = inputFor(grown, 21.0 + inserted, fixture);
  echo.scrollPhase = ScrollPhase::Settling;
  echo.commitToken = token;
  Virtualizer::update(&container, echo);
  CHECK(!container.containerOffsetCorrected);
  CHECK(!container.operation.has_value());
  CHECK_NEAR(container.revision.containerOffsetY, 21.0 + inserted, 1.0);

  // Momentum carries on from there without being pulled back.
  FrameInput carryOn = inputFor(grown, 5.0 + inserted, fixture);
  carryOn.userScrolled = true;
  carryOn.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, carryOn);
  CHECK(!container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, 5.0 + inserted, 1.0);

  /*
   * The old first row sat 37 px past the viewport start; the two momentum frames carried the
   * view 16 px up each, so it now sits 5 px past it -- the prepended rows moved nothing.
   */
  std::size_t firstOld = container.findElementIndexByKey("k0");
  CHECK_NEAR(container.revision.containerOffsetY - offsetOf(container, firstOld), 5.0, 1.0);
  checkNoRowLost(container, "prepend while settling");
}

/*
 * With followAppends, an inverted list resting at its bottom follows the rows appended below
 * the newest one, with no anchor policy: the new rows are measured taller than the estimate
 * after they land, and the view still ends on the true bottom rather than leaving them
 * below the fold.
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
 * By default an append keeps what the reader at the bottom is looking at, like any other
 * insert: the new rows land below the fold, measured or not, and nothing on screen moves.
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
 * The same append while the reader has scrolled up the conversation leaves them where they
 * are: the rows land below, off screen, and nothing on screen moves.
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
 * A chat's composer grows a line at a time as the message wraps, shrinking the list's
 * viewport from the bottom. A reader resting at the bottom keeps it: without that the offset
 * stays put, the newest rows slide under the composer, and after a few lines the reader sits
 * outside the follow band, so the message they send next lands below the fold. The composer
 * shrinking back after the send keeps the bottom too.
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

  // A page of history ends the opening settle, as it does on a device.
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

/*
 * The same resize under a reader who scrolled up the conversation moves nothing on screen.
 */
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
 * A scrollToEnd issued while a fling is still coasting must run. Momentum is not a reader
 * taking over: the command's own frame still carries the settling phase, and momentum frames
 * the host reported before it applied the command move the offset, yet the view lands on the
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

  // The command arrives through state on top of the last momentum report.
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
 * Report the core's offset back the way the host does, clamped to the scrollable range the
 * host's content size allows, until it stops moving. Returns the resting offset.
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
 * Stage the sizes the host predicts for rows that were still on the estimate when the list
 * settled: rows above the viewport-top row shrink and one below it grows. The core consumes
 * them inside the next update, after it has captured that frame's anchor, which is how
 * size specs arrive on a list that has just opened.
 */
void predictOpeningRemeasure(Container& container, const std::vector<std::string>& keys) {
  for (std::size_t index = 20; index < 30; ++index) {
    container.setPredictedSize(keys[index], {WINDOW_WIDTH, 100.0});
  }
  container.setPredictedSize(keys[38], {WINDOW_WIDTH, 160.0});
}

/*
 * An inverted list keeps its true bottom through the remeasure that follows opening. Holding
 * the row at the viewport top instead parks the view short of the bottom, and a list that
 * is not resting at its bottom stops following appended messages.
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
 * Once the reader has touched the list, the same remeasure is plain anchoring again: the row
 * at the viewport top holds still. A screen that stops following on purpose (an assistant
 * rewriting an older reply) relies on that.
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
 * The row at the viewport top often straddles it: only its trailing part is on screen.
 * When that row is measured for the first time, the rows below it are what the reader is
 * looking at, so they hold still and the correction is absorbed above the viewport. Holding
 * the row's leading edge instead would move every visible row by the estimate's error.
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

/* ------------------------------------------------------------------ *
 * Prepend while a fling settles at the top edge
 * ------------------------------------------------------------------ */

namespace {

/*
 * A header like the Feed screen's. Below the content start, it is what puts the viewport top
 * inside the last prepended row once the correction lands.
 */
constexpr double TOP_EDGE_HEADER = 79.0;
constexpr double TOP_EDGE_ROW_HEIGHT = 100.0;

// A host scroll report: where the host is, the gesture phase, and the last token it echoed.
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
 * The state the layout pass publishes for a correction, as the next commit adopts it: the core's
 * own offset write and commit token, with the gesture fields of the report it was built from.
 */
FrameInput publishedCorrection(const Container& container, const FrameInput& report) {
  FrameInput input = report;
  input.containerOffsetY = container.revision.containerOffsetY;
  input.containerOffsetEnabled = true;
  input.commitToken = container.operation ? container.operation->id : 0;
  return input;
}

// A list resting at its top with every row measured.
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

// How far below the viewport top the row with this key starts.
double belowViewportTop(const Container& container, const std::string& key) {
  return offsetOf(container, container.findElementIndexByKey(key)) - container.revision.containerOffsetY;
}

/*
 * Mount rows the way Fabric does after a prepend: each mounting child first reports the zero
 * frame it has before its content lays out, then the layout pass feeds back the real sizes in
 * one batch.
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
 * A prepend committed while a fling is still settling against the top edge, against a report at
 * or above the content start. The layout pass publishes the correction, and the commit that
 * adopts that state runs update() on it before the host has applied anything: it carries the
 * operation's own token and the settling phase of the report. Taking it for the host's echo
 * would release the correction before the view moved, and the spring-back the host reports next
 * would no longer carry the target along.
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

  // The spring carries the view 12 px down to the edge before the host applies the write.
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
 * The prepended rows mount while the list still settles, and measure unlike their estimate: some
 * before the host applies the correction, the rest after its echo released it. Each mounting row
 * first lays out to zero. The row the reader was looking at holds its place through all of it,
 * including once nothing is in flight and the viewport top still sits inside an unmeasured new row.
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
 * Two batches prepended in quick succession while the list settles at the top edge: the second
 * is committed before the host applied the first correction. Both land on one operation, and the
 * row the reader was looking at holds through both batches mounting.
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
 * The bounce ends between the report a prepend was committed against and the moment the host
 * applies the correction, and the report of that last bit of travel is superseded by the echo.
 * The host shifted its live offset by the correction, so the echo lands short of the core's
 * target by that travel and arrives with the motion already stopped; a retarget computed before
 * the echo, from the new rows measuring, is applied the same way. The echo confirms the
 * correction: driving the view on to the absolute target would put the travel back.
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

  // The host had come to rest at the edge, 16 px past the report, and shifted by the correction.
  double echoed = 0.0 + (firstTarget - 16.0);
  Virtualizer::update(&container, topEdgeReport(grown, echoed, fixture, ScrollPhase::Idle, token));
  CHECK(!container.operation.has_value());
  CHECK(!container.containerOffsetCorrected);

  // The retarget mounts next and moves the view by what it changed.
  double rest = echoed + (retarget - firstTarget);
  Virtualizer::update(&container, topEdgeReport(grown, rest, fixture, ScrollPhase::Idle, token));
  Virtualizer::update(&container, topEdgeReport(grown, rest, fixture, ScrollPhase::Idle, token));
  CHECK(!container.containerOffsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), TOP_EDGE_HEADER, 0.5);
  checkNoRowLost(container, "prepend whose bounce ends before the correction lands");
}

/*
 * The bounce ends after the core computed the correction but before the host applies it, and
 * this time the core does see the host's report of it: an idle report of the live offset at
 * the edge. That travel moves the correction's target just like a momentum frame, so the
 * correction the host applies holds the row where the reader saw it come to rest.
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
 * A pull-to-refresh batch lands on a report whose gesture flag is still set while the view rests
 * at the top, and the new rows then measure unlike their estimate. The host echoes the first
 * correction from where that write put it, after the retarget was computed. The echo is neither
 * travel nor a confirmation of a target the core has moved on from: the retarget stands, and the
 * report of the view reaching it releases the correction with the reader's row in place.
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
