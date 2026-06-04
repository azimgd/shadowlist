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
