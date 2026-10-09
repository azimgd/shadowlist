/*
 * Tests for the commit and layout pass logic the Fabric adapter runs: what a frame takes from
 * the host's report, what a layout pass publishes, the published sticky and snap lists, row
 * hiding and the size spec queue.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/host/ElementSizeSpec.hpp>
#include <shadowlist-core/host/ListCommit.hpp>
#include <shadowlist-core/host/ListLayout.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

/*
 * A container with count rows of rowHeight laid out at rest.
 */
void layOut(Container& core, std::size_t count, double rowHeight, double offset = 0.0) {
  auto keys = keysFor(count);
  Virtualizer::update(core, inputFor(keys, offset));
  std::vector<MeasuredRow> rows;
  for (std::size_t index = 0; index < count; ++index) {
    rows.push_back({index, WINDOW_WIDTH, rowHeight, index + 1});
  }
  std::vector<std::uint64_t> firstMeasured;
  applyMeasuredRows(core, rows, false, firstMeasured);
}

}

TEST(list_commit_host_scroll_fills_the_frame) {
  ListScrollState state;
  state.offsetY = 120.0;
  state.userScrolled = true;
  state.scrollPhase = SCROLL_PHASE_DRAGGING;
  state.commitToken = 4;
  FrameInput input;
  applyHostScroll(input, state);
  CHECK_EQ(input.containerOffsetY, 120.0);
  CHECK(input.userScrolled);
  CHECK(input.scrollPhase == ScrollPhase::Dragging);
  CHECK_EQ(input.commitToken, static_cast<std::uint64_t>(4));

}

TEST(list_commit_publish_keeps_the_first_base_of_a_correction) {
  ListScrollState state;
  state.offsetY = 200.0;
  ContainerStateUpdate update;
  update.changed = true;
  update.applyContainerOffset = true;
  update.containerOffsetY = 300.0;
  update.commitToken = 8;
  CHECK(publishStateUpdate(state, update));
  CHECK_EQ(state.baseY, 200.0);
  CHECK_EQ(state.offsetY, 300.0);
  CHECK(state.offsetEnabled);
  CHECK_EQ(state.commitToken, static_cast<std::uint64_t>(8));

  // A host report moved the offset, then the core retargets the same token.
  state.offsetY = 260.0;
  state.offsetEnabled = false;
  update.containerOffsetY = 330.0;
  CHECK(publishStateUpdate(state, update));
  CHECK_EQ(state.baseY, 200.0);

  // A new token starts a new base.
  state.offsetY = 330.0;
  update.commitToken = 9;
  update.containerOffsetY = 400.0;
  publishStateUpdate(state, update);
  CHECK_EQ(state.baseY, 330.0);

  ContainerStateUpdate unchanged;
  CHECK(!publishStateUpdate(state, unchanged));
}

TEST(list_commit_sticky_indices_drop_negatives) {
  std::vector<std::size_t> indices{99};
  stickyIndicesFromProps({-1, 0, 4, -3, 9}, indices);
  CHECK_EQ(indices.size(), static_cast<std::size_t>(3));
  CHECK_EQ(indices[0], static_cast<std::size_t>(0));
  CHECK_EQ(indices[2], static_cast<std::size_t>(9));
}

TEST(list_commit_band_is_empty_while_work_is_pending) {
  Container core;
  layOut(core, 50, 100.0);
  CHECK(publishedOffsetBand(core, false, false, false).isEmpty());
  CHECK(publishedOffsetBand(core, true, true, false).isEmpty());
  CHECK(publishedOffsetBand(core, true, false, true).isEmpty());
  OffsetBand band = publishedOffsetBand(core, true, false, false);
  CHECK(offsetBandPublished(band.low, band.high, core.computeOffsetBand()));
}

TEST(list_layout_inputs_reflow_and_mark_the_offset) {
  Container core;
  layOut(core, 20, 100.0);
  core.containerOffsetCorrected = false;
  CHECK(!applyLayoutInputs(core, 0.0, 0.0, WINDOW_WIDTH, WINDOW_HEIGHT));
  CHECK(!core.containerOffsetCorrected);

  CHECK(applyLayoutInputs(core, 60.0, 0.0, WINDOW_WIDTH, WINDOW_HEIGHT));
  CHECK(core.containerOffsetCorrected);
  CHECK_EQ(offsetOf(core, 0), 60.0);
  CHECK_EQ(offsetOf(core, 1), 160.0);
}

TEST(list_layout_measured_rows_report_first_measurements) {
  Container core;
  auto keys = keysFor(10);
  Virtualizer::update(core, inputFor(keys, 0.0));
  std::vector<std::uint64_t> firstMeasured;
  applyMeasuredRows(core, {{0, WINDOW_WIDTH, 80.0, 100}, {1, WINDOW_WIDTH, 90.0, 101}}, false, firstMeasured);
  CHECK_EQ(firstMeasured.size(), static_cast<std::size_t>(2));
  CHECK_EQ(offsetOf(core, 1), 80.0);
  CHECK_EQ(offsetOf(core, 2), 170.0);

  firstMeasured.clear();
  applyMeasuredRows(core, {{0, WINDOW_WIDTH, 80.0, 100}}, false, firstMeasured);
  CHECK(firstMeasured.empty());

  RowFrame frame = rowFrame(core, 2, false);
  CHECK_EQ(frame.y, 170.0);
  CHECK_EQ(frame.x, 0.0);
  CHECK(!frame.setsWidth);
}

TEST(list_layout_template_offsets) {
  Container core;
  layOut(core, 3, 100.0);
  applyLayoutInputs(core, 40.0, 30.0, WINDOW_WIDTH, WINDOW_HEIGHT);
  Virtualizer::recomputeTotalSize(core);
  TemplateOffsets offsets = templateOffsets(core, 40.0, 30.0);
  CHECK_EQ(offsets.header, 0.0);
  CHECK_EQ(offsets.empty, 40.0);
  CHECK_EQ(offsets.footer, core.getFooterOffset(30.0));
  CHECK_EQ(offsets.footer, 340.0);
}

TEST(list_layout_published_geometry_keeps_pointers_when_unchanged) {
  Container core;
  auto keys = keysFor(30);
  FrameInput input = inputFor(keys, 0.0);
  input.stickyIndices = {0, 10, 20};
  Virtualizer::update(core, input);
  PublishedGeometry published;
  CHECK(published.refresh(core));
  CHECK(published.stickyHeaderIndices != nullptr);
  CHECK_EQ(published.stickyHeaderIndices->size(), static_cast<std::size_t>(3));
  CHECK_EQ((*published.stickyHeaderOffsets)[1], offsetOf(core, 10));
  CHECK(published.snapOffsets == nullptr);

  auto indices = published.stickyHeaderIndices;
  CHECK(!published.refresh(core));
  CHECK(published.stickyHeaderIndices == indices);

  // A remeasure that leaves every header where it was keeps the same lists.
  std::vector<std::uint64_t> firstMeasured;
  applyMeasuredRows(core, {{25, WINDOW_WIDTH, ESTIMATED_ROW_HEIGHT, 1}}, false, firstMeasured);
  published.refresh(core);
  CHECK(published.stickyHeaderIndices == indices);

  // A header that moved publishes a new offsets list.
  auto offsets = published.stickyHeaderOffsets;
  applyMeasuredRows(core, {{5, WINDOW_WIDTH, 300.0, 2}}, false, firstMeasured);
  CHECK(published.refresh(core));
  CHECK(published.stickyHeaderOffsets != offsets);
  CHECK(published.stickyHeaderIndices == indices);
}

TEST(list_layout_inverted_lists_publish_no_sticky_headers) {
  Container core;
  auto keys = keysFor(30);
  FrameInput input = inputFor(keys, 0.0);
  input.stickyIndices = {0, 10};
  input.inverted = true;
  Virtualizer::update(core, input);
  PublishedGeometry published;
  published.refresh(core);
  CHECK(published.stickyHeaderIndices == nullptr);
}

TEST(conceal_tracker_generations_and_settling) {
  ConcealTracker<std::string> tracker;
  CHECK(tracker.isEmpty());
  CHECK_EQ(tracker.getPublishedGeneration(), 0.0);

  tracker.beginPass();
  CHECK_EQ(tracker.hide(7, "a"), static_cast<std::uint64_t>(1));
  CHECK_EQ(tracker.hide(8, "b"), static_cast<std::uint64_t>(1));
  CHECK_EQ(tracker.getPublishedGeneration(), 1.0);

  // A pass with no hide keeps the generation.
  tracker.beginPass();
  tracker.beginPass();
  CHECK_EQ(tracker.hide(9, "c"), static_cast<std::uint64_t>(2));

  auto* row = tracker.find(7);
  CHECK(row != nullptr);
  CHECK_EQ(row->payload, std::string("a"));
  // Not acknowledged yet, or a correction is still pending.
  CHECK(!tracker.settle(*row, 0, false));
  CHECK(!tracker.settle(*row, 1, true));
  CHECK(tracker.settle(*row, 1, false));

  // A row that never settles shows after the pass limit.
  auto* stuck = tracker.find(8);
  bool settled = false;
  for (std::size_t pass = 0; pass <= MAX_CONCEALED_LAYOUT_PASSES && !settled; ++pass) {
    settled = tracker.settle(*stuck, 0, true);
  }
  CHECK(settled);

  tracker.show(7);
  std::vector<std::uint64_t> stillHidden{9};
  tracker.forgetExcept(stillHidden);
  CHECK(tracker.find(8) == nullptr);
  CHECK(tracker.find(9) != nullptr);
  tracker.show(9);
  CHECK(tracker.isEmpty());
  CHECK_EQ(tracker.getPublishedGeneration(), 0.0);
}

TEST(conceal_tracker_hides_only_above_a_row_anchor) {
  Container core;
  CHECK_EQ(ConcealTracker<int>::hideBeforeIndex(core, true, true), static_cast<std::size_t>(0));
  layOut(core, 40, 100.0, 1500.0);
  CHECK_EQ(ConcealTracker<int>::hideBeforeIndex(core, false, true), static_cast<std::size_t>(0));
  CHECK_EQ(ConcealTracker<int>::hideBeforeIndex(core, true, false), static_cast<std::size_t>(0));
}

TEST(size_spec_queue_measures_within_a_budget) {
  Container core;
  auto keys = keysFor(60);
  Virtualizer::update(core, inputFor(keys, 0.0));
  std::vector<ElementSizeSpec> specs(60);
  for (std::size_t index = 0; index < specs.size(); ++index) {
    specs[index].key = "k" + std::to_string(index);
    specs[index].text = "t";
  }
  std::size_t parsed = 0;
  auto parse = [&]() {
    ++parsed;
    return specs;
  };
  auto source = std::make_shared<int>(1);
  SizeSpecQueue queue;
  std::size_t measured = 0;
  auto measure = [&measured](const ElementSizeSpec&, double width) {
    ++measured;
    return Size{width, 50.0};
  };
  queue.run(core, source, WINDOW_WIDTH, parse, measure);
  CHECK_EQ(measured, SizeSpecQueue::BUDGET_PER_RUN);
  CHECK(!queue.isFinished(source));
  queue.run(core, source, WINDOW_WIDTH, parse, measure);
  queue.run(core, source, WINDOW_WIDTH, parse, measure);
  CHECK_EQ(measured, static_cast<std::size_t>(60));
  CHECK(queue.isFinished(source));
  queue.run(core, source, WINDOW_WIDTH, parse, measure);
  CHECK_EQ(measured, static_cast<std::size_t>(60));
  CHECK_EQ(parsed, static_cast<std::size_t>(1));

  // A new source or a new width starts over. Zero width does nothing.
  auto other = std::make_shared<int>(2);
  CHECK(!queue.isFinished(other));
  queue.run(core, other, 0.0, parse, measure);
  CHECK_EQ(measured, static_cast<std::size_t>(60));
  queue.run(core, source, WINDOW_WIDTH / 2, parse, measure);
  CHECK_EQ(measured, static_cast<std::size_t>(60) + SizeSpecQueue::BUDGET_PER_RUN);
}

/*
 * A rotation in Fabric reaches the core's window width through the layout pass before the
 * next commit runs the queue. The queue must still notice the width its specs were measured at changed.
 */
TEST(size_spec_queue_remeasures_after_the_layout_pass_took_the_new_width) {
  Container core;
  auto keys = keysFor(10);
  Virtualizer::update(core, inputFor(keys, 0.0));
  std::vector<ElementSizeSpec> specs(10);
  for (std::size_t index = 0; index < specs.size(); ++index) {
    specs[index].key = "k" + std::to_string(index);
  }
  auto parse = [&]() { return specs; };
  std::size_t measured = 0;
  auto measure = [&measured](const ElementSizeSpec&, double width) {
    ++measured;
    return Size{width, 50.0};
  };
  auto source = std::make_shared<int>(1);
  SizeSpecQueue queue;
  queue.run(core, source, WINDOW_WIDTH, parse, measure);
  CHECK(queue.isFinished(source));

  applyLayoutInputs(core, 0.0, 0.0, WINDOW_WIDTH / 2, WINDOW_HEIGHT);
  queue.run(core, source, WINDOW_WIDTH / 2, parse, measure);
  CHECK_EQ(measured, static_cast<std::size_t>(20));
}
