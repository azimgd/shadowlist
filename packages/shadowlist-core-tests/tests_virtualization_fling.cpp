/*
 * Virtualization tests for flings: a prepend while a fling settles at the top edge, a fling
 * into the end of the list, and a fling or jump into rows that were never measured.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"
#include "VirtualizationHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/MountedRange.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

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
  FrameInput input = scrollReport(keys, offset, fixture, phase);
  input.headerSize = TOP_EDGE_HEADER;
  input.commitToken = echoedToken;
  return input;
}

/*
 * The next commit after a correction: the core's own offset and token, plus the gesture
 * fields of the report it came from.
 */
FrameInput publishedCorrection(const Container& container, const FrameInput& report) {
  FrameInput input = report;
  input.offsetY = container.revision.offsetY;
  input.offsetEnabled = true;
  input.commitToken = container.state.operation ? container.state.operation->commitToken : 0;
  return input;
}

/*
 * A list resting at its top with every row measured.
 */
void openAtTheTop(Container& container, const std::vector<std::string>& keys, const Fixture& fixture) {
  Virtualizer::update(container, topEdgeReport(keys, 0.0, fixture, ScrollPhase::Idle, 0));
  measureRows(container, std::vector<double>(keys.size(), TOP_EDGE_ROW_HEIGHT));
  for (int frame = 0; frame < 3; ++frame) {
    Virtualizer::update(container, topEdgeReport(keys, 0.0, fixture, ScrollPhase::Idle, 0));
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
  return offsetOf(container, container.indexOfKey(key)) - container.revision.offsetY;
}

/*
 * Mount rows like Fabric does after a prepend. Each row first reports zero height before its
 * content lays out, then the layout pass reports the real sizes in one batch.
 */
void mountRows(Container& container, const std::vector<std::string>& keys, double height) {
  for (const std::string& key : keys) {
    Virtualizer::updateRowAtIndex(container, container.indexOfKey(key), {WINDOW_WIDTH, 0.0});
  }
  std::size_t lowestChangedIndex = UNDEFINED_INDEX;
  for (const std::string& key : keys) {
    std::size_t index = container.indexOfKey(key);
    if (Virtualizer::applyRowSize(container, index, {WINDOW_WIDTH, height}) && index < lowestChangedIndex) {
      lowestChangedIndex = index;
    }
  }
  if (lowestChangedIndex != UNDEFINED_INDEX) {
    Virtualizer::commitRowSizes(container, lowestChangedIndex);
  }
  Virtualizer::recomputeContentSize(container);
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
    Virtualizer::update(container, topEdgeReport(keys, offset, fixture, ScrollPhase::Settling, 0));
  }
  double firstRowBelowTop = belowViewportTop(container, "k0");
  CHECK_NEAR(firstRowBelowTop, TOP_EDGE_HEADER + 12.0, 0.5);

  std::vector<std::string> grown = prependedTo(keys, "fresh");
  FrameInput prepend = topEdgeReport(grown, -12.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(container, prepend);
  Virtualizer::update(container, prepend);
  CHECK(container.state.operation.has_value());
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  std::uint64_t token = container.state.operation ? container.state.operation->commitToken : 0;

  Virtualizer::update(container, publishedCorrection(container, prepend));
  CHECK(container.state.operation.has_value());
  CHECK(container.state.offsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);

  // The bounce moves the view 12 pixels down to the edge before the host applies the offset.
  FrameInput spring = topEdgeReport(grown, 0.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(container, spring);
  CHECK(container.state.operation.has_value());
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop - 12.0, 0.5);
  Virtualizer::update(container, publishedCorrection(container, spring));
  CHECK_NEAR(belowViewportTop(container, "k0"), TOP_EDGE_HEADER, 0.5);

  double applied = container.revision.offsetY;
  Virtualizer::update(container, topEdgeReport(grown, applied, fixture, ScrollPhase::Settling, token));
  CHECK(!container.state.operation.has_value());
  Virtualizer::update(container, topEdgeReport(grown, applied, fixture, ScrollPhase::Idle, token));
  CHECK(!container.state.offsetCorrected);
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

  Virtualizer::update(container, topEdgeReport(keys, 30.0, fixture, ScrollPhase::Settling, 0));
  Virtualizer::update(container, topEdgeReport(keys, 0.0, fixture, ScrollPhase::Settling, 0));
  double firstRowBelowTop = belowViewportTop(container, "k0");

  std::vector<std::string> grown = prependedTo(keys, "fresh");
  std::vector<std::string> fresh = keysFor(10, "fresh");
  FrameInput prepend = topEdgeReport(grown, 0.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(container, prepend);
  Virtualizer::update(container, prepend);
  std::uint64_t token = container.state.operation ? container.state.operation->commitToken : 0;
  CHECK(token != 0);
  Virtualizer::update(container, publishedCorrection(container, prepend));

  mountRows(container, keyRange(fresh, 0, 5), 160.0);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  Virtualizer::update(container, publishedCorrection(container, prepend));
  CHECK(container.state.operation.has_value());
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);

  double applied = container.revision.offsetY;
  Virtualizer::update(container, topEdgeReport(grown, applied, fixture, ScrollPhase::Settling, token));
  CHECK(!container.state.operation.has_value());

  mountRows(container, keyRange(fresh, 5, 10), 60.0);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  Virtualizer::update(container, publishedCorrection(container, prepend));
  double rest = container.revision.offsetY;
  Virtualizer::update(container, topEdgeReport(grown, rest, fixture, ScrollPhase::Idle, token));
  Virtualizer::update(container, topEdgeReport(grown, rest, fixture, ScrollPhase::Idle, token));
  CHECK(!container.state.offsetCorrected);
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

  Virtualizer::update(container, topEdgeReport(keys, 40.0, fixture, ScrollPhase::Settling, 0));
  Virtualizer::update(container, topEdgeReport(keys, 0.0, fixture, ScrollPhase::Settling, 0));
  double firstRowBelowTop = belowViewportTop(container, "k0");

  std::vector<std::string> first = prependedTo(keys, "fresh");
  FrameInput firstPrepend = topEdgeReport(first, 0.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(container, firstPrepend);
  Virtualizer::update(container, firstPrepend);
  std::uint64_t token = container.state.operation ? container.state.operation->commitToken : 0;
  CHECK(token != 0);
  Virtualizer::update(container, publishedCorrection(container, firstPrepend));

  std::vector<std::string> second = prependedTo(first, "older");
  FrameInput secondPrepend = topEdgeReport(second, 0.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(container, secondPrepend);
  Virtualizer::update(container, secondPrepend);
  CHECK(container.state.operation.has_value());
  CHECK(container.state.operation && container.state.operation->commitToken == token);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  Virtualizer::update(container, publishedCorrection(container, secondPrepend));

  mountRows(container, keysFor(10, "older"), 140.0);
  mountRows(container, keysFor(10, "fresh"), 90.0);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  Virtualizer::update(container, publishedCorrection(container, secondPrepend));
  CHECK(container.state.operation.has_value());

  double applied = container.revision.offsetY;
  Virtualizer::update(container, topEdgeReport(second, applied, fixture, ScrollPhase::Settling, token));
  CHECK(!container.state.operation.has_value());
  Virtualizer::update(container, topEdgeReport(second, applied, fixture, ScrollPhase::Idle, token));
  CHECK(!container.state.offsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  checkNoRowLost(container, "two prepends in quick succession while bouncing");
}

/*
 * The bounce ends after the prepend but before the host applies the correction, and the host
 * never reports that last bit of movement. The host shifts its current offset by the correction.
 * Its report back lands short of the core's target by that movement. That report confirms the
 * correction. Pushing on to the exact target would undo the movement.
 */
TEST(prepend_whose_bounce_ends_before_the_correction_lands_is_confirmed_by_its_echo) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  openAtTheTop(container, keys, fixture);

  Virtualizer::update(container, topEdgeReport(keys, 40.0, fixture, ScrollPhase::Settling, 0));
  Virtualizer::update(container, topEdgeReport(keys, 16.0, fixture, ScrollPhase::Settling, 0));

  std::vector<std::string> grown = prependedTo(keys, "fresh");
  FrameInput prepend = topEdgeReport(grown, 16.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(container, prepend);
  Virtualizer::update(container, prepend);
  std::uint64_t token = container.state.operation ? container.state.operation->commitToken : 0;
  CHECK(token != 0);
  double firstTarget = container.revision.offsetY;
  Virtualizer::update(container, publishedCorrection(container, prepend));

  mountRows(container, keysFor(10, "fresh"), 150.0);
  Virtualizer::update(container, publishedCorrection(container, prepend));
  double retarget = container.revision.offsetY;

  // The host came to rest at the edge, 16 pixels past the report, and shifted by the correction.
  double echoed = 0.0 + (firstTarget - 16.0);
  Virtualizer::update(container, topEdgeReport(grown, echoed, fixture, ScrollPhase::Idle, token));
  CHECK(!container.state.operation.has_value());
  CHECK(!container.state.offsetCorrected);

  // The updated target applies next and moves the view by the difference.
  double rest = echoed + (retarget - firstTarget);
  Virtualizer::update(container, topEdgeReport(grown, rest, fixture, ScrollPhase::Idle, token));
  Virtualizer::update(container, topEdgeReport(grown, rest, fixture, ScrollPhase::Idle, token));
  CHECK(!container.state.offsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), TOP_EDGE_HEADER, 0.5);
  checkNoRowLost(container, "prepend whose bounce ends before the correction lands");
}

/*
 * The bounce ends after the core worked out the correction but before the host applies it, and
 * this time the host reports it as an idle report at the edge. That moves the target like a
 * momentum frame. The row holds where the reader saw it stop.
 */
TEST(prepend_while_bouncing_follows_the_idle_report_of_the_bounce_ending) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  openAtTheTop(container, keys, fixture);

  Virtualizer::update(container, topEdgeReport(keys, 50.0, fixture, ScrollPhase::Settling, 0));
  Virtualizer::update(container, topEdgeReport(keys, 24.0, fixture, ScrollPhase::Settling, 0));

  std::vector<std::string> grown = prependedTo(keys, "fresh");
  FrameInput prepend = topEdgeReport(grown, 24.0, fixture, ScrollPhase::Settling, 0);
  Virtualizer::update(container, prepend);
  Virtualizer::update(container, prepend);
  std::uint64_t token = container.state.operation ? container.state.operation->commitToken : 0;
  CHECK(token != 0);
  Virtualizer::update(container, publishedCorrection(container, prepend));

  FrameInput rested = topEdgeReport(grown, 0.0, fixture, ScrollPhase::Idle, 0);
  Virtualizer::update(container, rested);
  CHECK(container.state.operation.has_value());
  CHECK(container.state.offsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), TOP_EDGE_HEADER, 0.5);

  double applied = container.revision.offsetY;
  Virtualizer::update(container, topEdgeReport(grown, applied, fixture, ScrollPhase::Idle, token));
  CHECK(!container.state.operation.has_value());
  Virtualizer::update(container, topEdgeReport(grown, applied, fixture, ScrollPhase::Idle, token));
  CHECK(!container.state.offsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), TOP_EDGE_HEADER, 0.5);
  checkNoRowLost(container, "prepend while bouncing with the bounce ending reported idle");
}

/*
 * A pull to refresh batch lands while the gesture flag is still set and the view rests at the
 * top, then the new rows measure far from their estimate. The host reports back the first offset
 * after the core already moved the target. That report is not movement, and it does not confirm
 * the old target. The new target stands. Reaching it ends the correction with the reader's row in place.
 */
TEST(prepend_after_a_pull_to_refresh_keeps_its_retarget_through_the_echo_of_the_first_write) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  openAtTheTop(container, keys, fixture);

  FrameInput released = topEdgeReport(keys, 0.0, fixture, ScrollPhase::Idle, 0);
  released.userScrolled = true;
  Virtualizer::update(container, released);
  double firstRowBelowTop = belowViewportTop(container, "k0");

  std::vector<std::string> grown = prependedTo(keys, "fresh");
  FrameInput refresh = topEdgeReport(grown, 0.0, fixture, ScrollPhase::Idle, 0);
  refresh.userScrolled = true;
  Virtualizer::update(container, refresh);
  Virtualizer::update(container, refresh);
  std::uint64_t token = container.state.operation ? container.state.operation->commitToken : 0;
  CHECK(token != 0);
  double firstTarget = container.revision.offsetY;
  Virtualizer::update(container, publishedCorrection(container, refresh));

  mountRows(container, keysFor(10, "fresh"), 60.0);
  Virtualizer::update(container, publishedCorrection(container, refresh));
  double retarget = container.revision.offsetY;
  CHECK(std::fabs(retarget - firstTarget) > 100.0);

  Virtualizer::update(container, topEdgeReport(grown, firstTarget, fixture, ScrollPhase::Idle, token));
  CHECK(container.state.operation.has_value());
  CHECK(container.state.offsetCorrected);
  CHECK_NEAR(container.revision.offsetY, retarget, 0.5);

  Virtualizer::update(container, topEdgeReport(grown, retarget, fixture, ScrollPhase::Idle, token));
  Virtualizer::update(container, topEdgeReport(grown, retarget, fixture, ScrollPhase::Idle, token));
  CHECK(!container.state.operation.has_value());
  CHECK(!container.state.offsetCorrected);
  CHECK_NEAR(belowViewportTop(container, "k0"), firstRowBelowTop, 0.5);
  checkNoRowLost(container, "prepend after a pull-to-refresh with a retarget before the echo");
}

// Fling into the end of the list

/*
 * A fling lands on the end while the last rows are still estimates. They measure shorter and
 * the end moves in. The layout pass's correction must stay inside the new scroll range. A
 * write past it is clamped by the host, and the next correction would apply the gap again.
 */
TEST(fling_into_shorter_last_rows_keeps_the_correction_inside_the_range) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(90, ESTIMATED_ROW_HEIGHT));

  double end = 100.0 * ESTIMATED_ROW_HEIGHT - WINDOW_HEIGHT;
  FrameInput report = inputFor(keys, end, fixture);
  report.userScrolled = true;
  report.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(container, report);
  CHECK(!container.state.offsetCorrected);

  // The layout pass measures the mounted rows near the end at a third of the estimate.
  std::size_t lowest = UNDEFINED_INDEX;
  for (std::size_t index = 90; index < 100; ++index) {
    if (Virtualizer::applyRowSize(container, index, {WINDOW_WIDTH, 40.0}) && index < lowest) {
      lowest = index;
    }
  }
  Virtualizer::commitRowSizes(container, lowest);
  Virtualizer::recomputeContentSize(container);

  double newEnd = container.revision.contentHeight - WINDOW_HEIGHT;
  CHECK_NEAR(newEnd, 90.0 * ESTIMATED_ROW_HEIGHT + 10.0 * 40.0 - WINDOW_HEIGHT, 0.001);
  CHECK(container.state.offsetCorrected);
  CHECK_NEAR(container.revision.offsetY, newEnd, 0.001);

  // The next frame finds the view at the end and has nothing left to correct.
  FrameInput own = report;
  own.offsetY = container.revision.offsetY;
  own.offsetEnabled = true;
  Virtualizer::update(container, own);
  CHECK(!container.state.operation.has_value());
}

// Fling into rows that were never measured

namespace {

constexpr double FLING_ROW_HEIGHT = 100.0;

/*
 * The first row on screen that the measured range and the rows a host mounts around it leave
 * out, or UNDEFINED_INDEX when every row on screen is mounted.
 */
std::size_t firstBlankRow(const Container& container) {
  return firstRowNotMounted(container, container.revision.offsetY, DEFAULT_OVERSCAN_ROWS);
}

}

/*
 * A reader flings up an inverted list and lands with two rows of guessed size at the top of the
 * screen and measured rows below them. The layout pass then measures the two rows. The measured
 * rows the reader already saw stay where they were. Holding the top row would move them by the
 * size error of the second row.
 */
TEST(inverted_fling_into_guessed_rows_keeps_the_measured_rows_below) {
  Fixture fixture;
  fixture.inverted = true;
  Container container;
  std::vector<std::string> keys = keysFor(60);

  // The list opens at its bottom with rows 20 to 59 measured. The other rows take the average.
  settleAtBottom(container, keys, fixture);
  layoutRows(container, 20, std::vector<double>(40, FLING_ROW_HEIGHT));
  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  }
  CHECK_EQ(container.revision.averageRowHeight, FLING_ROW_HEIGHT);
  CHECK_EQ(container.revision.offsetY, 60.0 * FLING_ROW_HEIGHT - WINDOW_HEIGHT);

  // The fling leaves the bottom, then lands with the top of the screen inside row 18.
  Virtualizer::update(container, scrollReport(keys, 3000.0, fixture, ScrollPhase::Dragging));
  double landed = 18.0 * FLING_ROW_HEIGHT + 50.0;
  Virtualizer::update(container, scrollReport(keys, landed, fixture, ScrollPhase::Settling));
  CHECK(container.state.invertedBottomReleased);
  CHECK(!container.revision.rows[18].measured);
  CHECK(!container.revision.rows[19].measured);
  CHECK(container.revision.rows[20].measured);
  double before = offsetOf(container, 20) - container.revision.offsetY;
  CHECK_EQ(before, 150.0);

  // Rows 14 to 19 mount and come out taller than the average.
  layoutRows(container, 14, {140.0, 140.0, 140.0, 140.0, 160.0, 180.0});

  double after = offsetOf(container, 20) - container.revision.offsetY;
  CHECK(container.state.offsetCorrected);
  CHECK_EQ(after, before);
}

/*
 * An inverted list still resting at its bottom keeps the row at the top of the viewport as
 * its anchor, guessed size or not. The bottom hold reads that anchor to tell the newest row.
 */
TEST(inverted_list_at_its_bottom_keeps_the_top_row_as_its_anchor) {
  Fixture fixture;
  fixture.inverted = true;
  Container container;
  std::vector<std::string> keys = keysFor(60);

  settleAtBottom(container, keys, fixture);
  layoutRows(container, 53, std::vector<double>(7, FLING_ROW_HEIGHT));
  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  }
  double bottom = container.revision.offsetY;

  // The row at the top of the screen has a guessed size and measured rows follow it on screen.
  std::size_t top = 0;
  while (offsetOf(container, top) + sizeOf(container, top) <= bottom) {
    ++top;
  }
  CHECK(top < 53);
  CHECK(!container.revision.rows[top].measured);
  CHECK(offsetOf(container, 53) < bottom + WINDOW_HEIGHT);
  CHECK(!container.state.invertedBottomReleased);
  CHECK_EQ(container.state.anchor.key, keys[top]);
}

/*
 * A jump lands in rows far shorter than the average the first rows froze. The core picked
 * its measured range from the average, and the layout pass that measures those rows pulls rows
 * past the range into view. The core's next pass picks the range again from the measured
 * sizes, and that range and the host's pad hold every row on screen. Hosts run that pass before
 * they mount, see ListDriver::runPasses.
 */
TEST(jump_into_rows_shorter_than_the_average_is_covered_by_the_next_pass) {
  constexpr double TALL = 450.0;
  constexpr double SHORT = 60.0;
  Fixture fixture;
  Container container;
  std::vector<std::string> keys = keysFor(400);

  // The first five rows freeze the average.
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  layoutRows(container, 0, std::vector<double>(5, TALL));
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  CHECK_EQ(container.revision.averageRowHeight, TALL);

  // The jump lands on the top of row 200.
  FrameInput report = scrollReport(keys, offsetOf(container, 200), fixture, ScrollPhase::Settling);
  Virtualizer::update(container, report);
  CHECK_EQ(firstBlankRow(container), UNDEFINED_INDEX);

  // The mounted rows measure short and the rows after them move up into view.
  IndexRange picked = container.getMeasuredRange();
  std::size_t low = picked.low - DEFAULT_OVERSCAN_ROWS;
  std::size_t count = picked.high + DEFAULT_OVERSCAN_ROWS - low + 1;
  layoutRows(container, low, std::vector<double>(count, SHORT));
  CHECK_EQ(offsetOf(container, 200) - container.revision.offsetY, 0.0);
  CHECK(firstBlankRow(container) != UNDEFINED_INDEX);

  // The core's own write comes back and the range is picked again.
  Virtualizer::update(container, publishedCorrection(container, report));
  CHECK_EQ(firstBlankRow(container), UNDEFINED_INDEX);
  CHECK_EQ(offsetOf(container, 200) - container.revision.offsetY, 0.0);
}
