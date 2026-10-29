/*
 * ListUpdate tests: the data change rules both native kits share.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListSelection.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

using Indices = std::vector<std::size_t>;

TEST(insertion_positions_are_sorted_and_unique) {
  CHECK(insertionIndices({4, 1, 4, 2}, 10) == (Indices{1, 2, 4}));
  CHECK(insertionIndices({}, 10).empty());
}

TEST(insertion_positions_past_the_end_go_at_the_end) {
  // Three rows before. Inserts at 7 and 9 land at 3 and 4, after the old rows.
  CHECK(insertionIndices({7, 9}, 3) == (Indices{3, 4}));
  // One in range, then two past the end of the new data.
  CHECK(insertionIndices({1, 8, 20}, 3) == (Indices{1, 4, 5}));
  // An insert right at the end stays.
  CHECK(insertionIndices({3}, 3) == (Indices{3}));
  // Into an empty list every insert counts from 0.
  CHECK(insertionIndices({5, 6}, 0) == (Indices{0, 1}));
}

TEST(insertion_positions_stay_inside_the_new_data) {
  Indices positions = insertionIndices({100, 50, 2, 3}, 4);
  for (std::size_t position : positions) {
    CHECK(position < 4 + positions.size());
  }
  CHECK(positions == (Indices{2, 3, 6, 7}));
}

TEST(deletion_positions_drop_rows_past_the_end) {
  CHECK(deletionIndices({5, 1, 1, 9, 3}, 6) == (Indices{1, 3, 5}));
  CHECK(deletionIndices({6, 7}, 6).empty());
}

TEST(list_driver_appends_inserts_named_past_the_end) {
  ListDriver driver;
  driver.setKeys({"a", "b", "c"});
  Indices positions = insertionIndices({7, 9}, driver.getKeyCount());
  driver.insertKeys(positions, {"d", "e"});
  CHECK(driver.getKeys() == (std::vector<std::string>{"a", "b", "c", "d", "e"}));
  // The driver keeps the same rule when it gets the raw indices.
  driver.insertKeys({20}, {"f"});
  CHECK(driver.getKeys() == (std::vector<std::string>{"a", "b", "c", "d", "e", "f"}));
}

using Keys = std::vector<std::string>;

/*
 * ShadowListKitListView.applyRowKeys on Android finds the same splice with commonPrefix and commonSuffix
 * over its own key list. These are the cases it has to match.
 */
TEST(key_splice_finds_the_changed_middle) {
  KeySplice prepend = keySplice({"c", "d"}, {"a", "b", "c", "d"});
  CHECK_EQ(prepend.start, std::size_t{0});
  CHECK_EQ(prepend.deleted, std::size_t{0});
  CHECK_EQ(prepend.inserted, std::size_t{2});

  KeySplice trim = keySplice({"a", "b", "c", "d"}, {"a", "b"});
  CHECK_EQ(trim.start, std::size_t{2});
  CHECK_EQ(trim.deleted, std::size_t{2});
  CHECK_EQ(trim.inserted, std::size_t{0});

  KeySplice middle = keySplice({"a", "b", "c", "d"}, {"a", "x", "y", "d"});
  CHECK_EQ(middle.start, std::size_t{1});
  CHECK_EQ(middle.deleted, std::size_t{2});
  CHECK_EQ(middle.inserted, std::size_t{2});

  CHECK(keySplice({"a", "b"}, {"a", "b"}).isEmpty());
  CHECK(keySplice({}, {}).isEmpty());
}

TEST(key_splice_never_counts_a_key_in_both_ends) {
  // "a a" to "a": the prefix takes the one shared key and the suffix finds none left.
  KeySplice splice = keySplice({"a", "a"}, {"a"});
  CHECK_EQ(splice.start, std::size_t{1});
  CHECK_EQ(splice.deleted, std::size_t{1});
  CHECK_EQ(splice.inserted, std::size_t{0});

  KeySplice cleared = keySplice({"a", "b"}, {});
  CHECK_EQ(cleared.start, std::size_t{0});
  CHECK_EQ(cleared.deleted, std::size_t{2});
}

TEST(rows_of_keys_and_keys_of_rows_round_trip) {
  Keys keys = {"a", "b", "c", "d"};
  std::unordered_set<std::string> found = keysOfRows(keys, {3, 1, 9});
  CHECK_EQ(found.size(), std::size_t{2});
  CHECK(found.count("b") > 0 && found.count("d") > 0);
  CHECK(rowsOfKeys(keys, found) == (Indices{1, 3}));
  CHECK(rowsOfKeys(keys, {}).empty());
}

TEST(keys_from_plan_keep_old_keys_and_read_new_ones) {
  Keys previous = {"a", "b", "c"};
  BatchUpdate batch;
  batch.deleted = {1};
  batch.inserted = {0, 3};
  std::optional<BatchPlan> plan = planBatch(previous.size(), 4, batch);
  CHECK(plan.has_value());
  Indices read;
  Keys next = keysFromPlan(*plan, previous, [&](std::size_t index) {
    read.push_back(index);
    return "n" + std::to_string(index);
  });
  CHECK(next == (Keys{"n0", "a", "c", "n3"}));
  CHECK(read == (Indices{0, 3}));
}

/*
 * applyChanges on every kit. UIKit, the Android kit and the wasm kit keep their versions here,
 * beside the core and outliving it.
 */
TEST(content_versions_report_items_whose_version_changed) {
  ContentVersions versions;
  CHECK(versions.update({"a", "b"}, {1, 1}).empty());
  versions.record({"a", "b", "c"}, {1, 1, 1});
  // b changed, c moved first and kept its version, d is new and has nothing to compare.
  Indices changed = versions.update({"c", "a", "b", "d"}, {1, 1, 2, 5});
  CHECK(changed == (Indices{2}));
  // The new versions are kept: the same data again changes nothing.
  CHECK(versions.update({"c", "a", "b", "d"}, {1, 1, 2, 5}).empty());
  CHECK(versions.update({"d"}, {6}) == (Indices{0}));
  // Keys the data dropped are forgotten: a comes back with nothing to compare.
  CHECK(versions.update({"a", "d"}, {9, 6}).empty());
  CHECK(versions.update({"a", "d"}, {9, 7}) == (Indices{1}));
  // A key named twice keeps its first version.
  CHECK(versions.update({"a", "a"}, {9, 3}).empty());
  CHECK(versions.update({"a"}, {9}).empty());
}

/*
 * performBatchUpdates on every kit: changes inside nested blocks collect until the outermost
 * returns, and the batch taken plans the same as one built by hand.
 */
TEST(batch_collector_collects_until_the_outermost_block_returns) {
  BatchCollector collector;
  CHECK(!collector.isCollecting());
  CHECK(!collector.end());
  collector.begin();
  collector.addDeleted({4, 1});
  collector.begin();
  collector.addInserted({0});
  collector.addMoved(2, 3);
  collector.addReloaded({5});
  CHECK(!collector.end());
  CHECK(collector.isCollecting());
  collector.addInserted({5});
  CHECK(collector.end());
  CHECK(!collector.isCollecting());

  CollectedBatch batch = collector.take();
  CHECK(!batch.isEmpty() && !batch.needsReload);
  CHECK(batch.update.deleted == (Indices{4, 1}));
  CHECK(batch.update.inserted == (Indices{0, 5}));
  CHECK_EQ(batch.update.moved.size(), std::size_t(1));
  CHECK(batch.update.moved[0].from == 2 && batch.update.moved[0].to == 3);
  CHECK(batch.update.reloaded == (Indices{5}));
  // Taking leaves nothing behind.
  CHECK(collector.take().isEmpty());

  BatchUpdate byHand;
  byHand.deleted = {4, 1};
  byHand.inserted = {0, 5};
  byHand.moved = {{2, 3}};
  std::optional<BatchPlan> collected = planBatch(6, 6, batch.update);
  std::optional<BatchPlan> built = planBatch(6, 6, byHand);
  CHECK(collected.has_value() && built.has_value());
  CHECK(collected->sources == built->sources);
}

/*
 * A reloadData inside a batch makes the host read everything again, even with nothing else in it.
 */
TEST(batch_collector_keeps_a_reload_inside_the_batch) {
  BatchCollector collector;
  collector.begin();
  collector.markNeedsReload();
  CHECK(collector.end());
  CollectedBatch batch = collector.take();
  CHECK(batch.needsReload && batch.update.isEmpty() && !batch.isEmpty());
  CHECK(!collector.take().needsReload);
}

/*
 * The keys the Android and wasm kits hold beside their core. A change brings only the middle,
 * and the diff, the content versions and the selection read the keys held.
 */
namespace {

Keys keysOf(KeyList list) {
  return Keys(list.begin(), list.end());
}

}

TEST(held_keys_splice_the_middle_and_diff_the_items) {
  HeldKeys held;
  HeldKeysCommit first = held.commit({0, 0, {"a", "b", "c", "d"}}, std::nullopt, true);
  CHECK(first.rows.start == 0 && first.rows.deleted == 0 && first.rows.inserted == 4);
  CHECK(first.diff.inserted == (Indices{0, 1, 2, 3}));
  // a and d stay at the ends. c moves before b and x is new.
  HeldKeysCommit next = held.commit({1, 1, {"c", "x", "b"}}, std::nullopt, true);
  CHECK(next.rows.start == 1 && next.rows.deleted == 2 && next.rows.inserted == 3);
  CHECK(keysOf(held.rows()) == (Keys{"a", "c", "x", "b", "d"}));
  CHECK(keysOf(held.items()) == keysOf(held.rows()));
  CHECK(next.diff.deleted.empty());
  CHECK(next.diff.inserted == (Indices{2}));
  CHECK(next.diff.moved.size() == 1);
  CHECK(next.diff.moved[0].from == 2 && next.diff.moved[0].to == 1);
  // Ends past the keys held are clipped.
  HeldKeysCommit clipped = held.commit({9, 9, {}}, std::nullopt, false);
  CHECK(clipped.rows.start == 5 && clipped.rows.deleted == 0);
  CHECK(clipped.diff.isEmpty());
}

TEST(held_keys_keep_the_items_of_a_list_with_sections_apart) {
  HeldKeys held;
  held.commit({0, 0, {"h0", "a", "b", "h1", "c"}}, KeyChange{0, 0, {"a", "b", "c"}}, false);
  CHECK(keysOf(held.items()) == (Keys{"a", "b", "c"}));
  HeldKeysCommit next = held.commit({2, 2, {}}, KeyChange{1, 1, {}}, true);
  CHECK(keysOf(held.rows()) == (Keys{"h0", "a", "h1", "c"}));
  CHECK(keysOf(held.items()) == (Keys{"a", "c"}));
  CHECK(next.diff.deleted == (Indices{1}));
  // Without sections again the items are the rows.
  held.commit({0, 0, {"a", "c"}}, std::nullopt, false);
  CHECK(keysOf(held.items()) == keysOf(held.rows()));
}

TEST(held_keys_insert_and_delete_rows_at_positions) {
  HeldKeys held;
  held.insertRows(insertionIndices({0, 1}, 0), {"a", "b"});
  CHECK(keysOf(held.rows()) == (Keys{"a", "b"}));
  // One between, one past the end.
  held.insertRows(insertionIndices({1, 9}, 2), {"x", "y"});
  CHECK(keysOf(held.rows()) == (Keys{"a", "x", "b", "y"}));
  held.deleteRows({3, 0, 7, 0});
  CHECK(keysOf(held.rows()) == (Keys{"x", "b"}));
  held.deleteRows({0, 1});
  CHECK(held.rows().empty());
}

TEST(selection_and_content_versions_read_held_keys) {
  HeldKeys held;
  held.commit({0, 0, {"a", "b", "c"}}, std::nullopt, false);
  ListSelection selection;
  selection.setMultiple(true);
  std::vector<std::string> deselected;
  selection.select("a", deselected);
  selection.select("c", deselected);
  CHECK(selection.indicesIn(held.rows()) == (Indices{0, 2}));
  ContentVersions versions;
  versions.record(held.items(), {1, 1, 1});
  held.commit({0, 1, {"b"}}, std::nullopt, false);
  CHECK(selection.retainKeys(held.rows()));
  CHECK(selection.indicesIn(held.rows()) == (Indices{1}));
  CHECK(versions.update(held.items(), {2, 1}) == (Indices{0}));
}
