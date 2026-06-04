// Nested lists: one vertical "outer" list whose rows each host a horizontal
// "inner" list. In the core every list is its own Container, so this verifies
// that independent containers (vertical + several horizontal) coexist and do
// not interfere when driven interleaved.

#include "TestFramework.hpp"
#include "Harness.hpp"

#include <vector>

using namespace slt;

TEST(nested_vertical_outer_with_horizontal_inners) {
  // Outer: vertical, 20 rows of 300px.
  Sim outer;
  outer.winW = 400;
  outer.winH = 600;
  outer.estimated = {400, 300};
  outer.sizeOfKey = [](const std::string&) { return Size{400, 300}; };
  outer.setKeys(makeKeys(20, "row"));

  // Inners: horizontal, 10 cells of 180px wide / 300px tall.
  std::vector<Sim> inners(3);
  for (auto& inner : inners) {
    inner.horizontal = true;
    inner.winW = 400;
    inner.winH = 300;
    inner.estimated = {180, 300};
    inner.sizeOfKey = [](const std::string&) { return Size{180, 300}; };
    inner.setKeys(makeKeys(10, "cell"));
  }

  // Drive them interleaved, the way the host app commits them.
  for (int i = 0; i < 5; ++i) {
    outer.frame();
    for (auto& inner : inners) {
      inner.frame();
    }
  }
  outer.settle();
  for (auto& inner : inners) {
    inner.settle();
  }

  // Outer content height = 20 * 300.
  CHECK_NEAR(outer.totalAxis(), 6000.0, 0.5);

  // Each inner is horizontal: content width = 10 * 180, cross-axis height = 300.
  for (auto& inner : inners) {
    CHECK_NEAR(inner.totalAxis(), 1800.0, 0.5);
    CHECK_NEAR(inner.container.nextRevision.totalContainerHeight, 300.0, 0.5);
    std::size_t lo = 0, hi = 0;
    CHECK(inner.visibleRange(lo, hi));
    CHECK_EQ(lo, std::size_t(0));
  }

  // Independence: scrolling inner[0] must not move inner[1] or the outer list.
  inners[0].offsetX = 900;
  inners[0].settle();
  CHECK(inners[0].offsetX > 0.0);
  CHECK_NEAR(inners[1].offsetX, 0.0, 0.5);
  CHECK_NEAR(outer.offsetY, 0.0, 0.5);
}
