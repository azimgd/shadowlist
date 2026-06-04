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

// The average element size that sizes the unmeasured tail must be frozen from the
// REAL measured rows (65px), not from the first-revision estimate (120px). With the
// old "freeze the first window's estimate" behaviour the average stayed 120 forever,
// so the scroll extent / scrollbar was permanently ~80% too tall and never converged.
TEST(wrong_estimate_average_tracks_real_measured_size) {
  Sim sim;
  sim.winH = 600;
  sim.headerSize = 80;
  sim.estimated = {400, 120};                                  // wrong on purpose
  sim.sizeOfKey = [](const std::string&) { return Size{400, 65}; };  // real row height
  sim.setKeys(makeKeys(100));
  sim.settle();

  // The unmeasured-region average is seeded from the real 65px rows, not the 120
  // estimate (this is the direct regression guard for the frozen-estimate bug).
  CHECK_NEAR(sim.container.revision.averageElementHeight, 65.0, 0.5);

  // Once a re-layout runs (any scroll), the total content height converges toward
  // the real total (header 80 + 100*65 = 6580) instead of staying at the inflated
  // estimate-based ~11800.
  sim.userScrollTo(3000);
  sim.settle();
  CHECK(sim.totalAxis() < 8000.0);
}
