/*
 * ListSections tests: sections over the flat rows the native lists place.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/ListSections.hpp>

#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

/*
 * Three sections: a header and 2 items, an empty one with only a footer, a header and 3 items
 * and a footer. Rows: H0 i0 i1 F1 H2 i2 i3 i4 F2.
 */
ListSections threeSections() {
  ListSections sections;
  sections.setSections({{2, true, false}, {0, false, true}, {3, true, true}});
  return sections;
}

}

TEST(sections_without_sections_map_rows_to_the_same_items) {
  ListSections sections;
  sections.setPlain(5);
  CHECK(!sections.isSectioned());
  CHECK_EQ(sections.getRowCount(), std::size_t(5));
  CHECK_EQ(sections.rowForItem(3), std::size_t(3));
  CHECK_EQ(sections.itemForRow(4), std::size_t(4));
  CHECK_EQ(sections.itemForRow(5), UNDEFINED_INDEX);
  CHECK(sections.isItemBeforeItem(3));
  CHECK(!sections.isItemBeforeItem(4));
}

TEST(sections_place_headers_items_and_footers_in_order) {
  ListSections sections = threeSections();
  CHECK_EQ(sections.getRowCount(), std::size_t(9));
  CHECK_EQ(sections.getItemCount(), std::size_t(5));
  CHECK(sections.placeOfRow(0).kind == RowKind::Header);
  CHECK_EQ(sections.placeOfRow(0).section, std::size_t(0));
  CHECK_EQ(sections.itemForRow(1), std::size_t(0));
  CHECK_EQ(sections.itemForRow(2), std::size_t(1));
  CHECK(sections.placeOfRow(3).kind == RowKind::Footer);
  CHECK_EQ(sections.placeOfRow(3).section, std::size_t(1));
  CHECK(sections.placeOfRow(4).kind == RowKind::Header);
  CHECK_EQ(sections.placeOfRow(4).section, std::size_t(2));
  CHECK_EQ(sections.itemForRow(7), std::size_t(4));
  CHECK(sections.placeOfRow(8).kind == RowKind::Footer);
  CHECK_EQ(sections.itemForRow(9), UNDEFINED_INDEX);
}

TEST(sections_map_items_to_rows_and_sections) {
  ListSections sections = threeSections();
  std::vector<std::size_t> rows = {1, 2, 5, 6, 7};
  for (std::size_t item = 0; item < rows.size(); ++item) {
    CHECK_EQ(sections.rowForItem(item), rows[item]);
    CHECK_EQ(sections.itemForRow(rows[item]), item);
  }
  CHECK_EQ(sections.sectionForItem(1), std::size_t(0));
  CHECK_EQ(sections.sectionForItem(2), std::size_t(2));
  CHECK_EQ(sections.rowForItem(5), UNDEFINED_INDEX);
  CHECK_EQ(sections.firstItemInSection(2), std::size_t(2));
  CHECK_EQ(sections.itemCountInSection(1), std::size_t(0));
  CHECK_EQ(sections.headerRow(2), std::size_t(4));
  CHECK_EQ(sections.headerRow(1), UNDEFINED_INDEX);
  CHECK_EQ(sections.footerRow(1), std::size_t(3));
  CHECK_EQ(sections.footerRow(2), std::size_t(8));
  CHECK_EQ(sections.firstRowInSection(1), std::size_t(3));
  std::vector<std::size_t> headers = sections.headerRows();
  CHECK_EQ(headers.size(), std::size_t(2));
  CHECK_EQ(headers[1], std::size_t(4));
}

TEST(sections_without_rows_are_skipped_by_lookups) {
  ListSections sections;
  sections.setSections({{0, false, false}, {2, false, false}, {0, false, false}, {1, true, false}});
  CHECK_EQ(sections.getRowCount(), std::size_t(4));
  CHECK_EQ(sections.sectionForItem(0), std::size_t(1));
  CHECK_EQ(sections.sectionForItem(2), std::size_t(3));
  CHECK_EQ(sections.placeOfRow(2).section, std::size_t(3));
  CHECK(sections.placeOfRow(2).kind == RowKind::Header);
  CHECK_EQ(sections.rowForItem(2), std::size_t(3));
  CHECK_EQ(sections.firstRowInSection(0), UNDEFINED_INDEX);
  CHECK_EQ(sections.firstRowInSection(2), UNDEFINED_INDEX);
}

TEST(sections_separators_go_only_between_items_of_one_section) {
  ListSections sections = threeSections();
  CHECK(!sections.isItemBeforeItem(0));
  CHECK(sections.isItemBeforeItem(1));
  CHECK(!sections.isItemBeforeItem(2));
  CHECK(sections.isItemBeforeItem(5));
  CHECK(sections.isItemBeforeItem(6));
  CHECK(!sections.isItemBeforeItem(7));
}

TEST(sections_row_keys_round_trip_to_item_keys) {
  ListSections sections = threeSections();
  std::vector<std::string> items = {"a", "b", "c", "d", "e"};
  std::vector<std::string> rows = sections.rowKeys(items, {"s0", "s1", "s2"});
  CHECK_EQ(rows.size(), std::size_t(9));
  CHECK_EQ(rows[0], std::string(ListSections::HEADER_KEY_PREFIX) + "s0");
  CHECK_EQ(rows[1], std::string("a"));
  CHECK_EQ(rows[3], std::string(ListSections::FOOTER_KEY_PREFIX) + "s1");
  CHECK_EQ(rows[5], std::string("c"));
  std::vector<std::string> back = sections.itemKeys(rows);
  CHECK_EQ(back.size(), items.size());
  for (std::size_t index = 0; index < items.size(); ++index) CHECK_EQ(back[index], items[index]);
}

TEST(sections_drop_maps_to_an_item_inside_the_dragged_rows_section) {
  ListSections sections = threeSections();
  // Rows: H0 i0 i1 F1 H2 i2 i3 i4 F2. i2 (row 5) dropped at row 6 lands after i3, item 3.
  CHECK_EQ(sections.itemForDrop(5, 6), std::size_t(3));
  CHECK_EQ(sections.itemForDrop(5, 7), std::size_t(4));
  // i4 (row 7) dropped right below H2 at row 5 becomes the section's first item.
  CHECK_EQ(sections.itemForDrop(7, 5), std::size_t(2));
  // i3 dropped at the top of the list stays in its section, at its start.
  CHECK_EQ(sections.itemForDrop(6, 0), std::size_t(2));
  // i0 dropped far below stays in section 0, at its end.
  CHECK_EQ(sections.itemForDrop(1, 8), std::size_t(1));
  CHECK_EQ(sections.itemForDrop(0, 3), UNDEFINED_INDEX);
  ListSections plain;
  plain.setPlain(4);
  CHECK_EQ(plain.itemForDrop(1, 3), std::size_t(3));
}
