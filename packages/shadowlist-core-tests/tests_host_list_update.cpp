/*
 * ListUpdate tests: the data change rules both native kits share.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

using Indices = std::vector<std::size_t>;

TEST(insertion_positions_are_sorted_and_unique) {
  CHECK(insertionPositions({4, 1, 4, 2}, 10) == (Indices{1, 2, 4}));
  CHECK(insertionPositions({}, 10).empty());
}

TEST(insertion_positions_past_the_end_go_at_the_end) {
  // Three rows before. Inserts at 7 and 9 land at 3 and 4, after the old rows.
  CHECK(insertionPositions({7, 9}, 3) == (Indices{3, 4}));
  // One in range, then two past the end of the new data.
  CHECK(insertionPositions({1, 8, 20}, 3) == (Indices{1, 4, 5}));
  // An insert right at the end stays.
  CHECK(insertionPositions({3}, 3) == (Indices{3}));
  // Into an empty list every insert counts from 0.
  CHECK(insertionPositions({5, 6}, 0) == (Indices{0, 1}));
}

TEST(insertion_positions_stay_inside_the_new_data) {
  Indices positions = insertionPositions({100, 50, 2, 3}, 4);
  for (std::size_t position : positions) {
    CHECK(position < 4 + positions.size());
  }
  CHECK(positions == (Indices{2, 3, 6, 7}));
}

TEST(deletion_positions_drop_rows_past_the_end) {
  CHECK(deletionPositions({5, 1, 1, 9, 3}, 6) == (Indices{1, 3, 5}));
  CHECK(deletionPositions({6, 7}, 6).empty());
}

TEST(list_driver_appends_inserts_named_past_the_end) {
  ListDriver driver;
  driver.setKeys({"a", "b", "c"});
  Indices positions = insertionPositions({7, 9}, driver.getKeyCount());
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
  CHECK_EQ(prepend.removed, std::size_t{0});
  CHECK_EQ(prepend.added, std::size_t{2});

  KeySplice trim = keySplice({"a", "b", "c", "d"}, {"a", "b"});
  CHECK_EQ(trim.start, std::size_t{2});
  CHECK_EQ(trim.removed, std::size_t{2});
  CHECK_EQ(trim.added, std::size_t{0});

  KeySplice middle = keySplice({"a", "b", "c", "d"}, {"a", "x", "y", "d"});
  CHECK_EQ(middle.start, std::size_t{1});
  CHECK_EQ(middle.removed, std::size_t{2});
  CHECK_EQ(middle.added, std::size_t{2});

  CHECK(keySplice({"a", "b"}, {"a", "b"}).isEmpty());
  CHECK(keySplice({}, {}).isEmpty());
}

TEST(key_splice_never_counts_a_key_in_both_ends) {
  // "a a" to "a": the prefix takes the one shared key and the suffix finds none left.
  KeySplice splice = keySplice({"a", "a"}, {"a"});
  CHECK_EQ(splice.start, std::size_t{1});
  CHECK_EQ(splice.removed, std::size_t{1});
  CHECK_EQ(splice.added, std::size_t{0});

  KeySplice cleared = keySplice({"a", "b"}, {});
  CHECK_EQ(cleared.start, std::size_t{0});
  CHECK_EQ(cleared.removed, std::size_t{2});
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
 * ShadowListKitListView.applyChanges on Android keeps its versions in a map that outlives the core. It
 * reloads by the same rule.
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
}
