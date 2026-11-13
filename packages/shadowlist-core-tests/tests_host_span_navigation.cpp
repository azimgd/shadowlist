#include "TestFramework.hpp"
#include <shadowlist-core/host/KeyNavigation.hpp>
#include <shadowlist-core/host/ListDriver.hpp>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(span_navigation_full_width_rows_choose_nearest_beam_and_stable_ties) {
  std::vector<RowRect> rows{{0, 0, 300, 10}, {0, 10, 100, 20}, {100, 10, 100, 20},
    {200, 10, 100, 20}, {0, 30, 300, 10}, {0, 40, 100, 20}, {100, 40, 200, 20}};
  auto rect = [&](std::size_t index) { return rows[index]; };
  auto down = neighborRows(0, 1, true, rows.size(), rect);
  CHECK_EQ(down[0], 2u);
  CHECK_EQ(down[1], 1u);
  CHECK_EQ(down[2], 3u);
  CHECK_EQ(neighborRows(2, 1, true, rows.size(), rect).front(), 4u);
  CHECK_EQ(neighborRows(2, 1, false, rows.size(), rect).front(), 3u);
  CHECK(neighborRows(0, -1, false, rows.size(), rect).empty());
  CHECK(neighborRows(UNDEFINED_INDEX, 1, true, rows.size(), rect).empty());
  CHECK(neighborRows(rows.size(), 1, true, rows.size(), rect).empty());
}

TEST(span_navigation_preserves_the_beam_before_a_nearer_disjoint_track) {
  std::vector<RowRect> rows{{100, 0, 100, 10}, {0, 10, 100, 10},
    {100, 20, 100, 10}, {100, 20, 100, 10}};
  auto found = neighborRows(0, 1, true, rows.size(), [&](std::size_t index) { return rows[index]; });
  CHECK_EQ(found, (std::vector<std::size_t>{2, 3, 1}));
}

TEST(span_navigation_mirrored_direction_has_the_same_logical_targets) {
  std::vector<RowRect> rows{{0, 0, 100, 20}, {100, 0, 200, 20}, {0, 20, 300, 10}};
  auto forward = neighborRows(0, 1, false, rows.size(), [&](std::size_t index) { return rows[index]; });
  auto mirrored = neighborRows(0, -1, false, rows.size(), [&](std::size_t index) {
    RowRect rect = rows[index];
    rect.x = 300.0 - rect.x - rect.width;
    return rect;
  });
  CHECK_EQ(forward, mirrored);
  CHECK_EQ(forward.front(), 1u);
}
