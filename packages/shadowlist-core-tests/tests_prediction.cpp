/*
 * Ahead-of-time size predictions.
 *
 * A row's height is normally discovered the expensive way: JS renders it, Fabric commits
 * it, Yoga lays it out, and only then does the core learn it guessed wrong and reflow
 * everything after it. On a variable-height list that correction runs continuously during
 * a scroll, and every one of them can move the scroll offset via MVCP.
 *
 * A prediction is the same number arriving before the row is rendered, measured by the host
 * off the critical path. The properties that make that worth having:
 *
 *   * EXACTNESS: a correct prediction means the later native measurement reports "nothing
 *     changed" and reflows nothing. This is the whole point -- geometry is stable from the
 *     first frame instead of converging over the scroll.
 *   * DEFERENCE: a real native measurement always outranks a prediction, whenever it
 *     arrives and in whatever order. A prediction can never overwrite measured truth.
 *   * PURITY: predictions never enter the frozen average (Revision::measuredReal*). That
 *     average sizes rows nobody knows anything about and must stay a sample of real
 *     measurements, or estimates would start being derived from estimates.
 *   * TRUST: a predicted row carries geometry the core can rely on, which a merely
 *     estimated row does not.
 *
 * Geometry is cross-checked against a brute-force prefix sum, so a reflow that lands on the
 * wrong offsets shows up as a mismatch rather than as a plausible-looking number.
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
 * Deliberately nothing like the estimate, so a row still carrying the fallback is
 * distinguishable from one carrying its true size.
 */
std::vector<double> trueHeightsFor(std::size_t count) {
  std::vector<double> heights;
  heights.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    heights.push_back(44.0 + static_cast<double>((index * 53) % 301));
  }
  return heights;
}

// Stage a prediction for every row; the next update consumes them.
void predictAll(Container& container, const std::vector<std::string>& keys, const std::vector<double>& heights) {
  for (std::size_t index = 0; index < keys.size(); ++index) {
    container.setPredictedSize(keys[index], {WINDOW_WIDTH, heights[index]});
  }
}

/*
 * Feed real native measurements the way a Fabric layout pass does -- batched, with a single
 * reflow -- and report how many rows actually moved geometry. Zero is the win condition.
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
 * The headline property. With exact predictions in hand the native measurements that
 * eventually arrive are all no-ops, so the scroll never pays a reflow -- while the same
 * list without predictions reflows on essentially every row it reveals.
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

/*
 * A prediction is only useful if it produces the same geometry the real measurement would
 * have. Anything less just trades one wrong answer for another.
 */
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
 * The total is right before a single row has been laid out, which is what makes the
 * scrollbar honest and stops scrollToEnd converging over many frames.
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

/*
 * A prediction staged before its row exists waits, rather than being dropped.
 */
TEST(a_prediction_staged_early_lands_when_its_row_arrives) {
  std::vector<std::string> keys = keysFor(10);
  Container container;

  container.setPredictedSize("k7", {WINDOW_WIDTH, 777.0});
  CHECK_EQ(container.predictedSizes.size(), static_cast<std::size_t>(1));

  Virtualizer::update(&container, inputFor(keys, 0.0));

  std::size_t index = container.findElementIndexByKey("k7");
  CHECK_NEAR(container.revision.elements[index].height, 777.0, 0.001);
  CHECK(container.revision.elements[index].predicted);
  // Consumed, not retained: the size now lives on the element.
  CHECK_EQ(container.predictedSizes.size(), static_cast<std::size_t>(0));
}

/*
 * A prediction for a row that already exists applies immediately and reflows behind it.
 */
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
 * A prediction that matches what the row already carries moves nothing, so a host that
 * re-publishes its whole measurement window every frame costs a lookup, not a reflow.
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
 * DEFERENCE, both orderings: a measurement lands on a predicted row and wins; a prediction
 * lands on a measured row and is discarded.
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

  /*
   * And it must not have been quietly staged either -- a staged entry would resurface on
   * the next reconcile and overwrite the measurement then.
   */
  Virtualizer::update(&container, inputFor(keys, 0.0));
  CHECK_NEAR(container.revision.elements[5].height, 333.0, 0.001);
}

/*
 * PURITY: the frozen average is a sample of real measurements. A fully predicted list has
 * taken no such sample, so rows outside the prediction window must still fall back to the
 * configured estimate rather than to an average of guesses.
 */
TEST(predictions_stay_out_of_the_frozen_average) {
  std::vector<std::string> keys = keysFor(200);
  Container container;
  predictAll(container, keys, std::vector<double>(keys.size(), 500.0));
  Virtualizer::update(&container, inputFor(keys, 0.0));
  Virtualizer::recomputeTotalSize(&container);

  CHECK_EQ(container.revision.measuredRealCount, static_cast<std::size_t>(0));
  CHECK_NEAR(container.revision.averageElementHeight, 0.0, 0.001);

  // A row nobody predicted still gets the configured estimate, not 500.
  std::vector<std::string> grown = keys;
  grown.push_back("fresh");
  Virtualizer::update(&container, inputFor(grown, 0.0));

  std::size_t freshIndex = container.findElementIndexByKey("fresh");
  CHECK_NEAR(container.revision.elements[freshIndex].height, ESTIMATED_ROW_HEIGHT, 0.001);
  CHECK(!container.revision.elements[freshIndex].predicted);
}

/*
 * TRUST: a predicted or measured row has trusted geometry, an estimated one does not.
 */
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
 * scrollToIndex on a cold list: with predictions the target offset is known on the first
 * frame, so it lands immediately instead of converging as rows are revealed.
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
 * Predictions must survive the reconciles a live list actually performs. A prepend shifts
 * every index; predicted rows have to keep their sizes across it exactly as measured rows
 * do, or a paginating list would lose its geometry every page.
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
 * A partially predicted list is the realistic case: the host predicts what it can (text)
 * and leaves the rest to measurement. The two must coexist without the unpredicted rows
 * dragging the predicted ones off their exact offsets.
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

  // Measuring the odd rows only reports changes for the odd rows.
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
 * Predictions arrive whenever the host finishes measuring, which is almost never a frame
 * that changed keys. Draining only on reconcile would leave them stranded on a settled
 * list -- where the next data commit may never come -- so the drain runs per frame.
 */
TEST(a_prediction_staged_on_a_settled_list_lands_on_the_next_frame) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  double offsetBefore = container.revision.elements[40].offsetY;

  // Same keys, so this frame performs no reconcile at all.
  container.setPredictedSize("k3", {WINDOW_WIDTH, 400.0});
  Virtualizer::update(&container, inputFor(keys, 0.0));

  CHECK_NEAR(container.revision.elements[3].height, 400.0, 0.001);
  CHECK(container.revision.elements[3].predicted);
  CHECK_EQ(container.predictedSizes.size(), static_cast<std::size_t>(0));
  CHECK_NEAR(container.revision.elements[40].offsetY, offsetBefore + (400.0 - ESTIMATED_ROW_HEIGHT), 0.001);
}

/*
 * Same, with the props-identity shortcut engaged -- the path a real scroll frame takes,
 * where the core is explicitly told not to revalidate the key collection.
 */
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
 * Text wraps to the row width, so every height measured at the old width is wrong after a
 * resize. Predicted rows must return to the estimate rather than sit on confidently wrong
 * geometry -- while natively measured rows, whose sizes are real, are left alone.
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

  // The natively measured row keeps its real size.
  CHECK_NEAR(container.revision.elements[2].height, 333.0, 0.001);
  CHECK(container.hasTrustedSize(2));
}

/*
 * Appending is the most common data change a list sees (every chat message, every page of
 * pagination), so it takes an in-place path instead of rebuilding the element vector and
 * key->index map. These pin that path against the general one: same geometry, same map,
 * same surviving state, including the duplicate-key rule.
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
 * The in-place path must land on exactly the geometry a from-scratch build produces.
 *
 * The control is built directly with the final key set and given the same measurements, so
 * both containers hold the same frozen average -- otherwise the unmeasured tail is sized
 * from different data and the comparison tests the average, not the append.
 */
TEST(in_place_append_matches_a_full_rebuild) {
  std::vector<std::string> keys = keysFor(200);
  std::vector<double> heights = trueHeightsFor(keys.size());

  std::vector<std::string> grown = keys;
  for (std::size_t index = 0; index < 25; ++index) {
    grown.push_back("x" + std::to_string(index));
  }

  // Appended incrementally: takes the in-place fast path.
  Container appended;
  Virtualizer::update(&appended, inputFor(keys, 0.0));
  measureBatch(appended, heights, 0, keys.size() - 1);
  Virtualizer::update(&appended, inputFor(grown, 0.0));
  Virtualizer::recomputeTotalSize(&appended);

  // Built with the final key set from the start: never sees an append at all.
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

/*
 * A duplicate key appended to a list that already contains it must resolve to the FIRST
 * occurrence, exactly as the rebuild path's emplace does.
 */
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

/*
 * A shorter or reordered key set must NOT be mistaken for an append.
 */
TEST(append_fast_path_declines_non_append_shapes) {
  std::vector<std::string> keys = keysFor(50);
  Container container;
  Virtualizer::update(&container, inputFor(keys, 0.0));

  // Prepend: longer, but the prefix does not match.
  std::vector<std::string> prepended = {"new0", "new1"};
  prepended.insert(prepended.end(), keys.begin(), keys.end());
  Virtualizer::update(&container, inputFor(prepended, 0.0));
  CHECK_EQ(container.revision.elements[0].key, std::string("new0"));
  CHECK_EQ(container.findElementIndexByKey("k0"), static_cast<std::size_t>(2));
  for (std::size_t index = 0; index < prepended.size(); ++index) {
    CHECK_EQ(container.revision.elements[index].index, index);
  }

  // Removal: shorter.
  std::vector<std::string> shrunk(keys.begin(), keys.begin() + 10);
  Virtualizer::update(&container, inputFor(shrunk, 0.0));
  CHECK_EQ(container.revision.elements.size(), static_cast<std::size_t>(10));
  CHECK_EQ(container.findElementIndexByKey("k9"), static_cast<std::size_t>(9));
  CHECK_EQ(container.findElementIndexByKey("new0"), UNDEFINED_INDEX);
}

/*
 * Prepending is how a chat paginates older messages in, and the whole existing list
 * survives it. These pin the in-place prepend path: surviving rows keep their sizes and
 * measured state, every index is renumbered, and the key->index map still resolves.
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
 * The in-place prepend must leave the surviving list geometrically intact: every measured
 * row keeps its exact height, and the whole block simply shifts down by the height of what
 * was prepended.
 *
 * Checked as invariants rather than against a second container: a container built wholesale
 * freezes its average from a different sample, so its unmeasured rows are legitimately sized
 * differently and the comparison would test the average rather than the prepend.
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

  // Every prepended row is unmeasured, so they all carry one and the same fallback size.
  double fallback = container.revision.elements[0].height;
  double prependedTotal = 0.0;
  for (std::size_t index = 0; index < prependCount; ++index) {
    const Element& element = container.revision.elements[index];
    CHECK(!element.measured);
    CHECK_NEAR(element.height, fallback, 0.001);
    CHECK_NEAR(element.offsetY, prependedTotal, 0.001);
    prependedTotal += element.height;
  }

  // Survivors keep their measured heights and shift by exactly the prepended block.
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
 * A prepended key that already exists further down must resolve to the FIRST occurrence,
 * which is the prepended one. The fast path declines this shape rather than get it wrong.
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

/*
 * Chained prepends: the map must stay correct across several in-place shifts, not just one.
 */
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
