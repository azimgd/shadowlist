// Default (top-to-bottom, vertical) list behaviour.

#include "TestFramework.hpp"
#include "Harness.hpp"

using namespace slt;

// 50 fixed-height (100px) rows in a 600px window stack from the top. At rest the
// list never scrolls, so it settles on the first revision, which fills exactly
// one windowful from the top edge (6 rows of 100px fill the 600px window) and
// leaves the rest of the list unmeasured. The steady-state virtualization window
// (measureNextRevision) is covered by default_scroll_selects_window.
TEST(default_list_stacks_from_top) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(50));
  sim.settle();

  CHECK_NEAR(sim.totalAxis(), 5000.0, 0.5);  // 50 rows * 100
  CHECK_NEAR(sim.offsetY, 0.0, 0.5);          // stays at the top
  CHECK_NEAR(sim.elementOffset(0), 0.0, 0.5);
  CHECK_NEAR(sim.elementOffset(1), 100.0, 0.5);
  CHECK_NEAR(sim.elementOffset(10), 1000.0, 0.5);

  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK_EQ(lo, std::size_t(0));   // window starts at the first row
  CHECK_EQ(hi, std::size_t(5));   // exactly one 600px windowful (rows 0..5), not the whole list
}

// Scrolling re-selects the window around the offset (measureNextRevision keeps
// elements whose offset falls in [offset - window, offset + 2*window]) and keeps
// offsets exact. At offset 3000 with 100px rows that window is rows 24..42.
TEST(default_scroll_selects_window) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  sim.scrollTo(3000.0);  // row 30 reaches the top

  CHECK_NEAR(sim.elementOffset(30), 3000.0, 0.5);
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK_EQ(lo, std::size_t(24));  // [3000 - 600] / 100
  CHECK_EQ(hi, std::size_t(42));  // [3000 + 1200] / 100
}

// A header offsets every row and contributes to the content size.
TEST(default_header_and_footer_offset_content) {
  Sim sim;
  sim.winH = 600;
  sim.headerSize = 80;
  sim.footerSize = 40;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(20));
  sim.settle();

  CHECK_NEAR(sim.elementOffset(0), 80.0, 0.5);   // first row sits after the header
  CHECK_NEAR(sim.elementOffset(1), 180.0, 0.5);
  // content = header + 20 rows * 100 + footer
  CHECK_NEAR(sim.totalAxis(), 80.0 + 2000.0 + 40.0, 0.5);
}
