#include "TestFramework.hpp"
#include "TestHelpers.hpp"
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/DragReorder.hpp>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {
void layout(Container& core) {
  auto input = inputFor(keysFor(5), 0.0);
  input.windowWidth = 300;
  input.windowHeight = 20;
  input.numberOfColumns = 3;
  input.rowSpans = {2, 1, 3, 1, 2};
  Virtualizer::update(core, input);
  std::vector<double> heights{10, 20, 5, 7, 9};
  for (std::size_t index = 0; index < heights.size(); ++index) {
    Virtualizer::updateRowAtIndex(core, index, {100.0 * core.revision.rows[index].columnSpan, heights[index]});
  }
  Virtualizer::update(core, input);
}
DragRow dragRow(const Container& core, std::size_t index) {
  const Row& row = core.revision.rows[index];
  return {index, row.key, row.offsetY, row.height, row.offsetX, row.width};
}
std::vector<DragRow> mounted(const Container& core, std::size_t held = UNDEFINED_INDEX) {
  std::vector<DragRow> rows;
  for (std::size_t index = 0; index < core.getRowCount(); ++index) {
    if (index != held) {
      rows.push_back(dragRow(core, index));
    }
  }
  return rows;
}
}

TEST(drag_spans_reorder_uses_the_same_full_width_barriers_as_core_layout) {
  Container core;
  layout(core);
  DragReorder drag;
  auto held = dragRow(core, 0);
  drag.begin(held, 5.0, 100.0, 3);
  drag.placeRow(29.0, 200.0, held, 34.0, 300.0);
  auto rows = mounted(core, 0);
  drag.updateInsertion(rows);
  CHECK_EQ(drag.getInsertionIndex(), 4u);
  drag.updateSpanOffsets(core, rows);
  CHECK_EQ(drag.offsetFor(1).cross, -200.0);
  CHECK_EQ(drag.offsetFor(1).leading, 0.0);
  CHECK_EQ(drag.offsetFor(2).cross, 0.0);
  CHECK_EQ(drag.offsetFor(2).leading, 0.0);
  CHECK_EQ(drag.offsetFor(4).leading, 0.0);
}

TEST(drag_spans_removal_reflows_from_the_actual_prior_track_ends) {
  Container core;
  layout(core);
  DragReorder drag;
  drag.begin(dragRow(core, 2), 22.0, 150.0, 3);
  drag.setHeldAway(true);
  auto rows = mounted(core, 2);
  drag.updateInsertion(rows);
  drag.updateSpanOffsets(core, rows);
  CHECK_EQ(drag.offsetFor(3).leading, -15.0);
  CHECK_EQ(drag.offsetFor(3).cross, 0.0);
  CHECK_EQ(drag.offsetFor(4).leading, -5.0);
  CHECK_EQ(drag.offsetFor(4).cross, 0.0);
}

TEST(drag_spans_foreign_full_width_gap_advances_every_track_together) {
  Container core;
  layout(core);
  DragReorder drag;
  drag.beginForeign(8.0, 300.0, 3);
  drag.placeForeignCenter(26.0, 0.0);
  auto rows = mounted(core);
  drag.updateInsertion(rows);
  CHECK_EQ(drag.getInsertionIndex(), 3u);
  drag.updateSpanOffsets(core, rows);
  CHECK_EQ(drag.offsetFor(3).leading, 8.0);
  CHECK_EQ(drag.offsetFor(4).leading, 8.0);
  CHECK_EQ(drag.offsetFor(4).cross, 0.0);
}

TEST(drag_spans_replaced_producer_without_the_held_key_clears_the_preview) {
  Container core;
  layout(core);
  DragReorder drag;
  drag.begin(dragRow(core, 0), 5.0, 100.0, 3);
  drag.setHeldAway(true);
  auto rows = mounted(core, 0);
  drag.updateInsertion(rows);
  drag.updateSpanOffsets(core, rows);
  Container replacement;
  auto input = inputFor(keysFor(5, "replacement"), 0.0);
  input.numberOfColumns = 3;
  input.rowSpans = {2, 1, 3, 1, 2};
  Virtualizer::update(replacement, input);
  drag.updateSpanOffsets(replacement, mounted(replacement));
  CHECK_EQ(drag.getOriginIndex(), UNDEFINED_INDEX);
  CHECK_EQ(drag.getInsertionIndex(), UNDEFINED_INDEX);
  CHECK_EQ(drag.offsetFor(1).leading, 0.0);
  CHECK_EQ(drag.offsetFor(1).cross, 0.0);
}

TEST(drag_spans_plain_target_keeps_its_existing_foreign_grid_preview) {
  Container core;
  auto input = inputFor(keysFor(5), 0.0);
  input.numberOfColumns = 3;
  Virtualizer::update(core, input);
  DragReorder drag;
  drag.beginForeign(80.0, 300.0, 3);
  drag.placeForeignCenter(30.0, 0.0);
  auto rows = mounted(core);
  drag.updateInsertion(rows);
  std::vector<DragOffset> before;
  for (std::size_t index = 0; index < rows.size(); ++index) {
    before.push_back(drag.offsetFor(index));
  }
  drag.updateSpanOffsets(core, rows);
  for (std::size_t index = 0; index < rows.size(); ++index) {
    CHECK_EQ(drag.offsetFor(index).leading, before[index].leading);
    CHECK_EQ(drag.offsetFor(index).cross, before[index].cross);
  }
}

TEST(drag_spans_foreign_pixel_rounded_cross_preserves_the_requested_track_count) {
  Container core;
  auto input = inputFor(keysFor(5), 0.0);
  input.windowWidth = 320;
  input.windowHeight = 20;
  input.numberOfColumns = 3;
  input.rowSpans = {2, 1, 3, 1, 2};
  Virtualizer::update(core, input);
  for (std::size_t index = 0; index < core.getRowCount(); ++index) {
    Virtualizer::updateRowAtIndex(core, index, {320.0 * core.revision.rows[index].columnSpan / 3, 10});
  }
  Virtualizer::update(core, input);
  auto rows = mounted(core);
  DragReorder exact;
  DragReorder rounded;
  exact.beginForeign(8, 320.0 * 2 / 3, 3);
  rounded.beginForeign(8, 213, 3);
  exact.placeForeignCenter(26, 0);
  rounded.placeForeignCenter(26, 0);
  exact.updateInsertion(rows);
  rounded.updateInsertion(rows);
  CHECK_EQ(exact.getInsertionIndex(), rounded.getInsertionIndex());
  exact.updateSpanOffsets(core, rows);
  rounded.updateSpanOffsets(core, rows);
  for (const auto& row : rows) {
    CHECK_EQ(exact.offsetFor(row.index).leading, rounded.offsetFor(row.index).leading);
    CHECK_EQ(exact.offsetFor(row.index).cross, rounded.offsetFor(row.index).cross);
  }
}

TEST(drag_spans_foreign_width_is_resolved_against_the_destination_tracks) {
  Container core;
  auto input = inputFor(keysFor(5), 0.0);
  input.windowWidth = 240;
  input.windowHeight = 20;
  input.numberOfColumns = 3;
  input.rowSpans = {2, 1, 3, 1, 2};
  Virtualizer::update(core, input);
  for (std::size_t index = 0; index < core.getRowCount(); ++index) {
    Virtualizer::updateRowAtIndex(core, index, {80.0 * core.revision.rows[index].columnSpan, 10});
  }
  Virtualizer::update(core, input);
  DragReorder drag;
  // A two-track source cell from a 320px list occupies three 80px destination tracks.
  // Before insertion there is no destination source key/span to resolve; only its physical size.
  drag.beginForeign(8, 213, 3);
  drag.placeForeignCenter(26, 0);
  auto rows = mounted(core);
  drag.updateInsertion(rows);
  CHECK_EQ(drag.getInsertionIndex(), 3u);
  drag.updateSpanOffsets(core, rows);
  CHECK_EQ(drag.offsetFor(3).leading, 8.0);
  CHECK_EQ(drag.offsetFor(4).leading, 8.0);
  CHECK_EQ(drag.offsetFor(4).cross, 0.0);
}
