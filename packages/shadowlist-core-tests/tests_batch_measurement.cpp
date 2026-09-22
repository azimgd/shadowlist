/*
 * Batched measurement. One layout pass hands the core every mounted row's size, and
 * applyElementSize plus one commitElementSizes turns that into a single reflow.
 * These tests check the batch lands on the same layout as sizing rows one by one,
 * and that re-reporting a size a row already has moves nothing.
 * They also cover keysUnchanged, which lets a scroll commit skip checking every key.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

std::vector<double> heightsFor(std::size_t count) {
  std::vector<double> heights;
  heights.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    heights.push_back(60.0 + static_cast<double>((index * 37) % 240));
  }
  return heights;
}

std::vector<double> offsetsOf(const Container& container) {
  std::vector<double> offsets;
  offsets.reserve(container.revision.elements.size());
  for (const Element& element : container.revision.elements) {
    offsets.push_back(element.offsetY);
  }
  return offsets;
}

}

// The batch must give exactly the same layout as sizing rows one by one.
TEST(batched_measurement_matches_the_per_row_path) {
  std::vector<std::string> keys = keysFor(300);
  std::vector<double> heights = heightsFor(keys.size());

  Container perRow;
  Virtualizer::update(&perRow, inputFor(keys, 0.0));
  for (std::size_t index = 0; index < keys.size(); ++index) {
    Virtualizer::updateElementAtIndex(&perRow, index, {WINDOW_WIDTH, heights[index]});
  }
  Virtualizer::recomputeTotalSize(&perRow);

  Container batched;
  Virtualizer::update(&batched, inputFor(keys, 0.0));
  std::size_t lowestChanged = UNDEFINED_INDEX;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    if (Virtualizer::applyElementSize(&batched, index, {WINDOW_WIDTH, heights[index]}) &&
        index < lowestChanged) {
      lowestChanged = index;
    }
  }
  if (lowestChanged != UNDEFINED_INDEX) {
    Virtualizer::commitElementSizes(&batched, lowestChanged);
  }
  Virtualizer::recomputeTotalSize(&batched);

  CHECK(offsetsOf(perRow) == offsetsOf(batched));
  CHECK_NEAR(perRow.revision.totalContainerHeight, batched.revision.totalContainerHeight, 0.001);
  CHECK_NEAR(perRow.revision.containerOffsetY, batched.revision.containerOffsetY, 0.001);
  CHECK_EQ(perRow.revision.measuredRealCount, batched.revision.measuredRealCount);
  CHECK_NEAR(perRow.revision.measuredRealTotalHeight, batched.revision.measuredRealTotalHeight, 0.001);
}

TEST(batched_measurement_matches_the_per_row_path_while_scrolled) {
  std::vector<std::string> keys = keysFor(400);
  std::vector<double> heights = heightsFor(keys.size());

  auto prime = [&](Container& container) {
    Virtualizer::update(&container, inputFor(keys, 0.0));
    for (std::size_t index = 0; index < keys.size(); ++index) {
      Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, 120.0});
    }
    Virtualizer::update(&container, inputFor(keys, 9000.0));
  };

  Container perRow;
  prime(perRow);
  Container batched;
  prime(batched);

  // Rows in the middle resize, like content that loads late.
  const std::size_t low = 120;
  const std::size_t high = 150;

  for (std::size_t index = low; index <= high; ++index) {
    Virtualizer::updateElementAtIndex(&perRow, index, {WINDOW_WIDTH, heights[index]});
  }
  Virtualizer::recomputeTotalSize(&perRow);

  std::size_t lowestChanged = UNDEFINED_INDEX;
  for (std::size_t index = low; index <= high; ++index) {
    if (Virtualizer::applyElementSize(&batched, index, {WINDOW_WIDTH, heights[index]}) &&
        index < lowestChanged) {
      lowestChanged = index;
    }
  }
  if (lowestChanged != UNDEFINED_INDEX) {
    Virtualizer::commitElementSizes(&batched, lowestChanged);
  }
  Virtualizer::recomputeTotalSize(&batched);

  CHECK(offsetsOf(perRow) == offsetsOf(batched));
  // The scroll correction must match too, not just the row positions.
  CHECK_NEAR(perRow.revision.containerOffsetY, batched.revision.containerOffsetY, 0.001);
  CHECK_NEAR(perRow.revision.totalContainerHeight, batched.revision.totalContainerHeight, 0.001);
}

TEST(reporting_an_unchanged_size_reports_no_change) {
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  // The first size for a row always counts, even if it equals the estimate.
  CHECK(Virtualizer::applyElementSize(&container, 0, {WINDOW_WIDTH, 200.0}));
  CHECK(container.revision.elements[0].measured);

  // Reporting the same size again does not.
  CHECK(!Virtualizer::applyElementSize(&container, 0, {WINDOW_WIDTH, 200.0}));
  CHECK(!Virtualizer::applyElementSize(&container, 0, {WINDOW_WIDTH, 200.0}));

  // A real change counts again.
  CHECK(Virtualizer::applyElementSize(&container, 0, {WINDOW_WIDTH, 201.0}));
}

/*
 * A first size equal to the estimate still counts. It feeds the average used
 * for every row nobody has measured yet.
 */
TEST(a_first_measurement_equal_to_the_estimate_is_still_counted) {
  std::vector<std::string> keys = keysFor(50);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  std::size_t before = container.revision.measuredRealCount;
  double sizeMatchingEstimate = container.revision.elements[5].height;
  Virtualizer::applyElementSize(&container, 5, {WINDOW_WIDTH, sizeMatchingEstimate});

  CHECK_EQ(container.revision.measuredRealCount, before + 1);
  CHECK(container.revision.elements[5].measured);
}

TEST(measurement_out_of_bounds_is_rejected) {
  std::vector<std::string> keys = keysFor(10);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  bool threw = false;
  try {
    Virtualizer::applyElementSize(&container, 10, {WINDOW_WIDTH, 100.0});
  } catch (const InvalidOperationError&) {
    threw = true;
  }
  CHECK(threw);

  // Committing past the end does nothing, since a batch can race the list shrinking.
  Virtualizer::commitElementSizes(&container, 999);
  CHECK_EQ(container.getElementsSize(), static_cast<std::size_t>(10));
}

// Skipping the key check.

/*
 * A scroll commit carries the same props, so the host can tell the core the keys did not
 * change. The result must match letting the core check the keys itself.
 */
TEST(declaring_keys_unchanged_matches_revalidating_them) {
  std::vector<std::string> keys = keysFor(250);

  auto run = [&](bool declareUnchanged) {
    Container container;
    Virtualizer::update(&container, inputFor(keys, 0.0));
    for (std::size_t index = 0; index < keys.size(); ++index) {
      Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, 100.0});
    }
    for (double offset = 0.0; offset < 8000.0; offset += 250.0) {
      FrameInput input = inputFor(keys, offset);
      input.keysUnchanged = declareUnchanged;
      input.userScrolled = true;
      input.scrollPhase = ScrollPhase::Dragging;
      Virtualizer::update(&container, input);
    }
    return offsetsOf(container);
  };

  CHECK(run(true) == run(false));
}

// Borrowing the caller's keys must behave exactly like handing the core a copy.
TEST(borrowed_keys_behave_like_owned_keys) {
  std::vector<std::string> keys = keysFor(120);

  Container owned;
  Container borrowed;
  for (double offset = 0.0; offset < 4000.0; offset += 400.0) {
    FrameInput ownedInput = inputFor(keys, offset);
    Virtualizer::update(&owned, ownedInput);

    FrameInput borrowedInput = inputFor(keys, offset);
    borrowedInput.keys.clear();
    borrowedInput.keysRef = &keys;
    Virtualizer::update(&borrowed, borrowedInput);
  }

  CHECK_EQ(owned.getElementsSize(), borrowed.getElementsSize());
  CHECK(offsetsOf(owned) == offsetsOf(borrowed));
  for (std::size_t index = 0; index < keys.size(); ++index) {
    CHECK_EQ(owned.revision.elements[index].key, borrowed.revision.elements[index].key);
  }
}
