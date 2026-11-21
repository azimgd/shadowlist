#include "TestFramework.hpp"
#include "TestHelpers.hpp"
#include <shadowlist-core/host/ListLayout.hpp>
#include <shadowlist-core/Virtualizer.hpp>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(measured_row_size_preserves_literal_reports_for_one_column) {
  Container core;
  auto input = inputFor(keysFor(2), 0);
  Virtualizer::update(core, input);
  for (bool horizontal : {false, true}) {
    auto size = measuredRowSize(core, {0, 333.25, 87.75, 1}, horizontal);
    CHECK_EQ(size.width, 333.25);
    CHECK_EQ(size.height, 87.75);
  }
}

TEST(measured_row_size_normalizes_fractional_cross_reports_without_masking_along_changes) {
  for (bool horizontal : {false, true}) {
    Container core;
    auto input = inputFor(keysFor(6), 0);
    input.horizontal = horizontal;
    input.numberOfColumns = 3;
    input.rowSpans = {2, 1, 3, 2, 1, 3};
    input.windowWidth = horizontal ? 100 : 320;
    input.windowHeight = horizontal ? 320 : 100;
    Virtualizer::update(core, input);
    const double cross = horizontal ? core.revision.rows[0].height : core.revision.rows[0].width;
    const double reported = static_cast<double>(static_cast<float>(cross));
    CHECK(reported != cross);
    MeasuredRow row{0, horizontal ? 110 : reported, horizontal ? reported : 110, 1};
    auto size = measuredRowSize(core, row, horizontal);
    CHECK_EQ(horizontal ? size.height : size.width, cross);
    CHECK_EQ(horizontal ? size.width : size.height, 110.0);
    std::vector<std::uint64_t> firstMeasured;
    applyMeasuredRows(core, {row}, horizontal, firstMeasured);
    CHECK_EQ(firstMeasured.size(), 1u);
    Virtualizer::update(core, input);
    CHECK(!core.state.measuredRangeStale);
    const auto geometry = core.state.geometryVersion;
    for (int repeat = 0; repeat < 5; ++repeat) {
      Virtualizer::updateRowAtIndex(core, row.index, measuredRowSize(core, row, horizontal));
      CHECK(!core.state.measuredRangeStale);
      firstMeasured.clear();
      applyMeasuredRows(core, {row}, horizontal, firstMeasured);
      CHECK(firstMeasured.empty());
      CHECK(!core.state.measuredRangeStale);
      CHECK_EQ(core.state.geometryVersion, geometry);
    }
    if (horizontal) row.width += 1;
    else row.height += 1;
    Virtualizer::updateRowAtIndex(core, row.index, measuredRowSize(core, row, horizontal));
    CHECK(core.state.measuredRangeStale);
    CHECK(core.state.geometryVersion > geometry);
    firstMeasured.clear();
    applyMeasuredRows(core, {row}, horizontal, firstMeasured);
    CHECK(core.state.measuredRangeStale);
    Virtualizer::update(core, input);
    CHECK(!core.state.measuredRangeStale);
    CHECK_EQ(horizontal ? core.revision.rows[0].height : core.revision.rows[0].width, cross);
    CHECK_EQ(horizontal ? core.revision.rows[0].width : core.revision.rows[0].height, 111.0);
  }
}
