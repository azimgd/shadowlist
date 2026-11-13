/*
 * Randomized host simulation along the other axes: horizontal lists, grids and masonry with
 * uneven tracks on an AxisHost. See tests_fuzz.cpp for how the fuzz tests run.
 */

#include "AxisHostHelpers.hpp"
#include "FuzzHelpers.hpp"
#include "TestFramework.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace azimgd::shadowlist;
using namespace slt;

/*
 * Horizontal lists: the same holds along x.
 */
TEST(fuzz_horizontal_changes_hold_the_first_visible_column) {
  const FuzzKinds kinds = {
    {"prepend", "append", "insert", "delete", "scroll",
      "core maintain anchor"},
    {"core shrink clamp"},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    AxisHost host(seed, episode);
    host.horizontal = true;
    host.header = chance(host.rng, 50) ? 0.0 : 80.0;
    axisOpen(host, 20 + pickSize(host.rng, 0, 99));
    for (int step = 0; step < 10; ++step) {
      int roll = pickPercent(host.rng);
      std::size_t count = pickSize(host.rng, 1, 8);
      if (roll < 30) {
        episode.reach("prepend");
        axisChangeAndHold(host, withPrepend(host, count), "prepend " + std::to_string(count));
      } else if (roll < 50) {
        episode.reach("append");
        axisChangeAndHold(host, withAppend(host, count), "append " + std::to_string(count));
      } else if (roll < 65) {
        episode.reach("insert");
        std::size_t at = pickSize(host.rng, 0, host.keys.size());
        axisChangeAndHold(host, withInsert(host, at, count),
          "insert " + std::to_string(count) + " at " + std::to_string(at));
      } else if (roll < 80) {
        episode.reach("delete");
        std::size_t at = pickSize(host.rng, 0, host.keys.size() - 1);
        axisChangeAndHold(host, withRemove(host, at, count),
          "remove " + std::to_string(count) + " at " + std::to_string(at));
      } else {
        episode.reach("scroll");
        axisScrollTo(host, pickFraction(host.rng) * host.maxOffset());
      }
      if (host.keys.empty()) {
        break;
      }
    }
  });
}

/*
 * Grids: changes by whole rows of tracks hold the top row. The geometry stays contiguous
 * per track for any change.
 */
TEST(fuzz_grid_changes_by_whole_rows_hold_the_top_row) {
  const FuzzKinds kinds = {
    {"prepend", "append", "ragged delete", "scroll",
      "core maintain anchor"},
    {"core shrink clamp"},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    AxisHost host(seed, episode);
    host.numberOfColumns = pickSize(host.rng, 2, 4);
    host.header = chance(host.rng, 50) ? 0.0 : 80.0;
    axisOpen(host, 30 + pickSize(host.rng, 0, 99));
    for (int step = 0; step < 10; ++step) {
      int roll = pickPercent(host.rng);
      std::size_t count = host.numberOfColumns * pickSize(host.rng, 1, 4);
      if (roll < 35) {
        episode.reach("prepend");
        axisChangeAndHold(host, withPrepend(host, count), "prepend " + std::to_string(count));
      } else if (roll < 55) {
        episode.reach("append");
        axisChangeAndHold(host, withAppend(host, count), "append " + std::to_string(count));
      } else if (roll < 70) {
        episode.reach("ragged delete");
        // A ragged change only has to keep the geometry sound.
        std::size_t at = pickSize(host.rng, 0, host.keys.size() - 1);
        host.keys = withRemove(host, at, 1);
        host.note("ragged remove at " + std::to_string(at));
        axisCommit(host, false);
        axisRest(host);
      } else {
        episode.reach("scroll");
        axisScrollTo(host, pickFraction(host.rng) * host.maxOffset());
      }
      if (host.keys.empty()) {
        break;
      }
    }
  });
}

/*
 * Flings through grids, masonry with uneven tracks, and horizontal lists, plain or inverted.
 * Every frame mounts what is on screen, and at rest the measured range alone covers it.
 */
TEST(fuzz_grid_and_horizontal_flings_never_blank) {
  const FuzzKinds kinds = {
    {"fling forward", "fling back",
      "core bottom pin"},
    {"core shrink clamp"},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    AxisHost host(seed, episode);
    int shape = pickInt(host.rng, 0, 2);
    host.numberOfColumns = shape == 0 ? pickSize(host.rng, 2, 4) : 1;
    host.horizontal = shape == 1;
    host.inverted = shape == 2 || (shape == 1 && chance(host.rng, 30));
    host.header = chance(host.rng, 50) ? 0.0 : 80.0;
    axisOpen(host, 150 + pickSize(host.rng, 0, 99) * 3);
    if (host.inverted && std::fabs(host.hostOffset - host.maxOffset()) > FUZZ_TOLERANCE) {
      host.failWith("inverted list did not open at its end: off=" + std::to_string(host.hostOffset) +
        " max=" + std::to_string(host.maxOffset()));
    }
    for (int fling = 0; fling < 4; ++fling) {
      double velocity = 1200.0 + pickPercent(host.rng) * 40.0;
      if (chance(host.rng, 50)) {
        velocity = -velocity;
      }
      episode.reach(velocity < 0.0 ? "fling back" : "fling forward");
      host.note("fling v=" + std::to_string(velocity) + " from " + std::to_string(host.hostOffset));
      while (std::fabs(velocity) > 20.0) {
        double maxOffset = host.maxOffset();
        double next = std::min(std::max(host.hostOffset + velocity, 0.0), maxOffset);
        host.hostOffset = next;
        host.phase = ScrollPhase::Settling;
        host.userScrolled = true;
        axisCommit(host, false);
        /*
         * Masonry tracks drift apart as rows get measured. A layout pass that shrinks one
         * track can pull more of it into view than the pad holds, until the next frame picks
         * the measured range again. Only single track lists are judged mid fling.
         * A pass that measured rows is judged after the core's next pass, see
         * fuzz_flings_never_blank_or_jump.
         */
        bool judged = host.numberOfColumns <= 1;
        std::string coverage = judged && !host.layoutResized ? checkMounted(host, host.hostOffset, host.mountPad) : "";
        if (!coverage.empty()) {
          host.failWith("blank during fling: " + coverage);
        }
        axisCommit(host, true);
        coverage = judged ? checkMounted(host, host.hostOffset, host.mountPad) : "";
        if (!coverage.empty()) {
          host.failWith("blank during fling after mount: " + coverage);
        }
        if (next <= 0.0 || next >= maxOffset) {
          break;
        }
        velocity *= 0.9;
      }
      host.phase = ScrollPhase::Idle;
      host.userScrolled = false;
      axisRest(host);
      std::string coverage = checkMounted(host, host.hostOffset, 0);
      if (!coverage.empty()) {
        host.failWith("blank at rest: " + coverage);
      }
      if (host.hostOffset > host.maxOffset() + FUZZ_TOLERANCE) {
        host.failWith("rests past the end after a fling");
      }
    }
  });
}

/*
 * Scroll commands on grids, masonry and horizontal lists land where they aim and settle,
 * with data changes in between.
 */
TEST(fuzz_grid_and_horizontal_scroll_commands_land) {
  const FuzzKinds kinds = {
    {"command to row", "command to end", "command to start", "prepend", "append",
      "core maintain anchor", "core scroll to key", "core scroll to start", "core scroll to end"},
    {},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    AxisHost host(seed, episode);
    int shape = pickInt(host.rng, 0, 1);
    host.numberOfColumns = shape == 0 ? pickSize(host.rng, 2, 4) : 1;
    host.horizontal = shape == 1;
    host.header = chance(host.rng, 50) ? 0.0 : 80.0;
    axisOpen(host, 60 + pickSize(host.rng, 0, 99) * 2);
    for (int step = 0; step < 8; ++step) {
      int roll = pickPercent(host.rng);
      if (roll < 50) {
        episode.reach("command to row");
        std::size_t index = pickSize(host.rng, 0, host.keys.size() - 1);
        double viewPosition = pickInt(host.rng, 0, 2) * 0.5;
        std::string key = host.keys[index];
        host.note("scrollToRow " + std::to_string(index) + " pos " + std::to_string(viewPosition));
        host.container.scrollToRow(index, viewPosition);
        axisCommit(host, false);
        if (axisSettle(host) < 0) {
          host.failWith("scrollToRow never settled");
        }
        double wanted = landingOf(host, key, viewPosition);
        if (std::fabs(host.hostOffset - wanted) > FUZZ_TOLERANCE) {
          host.failWith("scrollToRow landed at " + std::to_string(host.hostOffset) + " wanted " +
            std::to_string(wanted));
        }
        std::string coverage = checkMounted(host, host.hostOffset, 0);
        if (!coverage.empty()) {
          host.failWith("blank after scrollToRow: " + coverage);
        }
      } else if (roll < 65) {
        episode.reach("command to end");
        host.note("scrollToEnd");
        host.container.scrollToEnd();
        axisCommit(host, false);
        if (axisSettle(host) < 0) {
          host.failWith("scrollToEnd never settled");
        }
        if (std::fabs(host.hostOffset - host.maxOffset()) > FUZZ_TOLERANCE) {
          host.failWith("scrollToEnd landed at " + std::to_string(host.hostOffset) + " max " +
            std::to_string(host.maxOffset()));
        }
      } else if (roll < 80) {
        episode.reach("command to start");
        host.note("scrollToStart");
        host.container.scrollToStart();
        axisCommit(host, false);
        if (axisSettle(host) < 0) {
          host.failWith("scrollToStart never settled");
        }
        if (host.hostOffset > FUZZ_TOLERANCE) {
          host.failWith("scrollToStart landed at " + std::to_string(host.hostOffset));
        }
      } else {
        std::size_t count = host.numberOfColumns * pickSize(host.rng, 1, 4);
        bool prepend = chance(host.rng, 50);
        episode.reach(prepend ? "prepend" : "append");
        axisChangeAndHold(host, prepend ? withPrepend(host, count) : withAppend(host, count),
          "change " + std::to_string(count));
      }
    }
  });
}
