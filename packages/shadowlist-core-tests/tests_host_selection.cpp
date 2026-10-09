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

/*
 * ShadowListKitListView.userSelected on Android keeps its own selection, which outlives the core and is
 * read on every cell bind. It follows the same tap rules.
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
