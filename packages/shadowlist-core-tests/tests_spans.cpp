#include "TestFramework.hpp"
#include "TestHelpers.hpp"
#include "VirtualizationHelpers.hpp"
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/RowSizeSpec.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListLayout.hpp>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {
FrameInput spanFrame(const std::vector<std::size_t>& spans) {
  FrameInput input = inputFor(keysFor(spans.size()), 0.0);
  input.windowWidth = 300.0;
  input.windowHeight = 10.0;
  input.numberOfColumns = 3;
  input.rowSpans = spans;
  input.estimatedRowSize = {100.0, 10.0};
  return input;
}

void measureSpanRows(Container& container, const std::vector<double>& sizes) {
  for (std::size_t index = 0; index < sizes.size(); ++index) {
    double cross =
      container.revision.windowWidth * container.revision.rows[index].columnSpan / container.numberOfColumns;
    Virtualizer::updateRowAtIndex(container, index, {cross, sizes[index]});
  }
}
}

TEST(spans_use_shared_contiguous_tracks_and_full_width_barriers) {
  Container container;
  auto input = spanFrame({2, 1, 3, 1, 2});
  Virtualizer::update(container, input);
  measureSpanRows(container, {10.0, 20.0, 5.0, 7.0, 9.0});
  Virtualizer::update(container, input);
  const auto& rows = container.revision.rows;
  CHECK_EQ(rows[0].column, 0u);
  CHECK_EQ(rows[0].width, 200.0);
  CHECK_EQ(rows[1].offsetX, 200.0);
  CHECK_EQ(rows[2].width, 300.0);
  CHECK_EQ(rows[2].offsetY, 20.0);
  CHECK_EQ(rows[3].offsetY, 25.0);
  CHECK_EQ(rows[4].offsetX, 100.0);
  CHECK_EQ(container.revision.contentHeight, 34.0);
  Virtualizer::updateRowAtIndex(container, 0, {200.0, 40.0});
  Virtualizer::update(container, input);
  CHECK_EQ(rows[2].offsetY, 40.0);
  CHECK_EQ(rows[4].offsetY, 45.0);
  CHECK_EQ(container.revision.contentHeight, 54.0);
}

TEST(spans_horizontal_grid_preserves_axis_geometry_and_publishes_its_cross_height) {
  Container vertical;
  auto input = spanFrame({2, 1, 3, 1, 2});
  Virtualizer::update(vertical, input);
  measureSpanRows(vertical, {10.0, 20.0, 5.0, 7.0, 9.0});
  Virtualizer::update(vertical, input);
  Container horizontal;
  input.horizontal = true;
  input.windowWidth = 10.0;
  input.windowHeight = 300.0;
  input.estimatedRowSize = {10.0, 100.0};
  Virtualizer::update(horizontal, input);
  std::vector<double> widths{10.0, 20.0, 5.0, 7.0, 9.0};
  for (std::size_t index = 0; index < widths.size(); ++index) {
    Virtualizer::updateRowAtIndex(
      horizontal, index, {widths[index], 100.0 * horizontal.revision.rows[index].columnSpan});
  }
  Virtualizer::update(horizontal, input);
  CHECK_EQ(horizontal.revision.contentWidth, vertical.revision.contentHeight);
  for (std::size_t index = 0; index < widths.size(); ++index) {
    const Row& a = horizontal.revision.rows[index];
    const Row& b = vertical.revision.rows[index];
    CHECK_EQ(a.offsetX, b.offsetY);
    CHECK_EQ(a.offsetY, b.offsetX);
    CHECK_EQ(a.width, b.height);
    CHECK_EQ(a.height, b.width);
    auto frame = rowFrame(horizontal, index, true);
    CHECK(frame.setsWidth);
    CHECK(frame.setsHeight);
    CHECK_EQ(frame.width, widths[index]);
    CHECK_EQ(frame.height, a.height);
    CHECK(!rowFrame(vertical, index, false).setsHeight);
  }
  Container plain;
  input.rowSpans.clear();
  Virtualizer::update(plain, input);
  CHECK(!rowFrame(plain, 0, true).setsHeight);
}

TEST(spans_reapply_index_metadata_after_key_moves_and_copy_borrowed_values) {
  Container container;
  auto input = spanFrame({2, 1, 3});
  Virtualizer::update(container, input);
  input.keys = {"k2", "k0", "k1"};
  input.rowSpansUnchanged = true;
  Virtualizer::update(container, input);
  CHECK_EQ(container.revision.rows[0].columnSpan, 2u);
  CHECK_EQ(container.revision.rows[1].columnSpan, 1u);
  CHECK_EQ(container.revision.rows[2].columnSpan, 3u);
  std::vector<std::size_t> borrowed{3, 2, 1};
  input.rowSpansRef = &borrowed;
  input.rowSpansUnchanged = false;
  Virtualizer::update(container, input);
  borrowed.clear();
  CHECK_EQ(container.revision.rows[0].columnSpan, 3u);
  CHECK_EQ(container.revision.rows[1].columnSpan, 2u);
}

TEST(spans_column_changes_renormalize_requests_and_invalidate_only_changed_cross_sizes) {
  Container container;
  auto input = spanFrame({99, 2, 1});
  Virtualizer::update(container, input);
  measureSpanRows(container, {10.0, 20.0, 30.0});
  input.rowSpansUnchanged = true;
  input.numberOfColumns = 2;
  Virtualizer::update(container, input);
  CHECK_EQ(container.revision.rows[0].columnSpan, 2u);
  CHECK(container.revision.rows[0].measured); // Full-width available size stayed at 300.
  CHECK(!container.revision.rows[1].measured);
  CHECK_EQ(container.revision.measuredRealCount, 1u);
  CHECK_EQ(container.revision.measuredRealTotalHeight, 10.0);
  measureSpanRows(container, {10.0, 20.0, 30.0});
  CHECK_EQ(container.revision.measuredRealCount, 3u);
  CHECK_EQ(container.revision.measuredRealTotalHeight, 60.0);
  input.numberOfColumns = 5;
  Virtualizer::update(container, input);
  CHECK_EQ(container.revision.rows[0].columnSpan, 5u);
  CHECK_EQ(container.revision.rows[1].width, 120.0);
}

TEST(spans_long_unused_track_still_defines_content_extent) {
  Container container;
  auto input = spanFrame(std::vector<std::size_t>(1026, 2));
  input.rowSpans[1] = 1;
  Virtualizer::update(container, input);
  auto sizes = std::vector<double>(1026, 1.0);
  sizes[1] = 10000.0;
  measureSpanRows(container, sizes);
  Virtualizer::update(container, input);
  CHECK_EQ(container.revision.contentHeight, 10000.0);
  input.offsetY = 9000.0;
  input.userScrolled = true;
  Virtualizer::update(container, input);
  auto range = container.getMeasuredRange();
  CHECK_EQ(range.low, 1u);
  CHECK_EQ(range.high, 1u);
  CHECK_EQ(container.getViewableIndices(ViewableRule{}).low, 1u);
  checkNoRowLost(container, "long unused span track");
  std::vector<std::size_t> found;
  auto inspected = container.state.spanRanges.visit(9000.0, 9010.0, [&](std::size_t index) { found.push_back(index); });
  CHECK_EQ(found, std::vector<std::size_t>{1});
  CHECK(inspected < 30u);
}

TEST(spans_default_plain_grid_keeps_the_existing_fast_path) {
  Container container;
  auto input = spanFrame({});
  input.keys = keysFor(100);
  Virtualizer::update(container, input);
  CHECK(!container.state.hasRowSpans);
  CHECK(container.state.spanLayout.getTrackEnds().empty());
  for (std::size_t index = 0; index < 100; ++index) {
    CHECK_EQ(container.revision.rows[index].column, index % 3);
  }
}

TEST(spans_prediction_width_uses_the_current_row_span_and_replays_after_span_changes) {
  Container container;
  auto input = spanFrame({2, 1, 3});
  Virtualizer::update(container, input);
  auto source = std::make_shared<int>(1);
  SizeSpecQueue queue;
  auto parse = [&] {
    std::vector<RowSizeSpec> specs(3);
    for (std::size_t index = 0; index < 3; ++index) {
      specs[index].key = input.keys[index];
      specs[index].fixedHeight = 180.0;
    }
    return specs;
  };
  std::vector<double> widths;
  auto measure = [&](const RowSizeSpec& spec, double width) {
    widths.push_back(width);
    CHECK_EQ(spec.fixedHeight, 180.0);
    return Size{width, spec.fixedHeight};
  };
  queue.run(container, source, 100.0, parse, measure);
  CHECK_EQ(widths, (std::vector<double>{200.0, 100.0, 300.0}));
  input.rowSpans = {1, 2, 3};
  Virtualizer::update(container, input);
  widths.clear();
  queue.run(container, source, 100.0, parse, measure);
  CHECK_EQ(widths, (std::vector<double>{100.0, 200.0, 300.0}));
  input.horizontal = true;
  input.windowWidth = 320.0;
  input.windowHeight = 300.0;
  Virtualizer::update(container, input);
  widths.clear();
  queue.run(container, source, 320.0, parse, measure);
  CHECK_EQ(widths, (std::vector<double>{320.0, 320.0, 320.0}));
  CHECK_EQ(container.state.predictedSizes.at("k0").width, 320.0);
  CHECK_EQ(container.state.predictedSizes.at("k0").height, 180.0);
  Virtualizer::update(container, input);
  // The descriptor stays literal; the established horizontal host fills its cross tracks.
  for (std::size_t index = 0; index < 3; ++index) {
    CHECK_EQ(container.revision.rows[index].width, 320.0);
    CHECK_EQ(container.revision.rows[index].height, 100.0 * input.rowSpans[index]);
  }
}

TEST(spans_zero_extent_section_edges_align_tracks_without_adding_space) {
  Container container;
  // Header, two uneven items, zero footer, empty section edges, then the next section's item.
  auto input = spanFrame({3, 1, 1, 3, 3, 3, 1});
  Virtualizer::update(container, input);
  measureSpanRows(container, {10.0, 30.0, 20.0, 0.0, 0.0, 0.0, 7.0});
  Virtualizer::update(container, input);
  CHECK_EQ(container.revision.rows[3].offsetY, 40.0);
  CHECK_EQ(container.revision.rows[4].offsetY, 40.0);
  CHECK_EQ(container.revision.rows[6].offsetY, 40.0);
  CHECK_EQ(container.revision.contentHeight, 47.0);
}

TEST(spans_columns_and_metadata_revision_do_not_depend_on_row_heights) {
  Container container;
  auto input = spanFrame({2, 1, 3, 1, 2});
  Virtualizer::update(container, input);
  measureSpanRows(container, {10.0, 20.0, 5.0, 7.0, 9.0});
  Virtualizer::update(container, input);
  auto version = container.state.rowSpansVersion;
  std::vector<std::size_t> before;
  for (const Row& row : container.revision.rows) {
    before.push_back(row.column);
  }
  Virtualizer::updateRowAtIndex(container, 0, {200.0, 1000.0});
  Virtualizer::update(container, input);
  CHECK_EQ(container.state.rowSpansVersion, version);
  for (std::size_t index = 0; index < before.size(); ++index) {
    CHECK_EQ(container.revision.rows[index].column, before[index]);
  }
  CHECK_EQ(container.revision.rows[2].offsetY, 1000.0);
}

TEST(spans_count_change_with_unchanged_track_width_rebuilds_ranges_before_search) {
  Container container;
  auto input = spanFrame({2, 1, 2, 3, 1});
  Virtualizer::update(container, input);
  measureSpanRows(container, {10, 10, 10, 10, 10});
  Virtualizer::update(container, input);
  auto version = container.state.rowSpansVersion;
  input.numberOfColumns = 4;
  input.windowWidth = 400;
  input.rowSpansUnchanged = true;
  Virtualizer::update(container, input);
  CHECK_EQ(container.state.rowSpansVersion, version);
  CHECK_EQ(container.revision.rows[4].column, 3u);
  CHECK_EQ(container.revision.rows[4].offsetY, 0.0);
  CHECK_EQ(container.getMeasuredRange().high, 4u);
  CHECK_EQ(container.revision.measuredRealCount, 5u);
  input.rowSpans.clear();
  input.rowSpansUnchanged = false;
  Virtualizer::update(container, input);
  CHECK(!container.state.hasRowSpans);
  CHECK(!container.state.hasRowSpanSource);
  CHECK(container.state.spanLayout.getTrackEnds().empty());
  CHECK_EQ(container.state.spanRanges.visit(0, 100, [](std::size_t) {}), 0u);
  for (std::size_t index = 0; index < container.getRowCount(); ++index) {
    CHECK_EQ(container.revision.rows[index].column, index % 4);
    CHECK_EQ(container.revision.rows[index].columnSpan, 1u);
  }
}

TEST(spans_synchronous_keyed_anchor_survives_prepend_reorder_and_late_sizes) {
  ListDriver driver;
  ListSettings settings;
  settings.numberOfColumns = 3;
  settings.estimatedRowSize = 40.0;
  driver.setSettings(settings);
  auto keys = keysFor(200);
  auto spans = std::vector<std::size_t>(keys.size());
  for (std::size_t index = 0; index < spans.size(); ++index) {
    spans[index] = index % 3 + 1;
  }
  driver.setKeys(keys);
  driver.setRowSpans(spans);
  driver.setMeasureRow([](std::size_t, const std::string& key, double) {
    return 30.0 + static_cast<double>(std::stoul(key.substr(1)) % 43);
  });
  PassInput input;
  input.windowAlong = 100.0;
  input.windowCross = 300.0;
  input.clampStart = true;
  input.clampEnd = true;
  auto settle = [&] {
    for (int pass = 0; pass < 20; ++pass) {
      input.offset = driver.runPasses(input).offset;
    }
  };
  settle();
  driver.scrollToRow(90, 0.0);
  settle();
  auto anchor = driver.getAnchorState(input.offset);
  CHECK(anchor.has_value());
  driver.insertKeys({0}, {"k900"});
  spans.insert(spans.begin(), 3);
  driver.setRowSpans(spans);
  settle();
  CHECK_NEAR(input.offset - driver.getLeadingAt(driver.indexOfKey(anchor->key)), anchor->offset, 0.001);
  auto nextKeys = driver.getKeys();
  std::swap(nextKeys[10], nextKeys[30]);
  driver.setKeys(nextKeys);
  settle();
  CHECK_NEAR(input.offset - driver.getLeadingAt(driver.indexOfKey(anchor->key)), anchor->offset, 0.001);
  driver.markRemeasure({driver.indexOfKey(anchor->key)});
  settle();
  CHECK_NEAR(input.offset - driver.getLeadingAt(driver.indexOfKey(anchor->key)), anchor->offset, 0.001);
}

TEST(spans_incremental_random_edits_match_fresh_layout_and_brute_force_ranges) {
  struct Item {
    std::string key;
    std::size_t span;
    double height;
  };
  std::uint32_t seed = 0x3217u;
  auto next = [&] {
    seed = seed * 1664525u + 1013904223u;
    return seed;
  };
  std::vector<Item> items;
  for (std::size_t index = 0; index < 40; ++index) {
    items.push_back({"item" + std::to_string(index), next() % 4 + 1, static_cast<double>(next() % 120 + 1)});
  }
  Container incremental;
  FrameInput input;
  input.numberOfColumns = 3;
  input.windowWidth = 300;
  input.windowHeight = 100;
  input.overscan = 0;
  auto measure = [&](Container& core) {
    Virtualizer::update(core, input);
    SizeBatch batch;
    for (std::size_t index = 0; index < items.size(); ++index) {
      batch.apply(
        core, index,
        {input.windowWidth * core.revision.rows[index].columnSpan / input.numberOfColumns, items[index].height});
    }
    batch.commit(core);
    Virtualizer::update(core, input);
  };
  for (std::size_t edit = 0; edit < 80; ++edit) {
    std::size_t index = next() % items.size();
    switch (edit % 8) {
      case 0:
        items.push_back({"append" + std::to_string(edit), next() % 5 + 1, 15.0});
        break;
      case 1:
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
        break;
      case 2:
        std::swap(items[index], items[next() % items.size()]);
        break;
      case 3:
        items[index].span = next() % 8 + 1;
        break;
      case 4:
        items[index].height = static_cast<double>(next() % 1000 + 1);
        break;
      case 5:
        input.numberOfColumns = next() % 4 + 2;
        break;
      case 6:
        input.windowWidth = static_cast<double>(next() % 300 + 100);
        break;
      case 7:
        input.headerSize = static_cast<double>(next() % 30);
        break;
    }
    input.keys.clear();
    input.rowSpans.clear();
    for (const Item& item : items) {
      input.keys.push_back(item.key);
      input.rowSpans.push_back(item.span);
    }
    input.userScrolled = false;
    input.scrollPhase = ScrollPhase::Idle;
    measure(incremental);
    Container fresh;
    measure(fresh);
    CHECK_NEAR(incremental.revision.contentHeight, fresh.revision.contentHeight, 1e-9);
    for (std::size_t row = 0; row < items.size(); ++row) {
      const Row& actual = incremental.revision.rows[row];
      const Row& expected = fresh.revision.rows[row];
      CHECK_EQ(actual.column, expected.column);
      CHECK_EQ(actual.columnSpan, expected.columnSpan);
      CHECK_NEAR(actual.offsetX, expected.offsetX, 1e-9);
      CHECK_NEAR(actual.offsetY, expected.offsetY, 1e-9);
      CHECK_NEAR(actual.width, expected.width, 1e-9);
    }
    input.userScrolled = true;
    input.scrollPhase = ScrollPhase::Dragging;
    for (int query = 0; query < 4; ++query) {
      input.offsetY =
        next() % static_cast<std::size_t>(std::max(1.0, incremental.revision.contentHeight - input.windowHeight));
      Virtualizer::update(incremental, input);
      checkNoRowLost(incremental, "random mixed-span edit " + std::to_string(edit));
    }
  }
}

TEST(spans_offset_reports_preserve_geometry_but_changed_measurements_advance_it) {
  Container core;
  auto input = spanFrame({2, 1, 3, 1, 2});
  Virtualizer::update(core, input);
  measureSpanRows(core, {10, 20, 5, 7, 9});
  Virtualizer::update(core, input);
  const auto geometry = core.state.geometryVersion;
  const auto revision = core.revisionCount;
  const auto cross = core.revision.rows[0].width;
  input.offsetY = 5;
  input.scrollPhase = ScrollPhase::Dragging;
  input.userScrolled = true;
  Virtualizer::update(core, input);
  CHECK(core.revisionCount > revision);
  CHECK_EQ(core.state.geometryVersion, geometry);
  CHECK_EQ(core.revision.rows[0].width, cross);
  SizeBatch batch;
  batch.apply(core, 0, {cross, 50});
  // The measured row is already changed before commit/reflow advances the version.
  CHECK_EQ(core.revision.rows[0].height, 50.0);
  CHECK_EQ(core.state.geometryVersion, geometry);
  batch.commit(core);
  Virtualizer::update(core, input);
  CHECK(core.state.geometryVersion > geometry);
  CHECK_EQ(core.revision.rows[0].width, cross);
}

TEST(spans_inverted_follow_appends_pins_the_end_and_respects_user_release_on_both_axes) {
  for (bool horizontal : {false, true}) {
    ListDriver driver;
    ListSettings settings;
    settings.horizontal = horizontal;
    settings.numberOfColumns = 3;
    settings.inverted = true;
    settings.followAppends = true;
    settings.estimatedRowSize = 40;
    driver.setSettings(settings);
    auto keys = keysFor(30);
    std::vector<std::size_t> spans;
    auto syncSpans = [&] {
      spans.resize(keys.size());
      for (std::size_t index = 0; index < spans.size(); ++index) {
        spans[index] = index % 3 == 0 ? 2 : index % 3 == 1 ? 1 : 3;
      }
      driver.setRowSpans(spans);
    };
    driver.setKeys(keys);
    syncSpans();
    driver.setMeasureRow(
      [](std::size_t, const std::string& key, double) { return 30.0 + std::stoul(key.substr(1)) % 7; });
    PassInput input;
    input.windowAlong = 120;
    input.windowCross = 320;
    input.clampStart = input.clampEnd = true;
    PassResult result;
    auto settle = [&] {
      input.phase = ScrollPhase::Idle;
      input.userScrolled = false;
      for (int frame = 0; frame < 24; ++frame) {
        result = driver.runPasses(input);
        input.offset = result.offset;
        if (!result.settling) {
          return;
        }
      }
      CHECK(!result.settling);
    };
    auto append = [&] {
      const auto previousCount = keys.size();
      keys = keysFor(previousCount + 3);
      driver.insertKeys(
        {previousCount, previousCount + 1, previousCount + 2},
        {keys[previousCount], keys[previousCount + 1], keys[previousCount + 2]});
      syncSpans();
      settle();
    };
    settle();
    CHECK_EQ(input.offset, result.contentAlong - input.windowAlong);
    CHECK(result.atEnd);
    append();
    CHECK_EQ(input.offset, result.contentAlong - input.windowAlong);
    CHECK(result.atEnd);
    input.offset = (result.contentAlong - input.windowAlong) / 3;
    input.phase = ScrollPhase::Dragging;
    input.userScrolled = true;
    input.offset = driver.runPasses(input).offset;
    settle();
    const double released = input.offset;
    CHECK(!result.atEnd);
    append();
    CHECK_EQ(input.offset, released);
    CHECK(!result.atEnd);
    driver.scrollToEnd();
    settle();
    CHECK(result.atEnd);
    append();
    CHECK_EQ(input.offset, result.contentAlong - input.windowAlong);
    CHECK(result.atEnd);
  }
}
