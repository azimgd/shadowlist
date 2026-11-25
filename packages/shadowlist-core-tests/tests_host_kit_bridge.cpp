/*
 * What the kits' JNI and wasm bridges share: numbers from the bridge turned into the driver's
 * types, out of range ones included, and the pass slots and commit result they marshal.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/KitBridge.hpp>

#include <cmath>
#include <limits>
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

TEST(kit_bridge_mount_slots_reject_non_integral_and_out_of_range_numbers) {
  double slots[kit::MOUNT_SLOTS] = {};
  const double invalid[] = {
    -1.0,
    -0.5,
    0.5,
    std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::infinity(),
    -std::numeric_limits<double>::infinity(),
    std::ldexp(1.0, 64)};
  for (double value : invalid) {
    slots[kit::MOUNT_LOW] = value;
    slots[kit::MOUNT_HIGH] = value;
    slots[kit::MOUNT_STICKY] = value;
    slots[kit::MOUNT_GEOMETRY] = value;
    slots[kit::MOUNT_STRUCTURE] = value;
    MountRecord record = kit::readMountRecord(slots);
    CHECK_EQ(record.low, UNDEFINED_INDEX);
    CHECK_EQ(record.high, UNDEFINED_INDEX);
    CHECK_EQ(record.sticky, UNDEFINED_INDEX);
    CHECK_EQ(record.geometry, std::numeric_limits<std::uint64_t>::max());
    CHECK_EQ(record.structure, std::numeric_limits<std::uint64_t>::max());
  }
}

TEST(kit_bridge_versions_keep_the_unsigned_range_without_a_signed_intermediate) {
  CHECK_EQ(kit::versionFrom(0.0), std::uint64_t{0});
  CHECK_EQ(kit::versionFrom(std::ldexp(1.0, 63)), std::uint64_t{1} << 63);
  double largest = std::nextafter(std::ldexp(1.0, 64), 0.0);
  CHECK_EQ(kit::versionFrom(largest), std::numeric_limits<std::uint64_t>::max() - 2047);
}

TEST(kit_bridge_indices_do_not_wrap_when_the_bridge_is_narrower) {
  CHECK_EQ(
    kit::intFrom(static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())),
    std::numeric_limits<std::int32_t>::max());
  CHECK_EQ(kit::intFrom(static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) + 1), -1);
  CHECK_EQ(kit::indexFrom(std::int64_t{0}), std::size_t{0});
  if (sizeof(std::size_t) < sizeof(std::int64_t)) {
    CHECK_EQ(kit::indexFrom(std::numeric_limits<std::int64_t>::max()), UNDEFINED_INDEX);
  }
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
  slots[kit::PASS_CLAMP_END] = 1.0;
  slots[kit::PASS_CLAMP_SLACK] = 0.5;
  PassInput input = kit::readPassInput(slots);
  CHECK(input.phase == ScrollPhase::Dragging);
  CHECK(input.userScrolled);
  CHECK(!input.clampStart);
  CHECK(input.clampEnd);
  CHECK_EQ(input.clampSlack, 0.5);
  PassResult result = driver.runPasses(input);
  kit::writePassResult(driver, result, slots);
  CHECK_EQ(slots[kit::PASS_OUT_CONTENT], 5000.0);
  CHECK(slots[kit::PASS_OUT_MEASURED_LOW] >= 0.0);
  CHECK_EQ(slots[kit::PASS_OUT_AT_END], 1.0);
  CHECK_EQ(slots[kit::PASS_OUT_SHOWS_EMPTY], 0.0);
  std::vector<double> rects(8);
  CHECK_EQ(kit::copyRowRects(driver, 2, 3, rects.data()), std::size_t{8});
  CHECK_EQ(rects[1], 100.0);
  CHECK_EQ(rects[7], 50.0);
  CHECK_EQ(kit::copyRowRects(driver, 3, 2, rects.data()), std::size_t{0});
  CHECK_EQ(kit::copyRowRects(driver, 99, 100, rects.data()), std::size_t{0});
}

TEST(kit_bridge_pass_slots_carry_the_empty_view_of_a_list_without_rows) {
  ListDriver driver;
  double slots[kit::PASS_SLOTS] = {};
  slots[kit::PASS_WINDOW_ALONG] = 400.0;
  slots[kit::PASS_WINDOW_CROSS] = 300.0;
  slots[kit::PASS_HEADER_SIZE] = 30.0;
  slots[kit::PASS_EMPTY_SIZE] = 200.0;
  PassInput input = kit::readPassInput(slots);
  CHECK_EQ(input.emptySize, 200.0);
  kit::writePassResult(driver, driver.runPasses(input), slots);
  CHECK_EQ(slots[kit::PASS_OUT_SHOWS_EMPTY], 1.0);
  CHECK_EQ(slots[kit::PASS_OUT_FOOTER_START], 230.0);
  CHECK_EQ(slots[kit::PASS_OUT_CONTENT], 230.0);
}

TEST(kit_bridge_sticky_slots_carry_the_translations_and_the_spans) {
  double slots[kit::STICKY_SLOTS] = {};
  slots[kit::STICKY_OFFSET] = 300.0;
  slots[kit::STICKY_WINDOW_SIZE] = 600.0;
  slots[kit::STICKY_CONTENT_SIZE] = 2000.0;
  slots[kit::STICKY_HEADER_SIZE] = 50.0;
  slots[kit::STICKY_FOOTER_SIZE] = 40.0;
  slots[kit::STICKY_FOOTER_START] = 1960.0;
  slots[kit::STICKY_HEADER] = 1.0;
  slots[kit::STICKY_FOOTER] = 1.0;
  kit::writeStickyLayout(slots);
  CHECK_EQ(slots[kit::STICKY_OUT_HEADER_TRANSLATION], 300.0);
  CHECK_EQ(slots[kit::STICKY_OUT_FOOTER_TRANSLATION], 300.0 + 600.0 - 40.0 - 1960.0);
  CHECK_EQ(slots[kit::STICKY_OUT_HEADER_SPAN_START], 0.0);
  CHECK_EQ(slots[kit::STICKY_OUT_HEADER_SPAN_END], 1960.0);
  CHECK_EQ(slots[kit::STICKY_OUT_FOOTER_SPAN_START], 50.0);
  CHECK_EQ(slots[kit::STICKY_OUT_FOOTER_SPAN_END], 2000.0);
  // A size of 0 is a missing header or footer, which never moves.
  slots[kit::STICKY_HEADER_SIZE] = 0.0;
  slots[kit::STICKY_FOOTER_SIZE] = 0.0;
  kit::writeStickyLayout(slots);
  CHECK_EQ(slots[kit::STICKY_OUT_HEADER_TRANSLATION], 0.0);
  CHECK_EQ(slots[kit::STICKY_OUT_FOOTER_TRANSLATION], 0.0);
}

TEST(kit_bridge_mount_slots_carry_a_mount_record) {
  double slots[kit::MOUNT_SLOTS] = {};
  slots[kit::MOUNT_LOW] = 4.0;
  slots[kit::MOUNT_HIGH] = 9.0;
  slots[kit::MOUNT_STICKY] = -1.0;
  slots[kit::MOUNT_GEOMETRY] = 12.0;
  slots[kit::MOUNT_STRUCTURE] = 3.0;
  slots[kit::MOUNT_SHIFT] = 0.25;
  slots[kit::MOUNT_BAND_LOW] = 100.0;
  slots[kit::MOUNT_BAND_HIGH] = 140.0;
  slots[kit::MOUNT_WINDOW_ALONG] = 600.0;
  slots[kit::MOUNT_PAD] = 300.0;
  slots[kit::MOUNT_LEADING_INSET] = 10.0;
  slots[kit::MOUNT_TRAILING_INSET] = 20.0;
  MountRecord record = kit::readMountRecord(slots);
  CHECK_EQ(record.low, std::size_t{4});
  CHECK_EQ(record.high, std::size_t{9});
  CHECK_EQ(record.sticky, UNDEFINED_INDEX);
  CHECK_EQ(record.geometry, std::uint64_t{12});
  CHECK_EQ(record.structure, std::uint64_t{3});
  CHECK_EQ(record.shift, 0.25);
  CHECK(record.band.contains(120.0) && !record.band.contains(141.0));
  CHECK(record.inputs == (MountInputs{600.0, 300.0, 10.0, 20.0}));
  // A bridge without a geometry yet says -1, which no version equals.
  slots[kit::MOUNT_GEOMETRY] = -1.0;
  CHECK(kit::readMountRecord(slots).geometry != record.geometry);
  CHECK(kit::readMountRecord(slots).geometry != std::uint64_t{0});
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

TEST(kit_bridge_auto_hide_slots_keep_driver_state_and_leave_output_slots_unchanged) {
  ListDriver driver;
  double slots[kit::STICKY_SLOTS] = {};
  slots[kit::STICKY_HEADER_SIZE] = 60.0;
  slots[kit::STICKY_AUTO_HIDE_HEADER] = 1.0;
  slots[kit::STICKY_OFFSET] = 200.0;
  kit::writeStickyLayout(slots, driver);
  CHECK_EQ(slots[kit::STICKY_OUT_HEADER_TRANSLATION], 200.0);
  slots[kit::STICKY_OFFSET] = 225.0;
  slots[kit::STICKY_ACCUMULATE] = 1.0;
  kit::writeStickyLayout(slots, driver);
  CHECK_EQ(slots[kit::STICKY_OUT_HEADER_TRANSLATION], 200.0);
  slots[kit::STICKY_OFFSET] = 325.0;
  slots[kit::STICKY_ACCUMULATE] = 0.0;
  kit::writeStickyLayout(slots, driver);
  CHECK_EQ(slots[kit::STICKY_OUT_HEADER_TRANSLATION], 300.0);
}
