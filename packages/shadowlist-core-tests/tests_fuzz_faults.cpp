/*
 * Randomized host simulation on hosts that stray from the ideal one the way real hosts do, see
 * HostFaults: corrections written late, never or twice, reports that arrive stale, fingers that
 * move the view before a write, rows measured late and sizes for rows the data lost. Each test
 * switches some faults on and checks what still holds. See tests_fuzz.cpp for how the fuzz
 * tests run.
 */

#include "AxisHostHelpers.hpp"
#include "FuzzHelpers.hpp"
#include "SimHostHelpers.hpp"
#include "TestFramework.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace azimgd::shadowlist;
using namespace slt;

namespace {

constexpr std::uint32_t FAULTY_FEED_SEEDS = 120;
constexpr int FAULTY_FEED_STEPS = 4;

/*
 * A prepend followed by a scroll command. A host that drops echoes never writes the prepend's
 * correction. The core keeps asking for it under one token, and the command that takes its
 * place lands where it aims.
 */
void prependThenCommand(SimHost& host, std::size_t count, bool toEnd) {
  host.droppingEchoes = host.faults.droppedEchoes;
  host.keys = withPrepend(host, count);
  host.note("prepend " + std::to_string(count) + " before a command off=" + std::to_string(host.hostOffset));
  host.commit(false);
  checkFrame(host);
  // A correction clamped to where the view already is needs no write and may end.
  bool awaitsWrite = host.droppingEchoes && host.container.state.operation &&
                     std::fabs(host.container.revision.offsetY - host.hostOffset) > FUZZ_TOLERANCE;
  std::uint64_t token = awaitsWrite ? host.container.state.operation->commitToken : 0;
  for (int frame = 0; frame < 3; ++frame) {
    host.commit(true);
    checkFrame(host);
    host.commit(false);
    checkFrame(host);
    const std::optional<Operation>& running = host.container.state.operation;
    if (awaitsWrite && (!running || running->commitToken != token)) {
      host.failWith("the core gave up a correction the host never wrote");
    }
  }
  host.droppingEchoes = false;
  if (toEnd) {
    scrollToEndAndLand(host);
  } else {
    scrollToStartAndLand(host);
  }
}

/*
 * A plain or inverted feed on a host with the given faults: data changes above, inside and
 * below the viewport, user scrolls, and scroll commands right after a prepend. The checks are
 * the ideal host's own.
 */
void runFaultyFeed(std::uint32_t seed, FuzzEpisode& episode, const HostFaults& faults) {
  SimHost host(seed, episode, faults);
  host.inverted = chance(host.rng, 30);
  host.header = chance(host.rng, 50) ? 0.0 : 100.0;
  open(host, pickSize(host.rng, 20, 59));

  for (int step = 0; step < FAULTY_FEED_STEPS; ++step) {
    int roll = pickPercent(host.rng);
    std::size_t count = pickSize(host.rng, 1, 8);
    std::size_t size = host.keys.size();
    if (roll < 25) {
      episode.reach("prepend");
      changeAndHold(host, withPrepend(host, count), "prepend " + std::to_string(count));
    } else if (roll < 37) {
      episode.reach("append");
      changeAndHold(host, withAppend(host, count), "append " + std::to_string(count));
    } else if (roll < 50) {
      episode.reach("insert");
      std::size_t at = pickSize(host.rng, 0, size);
      changeAndHold(host, withInsert(host, at, count), "insert " + std::to_string(count) + " at " + std::to_string(at));
    } else if (roll < 65) {
      episode.reach("delete");
      std::size_t at = pickSize(host.rng, 0, size - 1);
      changeAndHold(host, withRemove(host, at, count), "remove " + std::to_string(count) + " at " + std::to_string(at));
    } else if (roll < 80) {
      episode.reach("scroll");
      scrollTo(host, pickFraction(host.rng) * host.maxOffset());
    } else {
      bool toEnd = chance(host.rng, 50);
      episode.reach(toEnd ? "command to end" : "command to start");
      prependThenCommand(host, count, toEnd);
    }
    if (host.keys.empty()) {
      break;
    }
  }
}

/*
 * runFaultyFeed as the run of a seed loop.
 */
auto faultyFeed(const HostFaults& faults) {
  return [faults](std::uint32_t seed, FuzzEpisode& episode) { runFaultyFeed(seed, episode, faults); };
}

/*
 * The kinds runFaultyFeed reaches on any host, and the kinds of the faults switched on.
 */
FuzzKinds faultyFeedKinds(const HostFaults& faults) {
  FuzzKinds kinds = {
    {"prepend", "append", "insert", "delete", "scroll", "command to end", "command to start", "core maintain anchor",
     "core scroll to start", "core scroll to end", "core bottom pin"},
    {"core shrink clamp"},
    {},
  };
  std::vector<std::string> ofFaults = faultKinds(faults);
  kinds.expected.insert(kinds.expected.end(), ofFaults.begin(), ofFaults.end());
  return kinds;
}

/*
 * The faults together, as on a device under load, without stale echoes and late measurements
 * on screen.
 */
HostFaults mixedFaults() {
  HostFaults faults;
  faults.lateEchoes = true;
  faults.droppedEchoes = true;
  faults.repeatedEchoes = true;
  faults.scrollsBeforeEchoes = true;
  faults.lateMeasurements = true;
  faults.deletedKeyMeasurements = true;
  return faults;
}

}

/*
 * The write of a correction comes some mounts late. The core publishes the correction again
 * on every commit until a report confirms it, and the first row on screen ends up where it was.
 */
TEST(fuzz_a_host_with_late_echoes_still_holds_the_first_visible_row) {
  HostFaults faults;
  faults.lateEchoes = true;
  forEachFuzzSeed(__func__, FAULTY_FEED_SEEDS, faultyFeedKinds(faults), faultyFeed(faults));
}

/*
 * The host never writes a prepend's correction. The core keeps the correction and its token
 * while nothing else happens, and a scroll command sent then still lands.
 */
TEST(fuzz_a_host_with_dropped_echoes_is_asked_until_a_command_takes_over) {
  HostFaults faults;
  faults.droppedEchoes = true;
  forEachFuzzSeed(__func__, FAULTY_FEED_SEEDS, faultyFeedKinds(faults), faultyFeed(faults));
}

/*
 * The core sees a frame's input twice before the layout pass. The second run changes nothing
 * a reader could see.
 */
TEST(fuzz_a_host_with_repeated_echoes_changes_nothing_on_screen) {
  HostFaults faults;
  faults.repeatedEchoes = true;
  forEachFuzzSeed(__func__, FAULTY_FEED_SEEDS, faultyFeedKinds(faults), faultyFeed(faults));
}

/*
 * A report from before a write arrives after it. The core asks for the same correction again,
 * the host has nothing left to apply, and the row stays.
 */
TEST(fuzz_a_host_with_stale_echoes_applies_no_correction_twice) {
  HostFaults faults;
  faults.staleEchoes = true;
  forEachFuzzSeed(__func__, FAULTY_FEED_SEEDS, faultyFeedKinds(faults), faultyFeed(faults));
}

/*
 * A finger moves the view after the core asked for a correction and before the host wrote
 * it. The row the reader held is where it was minus how far the finger went.
 */
TEST(fuzz_a_host_with_scrolls_before_an_echo_takes_the_held_row_along) {
  HostFaults faults;
  faults.scrollsBeforeEchoes = true;
  forEachFuzzSeed(__func__, FAULTY_FEED_SEEDS, faultyFeedKinds(faults), faultyFeed(faults));
}

/*
 * Rows off screen are measured some layout passes after they enter the mounted range. The
 * row the reader was on holds while the rows around the viewport take their real sizes.
 */
TEST(fuzz_a_host_with_late_measurements_still_holds_the_first_visible_row) {
  HostFaults faults;
  faults.lateMeasurements = true;
  forEachFuzzSeed(__func__, FAULTY_FEED_SEEDS, faultyFeedKinds(faults), faultyFeed(faults));
}

/*
 * Rows on screen are measured late as well. A scroll to the end still ends on the end, and the
 * row the reader was on holds while the rows above it on screen take their real sizes.
 */
TEST(fuzz_a_host_with_late_measurements_on_screen_still_holds_the_first_visible_row) {
  HostFaults faults;
  faults.lateMeasurements = true;
  faults.lateMeasurementsOnScreen = true;
  forEachFuzzSeed(__func__, FAULTY_FEED_SEEDS, faultyFeedKinds(faults), faultyFeed(faults));
}

/*
 * The host still has the rows a data change deleted and their sizes. A deleted key finds no
 * row, a size predicted for it lands on none, and the rows that stayed hold.
 */
TEST(fuzz_a_host_with_measurements_for_deleted_keys_reaches_no_row) {
  HostFaults faults;
  faults.deletedKeyMeasurements = true;
  forEachFuzzSeed(__func__, FAULTY_FEED_SEEDS, faultyFeedKinds(faults), faultyFeed(faults));
}

/*
 * The faults of mixedFaults at once.
 */
TEST(fuzz_a_host_with_mixed_faults_still_holds_the_first_visible_row) {
  HostFaults faults = mixedFaults();
  forEachFuzzSeed(__func__, FAULTY_FEED_SEEDS, faultyFeedKinds(faults), faultyFeed(faults));
}

/*
 * Every fault at once, including reports from before later correction writes and rows that
 * measure after the view reached a short end.
 */
TEST(fuzz_a_host_with_every_fault_still_holds_the_first_visible_row) {
  HostFaults faults = mixedFaults();
  faults.staleEchoes = true;
  faults.lateMeasurementsOnScreen = true;
  forEachFuzzSeed(__func__, FAULTY_FEED_SEEDS, faultyFeedKinds(faults), faultyFeed(faults));
}

/*
 * Grids and horizontal lists on a host with the faults AxisHost has a model of, without late
 * measurements on screen. Changes by whole rows of tracks hold the top row, also when a
 * measured size arrives after a hold reached a short end.
 */
TEST(fuzz_a_grid_or_horizontal_host_with_faults_holds_the_top_row) {
  const FuzzKinds kinds = {
    {"prepend", "append", "scroll", "core maintain anchor", "late echo", "repeated echo", "late measurement"},
    {"core shrink clamp"},
    {"core shrink clamp"},
  };
  forEachFuzzSeed(__func__, FAULTY_FEED_SEEDS, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    HostFaults faults;
    faults.lateEchoes = true;
    faults.repeatedEchoes = true;
    faults.lateMeasurements = true;
    AxisHost host(seed, episode, faults);
    int shape = pickInt(host.rng, 0, 1);
    host.numberOfColumns = shape == 0 ? pickSize(host.rng, 2, 4) : 1;
    host.horizontal = shape == 1;
    host.header = chance(host.rng, 50) ? 0.0 : 80.0;
    axisOpen(host, pickSize(host.rng, 30, 69));
    for (int step = 0; step < FAULTY_FEED_STEPS; ++step) {
      int roll = pickPercent(host.rng);
      std::size_t count = host.numberOfColumns * pickSize(host.rng, 1, 4);
      if (roll < 40) {
        episode.reach("prepend");
        axisChangeAndHold(host, withPrepend(host, count), "prepend " + std::to_string(count));
      } else if (roll < 65) {
        episode.reach("append");
        axisChangeAndHold(host, withAppend(host, count), "append " + std::to_string(count));
      } else {
        episode.reach("scroll");
        axisScrollTo(host, pickFraction(host.rng) * host.maxOffset());
      }
    }
  });
}

namespace {

/*
 * The held row is on the second track, cut short by the end. The first row of the other
 * track is above the end viewport and can arrive after the hold settles.
 */
FrameInput clampedGridHold(Container& container) {
  const std::vector<std::string> keys = keysFor(12);
  FrameInput frame = inputFor(keys, 0.0);
  frame.numberOfColumns = 2;
  frame.estimatedRowSize = {WINDOW_WIDTH / 2.0, 200.0};
  Virtualizer::update(container, frame);
  for (std::size_t index = 1; index < keys.size(); ++index) {
    Virtualizer::applyRowSize(container, index, {WINDOW_WIDTH / 2.0, 200.0});
  }
  Virtualizer::commitRowSizes(container, 1);
  Virtualizer::recomputeContentSize(container);
  container.state.operation = Operation{1, OperationType::MaintainAnchor, Anchor{"k7", 100.0, AnchorMode::Row}};
  container.state.nextCommitToken = 2;
  frame.offsetY = 360.0;
  frame.commitToken = 1;
  for (int pass = 0; pass < 5; ++pass) {
    Virtualizer::update(container, frame);
  }
  CHECK(!container.state.operation);
  CHECK(container.state.clampedAnchor.has_value());
  return frame;
}

}

TEST(a_grid_row_hold_settles_and_recovers_when_late_sizes_make_room) {
  Container container;
  FrameInput frame = clampedGridHold(container);
  // Several sizes still leave the target out of reach. Its original sub offset stays intact.
  for (double height : {300.0, 400.0}) {
    Virtualizer::applyRowSize(container, 0, {WINDOW_WIDTH / 2.0, height});
    Virtualizer::commitRowSizes(container, 0);
    Virtualizer::recomputeContentSize(container);
    CHECK(container.state.clampedAnchor.has_value());
    CHECK_NEAR(container.state.clampedAnchor->offset, 100.0, FUZZ_TOLERANCE);
    frame.offsetY = container.revision.offsetY;
    Virtualizer::update(container, frame);
    CHECK(!container.state.operation);
  }
  Virtualizer::applyRowSize(container, 0, {WINDOW_WIDTH / 2.0, 600.0});
  Virtualizer::commitRowSizes(container, 0);
  Virtualizer::recomputeContentSize(container);
  CHECK_NEAR(container.revision.offsetY, 700.0, FUZZ_TOLERANCE);
  CHECK(!container.state.clampedAnchor);
  frame.offsetY = container.revision.offsetY;
  for (int pass = 0; pass < 5; ++pass) {
    Virtualizer::update(container, frame);
  }
  CHECK(!container.state.operation);
}

TEST(a_user_scroll_releases_a_grid_row_hold_cut_short_by_the_end) {
  Container container;
  FrameInput frame = clampedGridHold(container);
  frame.offsetY = 300.0;
  frame.scrollPhase = ScrollPhase::Dragging;
  frame.userScrolled = true;
  Virtualizer::update(container, frame);
  CHECK(!container.state.clampedAnchor);
  double screen = offsetOf(container, 7) - container.revision.offsetY;
  Virtualizer::applyRowSize(container, 0, {WINDOW_WIDTH / 2.0, 600.0});
  Virtualizer::commitRowSizes(container, 0);
  Virtualizer::recomputeContentSize(container);
  CHECK_NEAR(offsetOf(container, 7) - container.revision.offsetY, screen, FUZZ_TOLERANCE);
}

TEST(a_scroll_command_releases_a_grid_row_hold_cut_short_by_the_end) {
  for (int command = 0; command < 3; ++command) {
    Container container;
    clampedGridHold(container);
    if (command == 0) {
      container.scrollToStart();
    } else if (command == 1) {
      container.scrollToEnd();
    } else {
      container.scrollToRow(3);
    }
    CHECK(!container.state.clampedAnchor);
  }
}

TEST(deleting_the_held_row_releases_a_grid_row_hold_cut_short_by_the_end) {
  Container container;
  FrameInput frame = clampedGridHold(container);
  frame.keys.erase(frame.keys.begin() + 7);
  Virtualizer::update(container, frame);
  CHECK(!container.state.clampedAnchor);
}

TEST(a_new_data_change_releases_a_grid_row_hold_cut_short_by_the_end) {
  Container container;
  FrameInput frame = clampedGridHold(container);
  frame.keys.push_back("appended");
  Virtualizer::update(container, frame);
  CHECK(!container.state.clampedAnchor);
}

/*
 * Measurements alone can shorten the end without starting an operation. The host clamps
 * its content offset before the next report, and later sizes can make room again.
 */
TEST(a_measurement_clamp_keeps_the_row_until_late_sizes_make_room) {
  Container container;
  FrameInput frame = inputFor(keysFor(12), 0.0);
  frame.numberOfColumns = 2;
  frame.estimatedRowSize = {WINDOW_WIDTH / 2.0, 200.0};
  Virtualizer::update(container, frame);
  for (std::size_t index = 0; index < frame.keys.size(); ++index) {
    if (index != 1) {
      Virtualizer::applyRowSize(container, index, {WINDOW_WIDTH / 2.0, 200.0});
    }
  }
  Virtualizer::commitRowSizes(container, 0);
  Virtualizer::recomputeContentSize(container);
  frame.offsetY = 350.0;
  Virtualizer::update(container, frame);
  CHECK_EQ(container.state.anchor.key, std::string("k2"));
  Virtualizer::applyRowSize(container, 10, {WINDOW_WIDTH / 2.0, 100.0});
  Virtualizer::applyRowSize(container, 11, {WINDOW_WIDTH / 2.0, 100.0});
  Virtualizer::commitRowSizes(container, 10);
  Virtualizer::recomputeContentSize(container);
  CHECK(!container.state.operation);
  // The view clamps to the short end even when the core needed no offset write.
  frame.offsetY = 260.0;
  Virtualizer::update(container, frame);
  Virtualizer::applyRowSize(container, 1, {WINDOW_WIDTH / 2.0, 400.0});
  Virtualizer::commitRowSizes(container, 1);
  Virtualizer::recomputeContentSize(container);
  CHECK_NEAR(container.revision.offsetY, 350.0, FUZZ_TOLERANCE);
  CHECK(!container.state.clampedAnchor);
}
