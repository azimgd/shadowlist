/*
 * Materialization band contract tests.
 *
 * The band separates two decisions a single `overscan` would conflate: how far out rows
 * stay reconciled (retention, which keeps a fast scroll off the JS round trip) and how far
 * out they exist natively (materialization, which costs live views on the UI thread every
 * frame). Without it, widening retention to kill blank rows widens materialization too.
 *
 * So the properties that matter here are:
 *
 *   * SAFETY: every row that overlaps the actual viewport is materialized, always. A row
 *     the user can see must never be pruned, whatever the band arithmetic does.
 *   * SUBSET: the materialization band never exceeds the retention band -- pruning can
 *     only ever remove work, never ask for rows that were not retained.
 *   * INDEPENDENCE: widening retention leaves materialization unchanged. This is the whole
 *     point of the feature and the one property a single-overscan core cannot have.
 *   * FAIL-OPEN: every degenerate state (disabled, unmeasured, zero window) materializes
 *     everything, so a bug costs performance rather than blanking content.
 *
 * As in tests_virtualization.cpp the band is cross-checked against an independent brute
 * force pass over every element, so a seek that skips a row shows up as a lost row.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

struct Fixture {
  std::size_t columns = 1;
  bool horizontal = false;
  bool inverted = false;
  double overscan = 1.0;
  double materializationOverscan = -1.0;
};

FrameInput inputFor(const std::vector<std::string>& keys, double offset, const Fixture& fixture) {
  FrameInput input;
  input.keys = keys;
  input.windowContainerWidth = WINDOW_WIDTH;
  input.windowContainerHeight = WINDOW_HEIGHT;
  input.columns = fixture.columns;
  input.horizontal = fixture.horizontal;
  input.inverted = fixture.inverted;
  input.overscan = fixture.overscan;
  input.materializationOverscan = fixture.materializationOverscan;
  input.estimatedElementSize = {WINDOW_WIDTH, ESTIMATED_ROW_HEIGHT};
  if (fixture.horizontal) {
    input.containerOffsetX = offset;
  } else {
    input.containerOffsetY = offset;
  }
  return input;
}

/*
 * Normalise either band to the inclusive ascending range a host would act on, so inverted
 * lists (which report start > end) can be asserted with the same arithmetic.
 */
std::pair<std::size_t, std::size_t> ascending(std::pair<std::size_t, std::size_t> band) {
  if (band.first == UNDEFINED_INDEX || band.second == UNDEFINED_INDEX) {
    return {UNDEFINED_INDEX, UNDEFINED_INDEX};
  }
  return {std::min(band.first, band.second), std::max(band.first, band.second)};
}

/*
 * The safety property, asserted directly rather than via the band: nothing the user can
 * actually see may be pruned. Checked against the bare viewport, no overscan at all.
 */
void checkNothingVisibleIsPruned(const Container& container) {
  for (std::size_t index : overlappingIndices(container, 0.0)) {
    if (!container.shouldMaterialize(index)) {
      fail("row " + std::to_string(index) + " overlaps the viewport but was not materialized");
    }
  }
}

/*
 * Drive a settled list to `offset`. Two frames: the first establishes geometry from the
 * estimate, the second measures against it, which is what the band reads.
 */
void settleAt(Container& container, const std::vector<std::string>& keys, double offset, const Fixture& fixture) {
  Virtualizer::update(&container, inputFor(keys, offset, fixture));
  Virtualizer::update(&container, inputFor(keys, offset, fixture));
}

}

TEST(materialization_is_disabled_by_default) {
  Container container;
  auto keys = keysFor(500);
  Fixture fixture;  // materializationOverscan stays -1
  settleAt(container, keys, 6000.0, fixture);

  auto band = container.getMaterializedIndices();
  CHECK_EQ(band.first, UNDEFINED_INDEX);
  CHECK_EQ(band.second, UNDEFINED_INDEX);

  // Disabled must mean "materialize everything", including rows far outside any window.
  for (std::size_t index = 0; index < container.revision.elements.size(); ++index) {
    CHECK(container.shouldMaterialize(index));
  }
}

TEST(materialization_covers_everything_on_screen) {
  auto keys = keysFor(500);

  // Sweep the list, including the extremes, at an aggressively narrow band.
  for (double offset : {0.0, 500.0, 3000.0, 12000.0, 30000.0, 59000.0}) {
    Container container;
    Fixture fixture;
    fixture.overscan = 3.0;
    fixture.materializationOverscan = 0.0;  // strictly the viewport, nothing spare
    settleAt(container, keys, offset, fixture);

    checkNothingVisibleIsPruned(container);
  }
}

TEST(materialization_band_matches_the_brute_force_oracle) {
  Container container;
  auto keys = keysFor(400);
  Fixture fixture;
  fixture.overscan = 3.0;
  fixture.materializationOverscan = 0.5;
  settleAt(container, keys, 9000.0, fixture);

  auto expected = overlappingIndices(container, fixture.materializationOverscan);
  CHECK(!expected.empty());

  auto band = ascending(container.getMaterializedIndices());
  CHECK(band.first != UNDEFINED_INDEX);

  // The band is a contiguous span, so it must contain every overlapping row...
  for (std::size_t index : expected) {
    CHECK(index >= band.first);
    CHECK(index <= band.second);
  }
  // ...and its own endpoints must genuinely overlap, or it is padded with dead rows.
  CHECK_EQ(expected.count(band.first), std::size_t{1});
  CHECK_EQ(expected.count(band.second), std::size_t{1});
}

TEST(materialization_band_is_a_subset_of_the_retention_band) {
  Container container;
  auto keys = keysFor(400);
  Fixture fixture;
  fixture.overscan = 2.0;
  fixture.materializationOverscan = 0.5;
  settleAt(container, keys, 9000.0, fixture);

  auto retention = ascending(container.getVisibleIndices());
  auto materialization = ascending(container.getMaterializedIndices());

  CHECK(retention.first != UNDEFINED_INDEX);
  CHECK(materialization.first != UNDEFINED_INDEX);
  CHECK(materialization.first >= retention.first);
  CHECK(materialization.second <= retention.second);
}

TEST(widening_retention_does_not_widen_materialization) {
  auto keys = keysFor(400);

  /*
   * The point of the feature: retention can grow to kill blank rows while the natively
   * materialized set -- the thing that costs the UI thread every frame -- stays put.
   */
  std::pair<std::size_t, std::size_t> firstBand{};
  std::size_t retentionWidths[3] = {0, 0, 0};
  int slot = 0;

  for (double retentionOverscan : {1.0, 3.0, 6.0}) {
    Container container;
    Fixture fixture;
    fixture.overscan = retentionOverscan;
    fixture.materializationOverscan = 0.5;
    settleAt(container, keys, 9000.0, fixture);

    auto retention = ascending(container.getVisibleIndices());
    auto materialization = ascending(container.getMaterializedIndices());

    CHECK(materialization.first != UNDEFINED_INDEX);
    if (slot == 0) {
      firstBand = materialization;
    } else {
      CHECK_EQ(materialization.first, firstBand.first);
      CHECK_EQ(materialization.second, firstBand.second);
    }
    retentionWidths[slot] = retention.second - retention.first + 1;
    ++slot;
  }

  // Guard the premise: if retention did not actually grow, the assertion above is vacuous.
  CHECK(retentionWidths[1] > retentionWidths[0]);
  CHECK(retentionWidths[2] > retentionWidths[1]);
}

TEST(materialization_fails_open_before_measurement) {
  Container container;
  Fixture fixture;
  fixture.materializationOverscan = 0.5;

  // Nothing measured yet: the band is unknown, so every row must materialize.
  auto band = container.getMaterializedIndices();
  CHECK_EQ(band.first, UNDEFINED_INDEX);
  CHECK(container.shouldMaterialize(0));
  CHECK(container.shouldMaterialize(999));

  // A zero-sized window is equally untrustworthy and must also fail open.
  auto keys = keysFor(50);
  FrameInput input = inputFor(keys, 0.0, fixture);
  input.windowContainerHeight = 0.0;
  Virtualizer::update(&container, input);
  Virtualizer::update(&container, input);

  CHECK_EQ(container.getMaterializedIndices().first, UNDEFINED_INDEX);
  for (std::size_t index = 0; index < container.revision.elements.size(); ++index) {
    CHECK(container.shouldMaterialize(index));
  }
}

TEST(materialization_holds_for_inverted_and_horizontal_lists) {
  auto keys = keysFor(400);

  for (int variant = 0; variant < 3; ++variant) {
    Container container;
    Fixture fixture;
    fixture.overscan = 3.0;
    fixture.materializationOverscan = 0.5;
    fixture.inverted = (variant == 1);
    fixture.horizontal = (variant == 2);
    settleAt(container, keys, 9000.0, fixture);

    checkNothingVisibleIsPruned(container);

    auto retention = ascending(container.getVisibleIndices());
    auto materialization = ascending(container.getMaterializedIndices());
    CHECK(materialization.first != UNDEFINED_INDEX);
    CHECK(materialization.first >= retention.first);
    CHECK(materialization.second <= retention.second);
  }
}

TEST(materialization_holds_across_a_scroll_sweep) {
  auto keys = keysFor(600);
  Container container;
  Fixture fixture;
  fixture.overscan = 3.0;
  fixture.materializationOverscan = 0.5;

  std::size_t widestBand = 0;
  std::size_t widestRetention = 0;

  // Walk the whole list in viewport-sized steps, asserting the invariants every frame.
  for (double offset = 0.0; offset < 70000.0; offset += WINDOW_HEIGHT / 2.0) {
    Virtualizer::update(&container, inputFor(keys, offset, fixture));

    checkNothingVisibleIsPruned(container);

    auto retention = ascending(container.getVisibleIndices());
    auto materialization = ascending(container.getMaterializedIndices());
    if (materialization.first == UNDEFINED_INDEX) {
      continue;
    }

    CHECK(materialization.first >= retention.first);
    CHECK(materialization.second <= retention.second);

    widestBand = std::max(widestBand, materialization.second - materialization.first + 1);
    widestRetention = std::max(widestRetention, retention.second - retention.first + 1);
  }

  // The saving is the reason the feature exists; assert it actually materialized.
  CHECK(widestBand > 0);
  CHECK(widestBand * 2 < widestRetention);
}

TEST(materialization_keeps_unmeasured_rows_inside_the_band) {
  Container container;
  auto keys = keysFor(400);
  Fixture fixture;
  fixture.overscan = 3.0;
  fixture.materializationOverscan = 0.5;
  settleAt(container, keys, 9000.0, fixture);

  auto band = ascending(container.getMaterializedIndices());
  CHECK(band.first != UNDEFINED_INDEX);

  /*
   * Collapse a row inside the band to zero height. Unlike a viewability test, which would
   * drop it as invisible, the band must keep it: a pruned row can never be measured, so
   * dropping a zero-sized row would make its collapse permanent.
   */
  std::size_t collapsedIndex = band.first + (band.second - band.first) / 2;
  Virtualizer::updateElementAtIndex(&container, collapsedIndex, {WINDOW_WIDTH, 0.0});

  CHECK(container.shouldMaterialize(collapsedIndex));
}
