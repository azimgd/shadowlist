/*
 * ListSelection tests: selected rows by key.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/ListSelection.hpp>

#include <algorithm>
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

/*
 * UIKit's and the Android kit's taps on a row. Both kits keep this selection beside their core.
 */
TEST(selection_tap_selects_moves_and_toggles) {
  ListSelection selection;
  auto yes = [] { return true; };
  SelectionTap first = selection.tap("a", yes);
  CHECK(first.selected && !first.toggledOff && first.deselected.empty());
  // Single selection: a tap on the selected row keeps it, a tap on another moves to it.
  SelectionTap again = selection.tap("a", yes);
  CHECK(again.selected && again.deselected.empty());
  SelectionTap moved = selection.tap("b", yes);
  CHECK(moved.selected);
  CHECK(moved.deselected == (std::vector<std::string>{"a"}));
  CHECK(selection.contains("b") && !selection.contains("a"));

  // Multiple selection toggles the tapped row and never asks before toggling off.
  selection.setMultiple(true);
  SelectionTap added = selection.tap("c", yes);
  CHECK(added.selected && added.deselected.empty());
  bool asked = false;
  SelectionTap off = selection.tap("c", [&] {
    asked = true;
    return true;
  });
  CHECK(off.toggledOff && !off.selected && !asked);
  CHECK(!selection.contains("c") && selection.contains("b"));
}

TEST(selection_tap_refused_changes_nothing) {
  ListSelection selection;
  selection.tap("a", [] { return true; });
  SelectionTap refused = selection.tap("b", [] { return false; });
  CHECK(!refused.selected && !refused.toggledOff && refused.deselected.empty());
  CHECK(selection.contains("a") && !selection.contains("b"));
}

using Keys = std::vector<std::string>;
using Indices = std::vector<std::size_t>;

/*
 * Fabric's select, selectOnly and deselect, which useRowSelection calls through ShadowListStore.
 * The keys come back in the order they were selected, the order onSelectionChange reports.
 */
TEST(selection_keeps_keys_in_the_order_selected) {
  ListSelection selection;
  selection.setMultiple(true);
  Keys deselected;
  CHECK(selection.select("a", deselected));
  CHECK(selection.select("b", deselected));
  CHECK(!selection.select("a", deselected));
  CHECK(selection.getKeys() == (Keys{"a", "b"}));
  CHECK(selection.deselect("a"));
  CHECK(selection.getKeys() == (Keys{"b"}));
  CHECK(selection.select("a", deselected));
  CHECK(selection.getKeys() == (Keys{"b", "a"}));
  CHECK(deselected.empty());

  // selectOnly leaves one row under either mode, and the same row again changes nothing.
  CHECK(selection.selectOnly("b", deselected));
  CHECK(deselected == (Keys{"a"}));
  CHECK(selection.getKeys() == (Keys{"b"}));
  CHECK(!selection.selectOnly("b", deselected));
}

/*
 * A controlled selectedKeys may hold several rows under single selection. Selecting one of them
 * leaves only that one, the way selecting any other row would.
 */
TEST(selection_single_select_narrows_several_handed_keys) {
  ListSelection selection;
  CHECK(selection.setKeys({"a", "b", "a"}));
  CHECK(selection.getKeys() == (Keys{"a", "b"}));
  CHECK(!selection.setKeys({"a", "b"}));
  Keys deselected;
  CHECK(selection.select("a", deselected));
  CHECK(deselected == (Keys{"b"}));
  CHECK(selection.getKeys() == (Keys{"a"}));
  CHECK(!selection.select("a", deselected));
}

/*
 * Rows removed from the data leave the selection, which keeps its order. retainWhere asks only
 * the selected keys, the way Fabric looks them up in the key store.
 */
TEST(selection_retain_drops_removed_rows_and_reports_indices_low_to_high) {
  ListSelection selection;
  selection.setMultiple(true);
  selection.setKeys({"c", "a"});
  Keys data{"a", "b", "c"};
  auto indexOf = [&](const std::string& key) {
    auto found = std::find(data.begin(), data.end(), key);
    return found == data.end() ? data.size() : static_cast<std::size_t>(found - data.begin());
  };
  CHECK(selection.indicesWhere(indexOf, data.size()) == (Indices{0, 2}));
  CHECK(selection.indicesIn(data) == (Indices{0, 2}));
  CHECK(!selection.retainKeys(data));
  CHECK(!selection.retainWhere([&](const std::string& key) { return indexOf(key) < data.size(); }));

  selection.setKeys({"a", "gone", "c"});
  std::size_t asked = 0;
  CHECK(selection.retainWhere([&](const std::string& key) {
    ++asked;
    return indexOf(key) < data.size();
  }));
  CHECK_EQ(asked, std::size_t(3));
  CHECK(selection.getKeys() == (Keys{"a", "c"}));
  CHECK(!selection.contains("gone"));

  data = {"c", "x"};
  CHECK(selection.retainKeys(data));
  CHECK(selection.getKeys() == (Keys{"c"}));
  Keys deselected;
  selection.clear(deselected);
  CHECK(deselected == (Keys{"c"}));
  CHECK(selection.isEmpty());
  CHECK(!selection.retainKeys(data));
}

/*
 * The Android kit's selectItem and deselectItem: a programmatic select under single selection
 * moves the selection, under multiple it adds, and turning multiple selection off keeps the rows
 * until the host clears them.
 */
TEST(selection_programmatic_select_follows_the_mode) {
  ListSelection selection;
  Keys deselected;
  selection.select("a", deselected);
  selection.select("b", deselected);
  CHECK(deselected == (Keys{"a"}));
  selection.setMultiple(true);
  deselected.clear();
  selection.select("c", deselected);
  CHECK(deselected.empty());
  CHECK(selection.getKeys() == (Keys{"b", "c"}));
  selection.setMultiple(false);
  CHECK_EQ(selection.getCount(), std::size_t(2));
  CHECK(!selection.deselect("a"));
  CHECK(selection.deselect("b"));
  CHECK(selection.getKeys() == (Keys{"c"}));
}
