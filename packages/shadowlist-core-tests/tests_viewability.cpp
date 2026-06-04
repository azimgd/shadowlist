// Strictly-viewable range (getViewableIndices) and start/end reached thresholds.

#include "TestFramework.hpp"
#include "Harness.hpp"

using namespace slt;

// The viewable range is the strict viewport, a subset of the mounted window
// (which carries an off-screen buffer). At offset 3000 with 100px rows the mounted
// window is rows 24..42 but only rows 30..35 are actually on screen.
TEST(viewable_window_is_strict_viewport) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();
  sim.scrollTo(3000.0);

  std::size_t lo = 0, hi = 0;
  CHECK(sim.viewableRange(lo, hi));
  CHECK_EQ(lo, std::size_t(30));  // viewport [3000, 3600]
  CHECK_EQ(hi, std::size_t(35));  // row 35 spans 3500..3600; row 36 only touches the edge

  // The mounted window is wider than the viewable window.
  std::size_t mountedLo = 0, mountedHi = 0;
  CHECK(sim.visibleRange(mountedLo, mountedHi));
  CHECK(mountedLo <= lo);
  CHECK(mountedHi >= hi);
}

// itemVisiblePercentThreshold: a row only counts once at least that fraction of it
// is on screen. At offset 3060 row 30 is 40% visible (excluded at 0.5) and row 36
// is 60% visible (included).
TEST(viewable_respects_percent_threshold) {
  Sim sim;
  sim.winH = 600;
  sim.viewablePercentThreshold = 0.5;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();
  sim.scrollTo(3060.0);

  std::size_t lo = 0, hi = 0;
  CHECK(sim.viewableRange(lo, hi));
  CHECK_EQ(lo, std::size_t(31));  // row 30 only 40% visible -> excluded
  CHECK_EQ(hi, std::size_t(36));  // row 36 is 60% visible -> included
}

// The change callback fires (deduplicated) and its last payload matches the
// current viewable range.
TEST(viewable_change_callback_fires) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();
  sim.viewableChanges.clear();
  sim.scrollTo(3000.0);

  CHECK(!sim.viewableChanges.empty());
  auto last = sim.viewableChanges.back();
  CHECK_EQ(last.first, std::size_t(30));
  CHECK_EQ(last.second, std::size_t(35));
}

// onEndReached fires within endReachedThreshold * window of the content end. The
// default (1.0) does not fire 1.5 windows out; widening to 2.0 does.
TEST(end_reached_threshold_widens_trigger) {
  auto run = [](double threshold) {
    Sim sim;
    sim.winH = 600;
    sim.endReachedThreshold = threshold;
    sim.estimated = {400, 100};
    sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
    sim.setKeys(makeKeys(100));  // total 10000, maxOffset 9400
    sim.settle();
    sim.endReached = 0;
    sim.scrollTo(8500.0);  // viewport bottom 9100, i.e. 900px (1.5 windows) from end
    return sim.endReached;
  };

  CHECK_EQ(run(1.0), 0);  // 900px out is beyond one window
  CHECK(run(2.0) > 0);    // within two windows
}

// onStartReached threshold narrows symmetrically: at 450px from the top the default
// (1.0, i.e. within 600px) fires but 0.5 (within 300px) does not.
TEST(start_reached_threshold_narrows_trigger) {
  auto run = [](double threshold) {
    Sim sim;
    sim.winH = 600;
    sim.startReachedThreshold = threshold;
    sim.estimated = {400, 100};
    sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
    sim.setKeys(makeKeys(100));
    sim.settle();
    // onStartReached fires once per arrival at the start band, so move out of it
    // first; scrolling back in is the fresh arrival this asserts on.
    sim.scrollTo(2000.0);
    sim.startReached = 0;
    sim.scrollTo(450.0);
    return sim.startReached;
  };

  CHECK(run(1.0) > 0);    // 450 <= 600
  CHECK_EQ(run(0.5), 0);  // 450 > 300
}

// A row taller than the viewport fully covers the screen but can never reach 100%
// of ITSELF; it must still count as viewable at threshold 1.0 (regression for the
// min(element, viewport) reference-size clamp in getViewableIndices).
TEST(viewable_includes_item_taller_than_viewport) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 1000};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 1000}; };
  sim.viewablePercentThreshold = 1.0;
  sim.setKeys(makeKeys(20));
  sim.settle();
  sim.scrollTo(1200.0);  // viewport [1200,1800] sits entirely inside row 1 ([1000,2000])

  std::size_t lo = 0, hi = 0;
  CHECK(sim.viewableRange(lo, hi));
  CHECK(lo <= std::size_t(1) && std::size_t(1) <= hi);
}
