/*
 * What the kits' JNI and wasm bridges share: numbers from the bridge turned into the driver's
 * types, out of range ones included, and the pass slots and commit result they marshal.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/KitBridge.hpp>

#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(kit_bridge_numbers_out_of_range_turn_into_the_resting_values) {
  CHECK(kit::scrollPhaseFrom(1.0) == ScrollPhase::Dragging);
  CHECK(kit::scrollPhaseFrom(2.0) == ScrollPhase::Settling);
  CHECK(kit::scrollPhaseFrom(1.5) == ScrollPhase::Idle);
  CHECK(kit::scrollPhaseFrom(7.0) == ScrollPhase::Idle);
  CHECK(kit::snapAlignmentFrom(2) == SnapAlignment::End);
  CHECK(kit::snapAlignmentFrom(-1) == SnapAlignment::Start);
  CHECK(kit::landingFrom(3, 4, 0.5).target == ScrollLanding::Target::End);
  CHECK(kit::landingFrom(9, 4, 0.5).target == ScrollLanding::Target::None);
  CHECK_EQ(kit::landingFrom(1, -2, 0.0).index, std::size_t{0});
  CHECK_EQ(kit::indexFrom(-1), UNDEFINED_INDEX);
  CHECK_EQ(kit::intFrom(UNDEFINED_INDEX), -1);
}

TEST(kit_bridge_settings_count_at_least_one_column) {
  ListSettings settings = kit::listSettingsFrom(40.0, 1.0, 1.0, 1.0, 0, false, false, false, false, 1);
  CHECK_EQ(settings.numberOfColumns, std::size_t{1});
  CHECK(settings.snapAlignment == SnapAlignment::Center);
}

TEST(kit_bridge_pass_slots_carry_a_pass_there_and_back) {
  ListDriver driver;
  driver.setMeasureRow([](std::size_t, const std::string&, double) { return 50.0; });
  std::vector<std::string> keys;
  for (int index = 0; index < 100; ++index) {
    keys.push_back("k" + std::to_string(index));
  }
  driver.setKeys(keys);
  double slots[kit::PASS_SLOTS] = {};
  slots[kit::PASS_OFFSET] = 120.0;
  slots[kit::PASS_WINDOW_ALONG] = 400.0;
  slots[kit::PASS_WINDOW_CROSS] = 300.0;
  slots[kit::PASS_PHASE] = 1.0;
  slots[kit::PASS_USER_SCROLLED] = 1.0;
  PassInput input = kit::readPassInput(slots);
  CHECK(input.phase == ScrollPhase::Dragging);
  CHECK(input.userScrolled);
  PassResult result = driver.runPasses(input);
  kit::writePassResult(driver, result, slots);
  CHECK_EQ(slots[kit::PASS_OUT_CONTENT], 5000.0);
  CHECK(slots[kit::PASS_OUT_MEASURED_LOW] >= 0.0);
  std::vector<double> rects(8);
  CHECK_EQ(kit::copyRowRects(driver, 2, 3, rects.data()), std::size_t{8});
  CHECK_EQ(rects[1], 100.0);
  CHECK_EQ(rects[7], 50.0);
  CHECK_EQ(kit::copyRowRects(driver, 3, 2, rects.data()), std::size_t{0});
  CHECK_EQ(kit::copyRowRects(driver, 99, 100, rects.data()), std::size_t{0});
}

TEST(kit_bridge_packs_a_commit_as_counts_then_indices) {
  KeyDiff diff;
  diff.deleted = {1};
  diff.inserted = {0, 4};
  diff.moved = {{2, 3}};
  std::vector<std::int32_t> packed = kit::packCommit(diff, {5});
  std::vector<std::int32_t> expected = {1, 1, 2, 0, 4, 1, 2, 3, 1, 5};
  CHECK(packed == expected);
}
