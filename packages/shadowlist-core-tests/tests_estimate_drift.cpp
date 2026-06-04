// Repro: Contacts-like list. Non-inverted, 100 rows, a header, and an ESTIMATE
// that is wrong (120 like the web default) while rows really measure ~65px.
// The user scrolls down in steps (userScrolled = true) and we assert the visible
// window keeps tracking the scroll offset instead of collapsing / snapping back
// (which would render blank rows past the initial window).

#include "TestFramework.hpp"
#include "Harness.hpp"

using namespace slt;

TEST(default_scroll_fills_viewport_with_wrong_estimate) {
  Sim sim;
  sim.winH = 600;
  sim.headerSize = 80;
  sim.estimated = {400, 120};                                   // wrong on purpose
  sim.sizeOfKey = [](const std::string&) { return Size{400, 65}; };  // real row height
  sim.setKeys(makeKeys(100));
  sim.settle();

  // Walk down the list the way a finger drag would, then let it settle, and
  // assert the mounted window fills the WHOLE viewport (top and bottom edges),
  // not just the first few rows — otherwise the rows below render blank.
  for (double y = 200.0; y <= 4000.0; y += 200.0) {
    sim.userScrollTo(y);
    sim.settle();

    std::size_t lo = 0, hi = 0;
    CHECK(sim.visibleRange(lo, hi));

    auto indexAtOffset = [&](double target) {
      for (std::size_t i = 0; i < 100; ++i) {
        double off = sim.elementOffset(i);
        if (off <= target && off + 65.0 > target) return i;
      }
      return std::size_t(UNDEFINED_INDEX);
    };

    double viewTop = sim.offsetY + sim.headerSize;
    double viewBottom = sim.offsetY + sim.winH;  // bottom edge of the viewport
    std::size_t topIndex = indexAtOffset(viewTop);
    std::size_t bottomIndex = indexAtOffset(viewBottom);

    CHECK(topIndex != std::size_t(UNDEFINED_INDEX));
    CHECK(lo <= topIndex && topIndex <= hi);
    // The row covering the bottom of the viewport must also be mounted.
    if (bottomIndex != std::size_t(UNDEFINED_INDEX)) {
      CHECK(lo <= bottomIndex && bottomIndex <= hi);
    }
  }
}
