#pragma once

#include <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#include <react/renderer/components/ShadowListViewSpec/Props.h>

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/host/MountDriver.hpp>
#include <shadowlist-core/host/Prefetch.hpp>
#include <shadowlist-core/host/Viewability.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace facebook::react {

/*
 * The rows React mounts, which rows are viewable and which to prefetch, worked out natively for
 * one list. JS only applies what it gets: onMountedRangeChange hands it the next range by edge
 * keys, onViewabilityChange the changed keys per config and the pinned section header, and
 * onPrefetchChange the rows to load early with their keys.
 *
 * The commit side runs in the component descriptor's adopt, under the core's lock: beginCommit,
 * the core's reports during its update, then endCommit. The host side runs on the UI thread:
 * after a mount it calls needsTick, and while that is true tick on every display frame. A tick
 * sends the next paced step of the mounted range and the viewability reports whose
 * minimumViewTime passed. One driver is made with the list's first state and shared by every
 * copy, like ShadowListLiveScroll. A tick takes the driver's own lock, not the core's. It waits only
 * while beginCommit, a report or endCommit holds that lock, not for the whole commit.
 */
class ShadowListMountDriver final {
public:
  ShadowListMountDriver();

  ShadowListMountDriver(const ShadowListMountDriver&) = delete;
  ShadowListMountDriver& operator=(const ShadowListMountDriver&) = delete;

  /*
   * Start a commit with its props. propsChanged is false when the props are the ones the
   * previous commit ran on, like on a scroll frame.
   */
  void beginCommit(
    const ShadowListViewProps& props,
    const std::shared_ptr<const ShadowListViewEventEmitter>& emitter,
    bool propsChanged);

  /*
   * The core's measured rows changed. Called during the core's update.
   */
  void reportMeasured(std::size_t low, std::size_t high);

  /*
   * The core's viewable rows changed, one range per rule. Called during the core's update.
   */
  void reportViewable(const std::vector<std::size_t>& ranges);

  /*
   * Finish the commit after the core's update and send what changed. keys are the rows the core
   * took in, and stickyIndices are sorted.
   */
  void endCommit(
    const azimgd::shadowlist::Container& core,
    const ShadowListViewProps& props,
    const std::vector<std::string>& keys,
    const std::vector<std::size_t>& stickyIndices);

  /*
   * Whether the host should tick on its next display frame.
   */
  bool needsTick() const {
    return needsTick_.load(std::memory_order_acquire);
  }

  /*
   * One display frame on the UI thread. Returns whether to keep ticking.
   */
  bool tick();

  /*
   * The sticky and snap lists of the state the Android view mounted last, set when it reads the
   * state's MapBuffer. The view copies them through ShadowListGeometry instead of reading the
   * whole state. Null means empty.
   */
  struct MountedGeometry {
    std::shared_ptr<const std::vector<int>> stickyIndices;
    std::shared_ptr<const std::vector<double>> stickyOffsets;
    std::shared_ptr<const std::vector<double>> stickySizes;
    std::shared_ptr<const std::vector<double>> snapOffsets;
  };

  void setMountedGeometry(MountedGeometry geometry) {
    std::lock_guard<std::mutex> lock(mutex_);
    mountedGeometry_ = std::move(geometry);
  }

  MountedGeometry getMountedGeometry() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mountedGeometry_;
  }

  /*
   * A process wide id for this list. Android's Java view finds the driver by it, see find.
   */
  std::int64_t getHandle() const {
    return handle_;
  }

  /*
   * Make a driver findable by handle. Entries are weak and go away with their list.
   */
  static void registerHandle(const std::shared_ptr<ShadowListMountDriver>& driver);
  static std::shared_ptr<ShadowListMountDriver> find(std::int64_t handle);

private:
  /*
   * What one commit or tick sends, built under the lock and dispatched after it.
   */
  struct Outgoing {
    std::shared_ptr<const ShadowListViewEventEmitter> emitter;
    std::optional<azimgd::shadowlist::MountMessage> mount;
    bool viewability = false;
    std::size_t activeStickyIndex = azimgd::shadowlist::UNDEFINED_INDEX;
    std::size_t visibleLow = azimgd::shadowlist::UNDEFINED_INDEX;
    std::size_t visibleHigh = azimgd::shadowlist::UNDEFINED_INDEX;
    std::vector<std::pair<std::size_t, azimgd::shadowlist::ViewabilityReport>> reports;
    std::vector<std::size_t> prefetch;
    std::vector<std::size_t> cancel;
    std::vector<std::string> prefetchKeys;
    std::vector<std::string> cancelKeys;
  };

  void takeViewabilityReports(double now, Outgoing& outgoing);
  void updateNeedsTick();
  static void dispatch(Outgoing& outgoing);

  mutable std::mutex mutex_;
  const std::int64_t handle_;
  std::atomic<bool> needsTick_{false};
  std::shared_ptr<const ShadowListViewEventEmitter> emitter_;

  azimgd::shadowlist::MountDriver mount_;

  /*
   * One tracker per viewability config. The core's rules have the pinned header rule first.
   */
  std::vector<azimgd::shadowlist::ViewabilityTracker> trackers_;
  std::vector<std::size_t> viewableRanges_;
  bool viewableDirty_ = false;
  bool interacted_ = false;
  std::size_t activeStickyIndex_ = azimgd::shadowlist::UNDEFINED_INDEX;
  std::size_t visibleLow_ = azimgd::shadowlist::UNDEFINED_INDEX;
  std::size_t visibleHigh_ = azimgd::shadowlist::UNDEFINED_INDEX;
  bool propsChanged_ = false;

  MountedGeometry mountedGeometry_;

  azimgd::shadowlist::PrefetchTracker prefetch_;
  azimgd::shadowlist::MountedRange prefetchMounted_;
  std::size_t prefetchRows_ = 0;
};

}
