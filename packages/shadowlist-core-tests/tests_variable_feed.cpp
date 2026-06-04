// A realistic variable-height feed driven from a data fixture
// (fixtures/feed_heights.txt: one row height per line).

#include "TestFramework.hpp"
#include "Harness.hpp"

#include <fstream>
#include <string>
#include <vector>

#ifndef SLT_FIXTURES_DIR
#define SLT_FIXTURES_DIR "fixtures"
#endif

using namespace slt;

static std::vector<double> loadHeights() {
  std::vector<double> heights;
  std::ifstream file(std::string(SLT_FIXTURES_DIR) + "/feed_heights.txt");
  double value = 0.0;
  while (file >> value) {
    heights.push_back(value);
  }
  return heights;
}

TEST(variable_height_feed_stacks_visible_rows_exactly) {
  std::vector<double> heights = loadHeights();
  CHECK(heights.size() > std::size_t(50));  // fixture loaded

  Sim sim;
  sim.winH = 700;
  sim.estimated = {400, 140};
  sim.sizeOfKey = [&](const std::string& key) {
    std::size_t n = std::stoul(key.substr(1));
    return Size{400, heights[n % heights.size()]};
  };
  sim.setKeys(makeKeys(heights.size()));
  sim.settle();

  // The rows that are actually measured (the visible window) are stacked by
  // their real heights, edge to edge.
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  for (std::size_t i = lo; i + 1 <= hi; ++i) {
    double gap = sim.elementOffset(i + 1) - sim.elementOffset(i);
    CHECK_NEAR(gap, heights[i % heights.size()], 0.5);
  }
}

TEST(variable_height_feed_scroll_to_index_lands_on_target) {
  std::vector<double> heights = loadHeights();
  CHECK(heights.size() > std::size_t(50));

  Sim sim;
  sim.winH = 700;
  sim.estimated = {400, 140};
  sim.sizeOfKey = [&](const std::string& key) {
    std::size_t n = std::stoul(key.substr(1));
    return Size{400, heights[n % heights.size()]};
  };
  sim.setKeys(makeKeys(heights.size()));
  sim.settle();

  std::size_t target = heights.size() / 2;
  sim.container.scrollToIndex(target);
  sim.settle();

  // The anchored scroll converges so the target row lands at the viewport edge.
  CHECK_NEAR(sim.offsetY, sim.elementOffset(target), 2.0);
  std::size_t lo = 0, hi = 0;
  CHECK(sim.visibleRange(lo, hi));
  CHECK(lo <= target && target <= hi);

  // Ground truth: the landing region is genuinely measured, so the visible rows
  // stack edge to edge by their real fixture heights (not the 140px estimate).
  for (std::size_t i = lo; i + 1 <= hi; ++i) {
    double gap = sim.elementOffset(i + 1) - sim.elementOffset(i);
    CHECK_NEAR(gap, heights[i % heights.size()], 0.5);
  }
}
