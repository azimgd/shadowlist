// Observer callbacks: onEndReached / onStartReached / onVisibleIndicesChange.

#include "TestFramework.hpp"
#include "Harness.hpp"

using namespace slt;

// onEndReached fires once the offset comes within one window of the end. The
// threshold is offset + window >= total - window, i.e. offset >= total - 2*window
// (= 3000 - 1200 = 1800 here); it must not fire just short of that.
TEST(on_end_reached_fires_near_end_threshold) {
  auto deltaAfterScroll = [](double to) {
    Sim sim;
    sim.winH = 600;
    sim.estimated = {400, 100};
    sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
    sim.setKeys(makeKeys(30));  // 3000px content
    sim.settle();
    int before = sim.endReached;
    sim.scrollTo(to);
    return sim.endReached - before;
  };

  CHECK_EQ(deltaAfterScroll(1700.0), 0);  // just short of the threshold: no fire
  CHECK(deltaAfterScroll(1800.0) > 0);    // at the threshold: fires
}

// Reaching the top fires onStartReached.
TEST(on_start_reached_fires_at_top) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(30));
  sim.settle();

  sim.scrollTo(2400.0);          // go to the bottom first
  int before = sim.startReached;
  sim.scrollTo(0.0);             // back to the top
  CHECK(sim.startReached > before);
}

// onVisibleIndicesChange does not re-fire while the visible window is unchanged.
TEST(on_visible_indices_change_is_deduplicated) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(50));
  sim.settle();
  sim.frame();  // let the measurement window settle past the first revision
  sim.frame();

  std::size_t before = sim.visibleChanges.size();
  sim.frame();  // identical offset / window
  sim.frame();
  CHECK_EQ(sim.visibleChanges.size(), before);  // no duplicate events

  // Scrolling to a new window does fire more changes.
  sim.scrollTo(2000.0);
  CHECK(sim.visibleChanges.size() > before);
}

// onScroll fires when the offset changes and is deduplicated while it is steady.
TEST(on_scroll_fires_on_offset_change_and_dedups) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(50));
  sim.settle();

  sim.scrolls.clear();
  sim.frame();  // identical offset -> no new scroll event
  sim.frame();
  CHECK_EQ(sim.scrolls.size(), std::size_t(0));

  sim.scrollTo(2000.0);
  CHECK(sim.scrolls.size() > std::size_t(0));
  CHECK_NEAR(sim.scrolls.back().second, 2000.0, 0.5);  // last reported offset
}

// A list that fits within a single window technically reaches both edges; the
// dedup prefers the end callback so start does not also fire.
TEST(short_list_reaches_end_not_start) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(4));  // 400px content < 600px window
  sim.settle();

  CHECK(sim.endReached > 0);
  CHECK_EQ(sim.startReached, 0);
}
