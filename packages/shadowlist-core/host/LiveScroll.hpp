#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace azimgd::shadowlist {

/*
 * Gesture phase values a host reports. Stored as doubles in the list state.
 * Android's ShadowListView.SCROLL_PHASE_* constants must match.
 */
constexpr double SCROLL_PHASE_IDLE = 0.0;
constexpr double SCROLL_PHASE_DRAGGING = 1.0;
constexpr double SCROLL_PHASE_SETTLING = 2.0;

/*
 * The host's newest scroll report, shared by every state of one list.
 *
 * The host writes it on every scroll frame without a commit. A commit that runs for any
 * other reason, like a React render, reads it in adopt(),
 * so the core never works from an offset older than the screen. That matters for keeping
 * content in place: an update run on an old offset anchors there and moves the content by
 * the difference.
 *
 * The report is written and read whole under a lock, so the offset always goes with the
 * token and phase of the same frame. The sequence goes up with every write. A state update
 * carries the sequence of the report it was built from, so adopt() only takes a report that
 * is newer than its state.
 */
class LiveScroll final {
public:
  struct Report {
    double offsetX{0.0};
    double offsetY{0.0};
    bool userScrolled{false};
    double scrollPhase{SCROLL_PHASE_IDLE};
    double commitToken{0.0};
    double concealGenerationAck{0.0};
    // 0 until the host writes its first report.
    std::uint64_t sequence{0};
  };

  LiveScroll() : handle_(nextHandle().fetch_add(1, std::memory_order_relaxed)) {}

  LiveScroll(const LiveScroll&) = delete;
  LiveScroll& operator=(const LiveScroll&) = delete;

  /*
   * Store a report and return its sequence.
   */
  std::uint64_t write(Report report) {
    std::lock_guard<std::mutex> lock(mutex_);
    report.sequence = ++sequence_;
    report_ = report;
    return report.sequence;
  }

  Report read() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return report_;
  }

  /*
   * The newest report when a commit should run on it instead of its state: the host wrote
   * one after the state was built, and the state has no offset of the core's own to apply.
   */
  bool newerReport(double stateSequence, bool stateOffsetEnabled, Report& report) const {
    if (stateOffsetEnabled) {
      return false;
    }
    report = read();
    return report.sequence != 0 && static_cast<double>(report.sequence) > stateSequence;
  }

  /*
   * A process wide id for this list, so a host that can't hold the pointer, like Android's
   * Java view, can find it. See registerHandle.
   */
  std::int64_t handle() const {
    return handle_;
  }

  /*
   * Hosts that reach the report by handle read big lists, like sticky and snap offsets, only
   * when their version moved. A version goes up whenever the published pointer differs from
   * the last one seen here. The weak pointers keep the old control blocks alive, so a new
   * list can't reuse an old address and look unchanged.
   */
  std::pair<std::uint64_t, std::uint64_t> geometryVersions(
    const std::shared_ptr<const void>& stickyIndices,
    const std::shared_ptr<const void>& stickyOffsets,
    const std::shared_ptr<const void>& stickySizes,
    const std::shared_ptr<const void>& snapOffsets);

  /*
   * Make a list findable by handle. Entries are weak and go away with their list.
   */
  static void registerHandle(const std::shared_ptr<LiveScroll>& liveScroll);
  static std::shared_ptr<LiveScroll> find(std::int64_t handle);

private:
  static std::atomic<std::int64_t>& nextHandle() {
    static std::atomic<std::int64_t> next{1};
    return next;
  }

  mutable std::mutex mutex_;
  Report report_{};
  std::uint64_t sequence_{0};
  const std::int64_t handle_;
  std::weak_ptr<const void> seenStickyIndices_;
  std::weak_ptr<const void> seenStickyOffsets_;
  std::weak_ptr<const void> seenStickySizes_;
  std::weak_ptr<const void> seenSnapOffsets_;
  std::uint64_t stickyVersion_{1};
  std::uint64_t snapVersion_{1};
};

}
