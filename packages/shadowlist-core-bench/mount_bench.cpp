/*
 * Mounted range, viewability and prefetch benchmark: the work Fabric's ShadowListMountDriver
 * does per scroll, over a deterministic trace.
 *
 * The trace is the same as the JS engine benchmark it replaced used: a 390x844 viewport, row
 * heights 48 to 239 from mulberry32 seeded with the row count, the core's window one viewport
 * past each side, a section header every 20 rows, one viewability config at 50 percent, prefetch
 * 10 rows around the mounted ones and 1,369 frames of drags, flings and a jump to the middle.
 * The checksum it prints matches the JS trace's.
 *
 * Each frame runs like the list does: the display frame tick takes a paced step, then the
 * commit gives the driver its basis and the core's reports and sends what changed. The renderer
 * applies every range at once. Prints a TSV block with the median of the timed runs, and with
 * --dump writes the events JS gets, for replaying the JS side.
 */

#include <shadowlist-core/host/MountDriver.hpp>
#include <shadowlist-core/host/MountedRange.hpp>
#include <shadowlist-core/host/Prefetch.hpp>
#include <shadowlist-core/host/Viewability.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

using namespace azimgd::shadowlist;

namespace {

constexpr double VIEWPORT = 844.0;
constexpr int ITERATIONS = 9;
constexpr std::size_t PREFETCH_ROWS = 10;

struct Frame {
  std::size_t measuredLow;
  std::size_t measuredHigh;
  std::size_t ranges[4];
  long jump;
};

struct Trace {
  std::vector<Frame> frames;
  std::vector<std::size_t> stickyIndices;
  std::vector<std::string> keys;
  std::unordered_map<std::string, std::size_t> keyToIndex;
  long long checksum = 0;
};

/*
 * mulberry32, bit for bit like the JS one.
 */
struct Mulberry32 {
  std::uint32_t state;

  double next() {
    state += 0x6d2b79f5u;
    std::uint32_t t = state;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
  }
};

std::size_t rowAt(const std::vector<double>& starts, double position) {
  std::size_t low = 0;
  std::size_t high = starts.size() - 2;
  while (low < high) {
    std::size_t mid = (low + high) / 2;
    if (starts[mid + 1] <= position) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  return low;
}

std::pair<long, long> overlapRange(const std::vector<double>& starts, double top, double bottom) {
  long count = static_cast<long>(starts.size()) - 1;
  long low = static_cast<long>(rowAt(starts, std::max(0.0, top)));
  long high = static_cast<long>(rowAt(starts, std::max(0.0, bottom - 0.001)));
  return {std::min(low, count - 1), std::min(high, count - 1)};
}

std::pair<long, long> halfVisibleRange(const std::vector<double>& starts, double top, double bottom) {
  auto [low, high] = overlapRange(starts, top, bottom);
  auto share = [&](long index) {
    double start = starts[index];
    double end = starts[index + 1];
    return (std::min(end, bottom) - std::max(start, top)) / (end - start);
  };
  while (low <= high && share(low) < 0.5) {
    ++low;
  }
  while (high >= low && share(high) < 0.5) {
    --high;
  }
  return low <= high ? std::pair<long, long>{low, high} : std::pair<long, long>{-1, -1};
}

std::size_t toIndex(long value) {
  return value < 0 ? UNDEFINED_INDEX : static_cast<std::size_t>(value);
}

Trace buildTrace(std::size_t count) {
  Trace trace;
  Mulberry32 random{static_cast<std::uint32_t>(count)};
  std::vector<double> starts(count + 1, 0.0);
  for (std::size_t index = 0; index < count; ++index) {
    starts[index + 1] = starts[index] + (48.0 + std::floor(random.next() * 192.0));
  }
  double maxOffset = starts[count] - VIEWPORT;
  std::vector<double> offsets;
  double offset = 0.0;
  auto push = [&](double next) {
    offset = std::max(0.0, std::min(maxOffset, next));
    offsets.push_back(offset);
  };
  for (int frame = 0; frame < 600; ++frame) {
    push(offset + 4.0);
  }
  for (int fling = 0; fling < 3; ++fling) {
    for (double velocity = 90.0; velocity > 0.5; velocity *= 0.95) {
      push(offset + velocity);
    }
  }
  for (double velocity = 90.0; velocity > 0.5; velocity *= 0.95) {
    push(offset - velocity);
  }
  std::size_t jumpTarget = count / 2;
  std::size_t jumpFrame = offsets.size();
  push(starts[jumpTarget]);
  for (int idle = 0; idle < 60; ++idle) {
    push(offset);
  }
  for (int frame = 0; frame < 300; ++frame) {
    push(offset + 12.0);
  }
  for (std::size_t index = 0; index < offsets.size(); ++index) {
    double at = offsets[index];
    auto measured = overlapRange(starts, at - VIEWPORT, at + 2.0 * VIEWPORT);
    auto visible = overlapRange(starts, at, at + VIEWPORT);
    auto half = halfVisibleRange(starts, at, at + VIEWPORT);
    Frame frame{toIndex(measured.first), toIndex(measured.second),
      {toIndex(visible.first), toIndex(visible.second), toIndex(half.first), toIndex(half.second)},
      index == jumpFrame ? static_cast<long>(jumpTarget) : -1};
    trace.frames.push_back(frame);
    trace.checksum = (trace.checksum + measured.first * 3 + measured.second + half.first) % 1000000007LL;
  }
  for (std::size_t index = 0; index < count; index += 20) {
    trace.stickyIndices.push_back(index);
  }
  trace.keys.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    trace.keys.push_back("row-" + std::to_string(index));
    trace.keyToIndex.emplace(trace.keys.back(), index);
  }
  return trace;
}

struct Stats {
  std::size_t mountEvents = 0;
  std::size_t steps = 0;
  std::size_t viewabilityEvents = 0;
  std::size_t prefetchEvents = 0;
  std::size_t prefetched = 0;
  std::size_t cancelled = 0;
  std::size_t reports = 0;
  std::size_t ticks = 0;
  double mountUs = 0.0;
  double viewabilityUs = 0.0;
  double prefetchUs = 0.0;
};

double nowUs() {
  using namespace std::chrono;
  return duration<double, std::micro>(steady_clock::now().time_since_epoch()).count();
}

/*
 * One run over the trace, like ShadowListMountDriver: tick, then commit. dump gets the events
 * JS receives, as JSON lines, when set.
 */
Stats run(const Trace& trace, bool instrument, std::FILE* dump) {
  Stats stats;
  const std::vector<std::string>& keys = trace.keys;
  auto indexOfKey = [&trace](const std::string& key) {
    auto found = trace.keyToIndex.find(key);
    return found == trace.keyToIndex.end() ? UNDEFINED_INDEX : found->second;
  };
  MountDriver driver;
  MountSettings settings;
  settings.overscanRows = 4;
  settings.overscanRowsLeading = 10;
  driver.setSettings(settings);
  MountedBasis basis;
  basis.range = {0, 20};
  std::vector<ViewabilityTracker> trackers(1);
  PrefetchTracker prefetch;
  MountedRange prefetchMounted;
  int activeSticky = -1;
  std::size_t previousMeasuredLow = UNDEFINED_INDEX - 1;
  std::size_t previousMeasuredHigh = UNDEFINED_INDEX - 1;
  std::size_t previousRanges[4] = {0, 0, 0, 0};
  bool haveRanges = false;
  std::vector<std::size_t> prefetchRows;
  std::vector<std::size_t> cancelRows;
  double clock = 0.0;
  auto start = [&]() {
    if (instrument) {
      clock = nowUs();
    }
  };
  auto stop = [&](double& total) {
    if (instrument) {
      total += nowUs() - clock;
    }
  };

  std::size_t frameIndex = 0;
  auto apply = [&](const MountMessage& message) {
    ++stats.mountEvents;
    if (dump) {
      std::fprintf(dump,
        "{\"f\":%zu,\"t\":\"m\",\"lowKey\":\"%s\",\"highKey\":\"%s\",\"lowAtStart\":%s,\"highAtEnd\":%s,"
        "\"sequence\":%llu,\"generation\":%llu,\"landed\":%s}\n",
        frameIndex, message.lowKey.c_str(), message.highKey.c_str(), message.lowAtStart ? "true" : "false",
        message.highAtEnd ? "true" : "false", static_cast<unsigned long long>(message.sequence),
        static_cast<unsigned long long>(message.generation), message.landed ? "true" : "false");
    }
    basis.seedPending = false;
    if (!message.landed) {
      basis.range = message.range;
      basis.sequence = message.sequence;
    }
  };

  for (; frameIndex < trace.frames.size(); ++frameIndex) {
    const Frame& frame = trace.frames[frameIndex];

    // The display frame: a paced step.
    if (driver.hasStep()) {
      ++stats.ticks;
      start();
      auto step = driver.takeStep();
      stop(stats.mountUs);
      if (step) {
        ++stats.steps;
        apply(*step);
      }
    }
    // scrollToIndex mounts the target's rows next to the range until a report says it landed.
    if (frame.jump >= 0) {
      basis.seedPending = true;
    }

    // The commit.
    start();
    driver.setBasis(basis);
    bool measuredChanged = frame.measuredLow != previousMeasuredLow || frame.measuredHigh != previousMeasuredHigh;
    if (measuredChanged) {
      previousMeasuredLow = frame.measuredLow;
      previousMeasuredHigh = frame.measuredHigh;
      ++stats.reports;
      driver.reportMeasured({frame.measuredLow, frame.measuredHigh});
    }
    auto message = driver.process(keys, indexOfKey);
    stop(stats.mountUs);
    if (message) {
      apply(*message);
    }

    bool rangesChanged = !haveRanges || !std::equal(std::begin(frame.ranges), std::end(frame.ranges), previousRanges);
    if (rangesChanged) {
      haveRanges = true;
      std::copy(std::begin(frame.ranges), std::end(frame.ranges), previousRanges);
      start();
      bool send = false;
      int active = activeSticky;
      if (auto visible = viewableRange(frame.ranges[0], frame.ranges[1])) {
        std::size_t sticky = activeStickyIndexFor(trace.stickyIndices, visible->low);
        active = sticky == UNDEFINED_INDEX ? -1 : static_cast<int>(sticky);
      }
      if (active != activeSticky) {
        activeSticky = active;
        send = true;
      }
      std::vector<std::string> viewable;
      if (auto range = viewableRange(frame.ranges[2], frame.ranges[3])) {
        viewable.assign(keys.begin() + static_cast<std::ptrdiff_t>(range->low),
          keys.begin() + static_cast<std::ptrdiff_t>(range->high) + 1);
      }
      trackers[0].update(std::move(viewable), static_cast<double>(frameIndex));
      ViewabilityReport report;
      bool reported = trackers[0].take(static_cast<double>(frameIndex), report);
      stop(stats.viewabilityUs);
      if (reported || send) {
        ++stats.viewabilityEvents;
        if (dump) {
          std::fprintf(dump, "{\"f\":%zu,\"t\":\"v\",\"activeStickyIndex\":%d,\"changes\":[", frameIndex, activeSticky);
          if (reported) {
            auto list = [&](const std::vector<std::string>& values) {
              std::fputc('[', dump);
              for (std::size_t at = 0; at < values.size(); ++at) {
                std::fprintf(dump, "%s\"%s\"", at > 0 ? "," : "", values[at].c_str());
              }
              std::fputc(']', dump);
            };
            std::fprintf(dump, "{\"config\":0,\"viewableKeys\":");
            list(report.viewable);
            std::fprintf(dump, ",\"addedKeys\":");
            list(report.changes.added);
            std::fprintf(dump, ",\"removedKeys\":");
            list(report.changes.removed);
            std::fputc('}', dump);
          }
          std::fprintf(dump, "]}\n");
        }
      }
    }

    // Prefetch once per new mounted range.
    if (basis.range != prefetchMounted) {
      prefetchMounted = basis.range;
      start();
      std::optional<MountedRange> mounted = basis.range;
      prefetch.update(mounted, prefetchWindow(mounted, PREFETCH_ROWS, keys.size()), keys.size(),
        [&keys](std::size_t index) -> const std::string& { return keys[index]; }, indexOfKey, prefetchRows,
        cancelRows);
      stop(stats.prefetchUs);
      if (!prefetchRows.empty() || !cancelRows.empty()) {
        ++stats.prefetchEvents;
        stats.prefetched += prefetchRows.size();
        stats.cancelled += cancelRows.size();
        if (dump) {
          std::fprintf(dump, "{\"f\":%zu,\"t\":\"p\",\"prefetch\":%zu,\"cancel\":%zu}\n", frameIndex,
            prefetchRows.size(), cancelRows.size());
        }
      }
    }
  }
  return stats;
}

}

int main(int argc, char** argv) {
  const char* dumpDir = nullptr;
  for (int at = 1; at + 1 < argc; ++at) {
    if (std::strcmp(argv[at], "--dump") == 0) {
      dumpDir = argv[at + 1];
    }
  }
  std::printf("rows\tframes\tchecksum\tmedian_us\tmin_us\tmax_us\tmount_us\tviewability_us\tprefetch_us\t"
              "crossings\tmount_events\tsteps\tviewability_events\tprefetch_events\treports\tprefetched\tcancelled\n");
  for (std::size_t count : {std::size_t{10000}, std::size_t{100000}}) {
    Trace trace = buildTrace(count);
    run(trace, false, nullptr);
    run(trace, false, nullptr);
    std::vector<double> times;
    Stats stats;
    for (int iteration = 0; iteration < ITERATIONS; ++iteration) {
      double begin = nowUs();
      stats = run(trace, false, nullptr);
      times.push_back(nowUs() - begin);
    }
    Stats breakdown = run(trace, true, nullptr);
    if (dumpDir) {
      std::string path = std::string(dumpDir) + "/events-" + std::to_string(count) + ".jsonl";
      if (std::FILE* dump = std::fopen(path.c_str(), "w")) {
        run(trace, false, dump);
        std::fclose(dump);
      }
    }
    std::vector<double> sorted = times;
    std::sort(sorted.begin(), sorted.end());
    std::printf("%zu\t%zu\t%lld\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n", count,
      trace.frames.size(), trace.checksum, sorted[sorted.size() / 2], sorted.front(), sorted.back(),
      breakdown.mountUs, breakdown.viewabilityUs, breakdown.prefetchUs,
      stats.mountEvents + stats.viewabilityEvents + stats.prefetchEvents, stats.mountEvents, stats.steps,
      stats.viewabilityEvents, stats.prefetchEvents, stats.reports, stats.prefetched, stats.cancelled);
  }
  return 0;
}
