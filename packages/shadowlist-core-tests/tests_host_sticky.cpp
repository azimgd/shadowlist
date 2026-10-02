/*
 * Sticky pinning tests: the header and footer pinned or auto hidden, and the section header overlay.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/StickyLayout.hpp>

#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

/*
 * A 2000 long list in a 600 viewport with a 50 header and a 40 footer at the content end.
 */
StickyInput listAt(double offset) {
  StickyInput input;
  input.offset = offset;
  input.windowSize = 600.0;
  input.contentSize = 2000.0;
  input.hasHeader = true;
  input.headerSize = 50.0;
  input.hasFooter = true;
  input.footerSize = 40.0;
  input.footerStart = 1960.0;
  return input;
}

}

TEST(sticky_header_pins_to_the_top) {
  StickyState state;
  StickyInput input = listAt(300.0);
  input.stickyHeader = true;
  CHECK_EQ(stickyTranslations(input, state).header, 300.0);
}

TEST(sticky_header_is_pushed_off_by_the_footer) {
  StickyState state;
  StickyInput input = listAt(1950.0);
  input.stickyHeader = true;
  // Content end minus the footer and the header.
  CHECK_EQ(stickyTranslations(input, state).header, 1910.0);
}

TEST(plain_header_and_footer_stay_in_place) {
  StickyState state;
  StickyTranslations result = stickyTranslations(listAt(700.0), state);
  CHECK_EQ(result.header, 0.0);
  CHECK_EQ(result.footer, 0.0);
}

TEST(sticky_footer_pins_to_the_bottom) {
  StickyState state;
  StickyInput input = listAt(300.0);
  input.stickyFooter = true;
  // Its bottom meets the viewport bottom at 900.
  CHECK_EQ(stickyTranslations(input, state).footer, 300.0 + 600.0 - 40.0 - 1960.0);
  input.offset = 1400.0;
  CHECK_EQ(stickyTranslations(input, state).footer, 0.0);
}

TEST(sticky_footer_does_not_ride_into_the_header_in_a_short_viewport) {
  StickyState state;
  StickyInput input = listAt(0.0);
  input.windowSize = 60.0;
  input.stickyFooter = true;
  // Resting would put its top at 20, inside the 50 header, so it stops right under it.
  CHECK_EQ(stickyTranslations(input, state).footer, 50.0 - 1960.0);
}

TEST(auto_hide_header_slides_away_with_user_scrolls_only) {
  StickyState state;
  StickyInput input = listAt(100.0);
  input.autoHideHeader = true;
  stickyTranslations(input, state);
  input.offset = 130.0;
  input.accumulate = true;
  CHECK_EQ(stickyTranslations(input, state).header, 100.0);
  CHECK_EQ(state.headerHidden, 30.0);
  // Never more than its size.
  input.offset = 400.0;
  CHECK_EQ(stickyTranslations(input, state).header, 350.0);
  // A jump from code moves nothing.
  input.accumulate = false;
  input.offset = 380.0;
  CHECK_EQ(stickyTranslations(input, state).header, 330.0);
  // Scrolling back shows it again.
  input.accumulate = true;
  input.offset = 350.0;
  CHECK_EQ(state.headerHidden, 50.0);
  CHECK_EQ(stickyTranslations(input, state).header, 330.0);
}

TEST(auto_hide_header_shows_near_the_start) {
  StickyState state;
  state.headerHidden = 50.0;
  state.previousOffset = 60.0;
  StickyInput input = listAt(40.0);
  input.autoHideHeader = true;
  input.accumulate = true;
  CHECK_EQ(stickyTranslations(input, state).header, 40.0);
  CHECK_EQ(state.headerHidden, 0.0);
}

TEST(auto_hide_footer_slides_down_and_shows_near_the_end) {
  StickyState state;
  StickyInput input = listAt(300.0);
  input.autoHideFooter = true;
  stickyTranslations(input, state);
  input.accumulate = true;
  input.offset = 310.0;
  double resting = 310.0 + 600.0 - 40.0 - 1960.0;
  CHECK_EQ(stickyTranslations(input, state).footer, resting + 10.0);
  // Within its size of the end it shows fully.
  input.offset = 1380.0;
  CHECK_EQ(stickyTranslations(input, state).footer, 1380.0 + 600.0 - 40.0 - 1960.0);
  CHECK_EQ(state.footerHidden, 0.0);
}

TEST(section_overlay_follows_the_active_header) {
  std::vector<double> offsets{0.0, 500.0, 1000.0};
  std::vector<double> sizes{30.0, 30.0, 30.0};
  SectionOverlayPosition position = sectionOverlayPosition(offsets.data(), sizes.data(), 3, 200.0);
  CHECK(position.visible);
  CHECK_EQ(position.translation, 200.0);
  // The next header pushes it up as it arrives.
  position = sectionOverlayPosition(offsets.data(), sizes.data(), 3, 490.0);
  CHECK_EQ(position.translation, 470.0);
  // Past the next header that one is active.
  position = sectionOverlayPosition(offsets.data(), sizes.data(), 3, 1200.0);
  CHECK_EQ(position.translation, 1200.0);
}

TEST(section_overlay_hides_without_an_active_header) {
  std::vector<double> offsets{100.0, 500.0};
  std::vector<double> sizes{30.0, 30.0};
  CHECK(!sectionOverlayPosition(offsets.data(), sizes.data(), 2, 50.0).visible);
  CHECK(!sectionOverlayPosition(nullptr, nullptr, 0, 50.0).visible);
  // A pull past the top counts as the top.
  std::vector<double> atTop{0.0};
  std::vector<double> atTopSizes{30.0};
  SectionOverlayPosition position = sectionOverlayPosition(atTop.data(), atTopSizes.data(), 1, -40.0);
  CHECK(position.visible);
  CHECK_EQ(position.translation, 0.0);
}
