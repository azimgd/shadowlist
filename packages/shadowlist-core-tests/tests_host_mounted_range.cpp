/*
 * Mounted range and viewability tests. The same cases as the TypeScript tests in
 * shadowlist-fabric/src/__tests__/mountedRange.test.ts and viewability.test.ts. The two
 * implementations stay in step.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/MountedRange.hpp>

#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

bool sameRange(const MountedRange& range, std::size_t low, std::size_t high) {
  return range.low == low && range.high == high;
}

}

TEST(mounted_range_initial_seeds_around_a_target) {
  MountedRange range = initialMountedRange(1000, 20, false, 400);
  CHECK(sameRange(range, 400 - DEFAULT_OVERSCAN_ROWS, 420));
  for (std::size_t noTarget : {UNDEFINED_INDEX}) {
    CHECK(sameRange(initialMountedRange(1000, 20, false, noTarget), 0, 20));
  }
  CHECK(sameRange(initialMountedRange(1000, 20, true, UNDEFINED_INDEX), 980, 999));
  CHECK(sameRange(initialMountedRange(1000, 20, false, 400, 4), 396, 420));
  // viewPosition 0.5 puts half the initial rows before the target.
  CHECK(sameRange(initialMountedRange(1000, 20, false, 400, 4, 0.5), 386, 410));
  CHECK(sameRange(initialMountedRange(50, 20, false, 400, 4), 45, 49));
  CHECK(sameRange(initialMountedRange(0, 20, false, UNDEFINED_INDEX), UNDEFINED_INDEX, UNDEFINED_INDEX));
}

TEST(mounted_range_reseed_rules) {
  CHECK(shouldReseedFromOffsetIndex(UNDEFINED_INDEX, 40));
  CHECK(shouldReseedFromOffsetIndex(40, 41));
  CHECK(!shouldReseedFromOffsetIndex(40, 40));
  CHECK(!shouldReseedFromOffsetIndex(40, UNDEFINED_INDEX));
}

TEST(mounted_range_union_keeps_both_runs) {
  CHECK(unionRangeIndices({2, 5}, {40, 42}) == std::vector<std::size_t>({2, 3, 4, 5, 40, 41, 42}));
  CHECK(unionRangeIndices({40, 42}, {2, 5}) == std::vector<std::size_t>({2, 3, 4, 5, 40, 41, 42}));
  CHECK(unionRangeIndices({2, 6}, {5, 8}) == std::vector<std::size_t>({2, 3, 4, 5, 6, 7, 8}));
  CHECK(unionRangeIndices({2, 4}, {5, 6}) == std::vector<std::size_t>({2, 3, 4, 5, 6}));
  CHECK(unionRangeIndices({0, 9}, {3, 4}) == rangeToIndices({0, 9}));
  CHECK(unionRangeIndices({UNDEFINED_INDEX, UNDEFINED_INDEX}, {3, 4}) == std::vector<std::size_t>({3, 4}));
  CHECK(unionRangeIndices({3, 4}, {UNDEFINED_INDEX, UNDEFINED_INDEX}) == std::vector<std::size_t>({3, 4}));
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
  MountedRange window{875, 910};
  std::size_t step = mountStepForRange(window, 2);
  MountedRange range{875, 914};
  MountedRange target{865, 914};
  range = stepMountedRange(range, target, window, step);
  range = stepMountedRange(range, target, window, step);
  CHECK(range == target);
}

TEST(mounted_range_grows_at_data_edges) {
  CHECK(sameRange(grownMountedRange(30, 20, false, false, 100, 10, false), 20, 30));
  CHECK(sameRange(grownMountedRange(5, 20, true, false, 100, 10, false), 0, 20));
  CHECK(sameRange(grownMountedRange(80, 95, false, true, 100, 10, false), 80, 99));
  CHECK(sameRange(grownMountedRange(60, 69, false, true, 100, 10, true), 60, 99));
  std::size_t size = 70 + MAX_FOLLOWED_APPEND + 100;
  CHECK(sameRange(grownMountedRange(60, 69, false, true, size, 10, true), size - 1 - 9 - MAX_FOLLOWED_APPEND, size - 1));
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
  // A window past the initial range mounts the screen now and paces the pad.
  auto past = reportedMountedRange({0, 20}, {30, 33}, std::nullopt, true, 100, 4, 10, 2);
  CHECK(sameRange(past->range, 28, 35));
  CHECK(sameRange(past->target, 26, 37));
  // A window outside the range moves it on any report.
  auto moved = reportedMountedRange({0, 20}, {30, 33}, MountedRange{20, 23}, false, 100, 4, 10, 2);
  CHECK(sameRange(moved->target, 26, 43));
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
  // A reorder inside the window changes nothing.
  ViewableChanges reorder = viewableChanges({"a", "b"}, {"b", "a"});
  CHECK(reorder.added.empty());
  CHECK(reorder.removed.empty());
}
