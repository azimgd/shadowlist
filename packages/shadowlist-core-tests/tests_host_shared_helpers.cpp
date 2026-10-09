/*
 * The helpers the native lists and the React Native lists share: the pinned section search
 * and the DragReorder calls that pick list or grid by themselves.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/DragReorder.hpp>
#include <shadowlist-core/host/StickyLayout.hpp>

#include <algorithm>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

/*
 * The overlay the plain way, a scan over every header.
 */
SectionOverlayPosition scanOverlay(
  const std::vector<double>& offsets,
  const std::vector<double>& sizes,
  double offset) {
  SectionOverlayPosition result;
  offset = offset < 0.0 ? 0.0 : offset;
  std::size_t active = UNDEFINED_INDEX;
  for (std::size_t index = 0; index < offsets.size() && offsets[index] <= offset; ++index) {
    active = index;
  }
  if (active == UNDEFINED_INDEX) {
    return result;
  }
  result.visible = true;
  std::size_t next = active + 1;
  result.translation = next < offsets.size() ? std::min(offset, offsets[next] - sizes[active]) : offset;
  return result;
}

}

TEST(section_overlay_search_matches_a_scan) {
  std::vector<double> offsets;
  std::vector<double> sizes;
  double at = 40.0;
  for (int header = 0; header < 30; ++header) {
    offsets.push_back(at);
    sizes.push_back(20.0 + header % 3 * 8.0);
    at += 100.0 + header * 7.0;
  }
  for (double offset = -50.0; offset < at + 200.0; offset += 3.5) {
    SectionOverlayPosition expected = scanOverlay(offsets, sizes, offset);
    SectionOverlayPosition found = sectionOverlayPosition(offsets.data(), sizes.data(), offsets.size(), offset);
    CHECK_EQ(found.visible, expected.visible);
    CHECK_NEAR(found.translation, expected.translation, 1e-9);
  }
}

TEST(pinned_section_leading_never_goes_above_its_place_and_is_pushed_up) {
  CHECK_NEAR(pinnedSectionLeading(100.0, 30.0, 50.0, false, 0.0), 100.0, 1e-9);
  CHECK_NEAR(pinnedSectionLeading(100.0, 30.0, 150.0, true, 400.0), 150.0, 1e-9);
  CHECK_NEAR(pinnedSectionLeading(100.0, 30.0, 390.0, true, 400.0), 370.0, 1e-9);
  CHECK_EQ(pinnedSectionPosition(0, 10.0, [](std::size_t) { return 0.0; }), UNDEFINED_INDEX);
}

TEST(drag_begin_and_place_row_match_the_list_and_grid_calls) {
  DragRow row;
  row.index = 3;
  row.key = "k3";
  row.leading = 300.0;
  row.extent = 100.0;
  row.crossLeading = 120.0;
  row.crossExtent = 120.0;

  DragReorder list;
  list.begin(row, 350.0, 40.0, 1);
  DragReorder listPlain;
  listPlain.begin(row.index, row.key, row.leading, row.extent, 350.0);
  CHECK(!list.isGrid());
  CHECK_NEAR(list.placeRow(420.0, 40.0, row, 2000.0, 360.0).leading, listPlain.place(420.0, row.leading, row.extent, 2000.0), 1e-9);

  DragReorder grid;
  grid.begin(row, 350.0, 180.0, 3);
  DragReorder gridPlain;
  gridPlain.beginCell(row, 350.0, 180.0, 3);
  CHECK(grid.isGrid());
  DragOffset placed = grid.placeRow(500.0, 260.0, row, 2000.0, 360.0);
  DragOffset expected = gridPlain.placeCell(500.0, 260.0, row, 2000.0, 360.0);
  CHECK_NEAR(placed.leading, expected.leading, 1e-9);
  CHECK_NEAR(placed.cross, expected.cross, 1e-9);
}
