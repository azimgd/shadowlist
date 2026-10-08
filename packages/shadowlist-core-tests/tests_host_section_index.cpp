/*
 * Section index tests: where the titles sit and which one a touch picks, in both kits.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/SectionIndex.hpp>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(section_index_titles_are_centered_in_the_area) {
  CHECK_EQ(sectionIndexTitlesTop(400.0, 5, 1.0), (400.0 - 5 * SECTION_INDEX_TITLE_HEIGHT) / 2.0);
  // More titles than fit start at the top.
  CHECK_EQ(sectionIndexTitlesTop(40.0, 5, 1.0), 0.0);
  CHECK_EQ(sectionIndexTitlesTop(400.0, 5, 3.0), (400.0 - 15 * SECTION_INDEX_TITLE_HEIGHT) / 2.0);
}

TEST(section_index_touch_picks_the_title_under_it_clamped) {
  double top = sectionIndexTitlesTop(400.0, 5, 1.0);
  CHECK_EQ(sectionIndexTitleAt(top + 1.0, 400.0, 5, 1.0), std::size_t(0));
  CHECK_EQ(sectionIndexTitleAt(top + SECTION_INDEX_TITLE_HEIGHT * 2.5, 400.0, 5, 1.0), std::size_t(2));
  CHECK_EQ(sectionIndexTitleAt(0.0, 400.0, 5, 1.0), std::size_t(0));
  CHECK_EQ(sectionIndexTitleAt(399.0, 400.0, 5, 1.0), std::size_t(4));
  CHECK_EQ(sectionIndexTitleAt(10.0, 400.0, 0, 1.0), std::size_t(0));
}
