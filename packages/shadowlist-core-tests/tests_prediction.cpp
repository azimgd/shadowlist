/*
 * Size predictions. Normally the core learns a row's real height only after it renders,
 * and then reflows every row after it. A prediction gives the same number before the row
 * renders. These tests check that:
 * a correct prediction makes the later real size a no-op,
 * a real size always wins over a prediction, in any order,
 * predictions never feed the average used for unknown rows,
 * and a predicted row counts as a trusted size while an estimated one does not.
 * Positions are checked against a simple running sum of heights.
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

/*
 * Heights far from the estimate, so an estimated row is easy to tell from a real one.
 */
std::vector<double> trueHeightsFor(std::size_t count) {
  std::vector<double> heights;
  heights.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    heights.push_back(44.0 + static_cast<double>((index * 53) % 301));
  }
  return heights;
}

/*
 * Queue a prediction for every row. The next update applies them.
 */
void predictAll(Container& container, const std::vector<std::string>& keys, const std::vector<double>& heights) {
  for (std::size_t index = 0; index < keys.size(); ++index) {
    container.setPredictedSize(keys[index], {WINDOW_WIDTH, heights[index]});
  }
}

/*
 * Report real sizes in one batch like a Fabric layout pass, and return how many rows
 * changed. Zero is the goal.
 */
std::size_t measureBatch(
  Container& container,
  const std::vector<double>& heights,
  std::size_t low,
  std::size_t high) {
  std::size_t changedCount = 0;
  std::size_t lowestChanged = UNDEFINED_INDEX;
  for (std::size_t index = low; index <= high && index < container.revision.elements.size(); ++index) {
    if (Virtualizer::applyElementSize(&container, index, {WINDOW_WIDTH, heights[index]})) {
      changedCount++;
      if (index < lowestChanged) {
        lowestChanged = index;
      }
    }
  }
  if (lowestChanged != UNDEFINED_INDEX) {
    Virtualizer::commitElementSizes(&container, lowestChanged);
  }
  return changedCount;
}

void checkOffsetsAreExactPrefixSums(const Container& container, const std::vector<double>& heights, const char* label) {
  double expected = 0.0;
  for (std::size_t index = 0; index < container.revision.elements.size(); ++index) {
    const Element& element = container.revision.elements[index];
    if (element.offsetY != expected) {
      fail(std::string(label) + ": row " + std::to_string(index) + " sits at " +
           toStr(element.offsetY) + ", expected " + toStr(expected));
    }
    expected += heights[index];
  }
}

}

/*
 * The main point. With exact predictions every later real size is a no-op, while the same
 * list without predictions reflows on almost every row.
 */
TEST(exact_predictions_eliminate_measurement_reflow) {
  std::vector<std::string> keys = keysFor(400);
  std::vector<double> heights = trueHeightsFor(keys.size());

  Container predicted;
  predictAll(predicted, keys, heights);
  Virtualizer::update(&predicted, inputFor(keys, 0.0));
  std::size_t predictedChanges = measureBatch(predicted, heights, 0, keys.size() - 1);

  Container blind;
  Virtualizer::update(&blind, inputFor(keys, 0.0));
  std::size_t blindChanges = measureBatch(blind, heights, 0, keys.size() - 1);

  CHECK_EQ(predictedChanges, static_cast<std::size_t>(0));
  CHECK(blindChanges > 300);
}

// Predictions must give the same layout the real sizes would.
TEST(predicted_geometry_matches_fully_measured_geometry) {
  std::vector<std::string> keys = keysFor(300);
  std::vector<double> heights = trueHeightsFor(keys.size());

  Container predicted;
  predictAll(predicted, keys, heights);
  Virtualizer::update(&predicted, inputFor(keys, 0.0));
  Virtualizer::recomputeTotalSize(&predicted);

  Container measured;
  Virtualizer::update(&measured, inputFor(keys, 0.0));
  measureBatch(measured, heights, 0, keys.size() - 1);
  Virtualizer::recomputeTotalSize(&measured);

  checkOffsetsAreExactPrefixSums(predicted, heights, "predicted");
  checkOffsetsAreExactPrefixSums(measured, heights, "measured");
  CHECK_NEAR(predicted.revision.totalContainerHeight, measured.revision.totalContainerHeight, 0.001);
}

/*
 * The total size is right before any row is laid out, so the scroll bar is accurate and
 * scrollToEnd lands in one go.
 */
TEST(total_size_is_exact_before_anything_is_measured) {
  std::vector<std::string> keys = keysFor(1000);
  std::vector<double> heights = trueHeightsFor(keys.size());

  double expectedTotal = 0.0;
  for (double height : heights) {
    expectedTotal += height;
  }

  Container container;
  predictAll(container, keys, heights);
  Virtualizer::update(&container, inputFor(keys, 0.0));
  Virtualizer::recomputeTotalSize(&container);

  for (const Element& element : container.revision.elements) {
    CHECK(!element.measured);
  }
  CHECK_NEAR(container.revision.totalContainerHeight, expectedTotal, 0.001);
}

// A prediction queued before its row exists waits for it instead of being dropped.
TEST(a_prediction_staged_early_lands_when_its_row_arrives) {
  std::vector<std::string> keys = keysFor(10);
  Container container;

  container.setPredictedSize("k7", {WINDOW_WIDTH, 777.0});
  CHECK_EQ(container.predictedSizes.size(), static_cast<std::size_t>(1));

  Virtualizer::update(&container, inputFor(keys, 0.0));

  std::size_t index = container.findElementIndexByKey("k7");
  CHECK_NEAR(container.revision.elements[index].height, 777.0, 0.001);
  CHECK(container.revision.elements[index].predicted);
  // The queue is emptied once the size is on the row.
  CHECK_EQ(container.predictedSizes.size(), static_cast<std::size_t>(0));
}

// A prediction for an existing row applies right away and moves the rows after it.
TEST(a_prediction_for_a_live_row_reflows_the_rows_after_it) {
  std::vector<std::string> keys = keysFor(50);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  double offsetBefore = container.revision.elements[20].offsetY;

  std::size_t reflowFrom = Virtualizer::applyPredictedElementSize(&container, "k5", {WINDOW_WIDTH, 500.0});
  CHECK_EQ(reflowFrom, static_cast<std::size_t>(5));
  Virtualizer::commitElementSizes(&container, reflowFrom);

  CHECK_NEAR(container.revision.elements[5].height, 500.0, 0.001);
  CHECK_NEAR(container.revision.elements[20].offsetY, offsetBefore + (500.0 - ESTIMATED_ROW_HEIGHT), 0.001);
}

/*
 * A prediction equal to the row's current size moves nothing, so resending the same
 * predictions every frame is cheap.
 */
TEST(a_redundant_prediction_reflows_nothing) {
  std::vector<std::string> keys = keysFor(50);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  Virtualizer::applyPredictedElementSize(&container, "k5", {WINDOW_WIDTH, 500.0});
  CHECK_EQ(
    Virtualizer::applyPredictedElementSize(&container, "k5", {WINDOW_WIDTH, 500.0}),
    UNDEFINED_INDEX);
}

/*
 * A real size always wins. It replaces a prediction, and a prediction arriving after a
 * real size is dropped.
 */
TEST(a_real_measurement_supersedes_a_prediction) {
  std::vector<std::string> keys = keysFor(50);
  Container container;
  predictAll(container, keys, std::vector<double>(keys.size(), 200.0));
  Virtualizer::update(&container, inputFor(keys, 0.0));

  CHECK(container.revision.elements[5].predicted);
  Virtualizer::updateElementAtIndex(&container, 5, {WINDOW_WIDTH, 333.0});

  CHECK_NEAR(container.revision.elements[5].height, 333.0, 0.001);
  CHECK(container.revision.elements[5].measured);
  CHECK(!container.revision.elements[5].predicted);
}

TEST(a_late_prediction_never_overwrites_a_measurement) {
  std::vector<std::string> keys = keysFor(50);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));
  Virtualizer::updateElementAtIndex(&container, 5, {WINDOW_WIDTH, 333.0});

  CHECK_EQ(
    Virtualizer::applyPredictedElementSize(&container, "k5", {WINDOW_WIDTH, 999.0}),
    UNDEFINED_INDEX);
  CHECK_NEAR(container.revision.elements[5].height, 333.0, 0.001);

  // It must not be queued either, or it would overwrite the real size on the next update.
  Virtualizer::update(&container, inputFor(keys, 0.0));
  CHECK_NEAR(container.revision.elements[5].height, 333.0, 0.001);
}

/*
 * The average only uses real sizes. A fully predicted list has none, so a row nobody
 * predicted still uses the configured estimate.
 */
TEST(predictions_stay_out_of_the_frozen_average) {
  std::vector<std::string> keys = keysFor(200);
  Container container;
  predictAll(container, keys, std::vector<double>(keys.size(), 500.0));
  Virtualizer::update(&container, inputFor(keys, 0.0));
  Virtualizer::recomputeTotalSize(&container);

  CHECK_EQ(container.revision.measuredRealCount, static_cast<std::size_t>(0));
  CHECK_NEAR(container.revision.averageElementHeight, 0.0, 0.001);

  // A new unpredicted row gets the estimate, not 500.
  std::vector<std::string> grown = keys;
  grown.push_back("fresh");
  Virtualizer::update(&container, inputFor(grown, 0.0));

  std::size_t freshIndex = container.findElementIndexByKey("fresh");
  CHECK_NEAR(container.revision.elements[freshIndex].height, ESTIMATED_ROW_HEIGHT, 0.001);
  CHECK(!container.revision.elements[freshIndex].predicted);
}

// A predicted or measured row has a trusted size, an estimated one does not.
TEST(only_predicted_or_measured_rows_carry_trusted_geometry) {
  std::vector<std::string> keys = keysFor(30);
  Container container;
  container.setPredictedSize("k1", {WINDOW_WIDTH, 200.0});
  Virtualizer::update(&container, inputFor(keys, 0.0));
  Virtualizer::updateElementAtIndex(&container, 2, {WINDOW_WIDTH, 200.0});

  CHECK(container.hasTrustedSize(1));   // predicted
  CHECK(container.hasTrustedSize(2));   // measured
  CHECK(!container.hasTrustedSize(10)); // estimate only
  CHECK(!container.hasTrustedSize(999));
}

/*
 * scrollToIndex on a fresh list. With predictions the target offset is known in the first
 * frame, so it lands at once.
 */
TEST(scroll_to_index_lands_immediately_on_a_cold_predicted_list) {
  std::vector<std::string> keys = keysFor(500);
  std::vector<double> heights = trueHeightsFor(keys.size());

  double expectedOffset = 0.0;
  for (std::size_t index = 0; index < 321; ++index) {
    expectedOffset += heights[index];
  }

  Container container;
  predictAll(container, keys, heights);
  Virtualizer::update(&container, inputFor(keys, 0.0));
  container.scrollToIndex(321);
  Virtualizer::update(&container, inputFor(keys, 0.0));

  CHECK_NEAR(container.revision.containerOffsetY, expectedOffset, 1.0);
}

/*
 * Predicted rows keep their sizes through a prepend, just like measured rows, or a list
 * loading pages would lose its layout every page.
 */
TEST(predictions_survive_a_prepend) {
  std::vector<std::string> keys = keysFor(100);
  std::vector<double> heights = trueHeightsFor(keys.size());

  Container container;
  predictAll(container, keys, heights);
  Virtualizer::update(&container, inputFor(keys, 0.0));

  std::vector<std::string> prepended;
  std::vector<double> prependedHeights;
  for (std::size_t index = 0; index < 20; ++index) {
    prepended.push_back("p" + std::to_string(index));
    prependedHeights.push_back(90.0);
    container.setPredictedSize(prepended.back(), {WINDOW_WIDTH, 90.0});
  }
  prepended.insert(prepended.end(), keys.begin(), keys.end());
  prependedHeights.insert(prependedHeights.end(), heights.begin(), heights.end());

  Virtualizer::update(&container, inputFor(prepended, container.revision.containerOffsetY));

  checkOffsetsAreExactPrefixSums(container, prependedHeights, "after prepend");
  for (const Element& element : container.revision.elements) {
    CHECK(element.predicted);
  }
}

/*
 * The usual case: the host predicts what it can, like text, and measures the rest.
 * Unpredicted rows must not push predicted ones off their exact positions.
 */
TEST(predicted_and_unpredicted_rows_coexist) {
  std::vector<std::string> keys = keysFor(200);
  std::vector<double> heights = trueHeightsFor(keys.size());

  Container container;
  for (std::size_t index = 0; index < keys.size(); index += 2) {
    container.setPredictedSize(keys[index], {WINDOW_WIDTH, heights[index]});
  }
  Virtualizer::update(&container, inputFor(keys, 0.0));

  std::vector<double> expected;
  expected.reserve(keys.size());
  for (std::size_t index = 0; index < keys.size(); ++index) {
    expected.push_back(index % 2 == 0 ? heights[index] : ESTIMATED_ROW_HEIGHT);
  }
  checkOffsetsAreExactPrefixSums(container, expected, "mixed list");

  // Only the odd, unpredicted rows report a change when measured.
  std::size_t changed = 0;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    if (Virtualizer::applyElementSize(&container, index, {WINDOW_WIDTH, heights[index]})) {
      changed++;
      CHECK(index % 2 == 1);
    }
  }
  CHECK_EQ(changed, keys.size() / 2);
}

/*
 * Predictions arrive whenever the host finishes measuring, rarely in a frame that changes
 * keys. So they apply on every frame, not only when the data changes.
 */
TEST(a_prediction_staged_on_a_settled_list_lands_on_the_next_frame) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  double offsetBefore = container.revision.elements[40].offsetY;

  // Same keys, so the data is not reconciled in this frame.
  container.setPredictedSize("k3", {WINDOW_WIDTH, 400.0});
  Virtualizer::update(&container, inputFor(keys, 0.0));

  CHECK_NEAR(container.revision.elements[3].height, 400.0, 0.001);
  CHECK(container.revision.elements[3].predicted);
  CHECK_EQ(container.predictedSizes.size(), static_cast<std::size_t>(0));
  CHECK_NEAR(container.revision.elements[40].offsetY, offsetBefore + (400.0 - ESTIMATED_ROW_HEIGHT), 0.001);
}

// Same, when a scroll frame tells the core the keys did not change.
TEST(a_prediction_lands_even_when_the_keys_shortcut_is_engaged) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  container.setPredictedSize("k3", {WINDOW_WIDTH, 400.0});
  FrameInput scrollFrame = inputFor(keys, 0.0);
  scrollFrame.keysUnchanged = true;
  Virtualizer::update(&container, scrollFrame);

  CHECK_NEAR(container.revision.elements[3].height, 400.0, 0.001);
  CHECK(container.revision.elements[3].predicted);
}

/*
 * Text wraps to the row width, so predicted heights are wrong after a resize. Predicted rows
 * go back to the estimate, while natively measured rows keep their real sizes.
 */
TEST(invalidating_predictions_returns_rows_to_the_estimate) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  predictAll(container, keys, std::vector<double>(keys.size(), 400.0));
  Virtualizer::update(&container, inputFor(keys, 0.0));
  Virtualizer::updateElementAtIndex(&container, 2, {WINDOW_WIDTH, 333.0});

  container.setPredictedSize("k59", {WINDOW_WIDTH, 400.0});
  Virtualizer::invalidatePredictions(&container);
  Virtualizer::update(&container, inputFor(keys, 0.0));

  CHECK_EQ(container.predictedSizes.size(), static_cast<std::size_t>(0));
  CHECK_NEAR(container.revision.elements[0].height, ESTIMATED_ROW_HEIGHT, 0.001);
  CHECK(!container.revision.elements[0].predicted);
  CHECK(!container.hasTrustedSize(0));

  // The measured row keeps its real size.
  CHECK_NEAR(container.revision.elements[2].height, 333.0, 0.001);
  CHECK(container.hasTrustedSize(2));
}

/*
 * Appending is the most common change, like a chat message or a new page, so it updates
 * the rows in place instead of rebuilding them. These check it matches a full rebuild,
 * including how duplicate keys resolve.
 */
TEST(appending_preserves_every_surviving_row) {
  std::vector<std::string> keys = keysFor(500);
  std::vector<double> heights = trueHeightsFor(keys.size());

  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));
  measureBatch(container, heights, 0, keys.size() - 1);

  std::vector<double> before;
  for (const Element& element : container.revision.elements) {
    before.push_back(element.offsetY);
  }

  std::vector<std::string> grown = keys;
  for (std::size_t index = 0; index < 30; ++index) {
    grown.push_back("appended" + std::to_string(index));
  }
  Virtualizer::update(&container, inputFor(grown, 0.0));

  CHECK_EQ(container.revision.elements.size(), grown.size());
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const Element& element = container.revision.elements[index];
    CHECK_EQ(element.key, keys[index]);
    CHECK_EQ(element.index, index);
    CHECK(element.measured);
    CHECK_NEAR(element.height, heights[index], 0.001);
    CHECK_NEAR(element.offsetY, before[index], 0.001);
    CHECK_EQ(container.findElementIndexByKey(keys[index]), index);
  }
  for (std::size_t index = 0; index < 30; ++index) {
    std::size_t at = keys.size() + index;
    CHECK_EQ(container.revision.elements[at].key, "appended" + std::to_string(index));
    CHECK_EQ(container.findElementIndexByKey("appended" + std::to_string(index)), at);
    CHECK(!container.revision.elements[at].measured);
  }
}

/*
 * An append in place must match a list built from scratch. Both get the same sizes so they
 * share the same average, otherwise the test would compare averages instead.
 */
TEST(in_place_append_matches_a_full_rebuild) {
  std::vector<std::string> keys = keysFor(200);
  std::vector<double> heights = trueHeightsFor(keys.size());

  std::vector<std::string> grown = keys;
  for (std::size_t index = 0; index < 25; ++index) {
    grown.push_back("x" + std::to_string(index));
  }

  // Appended later, which takes the in place path.
  Container appended;
  Virtualizer::update(&appended, inputFor(keys, 0.0));
  measureBatch(appended, heights, 0, keys.size() - 1);
  Virtualizer::update(&appended, inputFor(grown, 0.0));
  Virtualizer::recomputeTotalSize(&appended);

  // Built with all the keys from the start, with no append.
  Container wholesale;
  Virtualizer::update(&wholesale, inputFor(grown, 0.0));
  measureBatch(wholesale, heights, 0, keys.size() - 1);
  Virtualizer::recomputeTotalSize(&wholesale);

  CHECK_EQ(appended.revision.elements.size(), wholesale.revision.elements.size());
  for (std::size_t index = 0; index < grown.size(); ++index) {
    CHECK_EQ(appended.revision.elements[index].key, wholesale.revision.elements[index].key);
    CHECK_EQ(appended.revision.elements[index].index, wholesale.revision.elements[index].index);
    CHECK_NEAR(
      appended.revision.elements[index].offsetY,
      wholesale.revision.elements[index].offsetY,
      0.001);
    CHECK_NEAR(
      appended.revision.elements[index].height,
      wholesale.revision.elements[index].height,
      0.001);
  }
  CHECK_NEAR(appended.revision.totalContainerHeight, wholesale.revision.totalContainerHeight, 0.001);
  CHECK_EQ(appended.revision.measuredRealCount, wholesale.revision.measuredRealCount);
}

// An appended duplicate key resolves to its first occurrence, same as a full rebuild.
TEST(appending_a_duplicate_key_keeps_the_first_occurrence) {
  std::vector<std::string> keys = keysFor(20);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  std::vector<std::string> grown = keys;
  grown.push_back("k3");
  Virtualizer::update(&container, inputFor(grown, 0.0));

  CHECK_EQ(container.revision.elements.size(), grown.size());
  CHECK_EQ(container.findElementIndexByKey("k3"), static_cast<std::size_t>(3));
  CHECK_EQ(container.revision.elements[20].key, std::string("k3"));
}

// A shorter or reordered key list must not be taken for an append.
TEST(append_fast_path_declines_non_append_shapes) {
  std::vector<std::string> keys = keysFor(50);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  // A prepend is longer, but the start does not match.
  std::vector<std::string> prepended = {"new0", "new1"};
  prepended.insert(prepended.end(), keys.begin(), keys.end());
  Virtualizer::update(&container, inputFor(prepended, 0.0));
  CHECK_EQ(container.revision.elements[0].key, std::string("new0"));
  CHECK_EQ(container.findElementIndexByKey("k0"), static_cast<std::size_t>(2));
  for (std::size_t index = 0; index < prepended.size(); ++index) {
    CHECK_EQ(container.revision.elements[index].index, index);
  }

  /*
   * A removal is shorter.
   */
  std::vector<std::string> shrunk(keys.begin(), keys.begin() + 10);
  Virtualizer::update(&container, inputFor(shrunk, 0.0));
  CHECK_EQ(container.revision.elements.size(), static_cast<std::size_t>(10));
  CHECK_EQ(container.findElementIndexByKey("k9"), static_cast<std::size_t>(9));
  CHECK_EQ(container.findElementIndexByKey("new0"), UNDEFINED_INDEX);
}

/*
 * Prepending is how a chat loads older messages. In the in place path the existing rows
 * keep their sizes, every index is renumbered, and keys still find their rows.
 */
TEST(prepending_preserves_every_surviving_row) {
  std::vector<std::string> keys = keysFor(300);
  std::vector<double> heights = trueHeightsFor(keys.size());

  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));
  measureBatch(container, heights, 0, keys.size() - 1);

  const std::size_t prependCount = 40;
  std::vector<std::string> prepended;
  for (std::size_t index = 0; index < prependCount; ++index) {
    prepended.push_back("older" + std::to_string(index));
  }
  prepended.insert(prepended.end(), keys.begin(), keys.end());

  Virtualizer::update(&container, inputFor(prepended, 0.0));

  CHECK_EQ(container.revision.elements.size(), prepended.size());
  for (std::size_t index = 0; index < prepended.size(); ++index) {
    CHECK_EQ(container.revision.elements[index].key, prepended[index]);
    CHECK_EQ(container.revision.elements[index].index, index);
    CHECK_EQ(container.findElementIndexByKey(prepended[index]), index);
  }
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const Element& element = container.revision.elements[index + prependCount];
    CHECK(element.measured);
    CHECK_NEAR(element.height, heights[index], 0.001);
  }
}

/*
 * After a prepend in place every measured row keeps its height, and the old rows move down
 * by exactly the height of the new ones. This is not compared with a rebuilt list, since
 * that one would fix its average from different rows.
 */
TEST(in_place_prepend_shifts_survivors_by_exactly_the_prepended_height) {
  std::vector<std::string> keys = keysFor(150);
  std::vector<double> heights = trueHeightsFor(keys.size());

  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));
  measureBatch(container, heights, 0, keys.size() - 1);
  Virtualizer::recomputeTotalSize(&container);

  std::vector<double> offsetsBefore;
  for (const Element& element : container.revision.elements) {
    offsetsBefore.push_back(element.offsetY);
  }
  double totalBefore = container.revision.totalContainerHeight;

  const std::size_t prependCount = 20;
  std::vector<std::string> prepended;
  for (std::size_t index = 0; index < prependCount; ++index) {
    prepended.push_back("p" + std::to_string(index));
  }
  prepended.insert(prepended.end(), keys.begin(), keys.end());

  Virtualizer::update(&container, inputFor(prepended, 0.0));
  Virtualizer::recomputeTotalSize(&container);

  // The prepended rows are unmeasured, so they all share the same fallback size.
  double fallback = container.revision.elements[0].height;
  double prependedTotal = 0.0;
  for (std::size_t index = 0; index < prependCount; ++index) {
    const Element& element = container.revision.elements[index];
    CHECK(!element.measured);
    CHECK_NEAR(element.height, fallback, 0.001);
    CHECK_NEAR(element.offsetY, prependedTotal, 0.001);
    prependedTotal += element.height;
  }

  // The old rows keep their heights and move down by exactly the new rows' height.
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const Element& element = container.revision.elements[index + prependCount];
    CHECK_EQ(element.key, keys[index]);
    CHECK(element.measured);
    CHECK_NEAR(element.height, heights[index], 0.001);
    CHECK_NEAR(element.offsetY, offsetsBefore[index] + prependedTotal, 0.001);
  }

  CHECK_NEAR(container.revision.totalContainerHeight, totalBefore + prependedTotal, 0.001);
}

/*
 * A prepended key that already exists further down resolves to its first occurrence, the
 * new one. The in place path skips this case instead of getting it wrong.
 */
TEST(prepending_an_existing_key_still_resolves_to_the_first_occurrence) {
  std::vector<std::string> keys = keysFor(30);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  std::vector<std::string> prepended = {"k7", "fresh"};
  prepended.insert(prepended.end(), keys.begin(), keys.end());
  Virtualizer::update(&container, inputFor(prepended, 0.0));

  CHECK_EQ(container.revision.elements.size(), prepended.size());
  CHECK_EQ(container.findElementIndexByKey("k7"), static_cast<std::size_t>(0));
  CHECK_EQ(container.revision.elements[9].key, std::string("k7"));
  for (std::size_t index = 0; index < prepended.size(); ++index) {
    CHECK_EQ(container.revision.elements[index].index, index);
  }
}

// Keys still find their rows after several prepends in a row.
TEST(chained_prepends_keep_the_key_map_correct) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  std::vector<std::string> current = keys;
  for (std::size_t round = 0; round < 4; ++round) {
    std::vector<std::string> next;
    for (std::size_t index = 0; index < 15; ++index) {
      next.push_back("r" + std::to_string(round) + "_" + std::to_string(index));
    }
    next.insert(next.end(), current.begin(), current.end());
    current = next;
    Virtualizer::update(&container, inputFor(current, 0.0));
  }

  CHECK_EQ(container.revision.elements.size(), current.size());
  for (std::size_t index = 0; index < current.size(); ++index) {
    CHECK_EQ(container.findElementIndexByKey(current[index]), index);
    CHECK_EQ(container.revision.elements[index].index, index);
  }
}
