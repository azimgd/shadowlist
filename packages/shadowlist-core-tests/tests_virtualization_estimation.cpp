/*
 * Virtualization tests for derived geometry and size estimation: snap offsets, fallback
 * sizes, averages and predictions.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"
#include "VirtualizationHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

// Derived geometry

TEST(snap_offsets_track_geometry_changes) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(120);
  Container container;
  FrameInput input = inputFor(keys, 0.0, fixture);
  input.snapToItem = true;
  Virtualizer::update(container, input);
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  Virtualizer::update(container, input);

  std::vector<double> first = container.getSnapOffsets();
  CHECK(!first.empty());
  // Snap points go up and start at the first row.
  for (std::size_t index = 1; index < first.size(); ++index) {
    CHECK(first[index] > first[index - 1]);
  }
  CHECK_NEAR(first.front(), 0.0, 0.001);

  // The same frame again gives the same snap points.
  Virtualizer::update(container, input);
  CHECK(container.getSnapOffsets() == first);

  // A real resize changes them.
  Virtualizer::updateElementAtIndex(container, 3, {WINDOW_WIDTH, 700.0});
  Virtualizer::recomputeTotalSize(container);
  std::vector<double> afterResize = container.getSnapOffsets();
  CHECK(afterResize != first);
  CHECK_NEAR(afterResize[4], offsetOf(container, 4), 0.001);

  // So does turning snapping off.
  FrameInput noSnap = inputFor(keys, 0.0, fixture);
  Virtualizer::update(container, noSnap);
  CHECK(container.getSnapOffsets().empty());
}

TEST(snap_offsets_survive_a_pure_scroll) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(80);
  Container container;
  FrameInput input = inputFor(keys, 0.0, fixture);
  input.snapToItem = true;
  Virtualizer::update(container, input);
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  Virtualizer::update(container, input);

  std::vector<double> before = container.getSnapOffsets();
  FrameInput scrolled = inputFor(keys, 2500.0, fixture);
  scrolled.snapToItem = true;
  scrolled.userScrolled = true;
  Virtualizer::update(container, scrolled);
  CHECK(container.getSnapOffsets() == before);
}

/*
 * A header and a footer stay reachable when snapping. The range's ends are snap points even
 * when no row aligns there, and a masonry grid's snap points come out sorted.
 */
TEST(snap_offsets_include_both_ends_and_stay_sorted) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(60);
  Container container;
  FrameInput input = inputFor(keys, 0.0, fixture);
  input.snapToItem = true;
  input.headerSize = 200.0;
  input.footerSize = 300.0;
  input.snapAlignment = 2;
  Virtualizer::update(container, input);
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  Virtualizer::update(container, input);

  const std::vector<double>& ends = container.getSnapOffsets();
  double maxOffset = container.revision.totalContainerHeight - WINDOW_HEIGHT;
  CHECK_NEAR(ends.front(), 0.0, 0.001);
  CHECK_NEAR(ends.back(), maxOffset, 0.001);
  // Aligned to the end, the last row rests with the footer below the fold.
  CHECK(std::find(ends.begin(), ends.end(), maxOffset - 300.0) != ends.end());

  Fixture grid;
  grid.columns = 2;
  Container masonry;
  FrameInput gridInput = inputFor(keys, 0.0, grid);
  gridInput.snapToItem = true;
  Virtualizer::update(masonry, gridInput);
  for (std::size_t index = 0; index < keys.size(); ++index) {
    double height = index % 2 == 0 ? 70.0 : 230.0;
    Virtualizer::updateElementAtIndex(masonry, index, {WINDOW_WIDTH / 2.0, height});
  }
  Virtualizer::update(masonry, gridInput);
  const std::vector<double>& sorted = masonry.getSnapOffsets();
  for (std::size_t index = 1; index < sorted.size(); ++index) {
    CHECK(sorted[index] > sorted[index - 1]);
  }
}

TEST(viewable_indices_stay_inside_the_viewport) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  container.onViewableIndicesChangeCallback = [](const std::vector<std::size_t>&) {};

  FrameInput input = inputFor(keys, 0.0, fixture);
  input.viewableRules = {ViewableRule{0.5, false}};
  Virtualizer::update(container, input);
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  FrameInput scrolled = inputFor(keys, 1000.0, fixture);
  scrolled.viewableRules = {ViewableRule{0.5, false}};
  Virtualizer::update(container, scrolled);

  auto viewable = container.getViewableIndices(container.viewableRules[0]);
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

/*
 * Coverage counts the share of the viewport a row covers. A short row fully on screen still
 * counts, and a tall row cut by an edge needs to cover enough of the viewport.
 */
TEST(viewable_coverage_rule_counts_the_viewport_share) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(20);
  Container container;
  FrameInput input = inputFor(keys, 0.0, fixture);
  Virtualizer::update(container, input);
  // Rows of 600 at 0, 600, 1200. The viewport of 840 shows 0 to 840, 50 pt past it at 790.
  measureRows(container, std::vector<double>(keys.size(), 600.0));
  FrameInput scrolled = inputFor(keys, 50.0, fixture);
  Virtualizer::update(container, scrolled);

  // Row 0 covers 550 of 840, row 1 covers 290 of 840.
  auto half = container.getViewableIndices(ViewableRule{0.5, true});
  CHECK_EQ(half.first, static_cast<std::size_t>(0));
  CHECK_EQ(half.second, static_cast<std::size_t>(0));
  auto third = container.getViewableIndices(ViewableRule{0.35, true});
  CHECK_EQ(third.second, static_cast<std::size_t>(0));
  auto quarter = container.getViewableIndices(ViewableRule{0.25, true});
  CHECK_EQ(quarter.second, static_cast<std::size_t>(1));
}

/*
 * Every rule gets its own range in one callback, sent only when one of them changes.
 */
TEST(viewable_rules_report_one_range_each) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(100);
  Container container;
  std::vector<std::vector<std::size_t>> calls;
  container.onViewableIndicesChangeCallback = [&](const std::vector<std::size_t>& ranges) { calls.push_back(ranges); };
  FrameInput input = inputFor(keys, 0.0, fixture);
  input.viewableRules = {ViewableRule{0.0, false}, ViewableRule{1.0, false}};
  Virtualizer::update(container, input);
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  FrameInput scrolled = inputFor(keys, 150.0, fixture);
  scrolled.viewableRules = input.viewableRules;
  Virtualizer::update(container, scrolled);
  CHECK(!calls.empty());
  const auto& ranges = calls.back();
  CHECK_EQ(ranges.size(), static_cast<std::size_t>(4));
  // Any overlap: row 1 (100 to 200) through row 9 (900 to 1000) for the viewport 150 to 990.
  CHECK_EQ(ranges[0], static_cast<std::size_t>(1));
  CHECK_EQ(ranges[1], static_cast<std::size_t>(9));
  // Fully visible only: rows 2 through 8.
  CHECK_EQ(ranges[2], static_cast<std::size_t>(2));
  CHECK_EQ(ranges[3], static_cast<std::size_t>(8));

  std::size_t before = calls.size();
  Virtualizer::update(container, scrolled);
  CHECK_EQ(calls.size(), before);
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
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  // Nothing is measured yet. Every far row uses the estimate.
  for (std::size_t index = 200; index < 500; ++index) {
    CHECK_NEAR(sizeOf(container, index), 120.0, 0.001);
  }

  // Measure the first screen of rows much taller, and the average becomes the fallback.
  for (std::size_t index = 0; index < 20; ++index) {
    Virtualizer::updateElementAtIndex(container, index, {WINDOW_WIDTH, 400.0});
  }
  /*
   * Two frames on purpose. Sizing runs before recomputeTotalSize fixes the average. The
   * new average only reaches unmeasured rows on the second frame.
   */
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  CHECK_NEAR(container.revision.averageElementHeight, 400.0, 0.001);
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  for (std::size_t index = 200; index < 500; ++index) {
    CHECK(!container.revision.elements[index].measured);
    CHECK_NEAR(sizeOf(container, index), 400.0, 0.001);
  }
  checkGeometryContiguous(container, "after average froze");

  // Repeating the settled frame leaves everything exactly where it is.
  double totalBefore = container.revision.totalContainerHeight;
  double lastOffsetBefore = offsetOf(container, 499);
  for (int repeat = 0; repeat < 3; ++repeat) {
    Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  }
  CHECK_NEAR(container.revision.totalContainerHeight, totalBefore, 0.001);
  CHECK_NEAR(offsetOf(container, 499), lastOffsetBefore, 0.001);
}

TEST(the_frozen_average_is_a_whole_size_and_corrections_stay_whole) {
  Fixture fixture;
  fixture.estimatedHeight = 120.0;

  std::vector<std::string> keys = keysFor(500);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  // Three rows of 100, 101 and 101 average 100.67. The frozen average rounds to 101.
  Virtualizer::updateElementAtIndex(container, 0, {WINDOW_WIDTH, 100.0});
  Virtualizer::updateElementAtIndex(container, 1, {WINDOW_WIDTH, 101.0});
  Virtualizer::updateElementAtIndex(container, 2, {WINDOW_WIDTH, 101.0});
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  CHECK_NEAR(container.revision.averageElementHeight, 101.0, 0.001);
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  // Every row edge is whole. A whole measurement then moves the rows after it by a whole amount.
  for (std::size_t index = 0; index < 500; ++index) {
    CHECK_NEAR(offsetOf(container, index), std::round(offsetOf(container, index)), 0.0001);
  }
  double before = offsetOf(container, 300);
  Virtualizer::updateElementAtIndex(container, 250, {WINDOW_WIDTH, 87.0});
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  CHECK_NEAR(offsetOf(container, 300) - before, -14.0, 0.0001);
}

TEST(newly_inserted_rows_get_a_fallback_size_immediately) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 250.0));
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  std::vector<std::string> grown = keys;
  for (std::size_t index = 0; index < 50; ++index) {
    grown.push_back("new" + std::to_string(index));
  }
  Virtualizer::update(container, inputFor(grown, 0.0, fixture));

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
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

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
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  Virtualizer::update(container, inputFor({}, 0.0, fixture));
  CHECK_EQ(container.getElementsSize(), static_cast<std::size_t>(0));
  CHECK_NEAR(container.revision.totalContainerHeight, 0.0, 0.001);

  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
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
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  std::uint64_t seed = 0x5DEECE66Dull;
  auto nextRandom = [&]() {
    seed = seed * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::size_t>((seed >> 33) & 0xFFFFFFFFull);
  };

  double offset = 0.0;
  std::size_t freshCounter = 0;
  /*
   * Whether the mounted range matches the current layout. Sizes arrive after the commit
   * that made the range. After a resize it is one frame behind, just like on device.
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
        Virtualizer::update(container, input);
        windowFresh = true;
        break;
      }
      case 1: {  // measure a mounted row
        if (container.getElementsSize() == 0) {
          break;
        }
        std::size_t index = nextRandom() % container.getElementsSize();
        double height = static_cast<double>(nextRandom() % 400);
        Virtualizer::updateElementAtIndex(container, index, {WINDOW_WIDTH, height});
        Virtualizer::recomputeTotalSize(container);
        windowFresh = false;
        break;
      }
      case 2: {  // insert a page somewhere
        std::size_t at = keys.empty() ? 0 : nextRandom() % keys.size();
        std::vector<std::string> inserted;
        for (std::size_t index = 0; index < 5; ++index) {
          inserted.push_back("fresh" + std::to_string(freshCounter++));
        }
        keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(at), inserted.begin(), inserted.end());
        Virtualizer::update(container, inputFor(keys, offset, fixture));
        windowFresh = true;
        break;
      }
      case 3: {  // remove a page
        if (keys.size() < 20) {
          break;
        }
        std::size_t at = nextRandom() % (keys.size() - 10);
        keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(at), keys.begin() + static_cast<std::ptrdiff_t>(at) + 8);
        Virtualizer::update(container, inputFor(keys, offset, fixture));
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
        keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(from));
        keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(std::min(to, keys.size())), moved);
        Virtualizer::update(container, inputFor(keys, offset, fixture));
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
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  for (int frame = 0; frame < 3; ++frame) {
    Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  }

  // The fling reaches the top area and is still slowing down.
  FrameInput coasting = inputFor(keys, 37.0, fixture);
  coasting.userScrolled = true;
  coasting.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(container, coasting);
  CHECK_NEAR(container.revision.containerOffsetY, 37.0, 0.5);

  // Ten unmeasured rows are prepended, and both runs of the commit see the old report.
  std::vector<std::string> grown = keysFor(10, "fresh");
  grown.insert(grown.end(), keys.begin(), keys.end());
  // Unmeasured rows use the fallback size from the measured rows.
  double inserted = 10 * 100.0;

  FrameInput prepend = inputFor(grown, 37.0, fixture);
  prepend.userScrolled = true;
  prepend.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(container, prepend);
  CHECK_NEAR(container.revision.containerOffsetY, 37.0 + inserted, 1.0);

  Virtualizer::update(container, prepend);
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, 37.0 + inserted, 1.0);
  CHECK(container.operation.has_value());

  // A momentum frame reported before the host applied the correction moves the target along.
  FrameInput momentum = inputFor(grown, 21.0, fixture);
  momentum.userScrolled = true;
  momentum.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(container, momentum);
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, 21.0 + inserted, 1.0);
  CHECK(container.operation.has_value());

  // The host applies it and reports the token back. The gesture owns the offset again.
  std::uint64_t token = container.operation ? container.operation->id : 0;
  FrameInput echo = inputFor(grown, 21.0 + inserted, fixture);
  echo.scrollPhase = ScrollPhase::Settling;
  echo.commitToken = token;
  Virtualizer::update(container, echo);
  CHECK(!container.containerOffsetCorrected);
  CHECK(!container.operation.has_value());
  CHECK_NEAR(container.revision.containerOffsetY, 21.0 + inserted, 1.0);

  // Momentum carries on from there and is not pulled back.
  FrameInput carryOn = inputFor(grown, 5.0 + inserted, fixture);
  carryOn.userScrolled = true;
  carryOn.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(container, carryOn);
  CHECK(!container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, 5.0 + inserted, 1.0);

  /*
   * The previous first row was 37 pixels past the top, and two momentum frames moved the view up
   * 16 pixels each. It is now 5 pixels past it. The prepend moved nothing.
   */
  std::size_t previousFirst = container.findElementIndexByKey("k0");
  CHECK_NEAR(container.revision.containerOffsetY - offsetOf(container, previousFirst), 5.0, 1.0);
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
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);
  CHECK_NEAR(bottom, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);

  std::vector<std::string> grown = keys;
  for (std::size_t index = 0; index < 3; ++index) {
    grown.push_back("appended" + std::to_string(index));
  }

  double offset = bottom;
  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(container, inputFor(grown, offset, fixture));
    if (frame == 1) {
      for (std::size_t index = keys.size(); index < grown.size(); ++index) {
        Virtualizer::updateElementAtIndex(container, index, {WINDOW_WIDTH, 180.0});
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
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);

  std::vector<std::string> grown = keys;
  for (std::size_t index = 0; index < 3; ++index) {
    grown.push_back("appended" + std::to_string(index));
  }
  double offset = bottom;
  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(container, inputFor(grown, offset, fixture));
    if (frame == 1) {
      for (std::size_t index = keys.size(); index < grown.size(); ++index) {
        Virtualizer::updateElementAtIndex(container, index, {WINDOW_WIDTH, 180.0});
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
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);

  double parked = bottom - 500.0;
  FrameInput drag = inputFor(keys, parked, fixture);
  drag.userScrolled = true;
  drag.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container, drag);
  Virtualizer::update(container, inputFor(keys, parked, fixture));
  CHECK(container.invertedBottomReleased);

  std::vector<std::string> grown = keys;
  grown.push_back("appended");
  for (int frame = 0; frame < 4; ++frame) {
    Virtualizer::update(container, inputFor(grown, parked, fixture));
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
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double offset = settleAtBottom(container, keys, fixture);

  auto frame = [&](const std::vector<std::string>& frameKeys, double windowHeight) {
    FrameInput input = inputFor(frameKeys, offset, fixture);
    input.windowContainerHeight = windowHeight;
    Virtualizer::update(container, input);
    offset = container.revision.containerOffsetY;
  };

  // A page of history lands at the end of the opening settle, as it does on device.
  keys.insert(keys.begin(), "earlier");
  for (int settle = 0; settle < 6; ++settle) {
    frame(keys, WINDOW_HEIGHT);
    if (settle == 1) {
      Virtualizer::updateElementAtIndex(container, 0, {WINDOW_WIDTH, 100.0});
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
      Virtualizer::updateElementAtIndex(container, grown.size() - 1, {WINDOW_WIDTH, 100.0});
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
 * The same resize for a reader who scrolled up moves nothing on screen.
 */
TEST(inverted_list_scrolled_up_holds_when_the_viewport_resizes) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);

  double parked = bottom - 500.0;
  FrameInput drag = inputFor(keys, parked, fixture);
  drag.userScrolled = true;
  drag.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container, drag);
  Virtualizer::update(container, inputFor(keys, parked, fixture));

  for (int frame = 0; frame < 4; ++frame) {
    FrameInput input = inputFor(keys, parked, fixture);
    input.windowContainerHeight = WINDOW_HEIGHT - 66.0;
    Virtualizer::update(container, input);
    CHECK_NEAR(container.revision.containerOffsetY, parked, 0.01);
  }
  CHECK(!container.pendingScrollToEnd);
}

/*
 * A scrollToEnd sent while a fling is still coasting must still run. Momentum is not the
 * reader taking over. Even with momentum frames moving the offset the view lands on the
 * bottom. Only a finger cancels it.
 */
TEST(scroll_to_end_requested_during_momentum_lands_on_the_bottom) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  double maxOffset = container.revision.totalContainerHeight - WINDOW_HEIGHT;

  FrameInput coasting = inputFor(keys, 300.0, fixture);
  coasting.userScrolled = true;
  coasting.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(container, coasting);

  // The command arrives on top of the last momentum report.
  container.requestScrollToIndex(SCROLL_TO_END_INDEX, 1.0, -2);
  FrameInput command = inputFor(keys, 300.0, fixture);
  command.userScrolled = true;
  command.scrollPhase = ScrollPhase::Settling;
  command.containerOffsetEnabled = true;
  Virtualizer::update(container, command);
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, maxOffset, 1.0);

  FrameInput momentum = inputFor(keys, 340.0, fixture);
  momentum.userScrolled = true;
  momentum.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(container, momentum);
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(container.revision.containerOffsetY, maxOffset, 1.0);

  std::uint64_t token = container.operation ? container.operation->id : 0;
  CHECK(token != 0);
  FrameInput echo = inputFor(keys, maxOffset, fixture);
  echo.commitToken = token;
  Virtualizer::update(container, echo);
  Virtualizer::update(container, inputFor(keys, maxOffset, fixture));
  CHECK(!container.operation.has_value());
  CHECK(!container.pendingScrollToEnd);
  CHECK_NEAR(container.revision.containerOffsetY, maxOffset, 1.0);
}

TEST(a_drag_cancels_a_scroll_command_but_momentum_does_not) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  container.requestScrollToIndex(120.0, 1.0, -2);
  FrameInput command = inputFor(keys, 500.0, fixture);
  command.userScrolled = true;
  command.scrollPhase = ScrollPhase::Settling;
  command.containerOffsetEnabled = true;
  Virtualizer::update(container, command);
  double target = offsetOf(container, 120);
  CHECK_NEAR(container.revision.containerOffsetY, target, 1.0);

  FrameInput momentum = inputFor(keys, 520.0, fixture);
  momentum.userScrolled = true;
  momentum.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(container, momentum);
  CHECK(container.operation.has_value());
  CHECK_NEAR(container.revision.containerOffsetY, target, 1.0);

  FrameInput drag = inputFor(keys, 540.0, fixture);
  drag.userScrolled = true;
  drag.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container, drag);
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
    Virtualizer::update(container, inputFor(keys, offset, fixture));
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
  Virtualizer::update(container, touch);
  Virtualizer::update(container, inputFor(keys, bottom, fixture));

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
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  double straddling = 5 * ESTIMATED_ROW_HEIGHT + 100.0;
  FrameInput scroll = inputFor(keys, straddling, fixture);
  scroll.userScrolled = true;
  scroll.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container, scroll);
  Virtualizer::update(container, inputFor(keys, straddling, fixture));
  CHECK(!container.operation.has_value());

  double belowOnScreen = offsetOf(container, 6) - straddling;
  Virtualizer::updateElementAtIndex(container, 5, {WINDOW_WIDTH, 60.0});
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(offsetOf(container, 6) - container.revision.containerOffsetY, belowOnScreen, 0.5);
}
