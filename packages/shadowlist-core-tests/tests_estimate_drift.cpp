// Wrong estimate (120) vs real row size (65): the visible window must keep
// tracking the scroll offset and not render blank rows.

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

  // Scroll down in steps; assert the mounted window covers the whole viewport
  // (top and bottom edges), not just the first few rows.
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
    double viewBottom = sim.offsetY + sim.winH;
    std::size_t topIndex = indexAtOffset(viewTop);
    std::size_t bottomIndex = indexAtOffset(viewBottom);

    CHECK(topIndex != std::size_t(UNDEFINED_INDEX));
    CHECK(lo <= topIndex && topIndex <= hi);
    // The row covering the viewport bottom must also be mounted.
    if (bottomIndex != std::size_t(UNDEFINED_INDEX)) {
      CHECK(lo <= bottomIndex && bottomIndex <= hi);
    }
  }
}

// The unmeasured-tail average must track real measured rows (65), not the
// initial estimate (120), so the scroll extent converges.
TEST(wrong_estimate_average_tracks_real_measured_size) {
  Sim sim;
  sim.winH = 600;
  sim.headerSize = 80;
  sim.estimated = {400, 120};                                  // wrong on purpose
  sim.sizeOfKey = [](const std::string&) { return Size{400, 65}; };  // real row height
  sim.setKeys(makeKeys(100));
  sim.settle();

  // Average is seeded from real 65px rows, not the 120 estimate.
  CHECK_NEAR(sim.container.revision.averageElementHeight, 65.0, 0.5);

  // After a scroll the total converges to the real total (80 + 100*65 = 6580).
  sim.userScrollTo(3000);
  sim.settle();
  CHECK(sim.totalAxis() < 8000.0);
}
