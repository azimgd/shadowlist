/*
 * MountDriver against a simulated renderer. A small model of Fabric's renderer applies the
 * ranges the driver sends late, changes its data and rebuilds its range around scroll targets,
 * and checks every message against what the driver promises. tests_host_mounted_range.cpp has
 * the driver's unit tests. See tests_fuzz.cpp for how the fuzz tests run.
 */

#include "FuzzHelpers.hpp"
#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/host/MountDriver.hpp>
#include <shadowlist-core/host/MountedRange.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

using namespace azimgd::shadowlist;
using namespace slt;

namespace {

/*
 * A stand in for Fabric's renderer around one MountDriver. It mounts a range by its edge keys,
 * applies a range the driver sent some commits later, rebuilds its range itself under a new
 * generation, and reports the measured rows when they change.
 */
struct MountRenderer : Trail {
  MountDriver driver;
  MountSettings settings;
  std::mt19937 rng;
  std::vector<std::string> keys;
  std::size_t nextKey = 0;

  /*
   * The range the driver sent last and the renderer applied, by its edge keys. Empty before
   * the first one of a generation, when the range is the renderer's own around seedIndex.
   */
  std::optional<MountMessage> applied;
  std::size_t seedIndex = 0;
  std::uint64_t generation = 0;
  bool seedPending = false;

  /*
   * The range sent and not applied yet, and the basis of the latest commit.
   */
  std::optional<MountMessage> inFlight;
  MountedBasis basis;
  std::uint64_t previousSequence = 0;

  /*
   * The measured rows: the screen and its overscan, held by the key of its first row.
   */
  std::string measuredKey;
  std::size_t measuredSpan = 4;
  std::optional<MountedRange> reported;

  /*
   * Whether a report waits in the driver, and the measured rows of the one it took last by key,
   * which its steps pace from.
   */
  bool reportWaiting = false;
  MountedRange waitingReport;
  std::string pacedLowKey;
  std::string pacedHighKey;

  MountRenderer(std::uint32_t seed, FuzzEpisode& episode) : Trail(seed, episode), rng(seed) {}

  std::size_t indexOf(const std::string& key) const {
    auto found = std::find(keys.begin(), keys.end(), key);
    return found == keys.end() ? UNDEFINED_INDEX : static_cast<std::size_t>(found - keys.begin());
  }

  std::vector<std::string> makeKeys(std::size_t count) {
    std::vector<std::string> made = keysFor(count, "k", nextKey);
    nextKey += count;
    return made;
  }

  /*
   * The measured rows in the keys now. A deleted first row hands over to the row at its place.
   */
  std::optional<MountedRange> measured() {
    if (keys.empty()) {
      return std::nullopt;
    }
    std::size_t low = indexOf(measuredKey);
    if (low == UNDEFINED_INDEX) {
      low = std::min(seedIndex, keys.size() - 1);
    }
    measuredKey = keys[low];
    return MountedRange{low, std::min(keys.size() - 1, low + measuredSpan)};
  }

  /*
   * The range the renderer mounts now. The edge keys of the applied range are looked up again,
   * and a range at an edge of the data grows over rows added past it. Without them, or with
   * an edge row deleted, the range is the renderer's own.
   */
  MountedRange mountedRange() const {
    if (keys.empty()) {
      return {};
    }
    if (applied) {
      std::size_t low = indexOf(applied->lowKey);
      std::size_t high = indexOf(applied->highKey);
      if (low != UNDEFINED_INDEX && high != UNDEFINED_INDEX) {
        std::size_t first = std::min(low, high);
        std::size_t last = std::max(low, high);
        std::size_t leading = settings.overscanRowsLeading;
        return {applied->lowAtStart ? first - std::min(first, leading) : first,
          applied->highAtEnd ? std::min(keys.size() - 1, last + leading) : last};
      }
    }
    std::size_t around = std::min(seedIndex, keys.size() - 1);
    return {around - std::min<std::size_t>(around, 4), std::min(keys.size() - 1, around + 20)};
  }

  /*
   * One commit: the basis, the measured rows when they changed, then the driver's message.
   */
  void commit() {
    basis.range = mountedRange();
    basis.sequence = applied ? applied->sequence : 0;
    basis.generation = generation;
    basis.seedPending = seedPending;
    driver.setBasis(basis);
    std::optional<MountedRange> now = measured();
    if (now && (!reported || *reported != *now)) {
      driver.reportMeasured(*now);
      reported = now;
      reportWaiting = true;
      waitingReport = *now;
    }
    bool takesReport = reportWaiting && !driver.isPending();
    std::optional<MountMessage> message = driver.process(keys, [this](const std::string& key) { return indexOf(key); });
    bool tookReport = takesReport && waitingReport.high < keys.size();
    if (takesReport) {
      reportWaiting = false;
    }
    if (tookReport) {
      pacedLowKey = keys[waitingReport.low];
      pacedHighKey = keys[waitingReport.high];
    }
    if (message) {
      receive(*message, tookReport, false);
    }
    check();
  }

  /*
   * The host's display frame: the step the driver prepared goes out.
   */
  void frame() {
    bool ready = driver.hasStep();
    std::optional<MountMessage> message = driver.takeStep();
    if (ready != message.has_value()) {
      failWith("hasStep said " + std::to_string(ready) + " and takeStep disagreed");
    }
    if (message) {
      receive(*message, false, true);
    }
    check();
  }

  /*
   * A message from the driver, checked against what it promises: one range in flight, built on
   * the commit's keys and generation, numbered in order, holding the rows a report measured,
   * and growing by at most one step beyond the rows mounted and measured.
   */
  void receive(const MountMessage& message, bool tookReport, bool step) {
    note(message.landed ? "landed gen=" + std::to_string(message.generation)
      : std::string(step ? "step " : "range ") + std::to_string(message.range.low) + ".." +
        std::to_string(message.range.high) + " seq=" + std::to_string(message.sequence) + " gen=" +
        std::to_string(message.generation));
    if (message.generation != generation) {
      failWith("a message carries generation " + std::to_string(message.generation) + " under " +
        std::to_string(generation));
    }
    if (message.landed) {
      if (step || !seedPending || !tookReport || message.sequence != 0) {
        failWith("a landed message came without a report on a pending seed");
      }
      // The renderer drops its command seed.
      seedPending = false;
      return;
    }
    if (inFlight && inFlight->generation == generation) {
      failWith("a range was sent while range " + std::to_string(inFlight->sequence) + " was in flight");
    }
    const MountedRange& range = message.range;
    if (range.isEmpty() || range.high >= keys.size()) {
      failWith("a range outside the keys");
    }
    if (message.lowKey != keys[range.low] || message.highKey != keys[range.high] ||
        message.lowAtStart != (range.low == 0) || message.highAtEnd != (range.high == keys.size() - 1)) {
      failWith("a range whose edge keys or flags are not the rows at its ends");
    }
    if (message.sequence != previousSequence + 1) {
      failWith("range " + std::to_string(message.sequence) + " follows range " + std::to_string(previousSequence));
    }
    previousSequence = message.sequence;
    bool valid = !basis.range.isEmpty() && basis.range.high < keys.size();
    if (valid && range == basis.range) {
      failWith("a range was sent that is the one mounted");
    }
    if (tookReport && (range.low > waitingReport.low || range.high < waitingReport.high)) {
      failWith("a report's range leaves measured rows out");
    }
    std::size_t pacedLow = indexOf(pacedLowKey);
    std::size_t pacedHigh = indexOf(pacedHighKey);
    if (pacedLow != UNDEFINED_INDEX && pacedHigh != UNDEFINED_INDEX) {
      MountedRange paced{std::min(pacedLow, pacedHigh), std::max(pacedLow, pacedHigh)};
      std::size_t pace = mountStepForRange(paced, settings.minimumStep);
      std::size_t floor = valid ? std::min(basis.range.low, paced.low) : paced.low;
      std::size_t ceiling = valid ? std::max(basis.range.high, paced.high) : paced.high;
      if (range.low + pace < floor || range.high > ceiling + pace) {
        failWith("a range grew more than a step of " + std::to_string(pace) + " past rows " + std::to_string(floor) +
          ".." + std::to_string(ceiling));
      }
    }
    if (!driver.isPending() || driver.hasStep()) {
      failWith("the driver sent a range and does not wait for it");
    }
    inFlight = message;
  }

  /*
   * The renderer renders the range it was sent, unless it rebuilt its own since.
   */
  void apply() {
    if (!inFlight) {
      return;
    }
    if (inFlight->generation == generation) {
      applied = inFlight;
      seedPending = false;
    }
    inFlight.reset();
  }

  void check() const {
    if (driver.isPending() && driver.hasStep()) {
      failWith("a step is ready while a range is in flight");
    }
  }

  /*
   * With a renderer that applies at once and a screen that stays, the driver comes to rest in a
   * few frames, and the range then mounted holds the measured rows.
   */
  void settle() {
    reported.reset();
    for (int round = 0; round < 40; ++round) {
      apply();
      commit();
      frame();
      if (!inFlight && !driver.isPending() && !driver.hasStep()) {
        std::optional<MountedRange> now = measured();
        MountedRange range = mountedRange();
        if (now && (range.isEmpty() || range.low > now->low || range.high < now->high)) {
          failWith("at rest rows " + std::to_string(now->low) + ".." + std::to_string(now->high) +
            " are measured outside the mounted range " + std::to_string(range.low) + ".." + std::to_string(range.high));
        }
        return;
      }
    }
    failWith("the mounted range never came to rest");
  }
};

}

/*
 * MountDriver against a renderer that applies ranges late, changes its data, and rebuilds its
 * range around scroll targets. One range is in flight at a time, every range is built on the
 * commit it was sent in, the rows a report measured mount at once, the overscan grows a step
 * per frame, and a renderer that keeps up ends with the measured rows mounted.
 */
TEST(fuzz_mount_driver_paces_one_range_at_a_time_and_ends_on_the_measured_rows) {
  const FuzzKinds kinds = {
    {"scroll", "jump", "apply", "frame", "commit", "prepend", "append", "delete", "reseed", "command", "settle"},
    {},
  };
  forEachFuzzSeed(__func__, 200, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    MountRenderer renderer(seed, episode);
    renderer.settings.overscanRows = pickSize(renderer.rng, 0, 5);
    renderer.settings.overscanRowsLeading = renderer.settings.overscanRows + pickSize(renderer.rng, 0, 7);
    renderer.driver.setSettings(renderer.settings);
    renderer.keys = renderer.makeKeys(30 + pickSize(renderer.rng, 0, 99) * 4);
    renderer.measuredSpan = pickSize(renderer.rng, 2, 31);
    renderer.seedIndex = chance(renderer.rng, 60) ? 0 : pickSize(renderer.rng, 0, renderer.keys.size() - 1);
    renderer.measuredKey = renderer.keys[renderer.seedIndex];
    renderer.note("open n=" + std::to_string(renderer.keys.size()) + " at " + std::to_string(renderer.seedIndex));
    renderer.commit();

    for (int step = 0; step < 30; ++step) {
      int roll = pickPercent(renderer.rng);
      std::size_t size = renderer.keys.size();
      std::size_t count = pickSize(renderer.rng, 1, 12);
      std::size_t at = size > 0 ? pickSize(renderer.rng, 0, size - 1) : 0;
      if (roll < 22 && size > 0) {
        episode.reach("scroll");
        std::size_t low = renderer.indexOf(renderer.measuredKey);
        bool forward = chance(renderer.rng, 50);
        low = forward ? std::min(size - 1, low + count) : low - std::min(low, count);
        renderer.measuredKey = renderer.keys[low];
        renderer.note("scroll to row " + std::to_string(low));
      } else if (roll < 30 && size > 0) {
        episode.reach("jump");
        renderer.measuredKey = renderer.keys[at];
        renderer.note("jump to row " + std::to_string(at));
      } else if (roll < 48) {
        episode.reach("apply");
        renderer.note("apply");
        renderer.apply();
      } else if (roll < 62) {
        episode.reach("frame");
        renderer.frame();
        continue;
      } else if (roll < 68) {
        episode.reach("commit");
      } else if (roll < 74) {
        episode.reach("prepend");
        std::vector<std::string> made = renderer.makeKeys(count);
        renderer.keys.insert(renderer.keys.begin(), made.begin(), made.end());
        renderer.note("prepend " + std::to_string(count));
      } else if (roll < 80) {
        episode.reach("append");
        std::vector<std::string> made = renderer.makeKeys(count);
        renderer.keys.insert(renderer.keys.end(), made.begin(), made.end());
        renderer.note("append " + std::to_string(count));
      } else if (roll < 86 && size > 0) {
        episode.reach("delete");
        count = std::min(count, size - at);
        renderer.keys.erase(renderer.keys.begin() + static_cast<std::ptrdiff_t>(at),
          renderer.keys.begin() + static_cast<std::ptrdiff_t>(at + count));
        renderer.note("delete " + std::to_string(count) + " at " + std::to_string(at));
      } else if (roll < 91 && size > 0) {
        /*
         * A new scroll target: the renderer mounts around it under a new generation, and the
         * screen follows.
         */
        episode.reach("reseed");
        ++renderer.generation;
        renderer.applied.reset();
        renderer.seedIndex = at;
        renderer.measuredKey = renderer.keys[at];
        renderer.note("reseed at " + std::to_string(at));
      } else if (roll < 95) {
        episode.reach("command");
        renderer.seedPending = true;
        renderer.note("command");
      } else {
        episode.reach("settle");
        renderer.note("settle");
        renderer.settle();
        continue;
      }
      renderer.commit();
    }
    renderer.settle();
  });
}
