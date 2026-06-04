// scrollToIndex, including the precision guarantee on variable-height lists:
// the *target element* lands at the viewport edge even when the rows above it
// are still estimated.

#include "TestFramework.hpp"
#include "Harness.hpp"

#include <string>

using namespace slt;

// Fixed heights: scrollToIndex(N) puts row N at the top, at exactly N * height.
TEST(scroll_to_index_fixed_height) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  sim.container.scrollToIndex(40);
  sim.settle();

  CHECK_NEAR(sim.offsetY, 4000.0, 1.0);
  CHECK_NEAR(sim.offsetY, sim.elementOffset(40), 1.0);
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK(lo <= std::size_t(40) && std::size_t(40) <= hi);
}

// Variable heights: the offset converges onto the target element's real offset
// (this is the anchored-scrollToIndex behaviour), not a stale estimated offset.
TEST(scroll_to_index_precise_with_variable_heights) {
  Sim sim;
  sim.winH = 700;
  sim.estimated = {400, 120};
  auto heightOf = [](std::size_t n) { return 60.0 + double((n * 37) % 200); };  // 60..259
  sim.sizeOfKey = [&](const std::string& key) {
    return Size{400, heightOf(std::stoul(key.substr(1)))};
  };
  sim.setKeys(makeKeys(300));
  sim.settle();

  sim.container.scrollToIndex(150);
  sim.settle();

  // The anchored scroll converges so the target sits at the viewport edge.
  CHECK_NEAR(sim.offsetY, sim.elementOffset(150), 2.0);
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK(lo <= std::size_t(150) && std::size_t(150) <= hi);

  // Ground truth (independent of the convergence target): the landing region is
  // really measured, so the visible rows stack edge to edge by their real height
  // rather than the 120px estimate. A scroll that landed on a stale estimated
  // offset would show 120px gaps here, not the 60..259 measured heights.
  for (std::size_t i = lo; i + 1 <= hi; ++i) {
    double gap = sim.elementOffset(i + 1) - sim.elementOffset(i);
    CHECK_NEAR(gap, heightOf(i), 0.5);
  }
}

// scrollToIndex works on inverted lists too.
TEST(scroll_to_index_inverted) {
  Sim sim;
  sim.inverted = true;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  sim.container.scrollToIndex(20);
  sim.settle();

  CHECK_NEAR(sim.offsetY, sim.elementOffset(20), 1.0);
  CHECK_NEAR(sim.offsetY, 2000.0, 1.0);
}

// Re-issuing the same index (via the imperative nonce path) scrolls again after
// the user has moved away.
TEST(scroll_to_index_repeats_same_index) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  // First request to index 0 (nonce 1).
  sim.container.requestScrollToIndex(/*commandIndex*/ 0.0, /*nonce*/ 1.0, /*propIndex*/ -2);
  sim.settle();
  CHECK_NEAR(sim.offsetY, 0.0, 1.0);

  // User scrolls away.
  sim.scrollTo(5000.0);
  CHECK_NEAR(sim.offsetY, 5000.0, 1.0);

  // Same index again, fresh nonce -> must scroll back to the top.
  sim.container.requestScrollToIndex(/*commandIndex*/ 0.0, /*nonce*/ 2.0, /*propIndex*/ -2);
  sim.settle();
  CHECK_NEAR(sim.offsetY, 0.0, 1.0);

  // User scrolls away again, then the SAME nonce repeats: the command fires once
  // per invocation, so an unchanged nonce must NOT re-scroll (the dedup half).
  sim.scrollTo(5000.0);
  sim.container.requestScrollToIndex(/*commandIndex*/ 0.0, /*nonce*/ 2.0, /*propIndex*/ -2);
  sim.settle();
  CHECK_NEAR(sim.offsetY, 5000.0, 1.0);  // stayed put, no re-scroll
}

// The declarative containerOffsetIndex prop scrolls when its value changes and
// stays quiet while it is unchanged (a negative value is inactive).
TEST(scroll_to_index_declarative_prop) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  // prop changes -1 -> 30: scroll so row 30 reaches the top.
  sim.container.requestScrollToIndex(/*commandIndex*/ -1.0, /*nonce*/ 0.0, /*propIndex*/ 30);
  sim.settle();
  CHECK_NEAR(sim.offsetY, 3000.0, 1.0);

  // User scrolls away; the prop is unchanged (still 30) -> must not re-fire.
  sim.scrollTo(5000.0);
  sim.container.requestScrollToIndex(/*commandIndex*/ -1.0, /*nonce*/ 0.0, /*propIndex*/ 30);
  sim.settle();
  CHECK_NEAR(sim.offsetY, 5000.0, 1.0);

  // prop changes 30 -> 20: scroll again to the new target.
  sim.container.requestScrollToIndex(/*commandIndex*/ -1.0, /*nonce*/ 0.0, /*propIndex*/ 20);
  sim.settle();
  CHECK_NEAR(sim.offsetY, 2000.0, 1.0);
}

// scrollToEnd lands exactly at the scrollable bottom (offset == total - window) with
// the final row mounted.
TEST(scroll_to_end_fixed_height) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(200));
  sim.settle();
  CHECK_NEAR(sim.offsetY, 0.0, 0.5);  // starts at the top

  sim.scrollToEnd();

  CHECK_NEAR(sim.totalAxis(), 20000.0, 0.5);
  CHECK_NEAR(sim.offsetY, 19400.0, 1.0);  // total - window
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK(hi == std::size_t(199));  // last row is mounted at the bottom
}

// scrollToEnd converges on the TRUE bottom of a variable-height list whose rows are
// taller than the estimate, so the total keeps growing as the bottom rows are
// measured during the scroll. A one-shot jump to the initially-estimated content
// size would stop short; the correction re-targets the bottom until the total
// settles. Asserted self-consistently (independent of the approximate middle): the
// view sits at the scrollable bottom and the last row's far edge is the content end.
TEST(scroll_to_end_converges_on_true_bottom_variable_heights) {
  Sim sim;
  sim.winH = 700;
  sim.estimated = {400, 100};  // deliberately smaller than every real row
  sim.sizeOfKey = [](const std::string& key) {
    std::size_t n = std::stoul(key.substr(1));
    return Size{400, 150.0 + double((n * 41) % 250)};  // 150..399, deterministic
  };
  sim.setKeys(makeKeys(400));
  sim.settle();
  CHECK_NEAR(sim.offsetY, 0.0, 0.5);

  sim.scrollToEnd();

  double maxOffset = sim.totalAxis() - sim.winH;
  CHECK(maxOffset > 10000.0);          // sanity: it actually travelled far
  CHECK_NEAR(sim.offsetY, maxOffset, 1.0);
  CHECK_NEAR(sim.elementOffset(399) + sim.container.getElementSize(399), sim.totalAxis(), 1.0);
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK(hi == std::size_t(399));
}

// scrollToEnd settles cleanly and does NOT linger as a bottom pin: after it lands,
// appending rows below the viewport must leave the user where they are (maintain
// visible position) rather than dragging them to the new bottom.
TEST(scroll_to_end_settles_and_does_not_re_pin_on_append) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(50));
  sim.settle();

  sim.scrollToEnd();
  CHECK_NEAR(sim.offsetY, 4400.0, 1.0);  // 50 * 100 - 600

  // Append 20 rows below; the user sits at the old bottom and must stay there.
  sim.setKeys(makeKeys(70));
  sim.settle();
  CHECK_NEAR(sim.offsetY, 4400.0, 1.0);  // unchanged, NOT dragged to the new bottom (6400)
}

// scrollToEnd rides the scrollToIndex command channel via the SCROLL_TO_END_INDEX
// sentinel, so the integrations need no extra state field. Driving that path
// through requestScrollToIndex must scroll to the bottom.
TEST(scroll_to_end_via_command_sentinel) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(200));
  sim.settle();

  sim.container.requestScrollToIndex(
    /*commandIndex*/ azimgd::shadowlist::SCROLL_TO_END_INDEX, /*nonce*/ 1.0, /*propIndex*/ -2);
  sim.settle();

  CHECK_NEAR(sim.offsetY, 19400.0, 1.0);  // total - window
}
