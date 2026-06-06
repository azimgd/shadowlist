// Regression: a genuine user scroll must abandon any in-flight scroll correction
// so the visible window tracks the user instead of latching at an edge.
//
// Reproduces an inverted, variable-height list scrolled deep that froze: a
// maintain-visible-content-position correction kept re-targeting the clamped bottom
// offset, measuring the window at that phantom offset so the user's rows never
// mounted. Each user-initiated frame is flagged userScrolled, cancelling the
// pending correction.

#include "TestFramework.hpp"
#include "Harness.hpp"

using namespace slt;

// Deterministic variable heights (~100..219px) so measurement feedback shifts
// element offsets frame to frame - the condition that made the correction latch.
static Size variableHeight(const std::string& key) {
  std::size_t h = 100 + (key.size() * 7 + static_cast<unsigned char>(key.back())) % 120;
  return Size{402, static_cast<double>(h)};
}

TEST(inverted_deep_user_scroll_tracks_window) {
  Sim sim;
  sim.inverted = true;
  sim.winH = 699;
  sim.headerSize = 84;
  sim.footerSize = 65;
  sim.estimated = {402, 120};
  sim.sizeOfKey = variableHeight;
  sim.setKeys(makeKeys(1000));
  sim.settle();  // inverted: pins to the bottom

  double total = sim.totalAxis();
  double maxOffset = total - sim.winH;
  CHECK(maxOffset > 8000.0);  // sanity: the list is far taller than the window

  // Drag up the list in user-initiated steps (each frame flagged userScrolled).
  for (double y = maxOffset; y > maxOffset - 6000.0; y -= 400.0) {
    sim.userScrollTo(y);

    std::size_t lo = 0, hi = 0;
    CHECK(sim.visibleRange(lo, hi));

    // The mounted window must bracket the viewport [y, y + win]: it tracks the
    // user's offset rather than latching at the far (bottom) edge. Under the bug
    // the window stayed pinned near the last index, far above y, and these fail.
    CHECK(sim.elementOffset(lo) <= y + sim.winH);
    CHECK(sim.elementOffset(hi) + sim.container.getElementSize(hi) >= y);
  }
}

// The same drag, with fixed heights, lands the window exactly on the offset.
TEST(inverted_user_scroll_selects_offset_window) {
  Sim sim;
  sim.inverted = true;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(1000));
  sim.settle();

  sim.userScrollTo(50000.0);  // row 500 reaches the viewport start

  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  // 100px rows, window [offset - win, offset + 2*win] => rows 494..520.
  CHECK(lo <= std::size_t(500));
  CHECK(hi >= std::size_t(500));
  CHECK_NEAR(sim.elementOffset(500), 50000.0, 0.5);
}
