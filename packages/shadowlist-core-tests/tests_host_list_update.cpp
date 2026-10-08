/*
 * ListUpdate tests: the data change rules both native kits share.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>

#include <string>
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
