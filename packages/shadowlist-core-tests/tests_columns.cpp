// Multi-column (grid) layout: columns > 1 places elements into N tracks along the
// scroll axis. Element i lives in track i % columns; the cross-axis position is
// the track index * trackSize and the cross-axis size is forced to the track size.

#include "TestFramework.hpp"
#include "Harness.hpp"

using namespace slt;

// A 2-column vertical grid of 20 uniform rows stacks 10 rows per track.
TEST(columns_two_column_grid_layout) {
  Sim sim;
  sim.winW = 400;
  sim.winH = 600;
  sim.columns = 2;
  sim.estimated = {200, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{200, 100}; };
  sim.setKeys(makeKeys(20));
  sim.settle();

  // 20 elements / 2 columns = 10 rows of 100px.
  CHECK_NEAR(sim.container.revision.totalContainerHeight, 1000.0, 0.5);
  // Cross axis is the window width (each track is half of it).
  CHECK_NEAR(sim.container.revision.totalContainerWidth, 400.0, 0.5);

  // Even indices fill the left track (offsetX 0), odd indices the right (offsetX 200).
  CHECK_NEAR(sim.container.getElementAtIndex(0).offsetX, 0.0, 0.5);
  CHECK_NEAR(sim.container.getElementAtIndex(0).offsetY, 0.0, 0.5);
  CHECK_NEAR(sim.container.getElementAtIndex(1).offsetX, 200.0, 0.5);
  CHECK_NEAR(sim.container.getElementAtIndex(1).offsetY, 0.0, 0.5);
  CHECK_NEAR(sim.container.getElementAtIndex(2).offsetX, 0.0, 0.5);
  CHECK_NEAR(sim.container.getElementAtIndex(2).offsetY, 100.0, 0.5);
  CHECK_NEAR(sim.container.getElementAtIndex(5).offsetX, 200.0, 0.5);
  CHECK_NEAR(sim.container.getElementAtIndex(5).offsetY, 200.0, 0.5);
}

// Variable row heights per track: re-flowing from a measured element seeds each
// track from its last element before the re-flow point (the trackSizes seed loop
// in recomputeElementOffsets), so the two tracks advance independently.
TEST(columns_variable_heights_stack_per_track) {
  Sim sim;
  sim.winW = 400;
  sim.winH = 600;
  sim.columns = 2;
  sim.estimated = {200, 100};
  // Track 0 holds even indices (100px), track 1 holds odd indices (150px).
  sim.sizeOfKey = [](const std::string& key) {
    std::size_t n = std::stoul(key.substr(1));
    return Size{200, 100.0 + double(n % 2) * 50.0};
  };
  sim.setKeys(makeKeys(20));
  sim.settle();

  // Left track (even indices): 0, 100, 200, ... by 100px steps.
  CHECK_NEAR(sim.container.getElementAtIndex(0).offsetY, 0.0, 0.5);
  CHECK_NEAR(sim.container.getElementAtIndex(2).offsetY, 100.0, 0.5);
  CHECK_NEAR(sim.container.getElementAtIndex(4).offsetY, 200.0, 0.5);
  // Right track (odd indices): 0, 150, 300, ... by 150px steps, independent of left.
  CHECK_NEAR(sim.container.getElementAtIndex(1).offsetY, 0.0, 0.5);
  CHECK_NEAR(sim.container.getElementAtIndex(3).offsetY, 150.0, 0.5);
  CHECK_NEAR(sim.container.getElementAtIndex(5).offsetY, 300.0, 0.5);
}
