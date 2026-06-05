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

// Collapsing/removing content while scrolled deep (e.g. a tree "collapse all" at the
// bottom) leaves the offset past the new end with nothing visible. The core must pull
// the offset back to the new bottom so content reappears instead of a blank screen.
TEST(content_shrink_below_offset_pulls_back_to_bottom) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));   // total 10000, maxOffset 9400
  sim.settle();
  sim.scrollTo(9000.0);
  CHECK_NEAR(sim.offsetY, 9000.0, 0.5);  // a valid deep offset is NOT spuriously clamped

  // Collapse to a handful of rows that fit within the window (new maxOffset 0).
  sim.setKeys(makeKeys(5));
  sim.settle();

  CHECK_NEAR(sim.offsetY, 0.0, 0.5);     // pulled back to the new bottom (top here)
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));        // content is visible again
  CHECK_EQ(lo, (std::size_t)0);
}

// A partial shrink pulls back to the new bottom (not all the way to the top).
TEST(content_shrink_pulls_back_to_new_max_offset) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();
  sim.scrollTo(9000.0);

  sim.setKeys(makeKeys(20));   // total 2000, new maxOffset 1400
  sim.settle();

  CHECK_NEAR(sim.offsetY, 1400.0, 0.5);
}

// The clamp must fire from the "no elements visible" signal, NOT from a !userScrolled
// gate: after a real gesture scroll the user-scroll flag can still be latched on the
// commit that collapses the content, and the old gate skipped the pull-back, stranding
// the user on a blank screen. Here the shrink commit reports userScrolled=true and must
// still pull back to the new bottom.
TEST(content_shrink_clamps_even_with_stale_user_scroll_flag) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();
  sim.scrollTo(9000.0);
  CHECK_NEAR(sim.offsetY, 9000.0, 0.5);

  // Collapse to a handful of rows; the commit still carries a (stale) user-scroll flag.
  sim.setKeys(makeKeys(5));
  sim.commit(1, /*userScrolled=*/true);
  sim.settle();

  CHECK_NEAR(sim.offsetY, 0.0, 0.5);
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
}

// The "first collapse" bug: the user scrolled only a little past the (small) collapsed
// content, so the buffered measure window (which looks ~2 viewports ahead) still reaches
// the few remaining rows - yet the viewport itself is blank. Keying the pull-back on the
// content shrinking (not on an empty measure window) handles this shallow case too.
TEST(content_shrink_shallow_scroll_still_pulls_back) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(11));   // total 1100, maxOffset 500
  sim.settle();
  sim.scrollTo(500.0);         // at the bottom of the initial content
  CHECK_NEAR(sim.offsetY, 500.0, 0.5);

  // Collapse to 2 rows (total 200 < window): offset 500 is past the new end but only
  // ~half a window beyond it, so the buffered window still reaches the rows.
  sim.setKeys(makeKeys(2));
  sim.settle();

  CHECK_NEAR(sim.offsetY, 0.0, 0.5);
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
}

// Fabric runs update() several times per commit (it clones the list node). The shrink
// is only detectable on the first clone (the next one sees the already-updated previous
// total), so the pull-back must be LATCHED (pendingScroll) to survive every clone -
// including the last one, which the layout publishes. A one-shot write only the first
// clone applied was reset by the later clones, leaving the screen blank on device.
TEST(content_shrink_clamps_across_multi_adopt_commit) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(11));
  sim.settle();
  sim.scrollTo(500.0);
  CHECK_NEAR(sim.offsetY, 500.0, 0.5);

  // Collapse, with THREE update() clones in the one commit (as Fabric does).
  sim.setKeys(makeKeys(2));
  sim.commit(3, /*userScrolled=*/false);
  sim.settle();

  CHECK_NEAR(sim.offsetY, 0.0, 0.5);
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
}
