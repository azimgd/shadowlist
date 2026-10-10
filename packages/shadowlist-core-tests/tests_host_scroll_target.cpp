/*
 * Where scroll commands aim before the core lands them, and the anchor a saved position keeps.
 * The Fabric lists and the native lists share both.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/host/ListLayout.hpp>
#include <shadowlist-core/host/ScrollTarget.hpp>

#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

constexpr double ROW_HEIGHT = 100.0;

/*
 * A container with count measured rows of ROW_HEIGHT at offset.
 */
void layOut(Container& core, std::size_t count, double offset) {
  auto keys = keysFor(count);
  Virtualizer::update(core, inputFor(keys, 0.0));
  std::vector<MeasuredRow> rows;
  for (std::size_t index = 0; index < count; ++index) {
    rows.push_back({index, WINDOW_WIDTH, ROW_HEIGHT, index + 1});
  }
  std::vector<std::uint64_t> firstMeasured;
  applyMeasuredRows(core, rows, false, firstMeasured);
  Virtualizer::update(core, inputFor(keys, offset));
}

}

TEST(command_target_places_the_row_at_its_view_position) {
  Container core;
  layOut(core, 100, 0.0);
  auto start = commandTargetOffset(core, 30.0, 0.0, 0.0);
  CHECK(start.has_value());
  CHECK_NEAR(*start, 3000.0, 0.01);
  auto centred = commandTargetOffset(core, 30.0, 0.5, 0.0);
  CHECK_NEAR(*centred, 3000.0 - (WINDOW_HEIGHT - ROW_HEIGHT) * 0.5, 0.01);
  auto moved = commandTargetOffset(core, 30.0, 0.0, -24.0);
  CHECK_NEAR(*moved, 2976.0, 0.01);
}

TEST(command_target_stays_inside_the_scroll_range) {
  Container core;
  layOut(core, 100, 0.0);
  double maxOffset = 100 * ROW_HEIGHT - WINDOW_HEIGHT;
  CHECK_NEAR(*commandTargetOffset(core, 99.0, 0.0, 0.0), maxOffset, 0.01);
  CHECK_NEAR(*commandTargetOffset(core, 0.0, 0.0, -50.0), 0.0, 0.01);
  CHECK_NEAR(*commandTargetOffset(core, SCROLL_TO_END_INDEX, 0.0, 0.0), maxOffset, 0.01);
  CHECK_NEAR(*commandTargetOffset(core, SCROLL_TO_OFFSET_INDEX, 0.0, 1234.0), 1234.0, 0.01);
  CHECK_NEAR(*commandTargetOffset(core, SCROLL_TO_OFFSET_INDEX, 0.0, 1.0e9), maxOffset, 0.01);
  CHECK_NEAR(*commandTargetOffset(core, SCROLL_TO_OFFSET_INDEX, 0.0, -10.0), 0.0, 0.01);
  CHECK(!commandTargetOffset(core, 100.0, 0.0, 0.0).has_value());
  CHECK(!commandTargetOffset(core, -1.0, 0.0, 0.0).has_value());
}

TEST(anchor_is_the_first_row_reaching_into_the_viewport) {
  Container core;
  layOut(core, 100, 1250.0);
  auto anchor = anchorStateAt(core, core.getOffset());
  CHECK(anchor.has_value());
  CHECK(anchor->key == "k12");
  CHECK_NEAR(anchor->offset, core.getOffset() - 1200.0, 0.01);
}

TEST(anchor_is_empty_before_the_first_layout) {
  Container core;
  CHECK(!anchorStateAt(core, 0.0).has_value());
}

TEST(page_scroll_moves_one_window_inside_the_content) {
  CHECK_EQ(pageScrollTarget(100.0, 600.0, 2000.0, 1), 700.0);
  CHECK_EQ(pageScrollTarget(100.0, 600.0, 2000.0, -1), 0.0);
  CHECK_EQ(pageScrollTarget(1800.0, 600.0, 2000.0, 1), 2000.0);
  CHECK_EQ(pageScrollTarget(100.0, 600.0, -5.0, 1), 0.0);
}
