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
  Virtualizer::update(&core, inputFor(keys, offset));
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
  state.commitToken = 4.0;
  FrameInput input;
  CHECK(!applyHostScroll(input, state, false));
  CHECK_EQ(input.containerOffsetY, 120.0);
  CHECK(input.userScrolled);
  CHECK(input.scrollPhase == ScrollPhase::Dragging);
  CHECK_EQ(input.commitToken, static_cast<std::uint64_t>(4));

  // A ShadowListNative command yields momentum, unless a finger is down.
  state.scrollPhase = SCROLL_PHASE_SETTLING;
  CHECK(applyHostScroll(input, state, true));
  CHECK(!input.userScrolled);
  CHECK(input.scrollPhase == ScrollPhase::Idle);
  state.scrollPhase = SCROLL_PHASE_DRAGGING;
  CHECK(!applyHostScroll(input, state, true));
  CHECK(input.scrollPhase == ScrollPhase::Dragging);
}

TEST(list_commit_publish_keeps_the_first_base_of_a_correction) {
  ListScrollState state;
  state.offsetY = 200.0;
  ContainerStateUpdate update;
  update.changed = true;
  update.applyContainerOffset = true;
  update.containerOffsetY = 300.0;
  update.commitToken = 8;
  CHECK(publishStateUpdate(state, update, 0));
  CHECK_EQ(state.baseY, 200.0);
  CHECK_EQ(state.offsetY, 300.0);
  CHECK(state.offsetEnabled);
  CHECK_EQ(state.commitToken, 8.0);

  // A host report moved the offset, then the core retargets the same token.
  state.offsetY = 260.0;
  state.offsetEnabled = false;
  update.containerOffsetY = 330.0;
  CHECK(publishStateUpdate(state, update, 0));
  CHECK_EQ(state.baseY, 200.0);

  // A new token starts a new base.
  state.offsetY = 330.0;
  update.commitToken = 9;
  update.containerOffsetY = 400.0;
  publishStateUpdate(state, update, 0);
  CHECK_EQ(state.baseY, 330.0);

  ContainerStateUpdate unchanged;
  CHECK(!publishStateUpdate(state, unchanged, 0));
}

TEST(list_commit_publish_marks_an_engine_scroll_command) {
  ListScrollState state;
  state.userScrolled = true;
  state.scrollPhase = SCROLL_PHASE_SETTLING;
  ContainerStateUpdate update;
  update.changed = true;
  update.applyContainerOffset = true;
  update.containerOffsetY = 900.0;
  update.commitToken = 15;
  publishStateUpdate(state, update, 15);
  CHECK_EQ(state.momentumYieldToken, 15.0);
  CHECK(!state.userScrolled);
  CHECK_EQ(state.scrollPhase, SCROLL_PHASE_IDLE);

  ListScrollState other;
  other.userScrolled = true;
  publishStateUpdate(other, update, 14);
  CHECK_EQ(other.momentumYieldToken, 0.0);
  CHECK(other.userScrolled);
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
  Virtualizer::update(&core, inputFor(keys, 0.0));
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
  Virtualizer::recomputeTotalSize(&core);
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
  Virtualizer::update(&core, input);
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
  Virtualizer::update(&core, input);
  PublishedGeometry published;
  published.refresh(core);
  CHECK(published.stickyHeaderIndices == nullptr);
}

TEST(conceal_tracker_generations_and_settling) {
  ConcealTracker<std::string> tracker;
  CHECK(tracker.empty());
  CHECK_EQ(tracker.publishedGeneration(), 0.0);

  tracker.beginPass();
  CHECK_EQ(tracker.hide(7, "a"), static_cast<std::uint64_t>(1));
  CHECK_EQ(tracker.hide(8, "b"), static_cast<std::uint64_t>(1));
  CHECK_EQ(tracker.publishedGeneration(), 1.0);

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
  CHECK(tracker.empty());
  CHECK_EQ(tracker.publishedGeneration(), 0.0);
}

TEST(conceal_tracker_hides_only_above_a_row_anchor) {
  Container core;
  CHECK_EQ(ConcealTracker<int>::hideBeforeIndex(core, true, true), static_cast<std::size_t>(0));
  layOut(core, 40, 100.0, 1500.0);
  CHECK_EQ(ConcealTracker<int>::hideBeforeIndex(core, false, true), static_cast<std::size_t>(0));
  CHECK_EQ(ConcealTracker<int>::hideBeforeIndex(core, true, false), static_cast<std::size_t>(0));
}

TEST(size_specs_parse_skips_bad_entries) {
  auto specs = parseElementSizeSpecs(
    R"([{"key":"a","text":"hi","fontSize":16,"fontWeight":700,"lineHeight":20},)"
    R"({"text":"no key"},5,{"key":"b","fixedHeight":44,"widthFraction":0.75,"numberOfLines":2}])");
  CHECK_EQ(specs.size(), static_cast<std::size_t>(2));
  CHECK_EQ(specs[0].key, std::string("a"));
  CHECK_EQ(specs[0].fontSize, 16.0);
  CHECK_EQ(specs[0].fontWeight, std::string("700"));
  CHECK_EQ(specs[0].lineHeight, 20.0);
  CHECK(std::isnan(specs[0].letterSpacing));
  CHECK_EQ(specs[1].fixedHeight, 44.0);
  CHECK_EQ(specs[1].widthFraction, 0.75);
  CHECK_EQ(specs[1].numberOfLines, 2);
  CHECK_EQ(specs[1].fontSize, ElementSizeSpec::DEFAULT_FONT_SIZE);
  CHECK(parseElementSizeSpecs("not json").empty());
  CHECK(parseElementSizeSpecs("{\"key\":\"a\"}").empty());
  CHECK(parseElementSizeSpecs("").empty());
}

TEST(size_spec_queue_measures_within_a_budget) {
  Container core;
  auto keys = keysFor(60);
  Virtualizer::update(&core, inputFor(keys, 0.0));
  std::string json = "[";
  for (std::size_t index = 0; index < 60; ++index) {
    json += std::string(index ? "," : "") + "{\"key\":\"k" + std::to_string(index) + "\",\"text\":\"t\"}";
  }
  json += "]";
  auto source = std::make_shared<int>(1);
  SizeSpecQueue queue;
  std::size_t measured = 0;
  auto measure = [&measured](const ElementSizeSpec&, double width) {
    ++measured;
    return Size{width, 50.0};
  };
  queue.run(core, source, json, WINDOW_WIDTH, measure);
  CHECK_EQ(measured, SizeSpecQueue::BUDGET_PER_RUN);
  CHECK(!queue.finished(source));
  queue.run(core, source, json, WINDOW_WIDTH, measure);
  queue.run(core, source, json, WINDOW_WIDTH, measure);
  CHECK_EQ(measured, static_cast<std::size_t>(60));
  CHECK(queue.finished(source));
  queue.run(core, source, json, WINDOW_WIDTH, measure);
  CHECK_EQ(measured, static_cast<std::size_t>(60));

  // A new source or a new width starts over. Zero width does nothing.
  auto other = std::make_shared<int>(2);
  CHECK(!queue.finished(other));
  queue.run(core, other, json, 0.0, measure);
  CHECK_EQ(measured, static_cast<std::size_t>(60));
  queue.run(core, source, json, WINDOW_WIDTH / 2, measure);
  CHECK_EQ(measured, static_cast<std::size_t>(60) + SizeSpecQueue::BUDGET_PER_RUN);
}
