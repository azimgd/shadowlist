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

// Opening an inverted list: frames committed before the viewport is measured must not
// report visible/viewable indices. A zero-size window selects the top rows, and emitting
// them tears down the bottom-anchored mount on the JS side ([0..1] flash) before the
// bottom pin applies. The first report comes from the settled bottom window.
TEST(inverted_open_emits_no_indices_before_window_measured) {
  Sim sim;
  sim.inverted = true;
  sim.winH = 0.0;  // viewport not laid out yet
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(1000));
  sim.frame();
  sim.frame();  // second commit at offset 0 used to emit the top window (0, 0)
  CHECK_EQ(sim.visibleChanges.size(), std::size_t(0));
  CHECK_EQ(sim.viewableChanges.size(), std::size_t(0));

  sim.winH = 600.0;  // viewport measured; the bottom anchor drives to the end
  sim.settle();
  CHECK(sim.visibleChanges.size() > std::size_t(0));
  for (const auto& change : sim.visibleChanges) {
    std::size_t lo = change.first < change.second ? change.first : change.second;
    CHECK(lo > std::size_t(900));  // every reported window sits at the bottom
  }
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK_EQ(hi, std::size_t(999));
}

// The host learns the real viewport size only in the layout pass - update() runs from
// adopt BEFORE layout, so on first open it always sees a zero frame and the bottom pin
// cannot apply there. resolveWindowChange must resolve the pin within that same layout
// pass; without it the list publishes offset 0, opens blank at the top, and the first
// user scroll cancels the pin for good.
TEST(inverted_window_measured_in_layout_resolves_bottom_pin) {
  Sim sim;
  sim.inverted = true;
  sim.winH = 0.0;  // adopt-time commits see an unmeasured viewport
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(1000));
  sim.frame();
  sim.frame();
  CHECK_NEAR(sim.offsetY, 0.0, 0.5);  // pin cannot apply against a zero window

  // The layout pass: write the measured viewport into the revision (as the shadow
  // node does), re-flow, then resolve pending corrections in place.
  sim.container.revision.windowContainerWidth = 400.0;
  sim.container.revision.windowContainerHeight = 600.0;
  Virtualizer::recomputeElementOffsets(&sim.container, 0);
  Virtualizer::recomputeTotalSize(&sim.container);
  CHECK(Virtualizer::resolveWindowChange(&sim.container));

  // The pin applied in the same pass: offset at the bottom, published as corrected.
  CHECK_NEAR(sim.container.revision.containerOffsetY, 100000.0 - 600.0, 1.0);
  CHECK(sim.container.containerOffsetCorrected);

  // The re-selected window and the emitted indices sit at the bottom of the list.
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK_EQ(hi, std::size_t(999));
  CHECK(!sim.visibleChanges.empty());
  std::size_t emittedLo = sim.visibleChanges.back().first < sim.visibleChanges.back().second
    ? sim.visibleChanges.back().first
    : sim.visibleChanges.back().second;
  CHECK(emittedLo > std::size_t(900));

  // The host applies the published offset; subsequent commits settle the pin.
  sim.winH = 600.0;
  sim.offsetY = sim.container.revision.containerOffsetY;
  sim.settle();
  CHECK_NEAR(sim.offsetY, 100000.0 - 600.0, 1.0);
  CHECK(sim.visibleRange(lo, hi));
  CHECK_EQ(hi, std::size_t(999));
}
