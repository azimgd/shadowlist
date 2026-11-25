/*
 * Randomized kit simulation. A small model of a native list view drives the host layer the way
 * ShadowListKitListView does on every platform: data changes reach ListDriver as key edits, a
 * layout pass runs the driver's passes when the offset left the band, and the cells of the mount
 * plan are mounted and recycled. After every layout pass the rows must be contiguous, every row
 * on screen must be mounted at its real size and frame, and the list must settle. Data changes
 * hold the row the reader was looking at, commands land where they aim and an edge the reader
 * rests on has been reported. KitHostHelpers.hpp has the model and KitFuzzHelpers.hpp what the
 * tests do to it. See tests_fuzz.cpp for how the fuzz tests run.
 */

#include "FuzzHelpers.hpp"
#include "KitFuzzHelpers.hpp"
#include "KitHostHelpers.hpp"
#include "TestFramework.hpp"

#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListSections.hpp>
#include <shadowlist-core/host/MountedRange.hpp>
#include <shadowlist-core/host/ScrollTarget.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

using namespace azimgd::shadowlist;
using namespace slt;

/*
 * A plain vertical list: rows come and go above, inside and below the viewport, move, change
 * their size under the same key, through every call an app can make the change with. The first
 * row on screen never moves. With non anchor rows on screen, the first row that is not one
 * never moves. The header and the footer are pinned or not.
 */
TEST(fuzz_kit_data_changes_hold_the_first_visible_row) {
  const FuzzKinds kinds = {
    {"prepend", "append", "insert", "delete", "move", "refresh", "scatter", "resize", "insert in view", "scroll",
     "non anchor rows", "through own call", "through batch", "through diff", "through reload"},
    {},
  };
  forEachFuzzSeed(__func__, KIT_FUZZ_SEEDS, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    KitHost host(seed, episode);
    host.headerView = chance(host.rng, 50) ? 0.0 : 120.0 + pickPercent(host.rng);
    host.footerView = chance(host.rng, 70) ? 0.0 : 60.0;
    host.stickyHeader = chance(host.rng, 50);
    host.stickyFooter = chance(host.rng, 50);
    open(host, 20 + pickSize(host.rng, 0, 99));

    for (int step = 0; step < 8 && !host.items.empty(); ++step) {
      int roll = pickPercent(host.rng);
      std::size_t count = pickSize(host.rng, 1, 12);
      std::size_t size = host.items.size();
      std::size_t at = pickSize(host.rng, 0, size - 1);
      std::optional<MountedRange> visible = host.driver.getVisibleRange();
      // Some of the rows on screen stop holding the content, the first one among them, or none do.
      if (chance(host.rng, 30)) {
        episode.reach("non anchor rows");
        std::unordered_set<std::string> keys;
        for (const ScreenRow& row : visibleRows(host)) {
          if (keys.empty() || chance(host.rng, 30)) {
            keys.insert(row.key);
          }
        }
        if (chance(host.rng, 20)) {
          keys.clear();
        }
        host.setNonAnchorKeys(std::move(keys));
        rest(host);
      }
      if (roll < 15) {
        episode.reach("prepend");
        changeAndHold(host, insertEdit(host, 0, count));
      } else if (roll < 27) {
        episode.reach("append");
        changeAndHold(host, insertEdit(host, size, count));
      } else if (roll < 37) {
        episode.reach("insert");
        changeAndHold(host, insertEdit(host, at, count));
      } else if (roll < 49) {
        episode.reach("delete");
        changeAndHold(host, deleteEdit(host, at, count));
      } else if (roll < 57) {
        episode.reach("move");
        changeAndHold(host, moveEdit(host, at, pickSize(host.rng, 0, size - 1)));
      } else if (roll < 64 && visible) {
        episode.reach("refresh");
        changeAndHold(host, refreshEdit(host, visible->low, count));
      } else if (roll < 72) {
        episode.reach("scatter");
        changeAndHold(host, scatterEdit(host, count % 5, 1 + count % 4));
      } else if (roll < 80) {
        episode.reach("resize");
        changeAndHold(host, resizeEdit(host, at, 1 + count % 3));
      } else if (roll < 87 && visible) {
        episode.reach("insert in view");
        changeAndHold(host, insertEdit(host, visible->low + 1, count));
      } else {
        episode.reach("scroll");
        scrollTo(host, pickFraction(host.rng) * host.maxOffset());
      }
    }
  });
}

/*
 * An inverted chat: history pages prepended, incoming rows appended, with and without
 * followAppends, from the bottom and from up in the history. The chat holds the reader, and
 * follows appends when asked to and the reader rests on the newest row. It tells its delegate
 * when the reader leaves its end and when a command is back at it.
 */
TEST(fuzz_kit_inverted_history_and_incoming_hold_the_reader) {
  const FuzzKinds kinds = {
    {"history", "incoming", "delete", "scroll", "scroll to end", "command to end", "animated command to end",
     "through own call", "through batch", "through diff", "through reload"},
    {},
    {},
  };
  forEachFuzzSeed(__func__, KIT_FUZZ_SEEDS, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    KitHost host(seed, episode);
    host.settings.inverted = true;
    host.settings.followAppends = chance(host.rng, 50);
    host.headerView = chance(host.rng, 60) ? 0.0 : 48.0;
    open(host, pickSize(host.rng, 15, 54));
    if (!atEnd(host)) {
      host.failWith(
        "inverted list did not open at the bottom: off=" + std::to_string(host.offset) +
        " max=" + std::to_string(host.maxOffset()));
    }

    /*
     * Whether the list is at its end, as it tells its delegate. A change of the rows never
     * changes it. A scroll by the reader that rests farther from the newest row than the band the
     * list follows in leaves the end, and a scroll command to the end is back at it.
     */
    int atEndChanges = 0;
    double offsetBefore = 0.0;
    auto afterDataChange = [&](const std::string& what) {
      if (host.atEndChanges != atEndChanges) {
        host.failWith("the list changed whether it is at its end after " + what);
      }
    };
    auto afterScroll = [&] {
      bool moved = std::fabs(host.offset - offsetBefore) >= INVERTED_FOLLOW_BAND;
      if (moved && host.offset < host.maxOffset() - INVERTED_FOLLOW_BAND && host.atEnd) {
        host.failWith("the list says it is at its end after the reader scrolled away from it");
      }
    };

    for (int step = 0; step < 8 && !host.items.empty(); ++step) {
      int roll = pickPercent(host.rng);
      std::size_t count = pickSize(host.rng, 1, 10);
      atEndChanges = host.atEndChanges;
      offsetBefore = host.offset;
      if (roll < 28) {
        episode.reach("history");
        changeAndHold(host, insertEdit(host, 0, count));
        afterDataChange("history");
      } else if (roll < 56) {
        episode.reach("incoming");
        changeAndHold(host, insertEdit(host, host.items.size(), count));
        afterDataChange("incoming");
      } else if (roll < 66) {
        episode.reach("delete");
        changeAndHold(host, deleteEdit(host, pickSize(host.rng, 0, host.items.size() - 1), 1));
        afterDataChange("delete");
      } else if (roll < 80) {
        episode.reach("scroll");
        scrollTo(host, pickFraction(host.rng) * host.maxOffset());
        afterScroll();
      } else if (roll < 90) {
        episode.reach("scroll to end");
        scrollTo(host, host.maxOffset());
        afterScroll();
      } else {
        bool animated = chance(host.rng, 50);
        episode.reach(animated ? "animated command to end" : "command to end");
        scrollToEnd(host, animated);
        if (!host.atEnd) {
          host.failWith("the list says it is away from its end after a scroll command to it");
        }
      }
    }
  });
}

/*
 * Scroll commands on a plain, inverted, horizontal or column list: to an item, to either end,
 * at once or after the kit's animation toward the driver's estimate, with data changes and
 * user scrolls in between. Every command lands where it aims, and a saved position lands its
 * row the same distance in again.
 */
TEST(fuzz_kit_scroll_commands_land_where_they_aim) {
  const FuzzKinds kinds = {
    {"command to item", "command to end", "command to start", "animated command", "restore position",
     "interrupted animation", "prepend", "append", "scroll", "through own call", "through batch", "through diff",
     "through reload"},
    {},
    {},
  };
  forEachFuzzSeed(__func__, KIT_FUZZ_SEEDS, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    KitHost host(seed, episode);
    int shape = pickInt(host.rng, 0, 3);
    host.settings.inverted = shape == 0;
    host.settings.horizontal = shape == 1;
    host.settings.numberOfColumns = shape == 2 ? pickSize(host.rng, 2, 4) : 1;
    host.headerView = chance(host.rng, 50) ? 0.0 : 90.0;
    open(host, 30 + pickSize(host.rng, 0, 99) * 2);
    std::optional<AnchorState> saved;

    for (int step = 0; step < 8; ++step) {
      int roll = pickPercent(host.rng);
      bool animated = chance(host.rng, 40);
      std::size_t count = host.getColumns() * pickSize(host.rng, 1, 4);
      if (animated && roll < 62) {
        episode.reach("animated command");
      }
      if (roll < 34) {
        episode.reach("command to item");
        std::size_t item = pickSize(host.rng, 0, host.items.size() - 1);
        scrollToItem(host, item, pickInt(host.rng, 0, 2) * 0.5, animated);
      } else if (roll < 48) {
        episode.reach("command to end");
        scrollToEnd(host, animated);
      } else if (roll < 62) {
        episode.reach("command to start");
        scrollToStart(host, animated);
      } else if (roll < 70) {
        /*
         * A finger lands while an animated command is on its way. The landing is dropped and the
         * list rests where the finger left it.
         */
        episode.reach("interrupted animation");
        host.note("interrupted animation");
        host.driver.setLanding({ScrollLanding::Target::Start, 0, 0.0});
        host.offset *= 0.5;
        host.layoutPass();
        beginDrag(host);
        scrollFrame(host, 30.0, ScrollPhase::Dragging);
        if (host.driver.land()) {
          host.failWith("a drag left the animated command's landing in place");
        }
        rest(host);
      } else if (roll < 78) {
        episode.reach("restore position");
        if (saved && host.driver.restoreAnchorState(*saved)) {
          host.note("restore position " + saved->key + " +" + std::to_string(saved->offset));
          host.driver.cancelLanding();
          host.needsFrame = true;
          host.layoutPass();
          if (host.settle() < 0) {
            host.failWith("restored position never settled");
          }
          rest(host);
          std::size_t row = host.driver.indexOfKey(saved->key);
          double wanted = std::min(std::max(host.driver.getLeadingAt(row) + saved->offset, 0.0), host.maxOffset());
          if (std::fabs(host.offset - wanted) > FUZZ_TOLERANCE) {
            host.failWith(
              "restored position landed at " + std::to_string(host.offset) + " wanted " + std::to_string(wanted));
          }
        }
        saved = host.driver.getAnchorState(host.offset);
      } else if (roll < 86) {
        episode.reach("prepend");
        changeAndHold(host, insertEdit(host, 0, count));
      } else if (roll < 93) {
        episode.reach("append");
        changeAndHold(host, insertEdit(host, host.items.size(), count));
      } else {
        episode.reach("scroll");
        scrollTo(host, pickFraction(host.rng) * host.maxOffset());
      }
    }
  });
}

/*
 * Drags, flings and pulls past an edge through long lists, with data landing while the finger
 * is down. Every frame mounts what is on screen at its real size, the rows on screen never
 * jump, and a row the reader held is where it was minus how far they scrolled since.
 */
TEST(fuzz_kit_gestures_keep_the_viewport_mounted) {
  const FuzzKinds kinds = {
    {"drag", "fling forward", "fling back", "pull past the start", "pull past the end", "prepend", "append", "insert",
     "through own call", "through batch", "through diff", "through reload"},
    {},
  };
  forEachFuzzSeed(__func__, KIT_FUZZ_SEEDS, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    KitHost host(seed, episode);
    int shape = pickInt(host.rng, 0, 4);
    host.settings.inverted = shape == 0;
    host.settings.horizontal = shape == 1;
    host.settings.numberOfColumns = shape == 2 ? pickSize(host.rng, 2, 4) : 1;
    host.headerView = chance(host.rng, 50) ? 0.0 : 100.0;
    open(host, 120 + pickSize(host.rng, 0, 99) * 2);
    scrollTo(host, host.maxOffset() * pickFraction(host.rng));

    for (int gesture = 0; gesture < 4; ++gesture) {
      int roll = pickPercent(host.rng);
      if (roll < 45) {
        episode.reach("drag");
        // The rows on screen when the data lands, and the travel since then.
        std::vector<ScreenRow> held;
        double travel = 0.0;
        int frames = 3 + pickInt(host.rng, 0, 5);
        int changeAt = pickInt(host.rng, 0, frames + frames / 2);
        double perFrame = (pickPercent(host.rng) - 50.0) * 4.0;
        std::string what;
        beginDrag(host);
        for (int frame = 0; frame < frames; ++frame) {
          host.note("drag frame off=" + std::to_string(host.offset));
          if (!what.empty()) {
            travel += std::min(std::max(host.offset + perFrame, 0.0), host.maxOffset()) - host.offset;
          }
          scrollFrame(host, perFrame, frame < frames - 2 ? ScrollPhase::Dragging : ScrollPhase::Settling);
          if (frame != changeAt) {
            continue;
          }
          held = visibleRows(host);
          std::size_t count = host.getColumns() * pickSize(host.rng, 1, 4);
          int change = pickPercent(host.rng);
          Edit edit;
          if (change < 34) {
            episode.reach("prepend");
            edit = insertEdit(host, 0, count);
          } else if (change < 67) {
            episode.reach("append");
            edit = insertEdit(host, host.items.size(), count);
          } else {
            episode.reach("insert");
            edit = insertEdit(host, pickSize(host.rng, 0, host.items.size() - 1), count);
          }
          what = edit.what;
          applyEdit(host, edit);
        }
        rest(host);
        if (what.empty() || host.getColumns() > 1) {
          continue;
        }
        for (const ScreenRow& row : held) {
          double after = screenOf(host, row.key);
          double expected = row.screen - travel;
          if (std::fabs(after - expected) <= FUZZ_TOLERANCE) {
            break;
          }
          if (isHoldClamped(host, row.key, expected)) {
            break;
          }
          if (host.settings.inverted && atEnd(host)) {
            break;
          }
          host.failWith(
            what + ": row " + row.key + " at " + std::to_string(after) + " expected " + std::to_string(expected) +
            " (travel " + std::to_string(travel) + ", host " + std::to_string(host.offset) + ", max " +
            std::to_string(host.maxOffset()) + ")");
        }
      } else if (roll < 85) {
        double velocity = 1500.0 + pickPercent(host.rng) * 45.0;
        if (chance(host.rng, 50)) {
          velocity = -velocity;
        }
        episode.reach(velocity < 0.0 ? "fling back" : "fling forward");
        host.note("fling v=" + std::to_string(velocity) + " from " + std::to_string(host.offset));
        beginDrag(host);
        scrollFrame(host, velocity / 10.0, ScrollPhase::Dragging);
        while (std::fabs(velocity) > 20.0) {
          host.note("fling frame off=" + std::to_string(host.offset));
          double next = host.offset + velocity;
          scrollFrame(host, velocity, ScrollPhase::Settling);
          if (next <= 0.0 || next >= host.maxOffset()) {
            break;
          }
          velocity *= 0.92;
        }
        rest(host);
      } else {
        /*
         * The finger holds the list past an edge, then lets go and the scroll view springs back.
         */
        bool start = chance(host.rng, 50);
        episode.reach(start ? "pull past the start" : "pull past the end");
        host.note(start ? "pull past the start" : "pull past the end");
        beginDrag(host);
        double edge = start ? 0.0 : host.maxOffset();
        scrollFrame(host, edge - host.offset, ScrollPhase::Dragging);
        for (int frame = 0; frame < 3; ++frame) {
          scrollFrame(host, start ? -40.0 : 40.0, ScrollPhase::Dragging, true);
        }
        // The spring brings the list back inside its range.
        scrollFrame(host, 0.0, ScrollPhase::Settling);
        rest(host);
      }
    }
  });
}

/*
 * Grids with uneven tracks and horizontal lists. Changes by whole rows of tracks hold a row on
 * screen, a ragged change keeps the geometry sound, a new length along the axis keeps the
 * reader inside the range, and a new cross size drops every size and holds the first visible row.
 */
TEST(fuzz_kit_grids_and_horizontal_lists_hold_through_changes_and_resizes) {
  const FuzzKinds kinds = {
    {"prepend", "append", "ragged delete", "scroll", "resize along", "resize across", "through own call",
     "through batch", "through diff", "through reload"},
    {},
  };
  forEachFuzzSeed(__func__, KIT_FUZZ_SEEDS, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    KitHost host(seed, episode);
    host.settings.horizontal = chance(host.rng, 40);
    host.settings.numberOfColumns = chance(host.rng, 70) ? pickSize(host.rng, 2, 4) : 1;
    host.headerView = chance(host.rng, 50) ? 0.0 : 80.0;
    open(host, 30 + pickSize(host.rng, 0, 99));

    for (int step = 0; step < 8 && !host.items.empty(); ++step) {
      int roll = pickPercent(host.rng);
      std::size_t count = host.getColumns() * pickSize(host.rng, 1, 4);
      if (roll < 25) {
        episode.reach("prepend");
        changeAndHold(host, insertEdit(host, 0, count));
      } else if (roll < 40) {
        episode.reach("append");
        changeAndHold(host, insertEdit(host, host.items.size(), count));
      } else if (roll < 52) {
        episode.reach("ragged delete");
        applyEdit(host, deleteEdit(host, pickSize(host.rng, 0, host.items.size() - 1), 1));
        rest(host);
      } else if (roll < 70) {
        episode.reach("scroll");
        scrollTo(host, pickFraction(host.rng) * host.maxOffset());
      } else if (roll < 85) {
        episode.reach("resize along");
        host.boundsAlong = chance(host.rng, 50) ? WINDOW_HEIGHT : WINDOW_HEIGHT - 300.0 - pickPercent(host.rng);
        host.note("along -> " + std::to_string(host.boundsAlong));
        host.layoutPass();
        rest(host);
      } else {
        episode.reach("resize across");
        std::vector<ScreenRow> before = visibleRows(host);
        host.boundsCross =
          host.boundsCross == WINDOW_WIDTH ? WINDOW_WIDTH - 60.0 - pickPercent(host.rng) : WINDOW_WIDTH;
        host.note("across -> " + std::to_string(host.boundsCross) + " off=" + std::to_string(host.offset));
        host.layoutPass();
        if (host.settle() < 0) {
          host.failWith("resize across never settled");
        }
        rest(host);
        // The viewport stays as far into the row at its start as it was. A grid holds one track.
        if (before.empty() || host.getColumns() > 1) {
          continue;
        }
        const ScreenRow& row = before.front();
        double after = screenOf(host, row.key);
        if (std::fabs(after - row.screen) > FUZZ_TOLERANCE && !isHoldClamped(host, row.key, row.screen)) {
          host.failWith(
            "resize across: row " + row.key + " moved from " + std::to_string(row.screen) + " to " +
            std::to_string(after));
        }
      }
    }
  });
}

/*
 * Sections with header and footer rows and sticky headers. Items come and go, sections appear,
 * empty and lose their header. The pinned header is always the last one above the viewport
 * start, mounted and pushed up by the next, and the first row on screen holds.
 */
TEST(fuzz_kit_sections_pin_one_header_and_hold_the_reader) {
  const FuzzKinds kinds = {
    {"add items", "delete items", "add section", "empty section", "toggle header", "scroll", "scroll to section",
     "through own call", "through batch", "through diff", "through reload"},
    {},
  };
  forEachFuzzSeed(__func__, KIT_FUZZ_SEEDS, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    KitHost host(seed, episode);
    host.stickySectionHeaders = chance(host.rng, 80);
    std::size_t made = 0;
    auto makeSection = [&] {
      return KitSection{"s" + std::to_string(made++), 0, chance(host.rng, 85), chance(host.rng, 30)};
    };
    int sectionCount = pickInt(host.rng, 3, 7);
    for (int section = 0; section < sectionCount; ++section) {
      host.sectionModel.push_back(makeSection());
    }
    open(host, 30 + pickSize(host.rng, 0, 99));
    auto firstItem = [&](std::size_t section) {
      std::size_t first = 0;
      for (std::size_t before = 0; before < section; ++before) {
        first += host.sectionModel[before].itemCount;
      }
      return first;
    };
    /*
     * A change of the sections reaches the kit through reloadData or applyChanges, and the first
     * row on screen that stayed holds.
     */
    auto changeSections = [&](const std::string& what, std::vector<std::string> next) {
      std::vector<ScreenRow> before = visibleRows(host);
      host.items = std::move(next);
      bool diff = chance(host.rng, 50);
      episode.reach(diff ? "through diff" : "through reload");
      host.note(what + (diff ? " through diff" : " through reload") + " off=" + std::to_string(host.offset));
      if (diff) {
        host.applyChanges();
      } else {
        host.reloadData();
      }
      if (host.settle() < 0) {
        host.failWith(what + ": correction never settled");
      }
      rest(host);
      for (const ScreenRow& row : before) {
        double after = screenOf(host, row.key);
        if (std::isnan(after)) {
          continue;
        }
        if (std::fabs(after - row.screen) > FUZZ_TOLERANCE && !isHoldClamped(host, row.key, row.screen)) {
          host.failWith(what + ": row moved from " + std::to_string(row.screen) + " to " + std::to_string(after));
        }
        return;
      }
    };

    for (int step = 0; step < 8; ++step) {
      int roll = pickPercent(host.rng);
      std::size_t section = pickSize(host.rng, 0, host.sectionModel.size() - 1);
      std::size_t count = pickSize(host.rng, 1, 6);
      std::size_t first = firstItem(section);
      if (roll < 22) {
        episode.reach("add items");
        std::size_t at = first + pickSize(host.rng, 0, host.sectionModel[section].itemCount);
        Edit edit = insertEdit(host, at, count);
        host.sectionModel[section].itemCount += count;
        changeAndHold(host, edit);
      } else if (roll < 40 && host.sectionModel[section].itemCount > 0) {
        episode.reach("delete items");
        count = std::min(count, host.sectionModel[section].itemCount);
        Edit edit = deleteEdit(host, first, count);
        host.sectionModel[section].itemCount -= count;
        changeAndHold(host, edit);
      } else if (roll < 50) {
        episode.reach("add section");
        KitSection added = makeSection();
        added.itemCount = count;
        std::vector<std::string> next = host.items;
        std::vector<std::string> keys = host.makeKeys(count);
        next.insert(next.begin() + static_cast<std::ptrdiff_t>(first), keys.begin(), keys.end());
        host.sectionModel.insert(host.sectionModel.begin() + static_cast<std::ptrdiff_t>(section), added);
        changeSections("add section at " + std::to_string(section), std::move(next));
      } else if (roll < 58) {
        episode.reach("empty section");
        std::vector<std::string> next = host.items;
        next.erase(
          next.begin() + static_cast<std::ptrdiff_t>(first),
          next.begin() + static_cast<std::ptrdiff_t>(first + host.sectionModel[section].itemCount));
        host.sectionModel[section].itemCount = 0;
        changeSections("empty section " + std::to_string(section), std::move(next));
      } else if (roll < 66) {
        episode.reach("toggle header");
        host.sectionModel[section].hasHeader = !host.sectionModel[section].hasHeader;
        // A header that comes back is a new row to measure.
        host.measuredKeys.erase(ListSections::headerRowKey(host.sectionModel[section].key));
        changeSections("toggle header of section " + std::to_string(section), host.items);
      } else if (roll < 86) {
        episode.reach("scroll");
        scrollTo(host, pickFraction(host.rng) * host.maxOffset());
      } else {
        episode.reach("scroll to section");
        std::size_t row = host.sections.firstRowInSection(section);
        if (row == UNDEFINED_INDEX) {
          continue;
        }
        std::string key = host.driver.getKeyAt(row);
        host.note("scroll to section " + std::to_string(section));
        runCommand(host, false, 0.0, {}, [&] { host.driver.scrollToRow(row, 0.0); });
        double wanted = landingOf(host, host.driver.indexOfKey(key), 0.0);
        if (std::fabs(host.offset - wanted) > FUZZ_TOLERANCE) {
          host.failWith(
            "scroll to section landed at " + std::to_string(host.offset) + " wanted " + std::to_string(wanted));
        }
      }
    }
  });
}

/*
 * A list starts small or empty, grows page by page as the reader reaches an edge, then shrinks
 * back to a few rows or none and grows again. It never rests outside its range, a list shorter
 * than the viewport rests at its start, an edge the reader rests on has reported for the rows
 * the list has and stays quiet after, and a list emptied gives every cell back. An empty view
 * shows while the list has no rows, between a header and a footer that are pinned or not.
 */
TEST(fuzz_kit_lists_grow_from_small_and_report_the_edge_they_rest_on) {
  const FuzzKinds kinds = {
    {"scroll to end", "load next page", "scroll to start", "load previous page", "append", "shrink", "grow", "empty",
     "through own call", "through batch", "through diff", "through reload"},
    {},
  };
  forEachFuzzSeed(__func__, KIT_FUZZ_SEEDS, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    KitHost host(seed, episode);
    host.settings.inverted = chance(host.rng, 40);
    host.settings.followAppends = host.settings.inverted && chance(host.rng, 50);
    host.headerView = chance(host.rng, 50) ? 0.0 : 60.0;
    host.footerView = chance(host.rng, 60) ? 0.0 : 40.0;
    // The view of a list without rows, shorter or longer than the viewport.
    host.emptyView = chance(host.rng, 40) ? 0.0 : 80.0 + 12.0 * pickPercent(host.rng);
    host.stickyHeader = chance(host.rng, 50);
    host.stickyFooter = chance(host.rng, 50);
    open(host, pickSize(host.rng, 0, 3));
    /*
     * An edge reports when the reader arrives. More frames of a list that rests there report
     * nothing until the data changes.
     */
    auto restsQuietly = [&](const std::string& where) {
      int start = host.reachedStart;
      int end = host.reachedEnd;
      rest(host);
      if (host.reachedStart != start || host.reachedEnd != end) {
        host.failWith("an edge was reported again while the list rested at " + where);
      }
    };

    for (int step = 0; step < 10; ++step) {
      int roll = pickPercent(host.rng);
      std::size_t page = pickSize(host.rng, 1, 15);
      if (roll < 30) {
        episode.reach("scroll to end");
        int before = host.reachedEnd;
        // Rows measured on the way move the end. Keep going until the reader rests on it.
        for (int attempt = 0; attempt < 8 && host.offset < host.maxOffset() - FUZZ_TOLERANCE; ++attempt) {
          scrollTo(host, host.maxOffset());
        }
        if (atEnd(host)) {
          checkEdgeFired(host, "end", host.driver.getKeyCount(), host.reachedEndCount);
        }
        restsQuietly("the end");
        if (host.reachedEnd > before) {
          episode.reach("load next page");
          changeAndHold(host, insertEdit(host, host.items.size(), page));
          restsQuietly("the end after a page");
        }
      } else if (roll < 50) {
        episode.reach("scroll to start");
        int before = host.reachedStart;
        scrollTo(host, 0.0);
        // A list that scrolls, resting on its first row, has fired for the rows it has.
        if (host.contentAlong > host.windowAlong && host.offset <= FUZZ_TOLERANCE) {
          checkEdgeFired(host, "start", host.driver.getKeyCount(), host.reachedStartCount);
        }
        restsQuietly("the start");
        if (host.reachedStart > before) {
          episode.reach("load previous page");
          changeAndHold(host, insertEdit(host, 0, page));
          restsQuietly("the start after a page");
        }
      } else if (roll < 62) {
        episode.reach("append");
        changeAndHold(host, insertEdit(host, host.items.size(), page));
      } else if (roll < 76) {
        episode.reach("shrink");
        std::size_t keep = std::min(pickSize(host.rng, 0, 3), host.items.size());
        bool fromStart = chance(host.rng, 50);
        Edit edit;
        edit.next =
          fromStart
            ? std::vector<std::string>(host.items.begin(), host.items.begin() + static_cast<std::ptrdiff_t>(keep))
            : std::vector<std::string>(host.items.end() - static_cast<std::ptrdiff_t>(keep), host.items.end());
        for (std::size_t index = 0; index + keep < host.items.size(); ++index) {
          edit.update.deleted.push_back(fromStart ? keep + index : index);
        }
        edit.what = std::string("shrink to ") + std::to_string(keep) + (fromStart ? " from start" : " from end");
        if (edit.update.deleted.empty()) {
          continue;
        }
        applyEdit(host, edit);
        rest(host);
      } else if (roll < 84) {
        episode.reach("empty");
        if (host.items.empty()) {
          continue;
        }
        Edit edit = deleteEdit(host, 0, host.items.size());
        edit.what = "empty";
        applyEdit(host, edit);
        rest(host);
        if (!host.mounted.empty() || host.cellsDisplayed != host.cellsEnded) {
          host.failWith("an emptied list kept " + std::to_string(host.mounted.size()) + " cells");
        }
      } else {
        episode.reach("grow");
        changeAndHold(host, insertEdit(host, host.items.size(), pickSize(host.rng, 20, 79)));
      }
      if (host.maxOffset() <= 0.0 && std::fabs(host.offset) > FUZZ_TOLERANCE) {
        host.failWith("a list shorter than the viewport rests at " + std::to_string(host.offset));
      }
    }
  });
}
