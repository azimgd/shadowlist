/*
 * Mounted range, viewability and prefetch tests, and the MountDriver that runs them for Fabric.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/MountDriver.hpp>
#include <shadowlist-core/host/MountedRange.hpp>
#include <shadowlist-core/host/Prefetch.hpp>
#include <shadowlist-core/host/Viewability.hpp>

#include <string>
#include <unordered_map>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

bool sameRange(const MountedRange& range, std::size_t low, std::size_t high) {
  return range.low == low && range.high == high;
}

}

TEST(mounted_range_steps_toward_the_target) {
  CHECK(sameRange(stepMountedRange({10, 20}, {14, 32}, {18, 21}, 2), 14, 22));
  CHECK(sameRange(stepMountedRange({10, 20}, {16, 36}, {22, 26}, 2), 16, 26));
  CHECK(sameRange(stepMountedRange({10, 20}, {0, 16}, {10, 12}, 2), 8, 16));
  CHECK(sameRange(stepMountedRange({0, 20}, {496, 513}, {500, 503}, 2), 498, 505));
  CHECK(sameRange(stepMountedRange({14, 30}, {14, 32}, {18, 21}, 2), 14, 32));
}

TEST(mounted_range_step_scales_with_the_measured_range) {
  CHECK_EQ(mountStepForRange({10, 15}, 2), std::size_t{2});
  CHECK_EQ(mountStepForRange({875, 910}, 2), std::size_t{9});
  MountedRange measured{875, 910};
  std::size_t step = mountStepForRange(measured, 2);
  MountedRange range{875, 914};
  MountedRange target{865, 914};
  range = stepMountedRange(range, target, measured, step);
  range = stepMountedRange(range, target, measured, step);
  CHECK(range == target);
}

TEST(mounted_range_visible_target_pads_ahead) {
  CHECK(sameRange(visibleTargetRange({50, 55}, std::nullopt, 200, 4, 12), 46, 59));
  CHECK(sameRange(visibleTargetRange({50, 55}, MountedRange{45, 50}, 200, 4, 12), 46, 67));
  CHECK(sameRange(visibleTargetRange({50, 55}, MountedRange{60, 65}, 200, 4, 12), 38, 59));
  CHECK(sameRange(visibleTargetRange({2, 8}, std::nullopt, 10, 4, 12), 0, 9));
}

TEST(mounted_range_first_report_trims_the_initial_range) {
  // Feed: 21 initial rows, 3 on screen. The first report keeps the screen plus overscan.
  auto first = reportedMountedRange({0, 20}, {0, 2}, std::nullopt, true, 100, 4, 10, 2);
  CHECK(first.has_value());
  CHECK(sameRange(first->range, 0, 6));
  CHECK(sameRange(first->target, 0, 6));
  // Later reports inside the range keep it.
  CHECK(!reportedMountedRange({0, 20}, {0, 2}, MountedRange{0, 2}, false, 100, 4, 10, 2).has_value());
  // Inverted chat starts at the tail.
  auto tail = reportedMountedRange({80, 99}, {95, 99}, std::nullopt, true, 100, 4, 10, 2);
  CHECK(sameRange(tail->range, 91, 99));
  // A start target keeps the rows around it.
  auto seeded = reportedMountedRange({396, 420}, {400, 403}, std::nullopt, true, 1000, 4, 10, 2);
  CHECK(sameRange(seeded->range, 396, 407));
  // A measured range past the initial range mounts the screen now and paces the pad.
  auto past = reportedMountedRange({0, 20}, {30, 33}, std::nullopt, true, 100, 4, 10, 2);
  CHECK(sameRange(past->range, 28, 35));
  CHECK(sameRange(past->target, 26, 37));
  // A measured range outside the mounted range moves it on any report.
  auto moved = reportedMountedRange({0, 20}, {30, 33}, MountedRange{20, 23}, false, 100, 4, 10, 2);
  CHECK(sameRange(moved->target, 26, 43));
}

TEST(mounted_range_shrinks_a_range_inserted_rows_made_far_too_big) {
  // A tree expanding every folder inside the range.
  auto shrunk = reportedMountedRange({0, 299}, {0, 12}, MountedRange{0, 12}, false, 300, 4, 10, 2);
  CHECK(shrunk.has_value());
  CHECK(sameRange(shrunk->range, 0, 16));
  CHECK(sameRange(shrunk->target, 0, 16));
  // A followed tail burst inside the limit is kept.
  CHECK(!reportedMountedRange({140, 199}, {190, 199}, MountedRange{189, 198}, false, 200, 4, 10, 2).has_value());
}

TEST(viewability_range_and_sticky_index) {
  CHECK(viewableRange(4, 12) == MountedRange({4, 12}));
  CHECK(!viewableRange(UNDEFINED_INDEX, UNDEFINED_INDEX).has_value());
  CHECK(!viewableRange(3, UNDEFINED_INDEX).has_value());

  std::vector<std::size_t> headers{0, 10, 25};
  CHECK_EQ(activeStickyIndexFor(headers, 0), std::size_t{0});
  CHECK_EQ(activeStickyIndexFor(headers, 9), std::size_t{0});
  CHECK_EQ(activeStickyIndexFor(headers, 10), std::size_t{10});
  CHECK_EQ(activeStickyIndexFor(headers, 400), std::size_t{25});
  CHECK_EQ(activeStickyIndexFor({3, 8}, 1), UNDEFINED_INDEX);
  CHECK_EQ(activeStickyIndexFor({}, 5), UNDEFINED_INDEX);
}

TEST(viewability_changes_by_key) {
  ViewableChanges changes = viewableChanges({"a", "b", "c"}, {"b", "c", "d", "e"});
  CHECK(changes.added == std::vector<std::string>({"d", "e"}));
  CHECK(changes.removed == std::vector<std::string>({"a"}));
  // A reorder inside the measured range changes nothing.
  ViewableChanges reorder = viewableChanges({"a", "b"}, {"b", "a"});
  CHECK(reorder.added.empty());
  CHECK(reorder.removed.empty());
}

TEST(viewability_tracker_reports_changes_by_key) {
  ViewabilityTracker tracker;
  ViewabilityReport report;
  CHECK(!tracker.take(0.0, report));
  tracker.update({"a", "b"}, 0.0);
  CHECK(tracker.take(0.0, report));
  CHECK(report.viewable == std::vector<std::string>({"a", "b"}));
  CHECK(report.changes.added == std::vector<std::string>({"a", "b"}));
  // The same keys, or the same keys reordered, report nothing.
  tracker.update({"b", "a"}, 1.0);
  CHECK(!tracker.take(1.0, report));
  tracker.update({"b", "c"}, 2.0);
  CHECK(tracker.take(2.0, report));
  CHECK(report.changes.added == std::vector<std::string>({"c"}));
  CHECK(report.changes.removed == std::vector<std::string>({"a"}));
}

TEST(viewability_tracker_waits_for_minimum_view_time_and_interaction) {
  ViewabilityTracker timed;
  timed.setTiming({250.0, false});
  ViewabilityReport report;
  timed.update({"a"}, 0.0);
  CHECK(timed.isWaiting());
  CHECK(!timed.take(100.0, report));
  // Each change starts the wait again, and the rows viewable when it ends are reported. The
  // same rows again are no change.
  timed.update({"b"}, 100.0);
  timed.update({"b"}, 200.0);
  CHECK(!timed.take(300.0, report));
  CHECK(timed.take(350.0, report));
  CHECK(report.viewable == std::vector<std::string>({"b"}));
  CHECK(!timed.isWaiting());

  ViewabilityTracker interaction;
  interaction.setTiming({0.0, true});
  interaction.update({"a"}, 0.0);
  CHECK(!interaction.take(10.0, report));
  interaction.recordInteraction(20.0);
  CHECK(interaction.take(20.0, report));
  CHECK(report.viewable == std::vector<std::string>({"a"}));
}

TEST(prefetch_tracker_prefetches_range_rows_once_and_cancels_them) {
  std::vector<std::string> keys;
  std::unordered_map<std::string, std::size_t> indices;
  for (std::size_t index = 0; index < 100; ++index) {
    keys.push_back("k" + std::to_string(index));
    indices[keys.back()] = index;
  }
  auto keyAt = [&](std::size_t index) -> const std::string& { return keys[index]; };
  auto indexOfKey = [&](const std::string& key) {
    auto found = indices.find(key);
    return found == indices.end() ? UNDEFINED_INDEX : found->second;
  };
  PrefetchTracker tracker;
  std::vector<std::size_t> prefetch;
  std::vector<std::size_t> cancel;
  MountedRange mounted{10, 20};
  tracker.update(mounted, prefetchRange(mounted, 3, keys.size()), keys.size(), keyAt, indexOfKey, prefetch, cancel);
  CHECK(prefetch == std::vector<std::size_t>({7, 8, 9, 21, 22, 23}));
  CHECK(cancel.empty());
  tracker.update(mounted, prefetchRange(mounted, 3, keys.size()), keys.size(), keyAt, indexOfKey, prefetch, cancel);
  CHECK(prefetch.empty());
  // Scrolled down: rows behind leave the range and are cancelled, mounted rows are dropped and
  // a row still in the range is not prefetched twice.
  MountedRange later{22, 32};
  tracker.update(later, prefetchRange(later, 3, keys.size()), keys.size(), keyAt, indexOfKey, prefetch, cancel);
  CHECK(prefetch == std::vector<std::size_t>({19, 20, 33, 34, 35}));
  CHECK(cancel == std::vector<std::size_t>({7, 8, 9}));
  CHECK(!prefetchRange(std::nullopt, 3, 100).has_value());
  CHECK(!prefetchRange(MountedRange{0, 5}, 0, 100).has_value());
  CHECK(sameRange(*prefetchRange(MountedRange{1, 98}, 3, 100), 0, 99));
}

namespace {

/*
 * A renderer for MountDriver tests: it applies every range it gets, like Fabric's JS.
 */
struct MountHost {
  std::vector<std::string> keys;
  MountDriver driver;
  MountedBasis basis;

  explicit MountHost(std::size_t count, MountedRange initial) {
    for (std::size_t index = 0; index < count; ++index) {
      keys.push_back("k" + std::to_string(index));
    }
    basis.range = initial;
  }

  std::size_t indexOf(const std::string& key) const {
    for (std::size_t index = 0; index < keys.size(); ++index) {
      if (keys[index] == key) {
        return index;
      }
    }
    return UNDEFINED_INDEX;
  }

  /*
   * One commit: the basis, an optional report, then process.
   */
  std::optional<MountMessage> commit(std::optional<MountedRange> measured = std::nullopt) {
    driver.setBasis(basis);
    if (measured) {
      driver.reportMeasured(*measured);
    }
    return driver.process(keys, [this](const std::string& key) { return indexOf(key); });
  }

  void apply(const MountMessage& message) {
    basis.range = {indexOf(message.lowKey), indexOf(message.highKey)};
    basis.sequence = message.sequence;
  }
};

}

TEST(mount_driver_first_report_trims_and_later_reports_hold) {
  MountHost host(100, {0, 20});
  auto first = host.commit(MountedRange{0, 2});
  CHECK(first.has_value());
  CHECK(sameRange(first->range, 0, 6));
  CHECK(first->lowAtStart);
  CHECK(!first->highAtEnd);
  CHECK_EQ(first->sequence, std::uint64_t{1});
  CHECK(host.driver.isPending());
  host.apply(*first);
  // A report inside the applied range sends nothing.
  CHECK(!host.commit(MountedRange{0, 3}).has_value());
  CHECK(!host.driver.isPending());
}

TEST(mount_driver_holds_reports_until_the_range_in_flight_is_applied) {
  MountHost host(100, {0, 20});
  auto first = host.commit(MountedRange{30, 33});
  CHECK(sameRange(first->range, 28, 35));
  // The renderer has not applied it yet. A newer report waits.
  CHECK(!host.commit(MountedRange{40, 43}).has_value());
  host.apply(*first);
  auto next = host.commit();
  CHECK(next.has_value());
  CHECK(sameRange(next->range, 38, 45));
}

TEST(mount_driver_paces_the_pad_one_step_per_frame) {
  MountHost host(1000, {0, 20});
  auto first = host.commit(MountedRange{30, 33});
  CHECK(sameRange(first->range, 28, 35));
  host.apply(*first);
  CHECK(!host.commit().has_value());
  // Target 26..37 is two rows away on each end: one step reaches it.
  CHECK(host.driver.hasStep());
  auto step = host.driver.takeStep();
  CHECK(step.has_value());
  CHECK(sameRange(step->range, 26, 37));
  CHECK(!host.driver.hasStep());
  CHECK(!host.driver.takeStep().has_value());
  host.apply(*step);
  CHECK(!host.commit().has_value());
  CHECK(!host.driver.hasStep());

  // A scroll forward mounts the screen now and builds the leading pad of 10 two rows per step.
  auto moved = host.commit(MountedRange{40, 43});
  CHECK(sameRange(moved->range, 36, 43));
  host.apply(*moved);
  std::size_t steps = 0;
  for (;;) {
    host.commit();
    auto paced = host.driver.takeStep();
    if (!paced) {
      break;
    }
    ++steps;
    host.apply(*paced);
  }
  CHECK_EQ(steps, std::size_t{5});
  CHECK(sameRange(host.basis.range, 36, 53));
}

TEST(mount_driver_steps_keep_their_rows_across_a_data_change) {
  MountHost host(1000, {0, 20});
  auto moved = host.commit(MountedRange{40, 43});
  host.apply(*moved);
  host.commit();
  CHECK(host.driver.hasStep());
  // Ten rows prepended before the step's frame: the step follows its keys.
  std::vector<std::string> prepended;
  for (std::size_t index = 0; index < 10; ++index) {
    prepended.push_back("new" + std::to_string(index));
  }
  host.keys.insert(host.keys.begin(), prepended.begin(), prepended.end());
  host.basis.range = {host.basis.range.low + 10, host.basis.range.high + 10};
  host.commit();
  auto step = host.driver.takeStep();
  CHECK(step.has_value());
  CHECK_EQ(step->lowKey, std::string("k36"));
}

TEST(mount_driver_drops_ranges_of_an_older_generation) {
  MountHost host(1000, {0, 20});
  auto first = host.commit(MountedRange{30, 33});
  CHECK(host.driver.isPending());
  // The renderer reseeded around a new scroll target before applying it.
  host.basis.range = {496, 520};
  host.basis.sequence = 0;
  host.basis.generation = 1;
  CHECK(!host.commit().has_value());
  CHECK(!host.driver.isPending());
  // The next report is a first report again, against the reseeded range. It trims the range to
  // the screen plus the leading pad of the scroll from the old screen.
  auto reseeded = host.commit(MountedRange{500, 503});
  CHECK(reseeded.has_value());
  CHECK_EQ(reseeded->generation, std::uint64_t{1});
  CHECK(sameRange(reseeded->range, 496, 513));
  (void)first;
}

TEST(mount_driver_says_a_scroll_command_landed) {
  MountHost host(100, {0, 20});
  host.apply(*host.commit(MountedRange{0, 2}));
  host.basis.seedPending = true;
  auto landed = host.commit(MountedRange{0, 3});
  CHECK(landed.has_value());
  CHECK(landed->landed);
  CHECK_EQ(landed->sequence, std::uint64_t{0});
  CHECK(!host.driver.isPending());
}
