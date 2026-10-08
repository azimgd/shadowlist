/*
 * ListSelection tests: selected rows by key.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/ListSelection.hpp>

#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(selection_single_replaces_the_previous_row) {
  ListSelection selection;
  std::vector<std::string> deselected;
  CHECK(selection.select("a", deselected));
  CHECK(deselected.empty());
  CHECK(!selection.select("a", deselected));
  CHECK(selection.select("b", deselected));
  CHECK_EQ(deselected.size(), std::size_t(1));
  CHECK_EQ(deselected[0], std::string("a"));
  CHECK(selection.contains("b"));
  CHECK(!selection.contains("a"));
}

TEST(selection_multiple_keeps_every_row_and_follows_keys) {
  ListSelection selection;
  selection.setMultiple(true);
  std::vector<std::string> deselected;
  selection.select("c", deselected);
  selection.select("a", deselected);
  CHECK(deselected.empty());
  std::vector<std::size_t> indices = selection.indicesIn({"x", "a", "b", "c"});
  CHECK_EQ(indices.size(), std::size_t(2));
  CHECK_EQ(indices[0], std::size_t(1));
  CHECK_EQ(indices[1], std::size_t(3));
  // c was removed from the data.
  selection.retainKeys({"a", "b"});
  CHECK_EQ(selection.getCount(), std::size_t(1));
  CHECK(selection.deselect("a"));
  CHECK(!selection.deselect("a"));
  CHECK(selection.isEmpty());
}
