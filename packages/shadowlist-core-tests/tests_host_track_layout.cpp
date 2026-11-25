#include "TestFramework.hpp"
#include <shadowlist-core/TrackLayout.hpp>
#include <shadowlist-core/TrackRanges.hpp>
#include <cstdint>
#include <limits>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(track_layout_default_spans_match_round_robin_stacks_exactly) {
  TrackLayout layout(3, 17.0);
  std::vector<double> expected(3, 17.0);
  for (std::size_t index = 0; index < 100; ++index) {
    double extent = static_cast<double>(index % 7);
    auto row = layout.place(1, extent);
    CHECK_EQ(row.column, index % 3);
    CHECK_EQ(row.span, 1u);
    CHECK_EQ(row.leading, expected[index % 3]);
    expected[index % 3] += extent;
  }
  CHECK_EQ(layout.getTrackEnds(), expected);
}

TEST(track_layout_wraps_contiguous_spans_and_full_span_barriers_add_only_their_extent) {
  TrackLayout layout(3);
  auto first = layout.place(2, 10.0);
  CHECK_EQ(first.column, 0u);
  CHECK_EQ(layout.place(1, 20.0).column, 2u);
  CHECK_EQ(layout.place(2, 5.0).leading, 10.0);
  auto wrapped = layout.place(2, 7.0);
  CHECK_EQ(wrapped.column, 0u);
  CHECK_EQ(wrapped.leading, 15.0);
  auto emptyFooter = layout.place(3, 0.0);
  CHECK_EQ(emptyFooter.leading, 22.0);
  CHECK_EQ(layout.getContentEnd(), 22.0);
  CHECK_EQ(layout.place(3, 0.0).leading, 22.0); // An empty section's zero-sized edges add no space.
  CHECK_EQ(layout.place(1, 4.0).leading, 22.0);
  CHECK_EQ(layout.place(3, 8.0).leading, 26.0);
  CHECK_EQ(layout.getContentEnd(), 34.0);
}

TEST(track_layout_seeds_a_track_unused_for_many_mixed_span_rows) {
  TrackLayout layout(3);
  std::vector<TrackPlacement> rows;
  rows.push_back(layout.place(2, 5.0));
  rows.push_back(layout.place(1, 100.0));
  for (std::size_t index = 0; index < 1024; ++index) {
    rows.push_back(layout.place(2, 1.0));
  }
  TrackLayout reflow(3);
  reflow.seed(rows.size(), [&](std::size_t index) { return rows[index]; });
  CHECK_EQ(reflow.getTrackEnds(), layout.getTrackEnds());
  CHECK_EQ(reflow.getNextColumn(), 2u);
  CHECK_EQ(reflow.place(1, 9.0).leading, 100.0);
}

TEST(track_layout_incremental_late_sizes_and_reordered_spans_match_full_reflow) {
  std::vector<std::size_t> spans{2, 1, 2, 3, 1, 2, 2, 1};
  std::vector<double> extents{5.0, 100.0, 7.0, 0.0, 9.0, 12.0, 3.0, 4.0};
  TrackLayout original(3, 11.0);
  std::vector<TrackPlacement> rows;
  for (std::size_t index = 0; index < spans.size(); ++index) {
    rows.push_back(original.place(spans[index], extents[index]));
  }
  extents[2] = 24.0;
  std::swap(spans[4], spans[5]);
  std::swap(extents[4], extents[5]);
  TrackLayout partial(3, 11.0);
  partial.seed(2, [&](std::size_t index) { return rows[index]; });
  TrackLayout full(3, 11.0);
  for (std::size_t index = 0; index < spans.size(); ++index) {
    auto expected = full.place(spans[index], extents[index]);
    if (index < 2) {
      continue;
    }
    auto actual = partial.place(spans[index], extents[index]);
    CHECK_EQ(actual.column, expected.column);
    CHECK_EQ(actual.span, expected.span);
    CHECK_EQ(actual.leading, expected.leading);
  }
  CHECK_EQ(partial.getTrackEnds(), full.getTrackEnds());
  // Append consumes the existing summary, without rebuilding the unchanged prefix.
  CHECK_EQ(partial.place(3, 6.0).leading, full.getContentEnd());
}

TEST(track_layout_span_input_normalization_is_bounded_before_integer_conversion) {
  CHECK_EQ(itemSpanFrom(-1.0, 3), 1u);
  CHECK_EQ(itemSpanFrom(0.0, 3), 1u);
  CHECK_EQ(itemSpanFrom(2.9, 3), 2u);
  CHECK_EQ(itemSpanFrom(1e100, 3), 3u);
  CHECK_EQ(itemSpanFrom(std::numeric_limits<double>::infinity(), 3), 1u);
  CHECK_EQ(itemSpanFrom(std::numeric_limits<double>::quiet_NaN(), 3), 1u);
  CHECK_EQ(itemSpanFrom(3.0, 0), 1u);
}

TEST(track_ranges_incremental_append_trim_and_resize_match_actual_overlaps) {
  std::vector<TrackRange> rows;
  TrackRanges ranges;
  auto read = [&](std::size_t index) { return rows[index]; };
  auto check = [&](double lower, double upper) {
    std::vector<std::size_t> expected;
    for (std::size_t index = 0; index < rows.size(); ++index) {
      if (rows[index].end > lower && rows[index].leading <= upper) {
        expected.push_back(index);
      }
    }
    std::vector<std::size_t> actual;
    ranges.visit(lower, upper, [&](std::size_t index) { actual.push_back(index); });
    CHECK_EQ(actual, expected);
  };
  for (std::size_t count = 1; count < 130; ++count) {
    rows.push_back({static_cast<double>(count), static_cast<double>(count + count % 11)});
    ranges.update(count - 1, count, read);
    check(50.0, 55.0);
  }
  rows[2] = {2.0, 1000.0};
  ranges.update(2, rows.size(), read);
  check(900.0, 910.0);
  rows.resize(3);
  ranges.update(3, rows.size(), read);
  check(100.0, 110.0);
  rows.clear();
  ranges.update(0, 0, read);
  check(0.0, 1000.0);
}

TEST(track_ranges_random_mixed_spans_and_late_edits_match_brute_force_rects) {
  std::uint32_t state = 0x724231u;
  auto next = [&] {
    state = state * 1664525u + 1013904223u;
    return state;
  };
  std::vector<std::size_t> spans(257);
  std::vector<double> extents(257);
  for (std::size_t index = 0; index < spans.size(); ++index) {
    spans[index] = next() % 5 + 1;
    extents[index] = next() % 701;
  }
  TrackRanges ranges;
  std::vector<TrackRange> rows(spans.size());
  for (std::size_t edit = 0; edit < 50; ++edit) {
    auto changed = next() % spans.size();
    extents[changed] = next() % 1201;
    spans[changed] = next() % 5 + 1;
    TrackLayout layout(5);
    for (std::size_t index = 0; index < rows.size(); ++index) {
      auto placed = layout.place(spans[index], extents[index]);
      rows[index] = {placed.leading, placed.leading + placed.extent};
    }
    ranges.update(edit == 0 ? 0 : changed, rows.size(), [&](std::size_t index) { return rows[index]; });
    for (std::size_t query = 0; query < 20; ++query) {
      double lower = next() % static_cast<std::size_t>(layout.getContentEnd() + 1.0);
      double upper = lower + next() % 1000;
      std::vector<std::size_t> expected;
      for (std::size_t index = 0; index < rows.size(); ++index) {
        if (rows[index].end > lower && rows[index].leading <= upper) {
          expected.push_back(index);
        }
      }
      std::vector<std::size_t> actual;
      ranges.visit(lower, upper, [&](std::size_t index) { actual.push_back(index); });
      CHECK_EQ(actual, expected);
    }
  }
}
