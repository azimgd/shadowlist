/*
 * Snap target tests. A fling rests on the snap offset nearest to where it would land.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/Snap.hpp>

#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(snap_picks_nearest_offset) {
  std::vector<double> offsets{0.0, 100.0, 250.0, 400.0};
  CHECK_EQ(nearestSnapOffset(offsets, 120.0), 100.0);
  CHECK_EQ(nearestSnapOffset(offsets, 190.0), 250.0);
  CHECK_EQ(nearestSnapOffset(offsets, -50.0), 0.0);
  CHECK_EQ(nearestSnapOffset(offsets, 9000.0), 400.0);
}

TEST(snap_tie_keeps_the_first_offset) {
  std::vector<double> offsets{0.0, 100.0, 200.0};
  CHECK_EQ(nearestSnapOffset(offsets, 50.0), 0.0);
  CHECK_EQ(nearestSnapOffset(offsets, 150.0), 100.0);
}

TEST(snap_without_offsets_keeps_the_target) {
  std::vector<double> offsets;
  CHECK_EQ(nearestSnapOffset(offsets, 42.5), 42.5);
}

TEST(snap_rounds_pixel_offsets_first) {
  std::vector<double> offsets{0.0, 262.5, 525.4};
  // Android compares whole pixels: 262.5 rounds up, 525.4 rounds down.
  CHECK_EQ(nearestSnapOffset(offsets, 250.0, true), 263.0);
  CHECK_EQ(nearestSnapOffset(offsets, 530.0, true), 525.0);
  CHECK_EQ(nearestSnapOffset(offsets, 250.0, false), 262.5);
}
