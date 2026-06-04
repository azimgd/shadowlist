// Append and prepend, including maintain-visible-content-position (MVCP).

#include "TestFramework.hpp"
#include "Harness.hpp"

#include <vector>

using namespace slt;

// Appending rows below the viewport grows the content but does not move it.
TEST(append_grows_content_and_keeps_top) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(20));
  sim.settle();
  CHECK_NEAR(sim.totalAxis(), 2000.0, 0.5);

  sim.setKeys(makeKeys(30));  // k0..k29 (10 appended)
  sim.settle();

  CHECK_NEAR(sim.totalAxis(), 3000.0, 0.5);
  CHECK_NEAR(sim.offsetY, 0.0, 0.5);            // viewport unchanged
  CHECK_NEAR(sim.elementOffset(0), 0.0, 0.5);   // row 0 still at the top
}

// Reconciling to a smaller key set removes the dropped keys, preserves the
// surviving elements (by key) and renumbers them, including removals from the
// middle of the list.
TEST(reconcile_shrink_preserves_surviving_keys) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(30));
  sim.settle();

  // Drop the first 5 keys (k0..k4): k5..k29 survive, so k10 moves to index 5.
  std::vector<std::string> next;
  for (int i = 5; i < 30; ++i) {
    next.push_back("k" + std::to_string(i));
  }
  sim.setKeys(next);
  sim.settle();
  CHECK_EQ(sim.container.getElementsSize(), std::size_t(25));
  CHECK_EQ(sim.indexOfKey("k10"), std::size_t(5));
  CHECK_NEAR(sim.totalAxis(), 2500.0, 0.5);

  // Drop a middle range: keep k5..k14 then k25..k29 (15 elements). k25 lands right
  // after k14, i.e. at index 10.
  std::vector<std::string> middle;
  for (int i = 5; i < 15; ++i) {
    middle.push_back("k" + std::to_string(i));
  }
  for (int i = 25; i < 30; ++i) {
    middle.push_back("k" + std::to_string(i));
  }
  sim.setKeys(middle);
  sim.settle();
  CHECK_EQ(sim.container.getElementsSize(), std::size_t(15));
  CHECK_EQ(sim.indexOfKey("k14"), std::size_t(9));
  CHECK_EQ(sim.indexOfKey("k25"), std::size_t(10));
  CHECK_EQ(sim.indexOfKey("k20"), UNDEFINED_INDEX);  // dropped key is gone
}

// Prepending rows above the viewport keeps the on-screen content in place by
// shifting the scroll offset by the inserted height (MVCP).
TEST(prepend_maintains_visible_content_position) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(30));
  sim.settle();

  // Put row "k10" exactly at the top of the viewport.
  sim.scrollTo(1000.0);
  CHECK_NEAR(sim.offsetY, 1000.0, 0.5);
  CHECK_EQ(sim.indexOfKey("k10"), std::size_t(10));
  double k10ScreenBefore = sim.elementOffset(sim.indexOfKey("k10")) - sim.offsetY;

  // Prepend 5 new rows: n0..n4, then the original k0..k29.
  std::vector<std::string> next = makeKeys(5, "n");
  for (const auto& key : makeKeys(30)) {
    next.push_back(key);
  }
  sim.setKeys(next);
  sim.settle();

  // k10 moved 5 slots down; the offset shifted by 5 * 100 to compensate.
  CHECK_EQ(sim.indexOfKey("k10"), std::size_t(15));
  CHECK_NEAR(sim.offsetY, 1500.0, 1.0);

  // Its on-screen position is unchanged.
  double k10ScreenAfter = sim.elementOffset(sim.indexOfKey("k10")) - sim.offsetY;
  CHECK_NEAR(k10ScreenAfter, k10ScreenBefore, 1.0);
}

// Prepending while scrolled to the very top still maintains visible position:
// the row that was at the top (k0) stays at the top, so the offset shifts down
// by the inserted height rather than the new rows shoving the content away.
TEST(prepend_at_top_keeps_anchor_row_in_view) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(20));
  sim.settle();
  CHECK_NEAR(sim.offsetY, 0.0, 0.5);
  double k0ScreenBefore = sim.elementOffset(sim.indexOfKey("k0")) - sim.offsetY;  // 0 (at top)

  std::vector<std::string> next = makeKeys(5, "n");
  for (const auto& key : makeKeys(20)) {
    next.push_back(key);
  }
  sim.setKeys(next);
  sim.settle();

  CHECK_NEAR(sim.totalAxis(), 2500.0, 0.5);
  CHECK_NEAR(sim.offsetY, 500.0, 1.0);  // shifted by the 5 inserted rows
  double k0ScreenAfter = sim.elementOffset(sim.indexOfKey("k0")) - sim.offsetY;
  CHECK_NEAR(k0ScreenAfter, k0ScreenBefore, 1.0);  // k0 still at the top
}

// MVCP on a variable-height list: the anchor row's on-screen position is held by
// re-targeting the measured anchor element, not by assuming the inserted rows are
// the estimated size. With non-uniform real heights a fixed "shift by estimate"
// would drift; anchoring keeps the anchor pinned exactly.
TEST(prepend_with_variable_heights_maintains_position) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 140};
  sim.sizeOfKey = [](const std::string& key) {
    std::size_t n = std::stoul(key.substr(1));
    return Size{400, 80.0 + double((n * 53) % 160)};  // 80..239, deterministic
  };
  sim.setKeys(makeKeys(40));
  sim.settle();
  sim.scrollTo(1500.0);

  std::size_t anchorBefore = sim.indexOfKey("k20");
  double anchorScreenBefore = sim.elementOffset(anchorBefore) - sim.offsetY;

  // Prepend 6 new rows above the viewport.
  std::vector<std::string> next = makeKeys(6, "n");
  for (const auto& key : makeKeys(40)) {
    next.push_back(key);
  }
  sim.setKeys(next);
  sim.settle();

  // k20 moved down by the 6 inserted rows but stays at the same screen position.
  CHECK_EQ(sim.indexOfKey("k20"), anchorBefore + 6);
  double anchorScreenAfter = sim.elementOffset(sim.indexOfKey("k20")) - sim.offsetY;
  CHECK_NEAR(anchorScreenAfter, anchorScreenBefore, 1.0);
}
