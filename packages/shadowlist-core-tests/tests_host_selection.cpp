/*
 * ListSelection tests: selected rows by key, and the keyboard's moves between them.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/KeyNavigation.hpp>
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

/*
 * A Shift-click or a Shift arrow key on the Mac selects from the anchor to the lead. A shorter
 * range deselects the rows it leaves, and rows selected before the anchor was set stay.
 */
TEST(selection_range_runs_from_the_anchor_and_replaces_the_previous_range) {
  ListSelection selection;
  selection.setMultiple(true);
  Keys deselected;
  selection.select("a", deselected);
  selection.select("c", deselected);
  selection.setAnchor("c");
  CHECK(selection.hasAnchor());
  CHECK_EQ(selection.getAnchorKey(), std::string("c"));
  CHECK_EQ(selection.getLeadKey(), std::string("c"));

  CHECK(selection.selectRange({"c", "d", "e"}, deselected));
  CHECK(deselected.empty());
  CHECK(selection.getKeys() == (Keys{"a", "c", "d", "e"}));
  CHECK_EQ(selection.getLeadKey(), std::string("e"));
  CHECK_EQ(selection.getAnchorKey(), std::string("c"));

  CHECK(selection.selectRange({"c", "d"}, deselected));
  CHECK(deselected == (Keys{"e"}));
  CHECK(selection.getKeys() == (Keys{"a", "c", "d"}));
  CHECK(!selection.selectRange({"c", "d"}, deselected));

  // Past the anchor the other way, the range turns around it.
  deselected.clear();
  CHECK(selection.selectRange({"c", "b"}, deselected));
  CHECK(deselected == (Keys{"d"}));
  CHECK(selection.getKeys() == (Keys{"a", "c", "b"}));
  CHECK_EQ(selection.getLeadKey(), std::string("b"));

  // A new anchor starts a new range and keeps what the last one selected.
  selection.setAnchor("b");
  deselected.clear();
  CHECK(selection.selectRange({"b", "x"}, deselected));
  CHECK(deselected.empty());
  CHECK(selection.getKeys() == (Keys{"a", "c", "b", "x"}));
}

TEST(selection_range_under_single_selection_selects_the_lead_only) {
  ListSelection selection;
  Keys deselected;
  selection.select("a", deselected);
  selection.setAnchor("a");
  CHECK(selection.selectRange({"a", "b", "c"}, deselected));
  CHECK(deselected == (Keys{"a"}));
  CHECK(selection.getKeys() == (Keys{"c"}));
  CHECK_EQ(selection.getLeadKey(), std::string("c"));
  CHECK_EQ(selection.getAnchorKey(), std::string("a"));
  CHECK(!selection.selectRange({}, deselected));
}

/*
 * The anchor follows its key and drops with its row or with clear. An anchor on a row that is
 * not selected, like one a Command-click deselected, stays while its row is in the data.
 */
TEST(selection_anchor_drops_when_its_row_leaves_the_data) {
  ListSelection selection;
  selection.setMultiple(true);
  Keys deselected;
  selection.select("a", deselected);
  selection.setAnchor("b");
  CHECK(!selection.retainKeys(Keys{"a", "b"}));
  CHECK(selection.hasAnchor());
  CHECK(selection.contains("a"));

  selection.selectRange({"b", "c"}, deselected);
  CHECK(!selection.retainKeys(Keys{"a", "b", "c"}));
  CHECK_EQ(selection.getLeadKey(), std::string("c"));
  // The lead's row went: the anchor goes with it.
  CHECK(selection.retainKeys(Keys{"a", "b"}));
  CHECK(!selection.hasAnchor());
  CHECK(selection.getKeys() == (Keys{"a", "b"}));

  selection.setAnchor("a");
  CHECK(selection.retainWhere([](const std::string& key) { return key != "a"; }));
  CHECK(!selection.contains("a"));
  CHECK(!selection.hasAnchor());

  selection.setAnchor("b");
  selection.clear(deselected);
  CHECK(!selection.hasAnchor());
  CHECK(selection.getAnchorKey().empty());
}

TEST(key_navigation_steps_to_selectable_rows_and_keeps_a_grid_column) {
  auto every = [](std::size_t) { return true; };
  auto noHeaders = [](std::size_t row) { return row % 4 != 0; };
  CHECK_EQ(stepRow(UNDEFINED_INDEX, 1, 10, every), std::size_t{0});
  CHECK_EQ(stepRow(UNDEFINED_INDEX, -3, 10, every), std::size_t{9});
  CHECK_EQ(stepRow(5, 1, 10, every), std::size_t{6});
  CHECK_EQ(stepRow(9, 1, 10, every), UNDEFINED_INDEX);
  // A step of three columns stays in the lead's column and skips a row that cannot be selected.
  CHECK_EQ(stepRow(1, 3, 10, [](std::size_t row) { return row != 4; }), std::size_t{7});
  CHECK_EQ(stepRow(3, 1, 10, noHeaders), std::size_t{5});
  CHECK_EQ(endRow(false, 10, noHeaders), std::size_t{1});
  CHECK_EQ(endRow(true, 10, noHeaders), std::size_t{9});
  CHECK_EQ(stepRow(0, 1, 0, every), UNDEFINED_INDEX);
}

TEST(key_navigation_pages_a_viewport_from_the_lead_and_reveals_at_the_nearest_edge) {
  auto leadingAt = [](std::size_t row) { return static_cast<double>(row) * 50.0; };
  auto every = [](std::size_t) { return true; };
  CHECK_EQ(pageRow(2, 1, 100, 200.0, leadingAt, every), std::size_t{6});
  CHECK_EQ(pageRow(6, -1, 100, 200.0, leadingAt, every), std::size_t{2});
  CHECK_EQ(pageRow(UNDEFINED_INDEX, 1, 100, 200.0, leadingAt, every), std::size_t{0});
  CHECK_EQ(pageRow(UNDEFINED_INDEX, -1, 100, 200.0, leadingAt, every), std::size_t{99});
  CHECK_EQ(pageRow(2, 1, 100, 200.0, leadingAt, [](std::size_t row) { return row == 3; }), std::size_t{3});
  CHECK(revealEdge(100.0, 50.0, 120.0, 400.0) == RevealEdge::Start);
  CHECK(revealEdge(500.0, 50.0, 120.0, 400.0) == RevealEdge::End);
  CHECK(revealEdge(200.0, 50.0, 120.0, 400.0) == RevealEdge::None);
}
