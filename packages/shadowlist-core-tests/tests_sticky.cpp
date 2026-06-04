// Sticky header/footer: the template stays pinned to the viewport edge instead
// of scrolling with the content. The core only changes where the header/footer
// template is positioned (getHeaderOffset / getFooterOffset); the reserved
// header/footer space in the content size is unchanged, so the template settles
// back onto it at the scroll extremes.

#include "TestFramework.hpp"
#include "Harness.hpp"

using namespace slt;

// Without sticky, the header rests at the content start (0) and the footer rests
// after the content (totalSize - footerSize), regardless of scroll position.
TEST(non_sticky_header_footer_rest_in_content) {
  Sim sim;
  sim.winH = 600;
  sim.headerSize = 80;
  sim.footerSize = 40;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();
  sim.scrollTo(3000.0);

  CHECK_NEAR(sim.container.getStickyHeaderOffset(), 0.0, 0.5);
  // total = (header 80 + 100*100) + footer 40 = 10120; footer rests at 10120-40.
  CHECK_NEAR(sim.totalAxis(), 10120.0, 0.5);
  CHECK_NEAR(sim.container.getStickyFooterOffset(sim.footerSize), 10080.0, 0.5);
  CHECK_NEAR(sim.container.getFooterOffset(sim.footerSize), 10080.0, 0.5);
}

// A sticky header tracks the scroll offset so it stays at the viewport start.
TEST(sticky_header_tracks_scroll_offset) {
  Sim sim;
  sim.winH = 600;
  sim.headerSize = 80;
  sim.stickyHeader = true;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  CHECK_NEAR(sim.container.getStickyHeaderOffset(), 0.0, 0.5);  // at the top

  sim.scrollTo(3000.0);
  CHECK_NEAR(sim.offsetY, 3000.0, 0.5);                     // offset is stable
  CHECK_NEAR(sim.container.getStickyHeaderOffset(), 3000.0, 0.5); // header followed it
}

// A sticky footer tracks the scroll offset so it stays at the viewport end:
// offset + window - footerSize.
TEST(sticky_footer_pinned_to_viewport_end) {
  Sim sim;
  sim.winH = 600;
  sim.footerSize = 40;
  sim.stickyFooter = true;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  // At the top the footer floats at the viewport bottom (600 - 40), not at the
  // far end of the content.
  CHECK_NEAR(sim.container.getStickyFooterOffset(sim.footerSize), 560.0, 0.5);

  sim.scrollTo(3000.0);
  CHECK_NEAR(sim.container.getStickyFooterOffset(sim.footerSize), 3000.0 + 600.0 - 40.0, 0.5);
}

// At the very bottom the sticky footer's tracked position equals its resting
// position (totalSize - footerSize), so it settles seamlessly onto the reserved
// footer space instead of jumping.
TEST(sticky_footer_settles_at_list_bottom) {
  Sim sim;
  sim.winH = 600;
  sim.footerSize = 40;
  sim.stickyFooter = true;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  // total = 100*100 + footer 40 = 10040; maxOffset = 10040 - 600 = 9440.
  double total = sim.totalAxis();
  double maxOffset = total - sim.winH;
  sim.scrollTo(maxOffset);

  CHECK_NEAR(sim.container.getStickyFooterOffset(sim.footerSize), total - sim.footerSize, 0.5);
}

// Sticky is orientation aware: a horizontal sticky header tracks the horizontal
// scroll offset (reads offsetX / windowContainerWidth via getContainerOffset).
TEST(sticky_header_horizontal_tracks_offset_x) {
  Sim sim;
  sim.horizontal = true;
  sim.winW = 400;
  sim.headerSize = 80;
  sim.stickyHeader = true;
  sim.estimated = {100, 300};
  sim.sizeOfKey = [](const std::string&) { return Size{100, 300}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  CHECK_NEAR(sim.container.getStickyHeaderOffset(), 0.0, 0.5);

  sim.offsetX = 2000.0;
  sim.settle();
  CHECK_NEAR(sim.container.getStickyHeaderOffset(), 2000.0, 0.5);
}
