#pragma once

#include <react/renderer/graphics/Float.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#ifdef ANDROID
#include <folly/dynamic.h>
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif

namespace facebook::react {

/*
 * Values of scrollPhase_. Android's ShadowListView.SCROLL_PHASE_* constants must match.
 * The component descriptor maps them to ScrollPhase.
 */
constexpr double SCROLL_PHASE_IDLE = 0.0;
constexpr double SCROLL_PHASE_DRAGGING = 1.0;
constexpr double SCROLL_PHASE_SETTLING = 2.0;

/*
 * Build switches for the scroll state path. Both can be A/B tested on one build with the
 * environment variables below, read once at launch.
 *
 * SHADOWLIST_SCROLL_BAND: hosts write every scroll frame into ShadowListLiveScroll and only
 * send a state update, which is a full commit, when the offset leaves the band the layout
 * pass published or something else the core needs changed. With the switch off the layout
 * pass publishes an empty band, so both hosts send every frame like before. Set the
 * environment variable SHADOWLIST_SCROLL_BAND=0 to turn it off. Android apps get no launch
 * environment, so there only the define counts.
 *
 * SHADOWLIST_IMMEDIATE_STATE: the iOS host and ShadowListNative data changes commit their
 * state update right away on the calling thread instead of on the next event beat. Off by
 * default. Set SHADOWLIST_IMMEDIATE_STATE=1 to try it.
 */
#ifndef SHADOWLIST_SCROLL_BAND
#define SHADOWLIST_SCROLL_BAND 1
#endif

#ifndef SHADOWLIST_IMMEDIATE_STATE
#define SHADOWLIST_IMMEDIATE_STATE 0
#endif

namespace shadowlist_detail {
/*
 * A compiled default that an environment variable set to 0 or 1 can flip.
 */
inline bool switchFromEnvironment(const char* name, bool compiledDefault) {
  const char* value = std::getenv(name);
  if (value == nullptr) {
    return compiledDefault;
  }
  if (std::strcmp(value, "0") == 0) {
    return false;
  }
  if (std::strcmp(value, "1") == 0) {
    return true;
  }
  return compiledDefault;
}
}

inline bool shadowListScrollBandEnabled() {
  static const bool enabled =
    shadowlist_detail::switchFromEnvironment("SHADOWLIST_SCROLL_BAND", SHADOWLIST_SCROLL_BAND != 0);
  return enabled;
}

inline bool shadowListImmediateStateEnabled() {
  static const bool enabled =
    shadowlist_detail::switchFromEnvironment("SHADOWLIST_IMMEDIATE_STATE", SHADOWLIST_IMMEDIATE_STATE != 0);
  return enabled;
}

/*
 * The host's newest scroll report, shared by every state of one list.
 *
 * The host writes it on every scroll frame without a commit. A commit that runs for any
 * other reason, like a React render or a ShadowListNative data change, reads it in adopt(),
 * so the core never works from an offset older than the screen. That matters for keeping
 * content in place: an update run on an old offset anchors there and moves the content by
 * the difference.
 *
 * The report is written and read whole under a lock, so the offset always goes with the
 * token and phase of the same frame. The sequence goes up with every write. A state update
 * carries the sequence of the report it was built from, see hostSequence_, so adopt() only
 * takes a report that is newer than its state.
 */
class ShadowListLiveScroll final {
public:
  struct Report {
    double offsetX{0.0};
    double offsetY{0.0};
    bool userScrolled{false};
    double scrollPhase{0.0};
    double commitToken{0.0};
    double concealGenerationAck{0.0};
    // 0 until the host writes its first report.
    std::uint64_t sequence{0};
  };

  ShadowListLiveScroll() : handle_(nextHandle().fetch_add(1, std::memory_order_relaxed)) {}

  ShadowListLiveScroll(const ShadowListLiveScroll&) = delete;
  ShadowListLiveScroll& operator=(const ShadowListLiveScroll&) = delete;

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
   * A process wide id for this list, so Android's Java host can find it through JNI.
   */
  std::int64_t handle() const {
    return handle_;
  }

  /*
   * Android reads state through a MapBuffer, which can't carry the big sticky and snap
   * lists cheaply. It gets a version for each instead, and fetches the lists only when the
   * version moved. A version goes up whenever the published pointer differs from the last
   * one seen here. The weak pointers keep the old control blocks alive, so a new list can't
   * reuse an old address and look unchanged.
   */
  std::pair<std::uint64_t, std::uint64_t> geometryVersions(
    const std::shared_ptr<const void>& stickyIndices,
    const std::shared_ptr<const void>& stickyOffsets,
    const std::shared_ptr<const void>& stickySizes,
    const std::shared_ptr<const void>& snapOffsets) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sameOwner(seenStickyIndices_, stickyIndices) || !sameOwner(seenStickyOffsets_, stickyOffsets) ||
        !sameOwner(seenStickySizes_, stickySizes)) {
      seenStickyIndices_ = stickyIndices;
      seenStickyOffsets_ = stickyOffsets;
      seenStickySizes_ = stickySizes;
      ++stickyVersion_;
    }
    if (!sameOwner(seenSnapOffsets_, snapOffsets)) {
      seenSnapOffsets_ = snapOffsets;
      ++snapVersion_;
    }
    return {stickyVersion_, snapVersion_};
  }

private:
  static std::atomic<std::int64_t>& nextHandle() {
    static std::atomic<std::int64_t> next{1};
    return next;
  }

  static bool sameOwner(const std::weak_ptr<const void>& seen, const std::shared_ptr<const void>& published) {
    return !seen.owner_before(published) && !published.owner_before(seen);
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

#ifdef ANDROID
/*
 * Lets Android's Java host reach a list's ShadowListLiveScroll by handle through JNI.
 * getMapBuffer registers the list, and an entry goes away with its list.
 */
void registerShadowListLiveScroll(const std::shared_ptr<ShadowListLiveScroll>& liveScroll);
std::shared_ptr<ShadowListLiveScroll> findShadowListLiveScroll(std::int64_t handle);

/*
 * Keys of the MapBuffer the Android host reads on every mount. ShadowListView.java uses
 * the same numbers.
 */
namespace ShadowListStateKey {
constexpr MapBuffer::Key TOTAL_WIDTH = 0;
constexpr MapBuffer::Key TOTAL_HEIGHT = 1;
constexpr MapBuffer::Key OFFSET_ENABLED = 2;
constexpr MapBuffer::Key OFFSET_X = 3;
constexpr MapBuffer::Key OFFSET_Y = 4;
constexpr MapBuffer::Key COMMIT_TOKEN = 5;
constexpr MapBuffer::Key MOMENTUM_YIELD_TOKEN = 6;
constexpr MapBuffer::Key OFFSET_BASE_X = 7;
constexpr MapBuffer::Key OFFSET_BASE_Y = 8;
constexpr MapBuffer::Key USER_SCROLLED = 9;
constexpr MapBuffer::Key SCROLL_PHASE = 10;
constexpr MapBuffer::Key COMMAND_SEQUENCE = 11;
constexpr MapBuffer::Key STICKY_VERSION = 12;
constexpr MapBuffer::Key SNAP_VERSION = 13;
constexpr MapBuffer::Key BAND_LOW = 14;
constexpr MapBuffer::Key BAND_HIGH = 15;
constexpr MapBuffer::Key LIVE_HANDLE = 16;
constexpr MapBuffer::Key CONCEAL_GENERATION = 17;
}
#endif

class ShadowListViewState final {
public:
  ShadowListViewState() = default;

  ShadowListViewState(
    double windowContainerHeight,
    double windowContainerWidth,
    double containerOffsetY,
    double containerOffsetX,
    double containerOffsetIndex,
    double containerOffsetIndexSequence,
    double totalContainerHeight,
    double totalContainerWidth,
    bool startReachedEnabled,
    bool endReachedEnabled,
    bool containerOffsetEnabled,
    double dragEventSequence,
    double dragEventType,
    std::string dragFromKey,
    std::string dragToKey) :
    windowContainerHeight_(windowContainerHeight),
    windowContainerWidth_(windowContainerWidth),
    containerOffsetY_(containerOffsetY),
    containerOffsetX_(containerOffsetX),
    containerOffsetIndex_(containerOffsetIndex),
    containerOffsetIndexSequence_(containerOffsetIndexSequence),
    totalContainerHeight_(totalContainerHeight),
    totalContainerWidth_(totalContainerWidth),
    startReachedEnabled_(startReachedEnabled),
    endReachedEnabled_(endReachedEnabled),
    containerOffsetEnabled_(containerOffsetEnabled),
    dragEventSequence_(dragEventSequence),
    dragEventType_(dragEventType),
    dragFromKey_(std::move(dragFromKey)),
    dragToKey_(std::move(dragToKey)) {}

#ifdef ANDROID
  ShadowListViewState(const ShadowListViewState& previousState, folly::dynamic data) :
    windowContainerHeight_(data.count("windowContainerHeight") ? (Float)data["windowContainerHeight"].getDouble() : previousState.windowContainerHeight_),
    windowContainerWidth_(data.count("windowContainerWidth") ? (Float)data["windowContainerWidth"].getDouble() : previousState.windowContainerWidth_),
    containerOffsetY_(data.count("containerOffsetY") ? (Float)data["containerOffsetY"].getDouble() : previousState.containerOffsetY_),
    containerOffsetX_(data.count("containerOffsetX") ? (Float)data["containerOffsetX"].getDouble() : previousState.containerOffsetX_),
    containerOffsetIndex_(data.count("containerOffsetIndex") ? (Float)data["containerOffsetIndex"].getDouble() : previousState.containerOffsetIndex_),
    containerOffsetIndexSequence_(data.count("containerOffsetIndexSequence") ? (Float)data["containerOffsetIndexSequence"].getDouble() : previousState.containerOffsetIndexSequence_),
    containerOffsetIndexViewPosition_(data.count("containerOffsetIndexViewPosition") ? (Float)data["containerOffsetIndexViewPosition"].getDouble() : previousState.containerOffsetIndexViewPosition_),
    totalContainerHeight_(data.count("totalContainerHeight") ? (Float)data["totalContainerHeight"].getDouble() : previousState.totalContainerHeight_),
    totalContainerWidth_(data.count("totalContainerWidth") ? (Float)data["totalContainerWidth"].getDouble() : previousState.totalContainerWidth_),
    startReachedEnabled_(data.count("startReachedEnabled") ? data["startReachedEnabled"].getBool() : previousState.startReachedEnabled_),
    endReachedEnabled_(data.count("endReachedEnabled") ? data["endReachedEnabled"].getBool() : previousState.endReachedEnabled_),
    containerOffsetEnabled_(data.count("containerOffsetEnabled") ? data["containerOffsetEnabled"].getBool() : previousState.containerOffsetEnabled_),
    dragEventSequence_(data.count("dragEventSequence") ? (Float)data["dragEventSequence"].getDouble() : previousState.dragEventSequence_),
    dragEventType_(data.count("dragEventType") ? (Float)data["dragEventType"].getDouble() : previousState.dragEventType_),
    dragFromKey_(data.count("dragFromKey") ? data["dragFromKey"].getString() : previousState.dragFromKey_),
    dragToKey_(data.count("dragToKey") ? data["dragToKey"].getString() : previousState.dragToKey_),
    userScrolled_(data.count("userScrolled") ? data["userScrolled"].getBool() : previousState.userScrolled_),
    scrollPhase_(data.count("scrollPhase") ? data["scrollPhase"].getDouble() : previousState.scrollPhase_),
    /*
     * Sticky header geometry only comes from the core, so a partial update from
     * the Android view, like a scroll commit, carries it over unchanged.
     */
    stickyHeaderIndices_(previousState.stickyHeaderIndices_),
    stickyHeaderOffsets_(previousState.stickyHeaderOffsets_),
    stickyHeaderSizes_(previousState.stickyHeaderSizes_),
    snapOffsets_(previousState.snapOffsets_),
    commitToken_(data.count("commitToken") ? (Float)data["commitToken"].getDouble() : previousState.commitToken_),
    /*
     * Carry the base over from the mounted state, like iOS does. A report that echoes a
     * correction keeps the base its first write started from, so a republished correction
     * stays one running delta the host has partly applied.
     */
    containerOffsetBaseX_(previousState.containerOffsetBaseX_),
    containerOffsetBaseY_(previousState.containerOffsetBaseY_),
    // Android never conceals rows, so just carry these over.
    concealGeneration_(previousState.concealGeneration_),
    concealGenerationAck_(previousState.concealGenerationAck_),
    // Only the core writes this, so carry it over.
    momentumYieldToken_(previousState.momentumYieldToken_),
    // The band comes from the layout pass and the live report is shared, so carry both over.
    offsetBandLow_(previousState.offsetBandLow_),
    offsetBandHigh_(previousState.offsetBandHigh_),
    hostSequence_(data.count("hostSequence") ? data["hostSequence"].asDouble() : previousState.hostSequence_),
    liveScroll_(previousState.liveScroll_) {
    if (data.count("stickyHeaderIndices") && data.count("stickyHeaderOffsets") && data.count("stickyHeaderSizes")) {
      auto stickyHeaderIndices = std::make_shared<std::vector<int>>();
      auto stickyHeaderOffsets = std::make_shared<std::vector<Float>>();
      auto stickyHeaderSizes = std::make_shared<std::vector<Float>>();
      for (const auto& value : data["stickyHeaderIndices"]) {
        stickyHeaderIndices->push_back((int)value.getInt());
      }
      for (const auto& value : data["stickyHeaderOffsets"]) {
        stickyHeaderOffsets->push_back((Float)value.getDouble());
      }
      for (const auto& value : data["stickyHeaderSizes"]) {
        stickyHeaderSizes->push_back((Float)value.getDouble());
      }
      /*
       * Turn empty back into null like the layout pass does. Otherwise a round trip
       * through Android hands back an empty list that the pointer check reads as a change.
       */
      stickyHeaderIndices_ = stickyHeaderIndices->empty() ? nullptr : std::move(stickyHeaderIndices);
      stickyHeaderOffsets_ = stickyHeaderOffsets->empty() ? nullptr : std::move(stickyHeaderOffsets);
      stickyHeaderSizes_ = stickyHeaderSizes->empty() ? nullptr : std::move(stickyHeaderSizes);
    }
    if (data.count("snapOffsets")) {
      auto snapOffsets = std::make_shared<std::vector<Float>>();
      for (const auto& value : data["snapOffsets"]) {
        snapOffsets->push_back((Float)value.getDouble());
      }
      snapOffsets_ = snapOffsets->empty() ? nullptr : std::move(snapOffsets);
    }
  };

  /*
   * Serializes the state into folly::dynamic for the Android renderer.
   */
  folly::dynamic getDynamic() const {
    folly::dynamic result = folly::dynamic::object;
    result["windowContainerHeight"] = windowContainerHeight_;
    result["windowContainerWidth"] = windowContainerWidth_;
    result["containerOffsetY"] = containerOffsetY_;
    result["containerOffsetX"] = containerOffsetX_;
    result["containerOffsetIndex"] = containerOffsetIndex_;
    result["containerOffsetIndexSequence"] = containerOffsetIndexSequence_;
    result["containerOffsetIndexViewPosition"] = containerOffsetIndexViewPosition_;
    result["totalContainerHeight"] = totalContainerHeight_;
    result["totalContainerWidth"] = totalContainerWidth_;
    result["startReachedEnabled"] = startReachedEnabled_;
    result["endReachedEnabled"] = endReachedEnabled_;
    result["containerOffsetEnabled"] = containerOffsetEnabled_;
    result["userScrolled"] = userScrolled_;
    result["scrollPhase"] = scrollPhase_;
    result["dragEventSequence"] = dragEventSequence_;
    result["dragEventType"] = dragEventType_;
    result["dragFromKey"] = dragFromKey_;
    result["dragToKey"] = dragToKey_;

    // A null pointer means empty.
    folly::dynamic stickyHeaderIndices = folly::dynamic::array;
    if (stickyHeaderIndices_) {
      for (auto stickyHeaderIndex : *stickyHeaderIndices_) {
        stickyHeaderIndices.push_back(stickyHeaderIndex);
      }
    }
    folly::dynamic stickyHeaderOffsets = folly::dynamic::array;
    if (stickyHeaderOffsets_) {
      for (auto stickyHeaderOffset : *stickyHeaderOffsets_) {
        stickyHeaderOffsets.push_back((double)stickyHeaderOffset);
      }
    }
    folly::dynamic stickyHeaderSizes = folly::dynamic::array;
    if (stickyHeaderSizes_) {
      for (auto stickyHeaderSize : *stickyHeaderSizes_) {
        stickyHeaderSizes.push_back((double)stickyHeaderSize);
      }
    }
    result["stickyHeaderIndices"] = stickyHeaderIndices;
    result["stickyHeaderOffsets"] = stickyHeaderOffsets;
    result["stickyHeaderSizes"] = stickyHeaderSizes;

    folly::dynamic snapOffsets = folly::dynamic::array;
    if (snapOffsets_) {
      for (auto snapOffset : *snapOffsets_) {
        snapOffsets.push_back((double)snapOffset);
      }
    }
    result["snapOffsets"] = snapOffsets;
    result["commitToken"] = commitToken_;
    result["containerOffsetBaseX"] = containerOffsetBaseX_;
    result["containerOffsetBaseY"] = containerOffsetBaseY_;
    result["momentumYieldToken"] = momentumYieldToken_;
    return result;
  };

  /*
   * The scalars the Android host reads on every mount. Much cheaper than getDynamic, which
   * copies the sticky and snap lists each time. Those lists get a version instead, see
   * ShadowListLiveScroll::geometryVersions, and the host reads getDynamic only when one moved.
   */
  MapBuffer getMapBuffer() const {
    MapBufferBuilder builder;
    builder.putDouble(ShadowListStateKey::TOTAL_WIDTH, totalContainerWidth_);
    builder.putDouble(ShadowListStateKey::TOTAL_HEIGHT, totalContainerHeight_);
    builder.putBool(ShadowListStateKey::OFFSET_ENABLED, containerOffsetEnabled_);
    builder.putDouble(ShadowListStateKey::OFFSET_X, containerOffsetX_);
    builder.putDouble(ShadowListStateKey::OFFSET_Y, containerOffsetY_);
    builder.putDouble(ShadowListStateKey::COMMIT_TOKEN, commitToken_);
    builder.putDouble(ShadowListStateKey::MOMENTUM_YIELD_TOKEN, momentumYieldToken_);
    builder.putDouble(ShadowListStateKey::OFFSET_BASE_X, containerOffsetBaseX_);
    builder.putDouble(ShadowListStateKey::OFFSET_BASE_Y, containerOffsetBaseY_);
    builder.putBool(ShadowListStateKey::USER_SCROLLED, userScrolled_);
    builder.putDouble(ShadowListStateKey::SCROLL_PHASE, scrollPhase_);
    builder.putDouble(ShadowListStateKey::COMMAND_SEQUENCE, containerOffsetIndexSequence_);
    std::pair<std::uint64_t, std::uint64_t> versions{0, 0};
    if (liveScroll_) {
      registerShadowListLiveScroll(liveScroll_);
      versions = liveScroll_->geometryVersions(stickyHeaderIndices_, stickyHeaderOffsets_, stickyHeaderSizes_, snapOffsets_);
    }
    builder.putLong(ShadowListStateKey::STICKY_VERSION, static_cast<std::int64_t>(versions.first));
    builder.putLong(ShadowListStateKey::SNAP_VERSION, static_cast<std::int64_t>(versions.second));
    builder.putDouble(ShadowListStateKey::BAND_LOW, offsetBandLow_);
    builder.putDouble(ShadowListStateKey::BAND_HIGH, offsetBandHigh_);
    builder.putLong(ShadowListStateKey::LIVE_HANDLE, liveScroll_ ? liveScroll_->handle() : 0);
    builder.putDouble(ShadowListStateKey::CONCEAL_GENERATION, concealGeneration_);
    return builder.build();
  }
#endif

  double windowContainerHeight_{0.0};
  double windowContainerWidth_{0.0};
  double containerOffsetY_{0.0};
  double containerOffsetX_{0.0};
  double containerOffsetIndex_{-2.0};
  double containerOffsetIndexSequence_{0.0};
  /*
   * Where scrollToIndex places its row in the viewport: 0 start, 0.5 center, 1 end.
   * The platform views carry it along with containerOffsetIndex_.
   */
  double containerOffsetIndexViewPosition_{0.0};
  double totalContainerHeight_{0.0};
  double totalContainerWidth_{0.0};
  bool startReachedEnabled_{true};
  bool endReachedEnabled_{true};
  bool containerOffsetEnabled_{false};

  /*
   * Drag to reorder. The platform view writes these as the gesture goes, and the
   * component descriptor fires each JS drag event once. The sequence goes up on every
   * drag event so a fresh event looks different from a carried one. The type is 1 for
   * start, 3 for end and 0 for none. Finger tracking stays native, so there is no
   * event in between. The keys name the moved row and its drop neighbor, and JS looks
   * them up in the current data so an edit mid drag can't move the wrong rows.
   * Keep these before userScrolled_ to match the Android constructor's init order.
   */
  double dragEventSequence_{0.0};
  double dragEventType_{0.0};
  std::string dragFromKey_{};
  std::string dragToKey_{};

  /*
   * True when this offset came from the user scrolling, false when it is the resting
   * position or an offset the core wrote. The core drops a pending correction as soon
   * as the user takes over, so a short keep in place nudge can't get stuck and freeze
   * the window. The platforms set it from their drag state.
   */
  bool userScrolled_{false};

  /*
   * The gesture phase the host last reported: idle, dragging with a finger down, or
   * settling with momentum. Unlike userScrolled_ it lasts across commits between touch
   * frames, so the core can tell a finger is still down even when the offset didn't move.
   * Keep it after userScrolled_ to match the Android constructor's init order.
   */
  double scrollPhase_{0.0};

  /*
   * Sticky header positions along the scroll axis from the layout pass, one entry per
   * sticky header in index order, empty for a plain list. The platforms pin the active
   * header from these each scroll frame, so they never read a view frame that may be
   * transformed. Keep them after scrollPhase_ to match the Android constructor's init order.
   *
   * These lists are held by pointer on purpose. State gets copied every commit, every
   * layout and every iOS scroll frame, but the lists change only when rows move. Copying
   * snap offsets by value meant copying a 100k entry vector several times a frame.
   * A shared pointer makes a copy cheap and makes the change check a pointer compare.
   *
   * A null pointer means empty, and readers must treat both the same. Nothing changes a
   * list after it is published, so sharing it across copies and threads is safe.
   */
  std::shared_ptr<const std::vector<int>> stickyHeaderIndices_{};
  std::shared_ptr<const std::vector<Float>> stickyHeaderOffsets_{};
  std::shared_ptr<const std::vector<Float>> stickyHeaderSizes_{};

  /*
   * Snap points along the scroll axis from the layout pass, empty unless snapToItem is
   * set. The platforms land the scroll on the nearest one. Held by pointer, see above.
   */
  std::shared_ptr<const std::vector<Float>> snapOffsets_{};

  /*
   * The id of the pending offset correction. The core sends it with the offset and the
   * view echoes it back, so the core knows its own write by id instead of guessing by
   * distance. Zero means no correction or a report from the host. Stored as a double like
   * the other fields. Keep it after snapOffsets_ to match the Android constructor's init order.
   */
  double commitToken_{0.0};

  /*
   * The offset the core started from when it published a correction. The container
   * offset minus this is the correction as a delta. A commit can mount frames after the
   * report it was built on, and a moving view has gone further by then, so writing the
   * absolute offset would make the content jump. While the view moves, both hosts add the
   * delta to the live offset instead, and also for an operation correction made from a
   * gesture report. Only meaningful when containerOffsetEnabled_ is set. Host reports
   * carry the mounted value. Keep it after commitToken_ to match the Android init order.
   */
  double containerOffsetBaseX_{0.0};
  double containerOffsetBaseY_{0.0};

  /*
   * Handshake for hiding rows, see ShadowListViewGeometryCache::concealedRows.
   * The core sets the generation of the newest hide it published, or 0 when nothing is
   * hidden. The host echoes back the generation of the state it mounted, which proves the
   * offset correction the hide waits for is on screen. The layout pass copies the echo
   * through untouched so it never claims more than the host mounted.
   * Keep these after containerOffsetBaseY_ to match the Android constructor's init order.
   */
  double concealGeneration_{0.0};
  double concealGenerationAck_{0.0};

  /*
   * The commit token of a scroll command issued in C++, like a ShadowListNative
   * scrollToIndex, or 0 when none. A host mounting that correction stops momentum first
   * and then writes the offset, just like its own scroll commands do.
   * Keep it after concealGenerationAck_ to match the Android constructor's init order.
   */
  double momentumYieldToken_{0.0};

  /*
   * The scroll offsets the host can move through without sending a state update, from the
   * layout pass, see azimgd::shadowlist::OffsetBand. Low above high means send every frame,
   * which is also the start value. Keep these after momentumYieldToken_ to match the Android
   * constructor's init order.
   */
  double offsetBandLow_{1.0};
  double offsetBandHigh_{0.0};

  /*
   * The sequence of the host report this state was built from, see ShadowListLiveScroll.
   * adopt() uses the live report instead when it is newer. Stored as a double like the rest.
   */
  double hostSequence_{0.0};

  /*
   * The host's newest scroll report. Made once with the list's first state and shared by
   * every copy after that, like the sticky and snap lists.
   */
  std::shared_ptr<ShadowListLiveScroll> liveScroll_{std::make_shared<ShadowListLiveScroll>()};
};

}
