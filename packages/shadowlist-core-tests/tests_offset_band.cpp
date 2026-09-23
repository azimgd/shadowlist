/*
 * Offset band tests. The band is the range of offsets a host may scroll through without
 * sending the core a frame. Every offset inside it must give the same window, fire no
 * edge or visible rows callback and start no correction. Crossing its end must change
 * something, or the band is narrower than it needs to be.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

/*
 * One list setup. Rows get uneven sizes so flips don't line up on a grid.
 */
struct BandScenario {
  std::size_t rows = 200;
  std::size_t columns = 1;
  bool inverted = false;
  double overscan = 1.0;
  double startThreshold = 1.0;
  double endThreshold = 1.0;
  double offset = 5000.0;
};

double rowHeight(std::size_t index) {
  return 60.0 + static_cast<double>((index * 37) % 90);
}

FrameInput bandInput(const BandScenario& scenario, const std::vector<std::string>& keys, double offset) {
  FrameInput input = inputFor(keys, offset);
  input.columns = scenario.columns;
  input.inverted = scenario.inverted;
  input.overscan = scenario.overscan;
  input.startReachedThreshold = scenario.startThreshold;
  input.endReachedThreshold = scenario.endThreshold;
  return input;
}

/*
 * Counts what one frame fires.
 */
struct Fired {
  int visible = 0;
  int startReached = 0;
  int endReached = 0;
};

/*
 * Build a list with every row measured, resting at the scenario offset after a finger
 * scroll ended there. The same steps always give the same core, so tests rebuild it to
 * try one more frame without disturbing the original.
 */
std::unique_ptr<Container> settledContainer(const BandScenario& scenario, Fired* fired = nullptr) {
  auto container = std::make_unique<Container>();
  std::vector<std::string> keys = keysFor(scenario.rows);
  Virtualizer::update(container.get(), bandInput(scenario, keys, 0.0));
  for (std::size_t index = 0; index < scenario.rows; ++index) {
    double width = scenario.columns > 1 ? WINDOW_WIDTH / static_cast<double>(scenario.columns) : WINDOW_WIDTH;
    Virtualizer::updateElementAtIndex(container.get(), index, {width, rowHeight(index)});
  }
  FrameInput drag = bandInput(scenario, keys, scenario.offset);
  drag.userScrolled = true;
  drag.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container.get(), drag);
  FrameInput rest = bandInput(scenario, keys, scenario.offset);
  Virtualizer::update(container.get(), rest);
  Virtualizer::update(container.get(), rest);

  if (fired != nullptr) {
    container->onVisibleIndicesChangeCallback = [fired](std::size_t, std::size_t) { fired->visible++; };
    container->onStartReachedCallback = [fired]() { fired->startReached++; };
    container->onEndReachedCallback = [fired]() { fired->endReached++; };
  }
  return container;
}

/*
 * What a frame at offset changes on a freshly settled copy of the scenario.
 */
struct FrameOutcome {
  std::pair<std::size_t, std::size_t> visible;
  Fired fired;
  bool corrected = false;
  bool operation = false;
};

FrameOutcome frameAt(const BandScenario& scenario, double offset, bool gesture) {
  Fired fired;
  auto container = settledContainer(scenario, &fired);
  std::vector<std::string> keys = keysFor(scenario.rows);
  FrameInput input = bandInput(scenario, keys, offset);
  input.keysUnchanged = false;
  if (gesture) {
    input.userScrolled = true;
    input.scrollPhase = ScrollPhase::Dragging;
  }
  Virtualizer::update(container.get(), input);
  FrameOutcome outcome;
  outcome.visible = container->getVisibleIndices();
  outcome.fired = fired;
  outcome.corrected = container->containerOffsetCorrected;
  outcome.operation = container->operation.has_value();
  return outcome;
}

bool changesSomething(const FrameOutcome& outcome, const std::pair<std::size_t, std::size_t>& baseline) {
  return outcome.visible != baseline || outcome.fired.visible > 0 || outcome.fired.startReached > 0 ||
    outcome.fired.endReached > 0 || outcome.corrected || outcome.operation;
}

/*
 * Frames anywhere inside the band change nothing. In a single column list, frames just
 * past each end that isn't the end of the scroll range do.
 */
void checkBandIsExact(const BandScenario& scenario) {
  auto container = settledContainer(scenario);
  OffsetBand band = container->computeOffsetBand();
  CHECK(!band.isEmpty());
  CHECK(band.contains(scenario.offset));
  auto baseline = container->getVisibleIndices();

  const int samples = 24;
  for (int sample = 0; sample <= samples; ++sample) {
    double offset = band.low + (band.high - band.low) * static_cast<double>(sample) / samples;
    for (bool gesture : {false, true}) {
      FrameOutcome outcome = frameAt(scenario, offset, gesture);
      CHECK(outcome.visible == baseline);
      CHECK_EQ(outcome.fired.visible, 0);
      CHECK_EQ(outcome.fired.startReached, 0);
      CHECK_EQ(outcome.fired.endReached, 0);
      CHECK(!outcome.corrected);
      CHECK(!outcome.operation);
    }
  }

  /*
   * The window event only carries the lowest and highest row. With several columns a row
   * inside that range can flip without changing either, so there the band may end early.
   */
  if (scenario.columns > 1) {
    return;
  }
  double totalSize = container->revision.totalContainerHeight;
  double maxOffset = std::max(0.0, totalSize - WINDOW_HEIGHT);
  if (band.high < maxOffset) {
    CHECK(changesSomething(frameAt(scenario, band.high + OFFSET_BAND_MARGIN + 0.01, true), baseline));
  }
  if (band.low > 0.0) {
    CHECK(changesSomething(frameAt(scenario, band.low - OFFSET_BAND_MARGIN - 0.01, true), baseline));
  }
}

}

TEST(offset_band_is_empty_before_the_first_frame) {
  Container container;
  CHECK(container.computeOffsetBand().isEmpty());
}

TEST(offset_band_keeps_the_window_for_every_offset_inside_it) {
  BandScenario scenario;
  checkBandIsExact(scenario);
}

TEST(offset_band_is_exact_with_small_overscan) {
  BandScenario scenario;
  scenario.overscan = 0.25;
  scenario.offset = 3100.0;
  checkBandIsExact(scenario);
}

TEST(offset_band_is_exact_in_a_multi_column_list) {
  BandScenario scenario;
  scenario.columns = 3;
  scenario.rows = 300;
  // Off the integer grid, where a row edge plus the overscan can land exactly on the offset.
  scenario.offset = 2601.3;
  checkBandIsExact(scenario);
}

TEST(offset_band_stops_before_the_start_reached_threshold) {
  BandScenario scenario;
  scenario.startThreshold = 1.5;
  // Inside the threshold the band ends before leaving it, outside it ends before entering it.
  scenario.offset = 1200.3;
  auto inside = settledContainer(scenario);
  OffsetBand insideBand = inside->computeOffsetBand();
  CHECK(!insideBand.isEmpty());
  CHECK(insideBand.high <= WINDOW_HEIGHT * 1.5 - OFFSET_BAND_MARGIN);
  checkBandIsExact(scenario);

  scenario.offset = 1400.3;
  auto outside = settledContainer(scenario);
  OffsetBand outsideBand = outside->computeOffsetBand();
  CHECK(!outsideBand.isEmpty());
  CHECK(outsideBand.low >= WINDOW_HEIGHT * 1.5 + OFFSET_BAND_MARGIN);
  checkBandIsExact(scenario);
}

TEST(offset_band_at_rest_on_the_top_starts_at_zero) {
  BandScenario scenario;
  scenario.offset = 0.0;
  auto container = settledContainer(scenario);
  OffsetBand band = container->computeOffsetBand();
  CHECK(!band.isEmpty());
  CHECK_EQ(band.low, 0.0);
  checkBandIsExact(scenario);
}

TEST(offset_band_is_empty_past_the_scroll_range) {
  BandScenario scenario;
  scenario.offset = -40.0;
  auto container = settledContainer(scenario);
  CHECK(container->computeOffsetBand().isEmpty());
}

TEST(offset_band_is_empty_while_a_scroll_command_runs) {
  BandScenario scenario;
  auto container = settledContainer(scenario);
  CHECK(!container->computeOffsetBand().isEmpty());
  std::vector<std::string> keys = keysFor(scenario.rows);
  container->scrollToIndex(150);
  CHECK(container->computeOffsetBand().isEmpty());
  Virtualizer::update(container.get(), bandInput(scenario, keys, scenario.offset));
  CHECK(container->operation.has_value());
  CHECK(container->computeOffsetBand().isEmpty());
}

TEST(offset_band_is_empty_while_a_prepend_is_held_in_place) {
  BandScenario scenario;
  auto container = settledContainer(scenario);
  std::vector<std::string> keys = keysFor(scenario.rows);
  std::vector<std::string> prepended = keysFor(10, "p");
  prepended.insert(prepended.end(), keys.begin(), keys.end());
  Virtualizer::update(container.get(), bandInput(scenario, prepended, scenario.offset));
  CHECK(container->containerOffsetCorrected);
  CHECK(container->computeOffsetBand().isEmpty());
}

TEST(offset_band_is_empty_with_scroll_or_viewable_listeners) {
  BandScenario scenario;
  auto container = settledContainer(scenario);
  CHECK(!container->computeOffsetBand().isEmpty());
  container->onScrollCallback = [](double, double) {};
  CHECK(container->computeOffsetBand().isEmpty());
  container->onScrollCallback = nullptr;
  container->onViewableIndicesChangeCallback = [](std::size_t, std::size_t) {};
  CHECK(container->computeOffsetBand().isEmpty());
  container->onViewableIndicesChangeCallback = nullptr;
  container->stickyIndices = {0, 40};
  CHECK(container->computeOffsetBand().isEmpty());
}

TEST(offset_band_is_empty_while_a_measured_size_waits_for_layout) {
  BandScenario scenario;
  auto container = settledContainer(scenario);
  CHECK(!container->computeOffsetBand().isEmpty());
  container->markElementSizeDirty(3);
  CHECK(container->computeOffsetBand().isEmpty());
}

TEST(offset_band_of_an_inverted_list_stops_before_the_bottom_pin) {
  BandScenario scenario;
  scenario.inverted = true;
  scenario.offset = 6000.0;
  auto container = settledContainer(scenario);
  double maxOffset = container->revision.totalContainerHeight - WINDOW_HEIGHT;
  CHECK(scenario.offset < maxOffset - INVERTED_FOLLOW_BAND - 200.0);
  OffsetBand band = container->computeOffsetBand();
  CHECK(!band.isEmpty());
  CHECK(band.high <= maxOffset - INVERTED_FOLLOW_BAND - OFFSET_BAND_MARGIN);
  checkBandIsExact(scenario);
}
