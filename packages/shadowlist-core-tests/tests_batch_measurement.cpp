/*
 * Batched measurement intake.
 *
 * Fabric hands the core every mounted row's measured size during one layout pass. The
 * batched API (applyElementSize + a single commitElementSizes) exists so that pass costs
 * one reflow instead of one per row. These tests pin the two things that makes that safe:
 *
 *   * the batched path lands on exactly the geometry the per-row path would have, and
 *   * a size a row already has is reported as "nothing changed", so a layout that
 *     re-reports unchanged sizes -- the overwhelmingly common case -- moves nothing.
 *
 * They also cover the props-identity shortcut (FrameInput::keysUnchanged), which lets a
 * scroll commit skip revalidating the whole key collection.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
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

/*
 * The batched path is only a performance change if it is also an exact one.
 */
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

  // A mounted band in the middle resizes, exactly as async content would.
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
  // Anchor compensation must land in the same place, not just the geometry.
  CHECK_NEAR(perRow.revision.containerOffsetY, batched.revision.containerOffsetY, 0.001);
  CHECK_NEAR(perRow.revision.totalContainerHeight, batched.revision.totalContainerHeight, 0.001);
}

TEST(reporting_an_unchanged_size_reports_no_change) {
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  // First report of a row always counts, even when it matches the estimate exactly.
  CHECK(Virtualizer::applyElementSize(&container, 0, {WINDOW_WIDTH, 200.0}));
  CHECK(container.revision.elements[0].measured);

  // Repeating it must not.
  CHECK(!Virtualizer::applyElementSize(&container, 0, {WINDOW_WIDTH, 200.0}));
  CHECK(!Virtualizer::applyElementSize(&container, 0, {WINDOW_WIDTH, 200.0}));

  // A real change must be reported again.
  CHECK(Virtualizer::applyElementSize(&container, 0, {WINDOW_WIDTH, 201.0}));
}

/*
 * A first measurement that happens to equal the estimate still has to be counted: it
 * feeds the frozen average that sizes every row nobody has measured yet.
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

  // Committing past the end is a no-op, not a crash: a batch can race a shrink.
  Virtualizer::commitElementSizes(&container, 999);
  CHECK_EQ(container.getElementsSize(), static_cast<std::size_t>(10));
}

/* ------------------------------------------------------------------ *
 * Props-identity shortcut
 * ------------------------------------------------------------------ */

/*
 * A scroll commit carries the same immutable props, so the host can tell the core the
 * keys did not change and skip revalidating all of them. The result must be identical to
 * letting the core work it out for itself.
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

/*
 * Borrowing the caller's key collection must behave exactly like handing the core a copy.
 */
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

// Every Element gets a distinct, stable debug id, generated on first use.
TEST(lazy_element_ids_are_unique_and_stable) {
  std::vector<std::string> keys = keysFor(500);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  std::vector<std::string> ids;
  ids.reserve(keys.size());
  for (const Element& element : container.revision.elements) {
    const std::string& id = element.getId();
    CHECK_EQ(id.size(), static_cast<std::size_t>(16));
    ids.push_back(id);
  }

  std::vector<std::string> sorted = ids;
  std::sort(sorted.begin(), sorted.end());
  CHECK(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());

  // Asking again must return the same id, not a fresh one.
  for (std::size_t index = 0; index < keys.size(); ++index) {
    CHECK_EQ(container.revision.elements[index].getId(), ids[index]);
  }
}
