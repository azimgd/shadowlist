#include "ShadowListMountDriver.h"

#include <jsi/jsi.h>

#include <algorithm>
#include <chrono>
#include <iterator>
#include <unordered_map>
#include <utility>

namespace facebook::react {

namespace sl = azimgd::shadowlist;

namespace {

double nowMs() {
  using namespace std::chrono;
  return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

std::atomic<std::int64_t>& nextHandle() {
  static std::atomic<std::int64_t> next{1};
  return next;
}

std::mutex& registryMutex() {
  static std::mutex mutex;
  return mutex;
}

/*
 * Weak entries, swept when a new list registers. There are only ever a few lists.
 */
std::unordered_map<std::int64_t, std::weak_ptr<ShadowListMountDriver>>& registry() {
  static std::unordered_map<std::int64_t, std::weak_ptr<ShadowListMountDriver>> entries;
  return entries;
}

std::size_t sizeFromProp(int value, std::size_t fallback) {
  return value >= 0 ? static_cast<std::size_t>(value) : fallback;
}

int indexToInt(std::size_t index) {
  return index == sl::UNDEFINED_INDEX ? -1 : static_cast<int>(index);
}

jsi::Array stringArray(jsi::Runtime& runtime, const std::vector<std::string>& values) {
  auto array = jsi::Array(runtime, values.size());
  for (std::size_t position = 0; position < values.size(); ++position) {
    array.setValueAtIndex(runtime, position, jsi::String::createFromUtf8(runtime, values[position]));
  }
  return array;
}

jsi::Array indexArray(jsi::Runtime& runtime, const std::vector<std::size_t>& values) {
  auto array = jsi::Array(runtime, values.size());
  for (std::size_t position = 0; position < values.size(); ++position) {
    array.setValueAtIndex(runtime, position, static_cast<int>(values[position]));
  }
  return array;
}

}

std::shared_ptr<ShadowListMountDriver> makeShadowListMountDriver() {
  return std::make_shared<ShadowListMountDriver>();
}

ShadowListMountDriver::ShadowListMountDriver() : handle_(nextHandle().fetch_add(1, std::memory_order_relaxed)) {}

void ShadowListMountDriver::registerHandle(const std::shared_ptr<ShadowListMountDriver>& driver) {
  if (!driver) {
    return;
  }
  std::lock_guard<std::mutex> lock(registryMutex());
  auto& entries = registry();
  if (entries.find(driver->getHandle()) != entries.end()) {
    return;
  }
  for (auto entry = entries.begin(); entry != entries.end();) {
    entry = entry->second.expired() ? entries.erase(entry) : std::next(entry);
  }
  entries.emplace(driver->getHandle(), driver);
}

std::shared_ptr<ShadowListMountDriver> ShadowListMountDriver::find(std::int64_t handle) {
  std::lock_guard<std::mutex> lock(registryMutex());
  auto& entries = registry();
  auto entry = entries.find(handle);
  return entry == entries.end() ? nullptr : entry->second.lock();
}

void ShadowListMountDriver::beginCommit(
  const ShadowListViewProps& props,
  const std::shared_ptr<const ShadowListViewEventEmitter>& emitter,
  bool propsChanged) {
  std::lock_guard<std::mutex> lock(mutex_);
  emitter_ = emitter;
  propsChanged_ = propsChanged;
  if (!propsChanged) {
    return;
  }
  sl::MountSettings settings;
  settings.overscanRows = sizeFromProp(props.mountOverscanRows, sl::DEFAULT_OVERSCAN_ROWS);
  settings.overscanRowsLeading = sizeFromProp(props.mountOverscanRowsLeading, sl::DEFAULT_OVERSCAN_ROWS_LEADING);
  mount_.setSettings(settings);

  sl::MountedBasis basis;
  if (props.mountedLow >= 0 && props.mountedHigh >= props.mountedLow) {
    basis.range = {static_cast<std::size_t>(props.mountedLow), static_cast<std::size_t>(props.mountedHigh)};
  }
  basis.sequence = props.mountedSequence > 0 ? static_cast<std::uint64_t>(props.mountedSequence) : 0;
  basis.generation = props.mountedGeneration > 0 ? static_cast<std::uint64_t>(props.mountedGeneration) : 0;
  basis.seedPending = props.seedPending;
  mount_.setBasis(basis);

  // Two values per config: minimumViewTime, then 1 for waitForInteraction.
  std::size_t configs = props.viewableTimings.size() / 2;
  trackers_.resize(configs);
  for (std::size_t position = 0; position < configs; ++position) {
    trackers_[position].setTiming(
      {props.viewableTimings[position * 2], props.viewableTimings[position * 2 + 1] != 0.0});
  }
}

void ShadowListMountDriver::reportMeasured(std::size_t low, std::size_t high) {
  std::lock_guard<std::mutex> lock(mutex_);
  mount_.reportMeasured({low, high});
}

void ShadowListMountDriver::reportViewable(const std::vector<std::size_t>& ranges) {
  std::lock_guard<std::mutex> lock(mutex_);
  viewableRanges_ = ranges;
  viewableDirty_ = true;
}

void ShadowListMountDriver::endCommit(
  const sl::Container& core,
  const ShadowListViewProps& props,
  const std::vector<std::string>& keys,
  const std::vector<std::size_t>& stickyIndices) {
  Outgoing outgoing;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    outgoing.emitter = emitter_;
    double now = nowMs();

    outgoing.mount = mount_.process(keys, [&core](const std::string& key) { return core.indexOfKey(key); });

    /*
     * Viewability. A new range or new rows in it report again, compared by key. The pinned
     * section header follows the first visible row.
     */
    if (props.viewableEventEnabled && (viewableDirty_ || propsChanged_)) {
      viewableDirty_ = false;
      auto rangeAt = [&](std::size_t rule) -> std::optional<sl::MountedRange> {
        if (viewableRanges_.size() < rule * 2 + 2) {
          return std::nullopt;
        }
        auto range = sl::viewableRange(viewableRanges_[rule * 2], viewableRanges_[rule * 2 + 1]);
        if (range && range->high >= keys.size()) {
          return std::nullopt;
        }
        return range;
      };
      if (auto visible = rangeAt(0)) {
        visibleLow_ = static_cast<int>(visible->low);
        visibleHigh_ = static_cast<int>(visible->high);
        int active = indexToInt(sl::activeStickyIndexFor(stickyIndices, visible->low));
        if (active != activeStickyIndex_) {
          activeStickyIndex_ = active;
          outgoing.viewability = true;
        }
      } else {
        visibleLow_ = -1;
        visibleHigh_ = -1;
      }
      if (props.visibleRangeEventEnabled) {
        outgoing.viewability = true;
      }
      if (props.viewabilityInteracted && !interacted_) {
        interacted_ = true;
        for (auto& tracker : trackers_) {
          tracker.recordInteraction(now);
        }
      }
      for (std::size_t position = 0; position < trackers_.size(); ++position) {
        std::vector<std::string> viewable;
        if (auto range = rangeAt(position + 1)) {
          viewable.assign(keys.begin() + static_cast<std::ptrdiff_t>(range->low),
            keys.begin() + static_cast<std::ptrdiff_t>(range->high) + 1);
        }
        trackers_[position].update(std::move(viewable), now);
      }
    } else if (props.viewabilityInteracted && !interacted_) {
      interacted_ = true;
      for (auto& tracker : trackers_) {
        tracker.recordInteraction(now);
      }
    }
    takeViewabilityReports(now, outgoing);

    /*
     * Prefetch around the rows React mounts, once per new range or new data.
     */
    int prefetchRows = std::max(0, props.prefetchRows);
    if (prefetchRows != prefetchRows_) {
      prefetchRows_ = prefetchRows;
      prefetch_.clear();
      prefetchMounted_ = {};
    }
    sl::MountedRange mounted;
    if (props.mountedLow >= 0 && props.mountedHigh >= props.mountedLow &&
        static_cast<std::size_t>(props.mountedHigh) < keys.size()) {
      mounted = {static_cast<std::size_t>(props.mountedLow), static_cast<std::size_t>(props.mountedHigh)};
    }
    if (prefetchRows_ > 0 && (mounted != prefetchMounted_ || propsChanged_)) {
      prefetchMounted_ = mounted;
      std::optional<sl::MountedRange> mountedRows;
      if (!mounted.isEmpty()) {
        mountedRows = mounted;
      }
      prefetch_.update(mountedRows, sl::prefetchWindow(mountedRows, static_cast<std::size_t>(prefetchRows_), keys.size()),
        keys.size(), [&keys](std::size_t index) -> const std::string& { return keys[index]; },
        [&core](const std::string& key) { return core.indexOfKey(key); }, outgoing.prefetch, outgoing.cancel);
    }
    updateNeedsTick();
  }
  dispatch(outgoing);
}

bool ShadowListMountDriver::tick() {
  Outgoing outgoing;
  bool keepTicking = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    outgoing.emitter = emitter_;
    outgoing.mount = mount_.takeStep();
    takeViewabilityReports(nowMs(), outgoing);
    updateNeedsTick();
    keepTicking = needsTick_.load(std::memory_order_relaxed);
  }
  dispatch(outgoing);
  return keepTicking;
}

void ShadowListMountDriver::takeViewabilityReports(double now, Outgoing& outgoing) {
  for (std::size_t position = 0; position < trackers_.size(); ++position) {
    sl::ViewabilityReport report;
    if (trackers_[position].take(now, report)) {
      outgoing.reports.emplace_back(static_cast<int>(position), std::move(report));
      outgoing.viewability = true;
    }
  }
  outgoing.activeStickyIndex = activeStickyIndex_;
  outgoing.visibleLow = visibleLow_;
  outgoing.visibleHigh = visibleHigh_;
}

/*
 * A paced step waits for its frame, and a viewability report for its minimumViewTime.
 */
void ShadowListMountDriver::updateNeedsTick() {
  bool waiting = mount_.hasStep();
  for (const auto& tracker : trackers_) {
    waiting = waiting || tracker.isWaiting();
  }
  needsTick_.store(waiting, std::memory_order_release);
}

/*
 * The mounted range goes out as a unique event, which Fabric merges with an unread one: only
 * the newest range matters. Viewability and prefetch reports are changes and must all arrive.
 */
void ShadowListMountDriver::dispatch(Outgoing& outgoing) {
  const auto& emitter = outgoing.emitter;
  if (!emitter) {
    return;
  }
  if (outgoing.mount) {
    emitter->dispatchUniqueEvent("mountedRangeChange", [message = std::move(*outgoing.mount)](jsi::Runtime& runtime) {
      auto payload = jsi::Object(runtime);
      payload.setProperty(runtime, "lowKey", jsi::String::createFromUtf8(runtime, message.lowKey));
      payload.setProperty(runtime, "highKey", jsi::String::createFromUtf8(runtime, message.highKey));
      // Where the edge rows were in the keys native had, for JS to find them without a lookup.
      payload.setProperty(runtime, "lowIndex", indexToInt(message.range.low));
      payload.setProperty(runtime, "highIndex", indexToInt(message.range.high));
      payload.setProperty(runtime, "lowAtStart", message.lowAtStart);
      payload.setProperty(runtime, "highAtEnd", message.highAtEnd);
      payload.setProperty(runtime, "sequence", static_cast<double>(message.sequence));
      payload.setProperty(runtime, "generation", static_cast<double>(message.generation));
      payload.setProperty(runtime, "landed", message.landed);
      return payload;
    });
  }
  if (outgoing.viewability) {
    emitter->dispatchEvent(
      "viewabilityChange",
      [activeStickyIndex = outgoing.activeStickyIndex, visibleLow = outgoing.visibleLow,
       visibleHigh = outgoing.visibleHigh, reports = std::move(outgoing.reports)](jsi::Runtime& runtime) {
        auto payload = jsi::Object(runtime);
        payload.setProperty(runtime, "activeStickyIndex", activeStickyIndex);
        payload.setProperty(runtime, "visibleLow", visibleLow);
        payload.setProperty(runtime, "visibleHigh", visibleHigh);
        auto changes = jsi::Array(runtime, reports.size());
        for (std::size_t position = 0; position < reports.size(); ++position) {
          const auto& [config, report] = reports[position];
          auto change = jsi::Object(runtime);
          change.setProperty(runtime, "config", config);
          change.setProperty(runtime, "viewableKeys", stringArray(runtime, report.viewable));
          change.setProperty(runtime, "addedKeys", stringArray(runtime, report.changes.added));
          change.setProperty(runtime, "removedKeys", stringArray(runtime, report.changes.removed));
          changes.setValueAtIndex(runtime, position, change);
        }
        payload.setProperty(runtime, "changes", changes);
        return payload;
      },
      RawEvent::Category::Continuous);
  }
  if (!outgoing.prefetch.empty() || !outgoing.cancel.empty()) {
    emitter->dispatchEvent(
      "prefetchChange",
      [prefetch = std::move(outgoing.prefetch), cancel = std::move(outgoing.cancel)](jsi::Runtime& runtime) {
        auto payload = jsi::Object(runtime);
        payload.setProperty(runtime, "prefetch", indexArray(runtime, prefetch));
        payload.setProperty(runtime, "cancel", indexArray(runtime, cancel));
        return payload;
      },
      RawEvent::Category::Continuous);
  }
}

}
