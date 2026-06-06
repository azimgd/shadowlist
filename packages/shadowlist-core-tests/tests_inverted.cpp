// Inverted list behaviour (visually bottom-anchored; offsets still ascend by index).

#include "TestFramework.hpp"
#include "Harness.hpp"

using namespace slt;

// An inverted list opens pinned to the bottom and shows the last rows.
TEST(inverted_list_pins_to_bottom) {
  Sim sim;
  sim.inverted = true;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(50));
  sim.settle();

  CHECK_NEAR(sim.totalAxis(), 5000.0, 0.5);
  CHECK_NEAR(sim.offsetY, 5000.0 - 600.0, 1.0);  // bottom: total - window

  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK_EQ(hi, std::size_t(49));  // last row is visible
}

// A short inverted list that fits the window needs no scrolling.
TEST(inverted_short_list_fits_window) {
  Sim sim;
  sim.inverted = true;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(3));  // 300px content < 600px window
  sim.settle();

  CHECK_NEAR(sim.totalAxis(), 300.0, 0.5);
  CHECK_NEAR(sim.offsetY, 0.0, 0.5);  // nothing to scroll
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK_EQ(lo, std::size_t(0));
  CHECK_EQ(hi, std::size_t(2));
}

// New content appended to an inverted list (the visual bottom) keeps it pinned.
TEST(inverted_repopulate_pins_to_bottom) {
  Sim sim;
  sim.inverted = true;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(40));
  sim.settle();
  CHECK_NEAR(sim.offsetY, 4000.0 - 600.0, 1.0);

  // Empty the list, then repopulate (re-arms the bottom pin).
  sim.setKeys({});
  sim.settle();
  sim.setKeys(makeKeys(60));
  sim.settle();

  CHECK_NEAR(sim.totalAxis(), 6000.0, 0.5);
  CHECK_NEAR(sim.offsetY, 6000.0 - 600.0, 1.0);  // pinned to the new bottom
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK_EQ(hi, std::size_t(59));
}

// The inverted bottom pin must yield to a user scroll, not snap back every frame.
TEST(inverted_bottom_pin_yields_to_user_scroll) {
  Sim sim;
  sim.inverted = true;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(3));  // 300px content < 600px window: pin never settles
  sim.settle();
  CHECK_NEAR(sim.offsetY, 0.0, 0.5);

  // Grow past the window: the pin engages and the offset jumps to the new bottom.
  std::vector<std::string> next = makeKeys(20, "n");
  for (const auto& key : makeKeys(3)) {
    next.push_back(key);
  }
  sim.setKeys(next);
  sim.frame();
  CHECK_NEAR(sim.offsetY, 2300.0 - 600.0, 1.0);  // pinned to the new bottom

  // The user drags up. The pin must release: the offset follows the user.
  sim.userScrollTo(800.0);
  CHECK_NEAR(sim.offsetY, 800.0, 1.0);
  sim.userScrollTo(300.0);
  CHECK_NEAR(sim.offsetY, 300.0, 1.0);
}

// When the estimate understates the real row size, the pin must re-pin across
// the resulting growth so the list ends at the bottom, not near the top.
TEST(inverted_estimate_grows_still_pins_to_bottom) {
  Sim sim;
  sim.inverted = true;
  sim.winH = 600;
  sim.estimated = {400, 100};                                  // underestimate
  sim.sizeOfKey = [](const std::string&) { return Size{400, 200}; };  // real is 2x
  sim.setKeys(makeKeys(50));
  sim.settle();

  // Totals are approximate under estimate error; assert the pin property: the
  // last row is visible with its bottom edge at the viewport bottom.
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK_EQ(hi, std::size_t(49));
  double lastRowBottom = sim.elementOffset(49) + 200.0;
  CHECK_NEAR(lastRowBottom, sim.offsetY + sim.winH, 1.0);
}
