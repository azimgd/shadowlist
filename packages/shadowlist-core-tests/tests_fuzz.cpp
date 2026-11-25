/*
 * Randomized host simulation. A small model of the Fabric host drives the core through
 * random data changes, gestures, header and window changes and scroll commands, and checks
 * after every frame that the layout is consistent, the measured range covers the viewport, every
 * correction settles, and the row the reader was looking at stays where it was.
 * Each test runs many seeds. A failure prints the seed and the steps that led to it. It
 * can be turned into a plain test.
 * FuzzHelpers.hpp holds the seed loop and the environment variables that trace a seed or run
 * a campaign over more seeds. SimHostHelpers.hpp has the host and what the tests do to it.
 * The host here is an ideal one on a vertical list of one column. tests_fuzz_axis.cpp has the
 * other axes and tests_fuzz_faults.cpp the hosts that stray the way real hosts do.
 */

#include "FuzzHelpers.hpp"
#include "SimHostHelpers.hpp"
#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace azimgd::shadowlist;
using namespace slt;

/*
 * A plain vertical feed: prepends, appends, inserts and removes above, inside and below the
 * viewport, at random resting offsets. The first row on screen never moves.
 */
TEST(fuzz_idle_data_changes_hold_the_first_visible_row) {
  const FuzzKinds kinds = {
    {"prepend", "append", "insert", "delete", "refresh", "insert in view", "scroll", "core maintain anchor",
     "core shrink clamp"},
    {},
  };
  forEachFuzzSeed(__func__, 300, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    SimHost host(seed, episode);
    host.header = chance(host.rng, 50) ? 0.0 : 120.0 + pickPercent(host.rng);
    host.footer = chance(host.rng, 70) ? 0.0 : 60.0;
    open(host, 20 + pickSize(host.rng, 0, 99));

    for (int step = 0; step < 12; ++step) {
      int roll = pickPercent(host.rng);
      std::size_t count = pickSize(host.rng, 1, 12);
      std::size_t size = host.keys.size();
      auto visible = host.container.getMeasuredRange();
      std::size_t visLow = visible.low;
      std::size_t visHigh = visible.high;
      if (roll < 25) {
        episode.reach("prepend");
        changeAndHold(host, withPrepend(host, count), "prepend " + std::to_string(count));
      } else if (roll < 40) {
        episode.reach("append");
        changeAndHold(host, withAppend(host, count), "append " + std::to_string(count));
      } else if (roll < 55) {
        episode.reach("insert");
        std::size_t at = pickSize(host.rng, 0, size);
        changeAndHold(
          host, withInsert(host, at, count), "insert " + std::to_string(count) + " at " + std::to_string(at));
      } else if (roll < 70) {
        episode.reach("delete");
        std::size_t at = pickSize(host.rng, 0, size - 1);
        changeAndHold(
          host, withRemove(host, at, count), "remove " + std::to_string(count) + " at " + std::to_string(at));
      } else if (roll < 80 && visLow != UNDEFINED_INDEX) {
        episode.reach("refresh");
        // Remove the row at the top of the screen along with a prepend, like a refresh.
        std::vector<std::string> next = withRemove(host, visLow, 1);
        std::vector<std::string> made = host.makeKeys(count);
        next.insert(next.begin(), made.begin(), made.end());
        changeAndHold(host, next, "refresh dropping " + std::to_string(visLow));
      } else if (roll < 88 && visHigh != UNDEFINED_INDEX) {
        episode.reach("insert in view");
        // Insert right inside the viewport, below the first row.
        std::size_t at = std::min(size, visLow + 1);
        changeAndHold(host, withInsert(host, at, count), "insert in view at " + std::to_string(at));
      } else {
        episode.reach("scroll");
        double target = pickFraction(host.rng) * host.maxOffset();
        scrollTo(host, target);
      }
      if (host.keys.empty()) {
        break;
      }
    }
  });
}

/*
 * An inverted chat: history pages prepended, incoming rows appended, with and without
 * followAppends, from the bottom and from up in the history.
 */
TEST(fuzz_inverted_history_and_incoming_hold_the_reader) {
  const FuzzKinds kinds = {
    {"history", "incoming", "delete", "scroll", "scroll to end", "core maintain anchor", "core scroll to end",
     "core bottom pin"},
    {},
  };
  forEachFuzzSeed(__func__, 300, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    SimHost host(seed, episode);
    host.inverted = true;
    host.followAppends = chance(host.rng, 50);
    host.header = chance(host.rng, 60) ? 0.0 : 48.0;
    open(host, pickSize(host.rng, 15, 54));
    if (std::fabs(host.hostOffset - host.maxOffset()) > FUZZ_TOLERANCE) {
      host.failWith(
        "inverted list did not open at the bottom: off=" + std::to_string(host.hostOffset) +
        " max=" + std::to_string(host.maxOffset()));
    }

    for (int step = 0; step < 12; ++step) {
      int roll = pickPercent(host.rng);
      std::size_t count = pickSize(host.rng, 1, 10);
      if (roll < 30) {
        episode.reach("history");
        changeAndHold(host, withPrepend(host, count), "history " + std::to_string(count));
      } else if (roll < 60) {
        episode.reach("incoming");
        changeAndHold(host, withAppend(host, count), "incoming " + std::to_string(count));
      } else if (roll < 70) {
        episode.reach("delete");
        std::size_t at = pickSize(host.rng, 0, host.keys.size() - 1);
        changeAndHold(host, withRemove(host, at, 1), "delete " + std::to_string(at));
      } else if (roll < 85) {
        episode.reach("scroll");
        double target = pickFraction(host.rng) * host.maxOffset();
        scrollTo(host, target);
      } else {
        episode.reach("scroll to end");
        scrollTo(host, host.maxOffset());
      }
      if (host.keys.empty()) {
        break;
      }
    }
  });
}

/*
 * Header and footer resize while the reader rests. Off screen headers keep the rows still,
 * a visible header pushes them by its growth. The footer never moves a row.
 */
TEST(fuzz_header_and_footer_changes_keep_the_rows) {
  const FuzzKinds kinds = {
    {"scroll", "footer", "header", "core shrink clamp"},
    {},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    SimHost host(seed, episode);
    host.header = 100.0;
    open(host, 40);
    for (int step = 0; step < 10; ++step) {
      if (chance(host.rng, 40)) {
        episode.reach("scroll");
        double target = pickFraction(host.rng) * host.maxOffset();
        scrollTo(host, target);
        continue;
      }
      ScreenRow row = firstVisibleRow(host);
      double offsetBefore = host.hostOffset;
      double previousHeader = host.header;
      bool footerChange = chance(host.rng, 30);
      if (footerChange) {
        episode.reach("footer");
        host.footer = host.footer > 0.0 ? 0.0 : 80.0;
        host.note("footer -> " + std::to_string(host.footer));
      } else {
        episode.reach("header");
        host.header = pickPercent(host.rng) * 2.0;
        host.note(
          "header " + std::to_string(previousHeader) + " -> " + std::to_string(host.header) +
          " off=" + std::to_string(host.hostOffset));
      }
      // The header changes in a layout pass with no data change. Run just that and settle.
      host.commit(false);
      checkFrame(host);
      if (host.settle() < 0) {
        host.failWith("header change never settled");
      }
      checkFrame(host, true);
      if (row.key.empty()) {
        continue;
      }
      double after = screenOf(host, row.key);
      double delta = host.header - previousHeader;
      bool headerOffScreen = offsetBefore > 0.0 && offsetBefore >= previousHeader;
      double expected = footerChange || headerOffScreen ? row.screen : row.screen + delta;
      bool clamped = isHoldClamped(host, row.key, expected);
      if (std::fabs(after - expected) > FUZZ_TOLERANCE && !clamped) {
        host.failWith(
          "row " + row.key + " at " + std::to_string(after) + " expected " + std::to_string(expected) + " (host " +
          std::to_string(host.hostOffset) + ")");
      }
    }
  });
}

/*
 * Scroll commands land where they aim and settle, with data changes in between.
 */
TEST(fuzz_scroll_commands_land_and_settle) {
  const FuzzKinds kinds = {
    {"command to row", "command to end", "command to start", "prepend", "append", "core maintain anchor",
     "core scroll to key", "core scroll to start", "core scroll to end", "core bottom pin"},
    {},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    SimHost host(seed, episode);
    host.inverted = chance(host.rng, 30);
    host.header = chance(host.rng, 50) ? 0.0 : 90.0;
    open(host, 30 + pickSize(host.rng, 0, 99));
    for (int step = 0; step < 8; ++step) {
      int roll = pickPercent(host.rng);
      if (roll < 40) {
        episode.reach("command to row");
        std::size_t index = pickSize(host.rng, 0, host.keys.size() - 1);
        double viewPosition = pickInt(host.rng, 0, 2) * 0.5;
        std::string key = host.keys[index];
        host.note("scrollToRow " + std::to_string(index) + " pos " + std::to_string(viewPosition));
        host.container.scrollToRow(index, viewPosition);
        host.commit(false);
        if (host.settle() < 0) {
          host.failWith("scrollToRow never settled");
        }
        checkFrame(host, true);
        double wanted = landingOf(host, key, viewPosition);
        if (std::fabs(host.hostOffset - wanted) > FUZZ_TOLERANCE) {
          host.failWith(
            "scrollToRow landed at " + std::to_string(host.hostOffset) + " wanted " + std::to_string(wanted));
        }
      } else if (roll < 55) {
        episode.reach("command to end");
        scrollToEndAndLand(host);
      } else if (roll < 70) {
        episode.reach("command to start");
        scrollToStartAndLand(host);
      } else if (roll < 85) {
        episode.reach("prepend");
        std::size_t count = pickSize(host.rng, 1, 8);
        changeAndHold(host, withPrepend(host, count), "prepend " + std::to_string(count));
      } else {
        episode.reach("append");
        std::size_t count = pickSize(host.rng, 1, 8);
        changeAndHold(host, withAppend(host, count), "append " + std::to_string(count));
      }
    }
  });
}

/*
 * The whole dataset is swapped, shrunk or emptied. The list must never end up past its end,
 * never leave the viewport uncovered, and an emptied inverted list must pin again when rows return.
 */
TEST(fuzz_dataset_swaps_and_shrinks_stay_in_range) {
  const FuzzKinds kinds = {
    {"scroll", "shrink", "swap", "refill", "append", "core bottom pin", "core shrink clamp"},
    {"core maintain anchor"},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    SimHost host(seed, episode);
    host.inverted = chance(host.rng, 40);
    open(host, 30 + pickSize(host.rng, 0, 99));
    for (int step = 0; step < 8; ++step) {
      int roll = pickPercent(host.rng);
      if (roll < 30) {
        episode.reach("scroll");
        double target = pickFraction(host.rng) * host.maxOffset();
        scrollTo(host, target);
      } else if (roll < 55) {
        episode.reach("shrink");
        std::size_t keep = pickSize(host.rng, 0, 5);
        std::vector<std::string> next(
          host.keys.begin(), host.keys.begin() + static_cast<std::ptrdiff_t>(std::min(keep, host.keys.size())));
        host.keys = next;
        host.note("shrink to " + std::to_string(keep) + " off=" + std::to_string(host.hostOffset));
        host.commit(false);
        checkFrame(host);
        if (host.settle() < 0) {
          host.failWith("shrink never settled");
        }
        checkFrame(host, true);
        if (host.hostOffset > host.maxOffset() + FUZZ_TOLERANCE) {
          host.failWith(
            "rests past the end after shrink: off=" + std::to_string(host.hostOffset) +
            " max=" + std::to_string(host.maxOffset()));
        }
      } else if (roll < 80) {
        episode.reach("swap");
        std::vector<std::string> next = host.makeKeys(pickSize(host.rng, 10, 49));
        host.keys = next;
        host.note("swap to " + std::to_string(next.size()) + " off=" + std::to_string(host.hostOffset));
        host.commit(false);
        checkFrame(host);
        if (host.settle() < 0) {
          host.failWith("swap never settled");
        }
        checkFrame(host, true);
        if (host.hostOffset > host.maxOffset() + FUZZ_TOLERANCE) {
          host.failWith("rests past the end after swap: off=" + std::to_string(host.hostOffset));
        }
        if (host.inverted && host.hostOffset < host.maxOffset() - FUZZ_TOLERANCE) {
          host.failWith(
            "inverted swap did not rest at the bottom: off=" + std::to_string(host.hostOffset) +
            " max=" + std::to_string(host.maxOffset()));
        }
      } else {
        std::size_t count = pickSize(host.rng, 1, 8);
        if (host.keys.empty()) {
          episode.reach("refill");
          host.keys = host.makeKeys(count);
          host.note("refill " + std::to_string(count));
          host.commit(false);
          if (host.settle() < 0) {
            host.failWith("refill never settled");
          }
          checkFrame(host);
        } else {
          episode.reach("append");
          changeAndHold(host, withAppend(host, count), "append " + std::to_string(count));
        }
      }
    }
  });
}

/*
 * Data lands while the finger is down or the list is settling. The row the reader held is
 * where it was minus how far they scrolled since.
 */
TEST(fuzz_changes_during_gestures_follow_the_finger) {
  const FuzzKinds kinds = {
    {"prepend", "append", "insert", "core maintain anchor", "core bottom pin"},
    {"core shrink clamp"},
  };
  forEachFuzzSeed(__func__, 300, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    SimHost host(seed, episode);
    host.inverted = chance(host.rng, 30);
    host.header = chance(host.rng, 50) ? 0.0 : 100.0;
    open(host, 40 + pickSize(host.rng, 0, 99));
    // Start somewhere in the middle so both directions have room.
    scrollTo(host, host.maxOffset() * 0.5);

    for (int gesture = 0; gesture < 4; ++gesture) {
      // The rows on screen when the data lands, and the travel since then.
      std::vector<ScreenRow> held;
      double travel = 0.0;
      int frames = 3 + pickInt(host.rng, 0, 5);
      int changeAt = pickInt(host.rng, 0, frames - 1);
      double perFrame = (pickPercent(host.rng) - 50.0) * 4.0;
      bool changed = false;
      std::string what;
      for (int frame = 0; frame < frames; ++frame) {
        double next = std::min(std::max(host.hostOffset + perFrame, 0.0), host.maxOffset());
        if (changed) {
          travel += next - host.hostOffset;
        }
        host.hostOffset = next;
        host.phase = frame < frames - 2 ? ScrollPhase::Dragging : ScrollPhase::Settling;
        host.userScrolled = true;
        if (frame == changeAt) {
          held = visibleRows(host);
          std::size_t count = pickSize(host.rng, 1, 8);
          int roll = pickPercent(host.rng);
          if (roll < 50) {
            episode.reach("prepend");
            host.keys = withPrepend(host, count);
            what = "prepend " + std::to_string(count);
          } else if (roll < 75) {
            episode.reach("append");
            host.keys = withAppend(host, count);
            what = "append " + std::to_string(count);
          } else {
            episode.reach("insert");
            std::size_t at = pickSize(host.rng, 0, host.keys.size() - 1);
            host.keys = withInsert(host, at, count);
            what = "insert " + std::to_string(count) + " at " + std::to_string(at);
          }
          host.note(
            what + " mid gesture at off=" + std::to_string(host.hostOffset) + " travel=" + std::to_string(travel));
          changed = true;
        } else {
          host.note("gesture frame off=" + std::to_string(host.hostOffset));
        }
        host.commit(false);
        checkFrame(host);
        // The state the pass produced mounts while the finger keeps moving.
        host.commit(true);
        checkFrame(host);
      }
      host.phase = ScrollPhase::Idle;
      host.userScrolled = false;
      host.commit(false);
      checkFrame(host);
      int settled = host.settle();
      if (settled < 0) {
        host.failWith(what + ": correction never settled after the gesture");
      }
      checkFrame(host, true);
      if (!changed) {
        continue;
      }
      for (const ScreenRow& row : held) {
        double after = screenOf(host, row.key);
        if (std::isnan(after)) {
          continue;
        }
        double expected = row.screen - travel;
        if (std::fabs(after - expected) <= FUZZ_TOLERANCE) {
          break;
        }
        if (isHoldClamped(host, row.key, expected)) {
          break;
        }
        host.failWith(
          what + ": row " + row.key + " at " + std::to_string(after) + " expected " + std::to_string(expected) +
          " (travel " + std::to_string(travel) + ", host " + std::to_string(host.hostOffset) + ", max " +
          std::to_string(host.maxOffset()) + ")");
      }
    }
  });
}

/*
 * Decoration rows, like date pills, are never the anchor. Changes that add or remove them
 * still hold the nearest content row.
 */
TEST(fuzz_non_anchorable_rows_never_hold_and_content_rows_do) {
  const FuzzKinds kinds = {
    {"prepend with pills", "append with pills", "drop pills", "scroll", "core maintain anchor", "core bottom pin",
     "core shrink clamp"},
    {},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    SimHost host(seed, episode);
    host.inverted = chance(host.rng, 50);
    open(host, pickSize(host.rng, 30, 59));
    std::vector<std::string> pills;
    auto addPills = [&](std::vector<std::string> keys) {
      std::vector<std::string> next;
      for (std::size_t index = 0; index < keys.size(); ++index) {
        if (chance(host.rng, 20)) {
          std::string pill = "pill" + std::to_string(host.nextKey++);
          host.trueSizes[pill] = 28.0;
          pills.push_back(pill);
          next.push_back(pill);
        }
        next.push_back(keys[index]);
      }
      return next;
    };
    for (int step = 0; step < 8; ++step) {
      int roll = pickPercent(host.rng);
      std::size_t count = pickSize(host.rng, 1, 6);
      std::vector<std::string> next;
      std::string what;
      if (roll < 40) {
        episode.reach("prepend with pills");
        next = addPills(withPrepend(host, count));
        what = "prepend with pills";
      } else if (roll < 60) {
        episode.reach("append with pills");
        next = addPills(withAppend(host, count));
        what = "append with pills";
      } else if (roll < 75 && !pills.empty()) {
        episode.reach("drop pills");
        // Drop every pill, as an unread divider disappears.
        for (const std::string& key : host.keys) {
          if (key.rfind("pill", 0) != 0) {
            next.push_back(key);
          }
        }
        pills.clear();
        what = "drop pills";
      } else {
        episode.reach("scroll");
        double target = pickFraction(host.rng) * host.maxOffset();
        scrollTo(host, target);
        continue;
      }
      // Pills are non anchorable from now on. Feed the list through the input each frame.
      struct PillGuard {
        SimHost& host;
        std::vector<std::string> saved;
      };
      std::vector<ScreenRow> before = visibleRows(host);
      host.keys = next;
      host.note(what + " n=" + std::to_string(next.size()));
      // Run the frames with the pill keys marked, the way the screen passes nonAnchorKeys.
      auto framed = [&](bool ownWrite) {
        if (!ownWrite) {
          host.stateOffset = host.hostOffset;
        }
        FrameInput frame = host.input(ownWrite);
        frame.nonAnchorKeys = pills;
        Virtualizer::update(host.container, frame);
        host.layoutPass();
        bool corrected = host.container.state.offsetCorrected;
        host.mountState();
        host.observeCommit();
        if (!ownWrite) {
          host.userScrolled = false;
        }
        return corrected;
      };
      framed(false);
      int settled = -1;
      for (int frame = 0; frame < FUZZ_MAX_SETTLE_FRAMES; ++frame) {
        bool corrected = framed(true);
        bool reported = framed(false);
        if (!corrected && !reported && !host.container.state.operation && !host.container.state.pendingScrollToEnd) {
          settled = frame;
          break;
        }
      }
      if (settled < 0) {
        host.failWith(what + ": never settled");
      }
      checkFrame(host, true);
      const std::string& anchorKey = host.container.state.anchor.key;
      if (
        host.container.isAnchorable(anchorKey) == false && !anchorKey.empty() &&
        host.container.state.anchor.key.rfind("pill", 0) == 0) {
        // Only allowed when nothing else is on screen.
        bool contentVisible = false;
        for (const ScreenRow& row : visibleRows(host)) {
          if (row.key.rfind("pill", 0) != 0) {
            contentVisible = true;
          }
        }
        if (contentVisible) {
          host.failWith(what + ": a pill became the anchor");
        }
      }
      for (const ScreenRow& row : before) {
        if (row.key.rfind("pill", 0) == 0) {
          continue;
        }
        double after = screenOf(host, row.key);
        if (std::isnan(after)) {
          continue;
        }
        if (std::fabs(after - row.screen) <= FUZZ_TOLERANCE) {
          break;
        }
        if (isHoldClamped(host, row.key, row.screen)) {
          break;
        }
        if (host.inverted && std::fabs(host.hostOffset - host.maxOffset()) <= FUZZ_TOLERANCE) {
          break;
        }
        host.failWith(
          what + ": content row " + row.key + " moved from " + std::to_string(row.screen) + " to " +
          std::to_string(after));
      }
    }
  });
}

/*
 * The window along the scroll axis resizes, like a keyboard or composer opening and closing.
 * An inverted list at its bottom stays at the bottom, one scrolled up keeps its row, and a
 * plain list keeps its row too. Nothing is left past the end.
 */
TEST(fuzz_window_resizes_keep_the_reader) {
  const FuzzKinds kinds = {
    {"scroll", "append", "resize", "core bottom pin", "core shrink clamp"},
    {},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    SimHost host(seed, episode);
    host.inverted = chance(host.rng, 60);
    open(host, pickSize(host.rng, 30, 59));
    for (int step = 0; step < 8; ++step) {
      int roll = pickPercent(host.rng);
      if (roll < 35) {
        episode.reach("scroll");
        double target = chance(host.rng, 40) ? host.maxOffset() : pickFraction(host.rng) * host.maxOffset();
        scrollTo(host, target);
        continue;
      }
      if (roll < 55) {
        episode.reach("append");
        changeAndHold(host, withAppend(host, pickSize(host.rng, 1, 4)), "append");
        continue;
      }
      double previousWindow = host.windowSize;
      bool atBottom = host.inverted && host.hostOffset >= host.maxOffset() - INVERTED_FOLLOW_BAND &&
                      !host.container.state.invertedBottomReleased;
      ScreenRow row = firstVisibleRow(host);
      host.windowSize = chance(host.rng, 50) ? WINDOW_HEIGHT : WINDOW_HEIGHT - 300.0 - pickPercent(host.rng);
      if (host.windowSize == previousWindow) {
        continue;
      }
      episode.reach("resize");
      host.note(
        "window " + std::to_string(previousWindow) + " -> " + std::to_string(host.windowSize) +
        " off=" + std::to_string(host.hostOffset) + (atBottom ? " at bottom" : ""));
      // The resize comes through the layout pass, like the Fabric host.
      host.commit(false);
      checkFrame(host);
      if (host.settle() < 0) {
        host.failWith("window resize never settled");
      }
      checkFrame(host, true);
      if (host.hostOffset > host.maxOffset() + FUZZ_TOLERANCE) {
        host.failWith(
          "rests past the end after a resize: off=" + std::to_string(host.hostOffset) +
          " max=" + std::to_string(host.maxOffset()));
      }
      if (atBottom) {
        if (std::fabs(host.hostOffset - host.maxOffset()) > FUZZ_TOLERANCE) {
          host.failWith(
            "inverted list left the bottom on a resize: off=" + std::to_string(host.hostOffset) +
            " max=" + std::to_string(host.maxOffset()));
        }
        continue;
      }
      if (row.key.empty()) {
        continue;
      }
      double after = screenOf(host, row.key);
      if (std::isnan(after) || std::fabs(after - row.screen) <= FUZZ_TOLERANCE) {
        continue;
      }
      if (isHoldClamped(host, row.key, row.screen)) {
        continue;
      }
      host.failWith(
        "row " + row.key + " moved from " + std::to_string(row.screen) + " to " + std::to_string(after) +
        " on a resize (host " + std::to_string(host.hostOffset) + ")");
    }
  });
}

/*
 * Predicted sizes arrive for rows not yet measured, some for keys not in the list yet. They
 * must never move the row on screen, always be replaced by the real measurement, and never
 * leave a stale prediction behind after invalidation.
 */
TEST(fuzz_predicted_sizes_never_move_the_reader) {
  const FuzzKinds kinds = {
    {"scroll", "predicted prepend", "predict", "invalidate", "append", "core maintain anchor", "core bottom pin",
     "core shrink clamp"},
    {},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    SimHost host(seed, episode);
    host.inverted = chance(host.rng, 30);
    open(host, pickSize(host.rng, 30, 59));
    for (int step = 0; step < 10; ++step) {
      int roll = pickPercent(host.rng);
      if (roll < 25) {
        episode.reach("scroll");
        double target = pickFraction(host.rng) * host.maxOffset();
        scrollTo(host, target);
        continue;
      }
      if (roll < 45) {
        episode.reach("predicted prepend");
        // Predict the sizes of rows about to be prepended, then prepend them.
        std::size_t count = pickSize(host.rng, 1, 6);
        std::vector<std::string> next = withPrepend(host, count);
        for (std::size_t index = 0; index < count; ++index) {
          double guess = host.trueSizes[next[index]] + (chance(host.rng, 50) ? 0.0 : 30.0);
          Virtualizer::applyPredictedRowSize(host.container, next[index], {WINDOW_WIDTH, guess});
        }
        changeAndHold(host, next, "predicted prepend " + std::to_string(count));
        continue;
      }
      if (roll < 70) {
        episode.reach("predict");
        // Predict sizes for existing unmeasured rows off screen.
        ScreenRow row = firstVisibleRow(host);
        // Predictions arrive before a frame and the frame's layout reflows them.
        for (std::size_t index = 0; index < host.keys.size(); ++index) {
          const Row& placed = host.container.revision.rows[index];
          if (placed.measured || chance(host.rng, 50)) {
            continue;
          }
          Virtualizer::applyPredictedRowSize(
            host.container, placed.key, {WINDOW_WIDTH, host.trueSizes[placed.key] + 10.0});
        }
        host.note("predict unmeasured rows off=" + std::to_string(host.hostOffset));
        host.commit(false);
        if (host.settle() < 0) {
          host.failWith("predictions never settled");
        }
        checkFrame(host, true);
        if (!row.key.empty()) {
          double after = screenOf(host, row.key);
          bool clamped = isHoldClamped(host, row.key, row.screen);
          bool bottomHeld = host.inverted && std::fabs(host.hostOffset - host.maxOffset()) <= FUZZ_TOLERANCE;
          if (!std::isnan(after) && std::fabs(after - row.screen) > FUZZ_TOLERANCE && !clamped && !bottomHeld) {
            host.failWith(
              "predictions moved row " + row.key + " from " + std::to_string(row.screen) + " to " +
              std::to_string(after));
          }
        }
        continue;
      }
      if (roll < 85) {
        episode.reach("invalidate");
        ScreenRow row = firstVisibleRow(host);
        host.note("invalidate predictions off=" + std::to_string(host.hostOffset));
        Virtualizer::invalidatePredictions(host.container);
        for (const Row& placed : host.container.revision.rows) {
          if (placed.predicted) {
            host.failWith("a prediction survived invalidation");
          }
        }
        host.commit(false);
        if (host.settle() < 0) {
          host.failWith("invalidation never settled");
        }
        checkFrame(host, true);
        if (!row.key.empty()) {
          double after = screenOf(host, row.key);
          bool clamped = isHoldClamped(host, row.key, row.screen);
          bool bottomHeld = host.inverted && std::fabs(host.hostOffset - host.maxOffset()) <= FUZZ_TOLERANCE;
          if (!std::isnan(after) && std::fabs(after - row.screen) > FUZZ_TOLERANCE && !clamped && !bottomHeld) {
            host.failWith(
              "invalidation moved row " + row.key + " from " + std::to_string(row.screen) + " to " +
              std::to_string(after));
          }
        }
        continue;
      }
      episode.reach("append");
      changeAndHold(host, withAppend(host, pickSize(host.rng, 1, 4)), "append");
    }
    // Every mounted row ends up measured, not predicted.
    for (const Row& row : host.container.revision.rows) {
      if (row.predicted && row.measured) {
        host.failWith("row " + row.key + " is both measured and predicted");
      }
    }
  });
}

/*
 * Long flings through unmeasured rows, in both directions, on plain and inverted lists.
 * Every frame mounts what is on screen and the content under the reader never jumps,
 * however far the measured rows are from the estimate.
 */
TEST(fuzz_flings_never_blank_or_jump) {
  const FuzzKinds kinds = {
    {"fling forward", "fling back", "core bottom pin"},
    {"core shrink clamp"},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    SimHost host(seed, episode);
    host.inverted = chance(host.rng, 30);
    host.header = chance(host.rng, 50) ? 0.0 : 120.0;
    open(host, 300 + pickSize(host.rng, 0, 99) * 4);
    scrollTo(host, host.maxOffset() * pickFraction(host.rng));

    for (int fling = 0; fling < 4; ++fling) {
      double velocity = 1500.0 + pickPercent(host.rng) * 45.0;
      if (chance(host.rng, 50)) {
        velocity = -velocity;
      }
      episode.reach(velocity < 0.0 ? "fling back" : "fling forward");
      host.note("fling v=" + std::to_string(velocity) + " from " + std::to_string(host.hostOffset));
      while (std::fabs(velocity) > 20.0) {
        double next = std::min(std::max(host.hostOffset + velocity, 0.0), host.maxOffset());
        bool clamped = next <= 0.0 || next >= host.maxOffset();
        double previousOffset = host.hostOffset;
        host.hostOffset = next;
        host.phase = ScrollPhase::Settling;
        host.userScrolled = true;
        ScreenRow held = firstRowStartingOnScreen(host, previousOffset);
        host.note("fling frame off=" + std::to_string(host.hostOffset) + " held=" + held.key);
        host.commit(false);
        checkFrame(host);
        /*
         * A layout pass that measured rows moved them after the core picked its measured range.
         * No host shows that range: ListDriver::runPasses runs the core again before it mounts,
         * and Fabric commits the core's own write. That next pass is judged below. A pass that
         * left every size alone is judged right here.
         */
        std::string coverage = host.layoutResized ? "" : checkMounted(host, host.hostOffset, host.mountPad);
        if (!coverage.empty()) {
          host.failWith("blank during fling: " + coverage);
        }
        host.commit(true);
        checkFrame(host);
        coverage = checkMounted(host, host.hostOffset, host.mountPad);
        if (!coverage.empty()) {
          host.failWith("blank during fling after mount: " + coverage);
        }
        bool atEdge =
          host.hostOffset <= FUZZ_TOLERANCE || std::fabs(host.hostOffset - host.maxOffset()) <= FUZZ_TOLERANCE;
        if (!held.key.empty() && !clamped && !atEdge) {
          double after = screenOf(host, held.key);
          if (std::fabs(after - held.screen) > FUZZ_TOLERANCE) {
            host.failWith(
              "row " + held.key + " jumped from " + std::to_string(held.screen) + " to " + std::to_string(after) +
              " mid fling");
          }
        }
        if (clamped) {
          break;
        }
        velocity *= 0.92;
      }
      host.phase = ScrollPhase::Idle;
      host.userScrolled = false;
      host.commit(false);
      rest(host);
    }
  });
}

/*
 * A list starts small or empty, grows page by page as the reader reaches an edge, then
 * shrinks back to a few rows or none and grows again. It never rests outside its range,
 * a list shorter than the viewport rests at its start, and every arrival at an edge fires
 * its callback once per page.
 * An arrival is the core's: an update that ends near the edge after one that ended away
 * from it, or after the row count changed. One scroll can arrive twice. Rows measured on
 * the way move the edge, and the reader can be near it, then away, then near it again.
 */
TEST(fuzz_lists_grow_from_small_and_shrink_back) {
  const FuzzKinds kinds = {
    {"scroll to end", "load next page", "scroll to start", "load previous page", "append", "shrink", "grow",
     "core maintain anchor", "core scroll to end", "core bottom pin", "core shrink clamp"},
    {},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    SimHost host(seed, episode);
    host.inverted = chance(host.rng, 40);
    host.followAppends = host.inverted && chance(host.rng, 50);
    host.header = chance(host.rng, 50) ? 0.0 : 60.0;
    int endReached = 0;
    int startReached = 0;
    std::size_t endReachedCount = UNDEFINED_INDEX;
    /*
     * Whether an update ended away from the edge since the edge last fired, and how often
     * the edge fired again for the same rows without one.
     */
    bool awayFromEnd = true;
    bool awayFromStart = true;
    int endRepeats = 0;
    int startRepeats = 0;
    host.container.onEndReachedCallback = [&] {
      ++endReached;
      if (!awayFromEnd && endReachedCount == host.container.revision.rows.size()) {
        ++endRepeats;
      }
      awayFromEnd = false;
      endReachedCount = host.container.revision.rows.size();
    };
    std::size_t startReachedCount = UNDEFINED_INDEX;
    host.container.onStartReachedCallback = [&] {
      ++startReached;
      if (!awayFromStart && startReachedCount == host.container.revision.rows.size()) {
        ++startRepeats;
      }
      awayFromStart = false;
      startReachedCount = host.container.revision.rows.size();
    };
    /*
     * The edges as Container::endRevision judges them. A list shorter than the viewport
     * is at its end only.
     */
    host.afterUpdate = [&] {
      const Container& core = host.container;
      double offset = core.revision.offsetY;
      double window = core.getWindowSize();
      double total = core.revision.contentHeight;
      bool nearEnd = offset + window >= total - window * core.endReachedThreshold;
      bool nearStart = offset <= window * core.startReachedThreshold && !(nearEnd && total <= window);
      awayFromEnd = awayFromEnd || !nearEnd;
      awayFromStart = awayFromStart || !nearStart;
    };
    open(host, pickSize(host.rng, 0, 3));

    auto checkRange = [&](const std::string& what) {
      if (host.hostOffset < -FUZZ_TOLERANCE || host.hostOffset > host.maxOffset() + FUZZ_TOLERANCE) {
        host.failWith(
          what + ": rests outside the range off=" + std::to_string(host.hostOffset) +
          " max=" + std::to_string(host.maxOffset()));
      }
      if (host.maxOffset() <= 0.0 && std::fabs(host.hostOffset) > FUZZ_TOLERANCE) {
        host.failWith(what + ": a list shorter than the viewport rests at " + std::to_string(host.hostOffset));
      }
    };

    for (int step = 0; step < 14; ++step) {
      int roll = pickPercent(host.rng);
      std::size_t page = pickSize(host.rng, 1, 15);
      if (roll < 30) {
        episode.reach("scroll to end");
        // The reader goes to the end. Arriving there fires once and loads the next page.
        int before = endReached;
        // Rows measured on the way move the end. Keep going until the reader rests on it.
        for (int attempt = 0; attempt < 8 && host.hostOffset < host.maxOffset() - FUZZ_TOLERANCE; ++attempt) {
          scrollTo(host, host.maxOffset());
        }
        if (host.hostOffset >= host.maxOffset() - FUZZ_TOLERANCE) {
          checkEdgeFired(host, "end", host.container.revision.rows.size(), endReachedCount);
        }
        if (endReached > before) {
          episode.reach("load next page");
          changeAndHold(host, withAppend(host, page), "load next page " + std::to_string(page));
          // A page that keeps the reader at the end fires again only after it changed the count.
          int afterPage = endReached;
          host.commit(false);
          rest(host);
          if (endReached - afterPage > 1) {
            host.failWith("end reached fired repeatedly after a page");
          }
        }
      } else if (roll < 50) {
        episode.reach("scroll to start");
        int before = startReached;
        scrollTo(host, 0.0);
        // A list that scrolls, resting on its first row, has fired for the rows it has.
        bool scrollable = host.container.revision.contentHeight > host.windowSize;
        if (scrollable && host.hostOffset <= FUZZ_TOLERANCE) {
          checkEdgeFired(host, "start", host.container.revision.rows.size(), startReachedCount);
        }
        if (startReached > before) {
          episode.reach("load previous page");
          changeAndHold(host, withPrepend(host, page), "load previous page " + std::to_string(page));
        }
      } else if (roll < 65) {
        episode.reach("append");
        changeAndHold(host, withAppend(host, page), "append " + std::to_string(page));
      } else if (roll < 80) {
        episode.reach("shrink");
        std::size_t keep = pickSize(host.rng, 0, 3);
        keep = std::min(keep, host.keys.size());
        bool fromStart = chance(host.rng, 50);
        std::vector<std::string> next =
          fromStart ? std::vector<std::string>(host.keys.begin(), host.keys.begin() + static_cast<std::ptrdiff_t>(keep))
                    : std::vector<std::string>(host.keys.end() - static_cast<std::ptrdiff_t>(keep), host.keys.end());
        host.keys = next;
        host.note(
          std::string("shrink to ") + std::to_string(keep) + (fromStart ? " from start" : " from end") +
          " off=" + std::to_string(host.hostOffset));
        host.commit(false);
        checkFrame(host);
        rest(host);
      } else {
        episode.reach("grow");
        std::size_t count = pickSize(host.rng, 20, 79);
        changeAndHold(host, withAppend(host, count), "grow by " + std::to_string(count));
      }
      checkRange("step " + std::to_string(step));
      // Checked after every step. Any commit can fire an edge, not only a scroll to it.
      if (endRepeats > 0) {
        host.failWith("end reached fired " + std::to_string(endRepeats + 1) + " times on one arrival");
      }
      if (startRepeats > 0) {
        host.failWith("start reached fired " + std::to_string(startRepeats + 1) + " times on one arrival");
      }
    }
  });
}

/*
 * An inverted chat whose header or footer resizes, like a loading spinner above the history
 * or a typing indicator below the newest row. The rows on screen hold, the same as for an
 * append. A footer growing at the bottom lands below the fold. Shrinking clamps to the end.
 */
TEST(fuzz_inverted_header_and_footer_changes_keep_the_reader) {
  const FuzzKinds kinds = {
    {"scroll", "footer", "header", "core bottom pin"},
    {},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    SimHost host(seed, episode);
    host.inverted = true;
    host.header = chance(host.rng, 50) ? 0.0 : 60.0;
    open(host, pickSize(host.rng, 20, 59));
    for (int step = 0; step < 10; ++step) {
      if (chance(host.rng, 35)) {
        episode.reach("scroll");
        double target = chance(host.rng, 40) ? host.maxOffset() : pickFraction(host.rng) * host.maxOffset();
        scrollTo(host, target);
        continue;
      }
      bool atBottom =
        host.hostOffset >= host.maxOffset() - INVERTED_FOLLOW_BAND && !host.container.state.invertedBottomReleased;
      bool openingPin = atBottom && host.container.state.invertedOpeningPin;
      ScreenRow row = firstVisibleRow(host);
      double offsetBefore = host.hostOffset;
      double previousHeader = host.header;
      bool footerChange = chance(host.rng, 50);
      if (footerChange) {
        episode.reach("footer");
        host.footer = host.footer > 0.0 ? 0.0 : 40.0 + pickPercent(host.rng);
        host.note("footer -> " + std::to_string(host.footer) + (atBottom ? " at bottom" : ""));
      } else {
        episode.reach("header");
        host.header = host.header > 0.0 ? 0.0 : 60.0;
        host.note("header -> " + std::to_string(host.header) + (atBottom ? " at bottom" : ""));
      }
      host.commit(false);
      checkFrame(host);
      if (host.settle() < 0) {
        host.failWith("change never settled");
      }
      checkFrame(host, true);
      if (host.hostOffset > host.maxOffset() + FUZZ_TOLERANCE) {
        host.failWith("rests past the end");
      }
      /*
       * A chat still settling on the bottom it opened at keeps that bottom, and so does one
       * resting on its newest row.
       */
      if (openingPin || (atBottom && !row.key.empty() && row.key == host.keys.back())) {
        if (std::fabs(host.hostOffset - host.maxOffset()) > FUZZ_TOLERANCE) {
          host.failWith(
            "left the opening bottom: off=" + std::to_string(host.hostOffset) +
            " max=" + std::to_string(host.maxOffset()));
        }
        continue;
      }
      if (row.key.empty()) {
        continue;
      }
      double after = screenOf(host, row.key);
      bool headerOffScreen = offsetBefore > 0.0 && offsetBefore >= previousHeader;
      double expected = footerChange || headerOffScreen ? row.screen : row.screen + (host.header - previousHeader);
      bool clamped = isHoldClamped(host, row.key, expected);
      if (std::fabs(after - expected) > FUZZ_TOLERANCE && !clamped) {
        host.failWith("row " + row.key + " at " + std::to_string(after) + " expected " + std::to_string(expected));
      }
    }
  });
}
