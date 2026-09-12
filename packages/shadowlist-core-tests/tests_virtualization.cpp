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
 * They are deliberately implementation-agnostic (public core API only) so the same file
 * passes against the pre-optimization core.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

constexpr double WINDOW_WIDTH = 390.0;
constexpr double WINDOW_HEIGHT = 840.0;

std::vector<std::string> keysFor(std::size_t count, const std::string& prefix = "k") {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    keys.push_back(prefix + std::to_string(index));
  }
  return keys;
}

struct Fixture {
  std::size_t columns = 1;
  bool horizontal = false;
  bool inverted = false;
  double overscan = 1.0;
  double estimatedWidth = WINDOW_WIDTH;
  double estimatedHeight = 120.0;
};

FrameInput inputFor(const std::vector<std::string>& keys, double offset, const Fixture& fixture) {
  FrameInput input;
  input.keys = keys;
  input.windowContainerWidth = WINDOW_WIDTH;
  input.windowContainerHeight = WINDOW_HEIGHT;
  input.columns = fixture.columns;
  input.horizontal = fixture.horizontal;
  input.inverted = fixture.inverted;
  input.overscan = fixture.overscan;
  input.estimatedElementSize = {fixture.estimatedWidth, fixture.estimatedHeight};
  if (fixture.horizontal) {
    input.containerOffsetX = offset;
  } else {
    input.containerOffsetY = offset;
  }
  return input;
}

double offsetOf(const Container& container, std::size_t index) {
  const Element& element = container.revision.elements[index];
  return container.horizontal ? element.offsetX : element.offsetY;
}

double sizeOf(const Container& container, std::size_t index) {
  const Element& element = container.revision.elements[index];
  return container.horizontal ? element.width : element.height;
}

/*
 * Every index whose geometry actually overlaps the viewport plus overscan, decided the
 * slow, obvious way. This is the oracle the core's window is checked against.
 */
std::set<std::size_t> overlappingIndices(const Container& container) {
  double windowSize = container.horizontal
    ? container.revision.windowContainerWidth
    : container.revision.windowContainerHeight;
  double offset = container.horizontal
    ? container.revision.containerOffsetX
    : container.revision.containerOffsetY;
  double overscanSize = windowSize * container.overscan;
  double lowerBound = offset - overscanSize;
  double upperBound = offset + windowSize + overscanSize;

  std::set<std::size_t> overlapping;
  for (std::size_t index = 0; index < container.revision.elements.size(); ++index) {
    double elementOffset = offsetOf(container, index);
    double elementSize = sizeOf(container, index);
    /*
     * Genuine overlap only. A row whose leading edge sits exactly on the far bound covers
     * zero pixels of the region, so whether a given pass includes it is a tie-break, not a
     * lost row. Requiring strict overlap keeps this an assertion about visible content;
     * the core is free to report a superset.
     */
    if (elementOffset >= upperBound) {
      continue;
    }
    if (elementOffset + elementSize <= lowerBound) {
      continue;
    }
    overlapping.insert(index);
  }
  return overlapping;
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
  std::set<std::size_t> overlapping = overlappingIndices(container);
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
    const Element& element = container.revision.elements[index];
    std::size_t track = columns > 1 ? index % columns : 0;
    double expected = trackEdges[track];
    double actual = offsetOf(container, index);
    if (std::fabs(actual - expected) > 0.001) {
      fail(context + ": row " + std::to_string(index) + " sits at " + std::to_string(actual) +
        " but the running track edge is " + std::to_string(expected));
    }
    trackEdges[track] = actual + sizeOf(container, index) +
      (container.horizontal ? element.gapX : element.gapY);
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
    }
    else if (index % 5 == 0) heights.push_back(420.0);
    else if (index % 3 == 0) heights.push_back(24.0);
    else heights.push_back(96.0 + static_cast<double>(index % 7) * 11.0);
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
