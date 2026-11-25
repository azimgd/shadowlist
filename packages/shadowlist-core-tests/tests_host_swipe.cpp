/*
 * SwipeReveal tests: the swipe action offsets both native lists use, and a fuzz test of
 * swipes on rows with every mix of actions.
 */

#include "FuzzHelpers.hpp"
#include "TestFramework.hpp"

#include <shadowlist-core/host/SwipeReveal.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

SwipeSpec spec() {
  SwipeSpec value;
  value.leadingWidth = 80.0;
  value.trailingWidth = 160.0;
  value.trailingFullSwipe = true;
  value.rowSize = 400.0;
  return value;
}

}

TEST(swipe_follows_the_finger_then_rubber_bands_past_the_actions) {
  SwipeReveal swipe;
  swipe.begin(spec(), 0.0);
  CHECK_EQ(swipe.drag(40.0), 40.0);
  CHECK_EQ(swipe.drag(120.0), 80.0 + 40.0 * SWIPE_RUBBER_BAND);
  // The trailing side allows a full swipe and follows all the way.
  CHECK_EQ(swipe.drag(-300.0), -300.0);
  CHECK_EQ(swipe.drag(-900.0), -400.0);
}

TEST(swipe_side_without_actions_stays_closed) {
  SwipeSpec only = spec();
  only.leadingWidth = 0.0;
  SwipeReveal swipe;
  swipe.begin(only, 0.0);
  CHECK_EQ(swipe.drag(50.0), 0.0);
}

TEST(swipe_settles_open_closed_or_full) {
  SwipeReveal swipe;
  swipe.begin(spec(), 0.0);
  SwipeRest open = swipe.settle(50.0, 0.0, 500.0);
  CHECK(open.side == SwipeSide::Leading);
  CHECK_EQ(open.offset, 80.0);
  SwipeRest closed = swipe.settle(30.0, 0.0, 500.0);
  CHECK(closed.side == SwipeSide::None);
  CHECK_EQ(closed.offset, 0.0);
  // A short flick toward the side opens it, one away from it closes it.
  CHECK(swipe.settle(-20.0, -900.0, 500.0).side == SwipeSide::Trailing);
  CHECK(swipe.settle(-120.0, 900.0, 500.0).side == SwipeSide::None);
  SwipeRest full = swipe.settle(-250.0, 0.0, 500.0);
  CHECK(full.full);
  CHECK(full.side == SwipeSide::Trailing);
  CHECK_EQ(full.offset, -400.0);
  CHECK(!swipe.isPastFullSwipe(200.0));
}

TEST(swipe_starting_open_continues_from_the_open_offset) {
  SwipeReveal swipe;
  swipe.begin(spec(), -160.0);
  CHECK_EQ(swipe.drag(100.0), -60.0);
  CHECK(swipe.settle(-60.0, 0.0, 500.0).side == SwipeSide::None);
}

TEST(swipe_button_size_pads_the_title_and_never_goes_below_the_minimum) {
  CHECK_EQ(swipeButtonSize(20.0, 1.0), SWIPE_BUTTON_MIN);
  CHECK_EQ(swipeButtonSize(80.0, 1.0), 80.0 + SWIPE_BUTTON_PADDING);
  // Android scales the dp constants by the density.
  CHECK_EQ(swipeButtonSize(30.0, 3.0), SWIPE_BUTTON_MIN * 3.0);
  CHECK_EQ(swipeButtonSize(300.0, 3.0), 300.0 + SWIPE_BUTTON_PADDING * 3.0);
}

TEST(swipe_buttons_stretch_over_the_gap_from_the_outer_edge) {
  std::vector<SwipeSpan> spans;
  // Leading side, opened to twice the buttons' width.
  swipeButtonSpans({100.0, 50.0}, 300.0, false, 400.0, spans);
  CHECK_EQ(spans.size(), std::size_t{2});
  CHECK_NEAR(spans[0].start, 0.0, 1e-9);
  CHECK_NEAR(spans[0].size, 200.0, 1e-9);
  CHECK_NEAR(spans[1].start, 200.0, 1e-9);
  CHECK_NEAR(spans[1].size, 100.0, 1e-9);

  // Trailing side: the first action sits at the trailing edge.
  swipeButtonSpans({100.0, 50.0}, -150.0, false, 400.0, spans);
  CHECK_NEAR(spans[0].start, 300.0, 1e-9);
  CHECK_NEAR(spans[0].size, 100.0, 1e-9);
  CHECK_NEAR(spans[1].start, 250.0, 1e-9);
  CHECK_NEAR(spans[1].size, 50.0, 1e-9);
}

TEST(swipe_buttons_past_a_full_swipe_leave_the_gap_to_the_first) {
  std::vector<SwipeSpan> spans;
  swipeButtonSpans({100.0, 50.0}, -320.0, true, 400.0, spans);
  CHECK_NEAR(spans[0].start, 80.0, 1e-9);
  CHECK_NEAR(spans[0].size, 320.0, 1e-9);
  CHECK_NEAR(spans[1].size, 0.0, 1e-9);
}

TEST(swipe_button_edges_round_without_drifting) {
  /*
   * Three buttons over 100 pixels: rounding each size alone gives 33 + 33 + 33. Rounding the
   * edges keeps them touching and ending at the gap.
   */
  std::vector<SwipeSpan> spans;
  swipeButtonSpans({1.0, 1.0, 1.0}, 100.0, false, 300.0, spans);
  long previousEnd = 0;
  for (const SwipeSpan& span : spans) {
    long start = std::lround(span.start);
    long end = std::lround(span.start + span.size);
    CHECK_EQ(start, previousEnd);
    previousEnd = end;
  }
  CHECK_EQ(previousEnd, 100L);
}

TEST(swipe_revealed_span_is_the_gap_at_the_revealed_side) {
  SwipeSpan leading = swipeRevealedSpan(120.0, 400.0);
  CHECK_NEAR(leading.start, 0.0, 1e-9);
  CHECK_NEAR(leading.size, 120.0, 1e-9);
  SwipeSpan trailing = swipeRevealedSpan(-120.0, 400.0);
  CHECK_NEAR(trailing.start, 280.0, 1e-9);
  CHECK_NEAR(trailing.size, 120.0, 1e-9);
}

TEST(swipe_row_slid_all_the_way_counts_as_out) {
  SwipeReveal swipe;
  swipe.begin(spec(), 0.0);
  CHECK(swipe.isSwipedOut(-400.0));
  CHECK(swipe.isSwipedOut(399.5));
  CHECK(!swipe.isSwipedOut(-160.0));
  SwipeReveal closed;
  CHECK(!closed.isSwipedOut(0.0));
}

TEST(swipe_action_colors_are_opaque) {
  CHECK_EQ(SWIPE_ACTION_COLOR_NORMAL >> 24, std::uint32_t(0xFF));
  CHECK_EQ(SWIPE_ACTION_COLOR_DESTRUCTIVE >> 24, std::uint32_t(0xFF));
}

TEST(a_destructive_swipe_action_is_red_and_any_other_gray) {
  CHECK_EQ(SWIPE_ACTION_COLOR_DESTRUCTIVE & 0xFFFFFF, std::uint32_t(0xFF3B30));
  CHECK_EQ(SWIPE_ACTION_COLOR_NORMAL & 0xFFFFFF, std::uint32_t(0x8E8E93));
}

/*
 * SwipeReveal through swipes on rows with every mix of actions. The row follows the finger up
 * to its actions and never past where a swipe may go, never moves toward a side without actions,
 * rests closed, open on a side or swiped out, picks up from where it rested, and the action
 * buttons always fill the gap the row leaves.
 */
TEST(fuzz_swipe_rests_closed_open_or_swiped_out_and_its_buttons_fill_the_gap) {
  const FuzzKinds kinds = {
    {"drag", "release closed", "release open", "release full"},
    {},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    Trail trail(seed, episode);
    std::mt19937 rng(seed);
    SwipeReveal swipe;
    SwipeSpec spec;
    spec.rowSize = 240.0 + pickPercent(rng) * 3.0;
    spec.leadingWidth = chance(rng, 30) ? 0.0 : 74.0 * (1 + pickInt(rng, 0, 2));
    spec.trailingWidth = chance(rng, 30) ? 0.0 : 74.0 * (1 + pickInt(rng, 0, 2));
    spec.leadingFullSwipe = chance(rng, 50);
    spec.trailingFullSwipe = chance(rng, 50);
    trail.note(
      "row " + std::to_string(spec.rowSize) + " leading " + std::to_string(spec.leadingWidth) +
      (spec.leadingFullSwipe ? " full" : "") + " trailing " + std::to_string(spec.trailingWidth) +
      (spec.trailingFullSwipe ? " full" : ""));
    double resting = 0.0;

    for (int gesture = 0; gesture < 6; ++gesture) {
      swipe.begin(spec, resting);
      if (std::fabs(swipe.drag(0.0) - resting) > 1e-9) {
        trail.failWith(
          "a touch moved the row from " + std::to_string(resting) + " to " + std::to_string(swipe.drag(0.0)));
      }
      double translation = 0.0;
      double offset = resting;
      int frames = pickInt(rng, 2, 7);
      for (int frame = 0; frame < frames; ++frame) {
        episode.reach("drag");
        double before = swipe.drag(translation);
        double delta = (pickPercent(rng) - 50) * 6.0;
        translation += delta;
        offset = swipe.drag(translation);
        trail.note("drag by " + std::to_string(translation) + " shows " + std::to_string(offset));
        // The side the finger is on decides which actions show.
        double raw = resting + translation;
        double width = raw > 0.0 ? spec.leadingWidth : spec.trailingWidth;
        bool full = raw > 0.0 ? spec.leadingFullSwipe : spec.trailingFullSwipe;
        if ((offset - before) * delta < 0.0) {
          trail.failWith("the row moved against the finger");
        }
        if (offset != 0.0 && (width <= 0.0 || offset * raw < 0.0 || std::fabs(offset) > std::fabs(raw) + 1e-9)) {
          trail.failWith("the row shows " + std::to_string(offset) + " for a finger at " + std::to_string(raw));
        }
        if (width > 0.0 && std::fabs(raw) <= width && std::fabs(offset - raw) > 1e-9) {
          trail.failWith("the row left the finger inside its actions");
        }
        if (std::fabs(offset) > (full ? std::max(spec.rowSize, width) : std::max(width, std::fabs(raw))) + 1e-9) {
          trail.failWith("the row went further than a swipe may");
        }
        // The buttons of the side shown tile the gap from the outer edge inward.
        std::vector<double> sizes(pickSize(rng, 1, 3), width > 0.0 ? width / 2.0 : 0.0);
        std::vector<SwipeSpan> spans;
        bool past = swipe.isPastFullSwipe(offset);
        swipeButtonSpans(sizes, offset, past, spec.rowSize, spans);
        SwipeSpan gap = swipeRevealedSpan(offset, spec.rowSize);
        double filled = 0.0;
        for (std::size_t at = 0; at < spans.size(); ++at) {
          double outer = offset > 0.0 ? spans[at].start : spec.rowSize - spans[at].start - spans[at].size;
          if (spans[at].size < 0.0 || std::fabs(outer - filled) > 1e-6) {
            trail.failWith("button " + std::to_string(at) + " does not follow the one before it");
          }
          filled += spans[at].size;
        }
        if (width > 0.0 && std::fabs(filled - gap.size) > 1e-6) {
          trail.failWith("the buttons fill " + std::to_string(filled) + " of a gap of " + std::to_string(gap.size));
        }
        if (past && (spans.empty() || std::fabs(spans.front().size - gap.size) > 1e-6)) {
          trail.failWith("the first button does not fill the gap of a full swipe");
        }
      }
      double velocity = (pickPercent(rng) - 50) * 20.0;
      SwipeRest rest = swipe.settle(offset, velocity, SWIPE_FLING_VELOCITY);
      episode.reach(rest.full ? "release full" : rest.side == SwipeSide::None ? "release closed" : "release open");
      trail.note(
        "release at " + std::to_string(offset) + " v=" + std::to_string(velocity) + " rests at " +
        std::to_string(rest.offset));
      episode.observe(rest.offset);
      bool leading = rest.side == SwipeSide::Leading;
      if (rest.side == SwipeSide::None) {
        if (rest.offset != 0.0 || rest.full) {
          trail.failWith("a closed row rests at " + std::to_string(rest.offset));
        }
      } else if ((leading ? spec.leadingWidth : spec.trailingWidth) <= 0.0 || (offset > 0.0) != leading) {
        trail.failWith("the row opened a side it has no actions on or was not swiped to");
      } else if (rest.full) {
        if (
          !(leading ? spec.leadingFullSwipe : spec.trailingFullSwipe) || !swipe.isPastFullSwipe(offset) ||
          !swipe.isSwipedOut(rest.offset)) {
          trail.failWith("a full swipe the side does not allow, or one that does not slide the row out");
        }
      } else if (rest.offset != swipe.openOffset(rest.side) || swipe.isPastFullSwipe(offset)) {
        trail.failWith("an open row rests at " + std::to_string(rest.offset) + " off its actions");
      }
      // A row swiped out is gone with its action. The next row starts closed.
      resting = rest.full ? 0.0 : rest.offset;
    }
  });
}
