#pragma once

#include <react/renderer/graphics/Float.h>

#include <shadowlist-core/host/ListCommit.hpp>
#include <shadowlist-core/host/LiveScroll.hpp>
#include <shadowlist-core/host/ScrollSync.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
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
 * Values of scrollPhase_, see azimgd::shadowlist::LiveScroll.
 */
using azimgd::shadowlist::SCROLL_PHASE_DRAGGING;
using azimgd::shadowlist::SCROLL_PHASE_IDLE;
using azimgd::shadowlist::SCROLL_PHASE_SETTLING;

using ShadowListLiveScroll = azimgd::shadowlist::LiveScroll;

/*
 * Build switches for the scroll state path. Both can be A/B tested on one build with the
 * environment variables below, read once at launch.
 *
 * SHADOWLIST_SCROLL_BAND: hosts write every scroll frame into ShadowListLiveScroll and only
 * send a state update, which is a full commit, when the offset leaves the band the layout
 * pass published or something else the core needs changed. With the switch off the layout
 * pass publishes an empty band and both hosts send every frame. Set the
 * environment variable SHADOWLIST_SCROLL_BAND=0 to turn it off. Android apps get no launch
 * environment. There only the define counts.
 *
 * SHADOWLIST_IMMEDIATE_STATE: the iOS host commits its state update right away on the
 * calling thread instead of on the next event beat. Off by default. Set
 * SHADOWLIST_IMMEDIATE_STATE=1 to try it.
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

#ifdef ANDROID
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
constexpr MapBuffer::Key ANIMATION_SEQUENCE = 18;
constexpr MapBuffer::Key ANIMATION_OFFSET = 19;
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
  ShadowListViewState(const ShadowListViewState& previousState, folly::dynamic data);

  /*
   * Serializes the state into folly::dynamic for the Android renderer.
   */
  folly::dynamic getDynamic() const;

  /*
   * The scalars the Android host reads on every mount. Much cheaper than getDynamic, which
   * copies the sticky and snap lists each time. Those lists get a version instead, see
   * ShadowListLiveScroll::geometryVersions, and the host reads getDynamic only when one moved.
   */
  MapBuffer getMapBuffer() const;
#endif

  /*
   * The scroll fields as the host layer's plain struct, and back.
   */
  azimgd::shadowlist::ListScrollState scrollState() const {
    azimgd::shadowlist::ListScrollState state;
    state.offsetX = containerOffsetX_;
    state.offsetY = containerOffsetY_;
    state.offsetEnabled = containerOffsetEnabled_;
    state.baseX = containerOffsetBaseX_;
    state.baseY = containerOffsetBaseY_;
    state.commitToken = commitToken_;
    state.userScrolled = userScrolled_;
    state.scrollPhase = scrollPhase_;
    state.totalWidth = totalContainerWidth_;
    state.totalHeight = totalContainerHeight_;
    return state;
  }

  void setScrollState(const azimgd::shadowlist::ListScrollState& state) {
    containerOffsetX_ = state.offsetX;
    containerOffsetY_ = state.offsetY;
    containerOffsetEnabled_ = state.offsetEnabled;
    containerOffsetBaseX_ = state.baseX;
    containerOffsetBaseY_ = state.baseY;
    commitToken_ = state.commitToken;
    userScrolled_ = state.userScrolled;
    scrollPhase_ = state.scrollPhase;
    totalContainerWidth_ = state.totalWidth;
    totalContainerHeight_ = state.totalHeight;
  }

  /*
   * Take a live report over the host owned fields.
   */
  void applyLiveReport(const ShadowListLiveScroll::Report& report) {
    containerOffsetX_ = report.offsetX;
    containerOffsetY_ = report.offsetY;
    containerOffsetEnabled_ = false;
    commitToken_ = report.commitToken;
    concealGenerationAck_ = report.concealGenerationAck;
    userScrolled_ = report.userScrolled;
    scrollPhase_ = report.scrollPhase;
    hostSequence_ = static_cast<double>(report.sequence);
  }

  /*
   * Patch a host update onto this state, see azimgd::shadowlist::ScrollPatch.
   */
  void applyPatch(const azimgd::shadowlist::ScrollPatch& patch);

  /*
   * Whether this state already holds all of a patch and the update can be dropped. Only when
   * nothing waits on the host: a state with an offset to apply or hidden rows needs its report
   * even if it repeats, since each commit moves those along.
   */
  bool holdsPatch(const azimgd::shadowlist::ScrollPatch& patch) const {
    const auto& report = patch.report;
    if (containerOffsetEnabled_ || patch.offsetEnabled || concealGeneration_ != 0.0) {
      return false;
    }
    return containerOffsetX_ == report.offsetX && containerOffsetY_ == report.offsetY &&
      commitToken_ == report.commitToken && concealGenerationAck_ == report.concealGenerationAck &&
      userScrolled_ == report.userScrolled && scrollPhase_ == report.scrollPhase &&
      (!patch.hasCommand ||
       (containerOffsetIndex_ == patch.commandIndex && containerOffsetIndexSequence_ == patch.commandSequence &&
        containerOffsetIndexViewPosition_ == patch.commandViewPosition &&
        containerOffsetIndexRowOffset_ == patch.commandRowOffset &&
        containerOffsetIndexAnimated_ == patch.commandAnimated)) &&
      (!patch.hasAnchorRequest || anchorRequestSequence_ == patch.anchorRequestSequence) &&
      (!patch.hasStartReachedEnabled || startReachedEnabled_ == patch.startReachedEnabled) &&
      (!patch.hasEndReachedEnabled || endReachedEnabled_ == patch.endReachedEnabled);
  }

  /*
   * What a host mounting this state needs, see azimgd::shadowlist::ScrollSync.
   */
  azimgd::shadowlist::MountedScroll mountedScroll() const {
    azimgd::shadowlist::MountedScroll mounted;
    mounted.offsetEnabled = containerOffsetEnabled_;
    mounted.offsetX = containerOffsetX_;
    mounted.offsetY = containerOffsetY_;
    mounted.baseX = containerOffsetBaseX_;
    mounted.baseY = containerOffsetBaseY_;
    mounted.commitToken = commitToken_;
    mounted.userScrolled = userScrolled_;
    mounted.scrollPhase = scrollPhase_;
    mounted.concealGeneration = concealGeneration_;
    mounted.concealGenerationAck = concealGenerationAck_;
    mounted.commandSequence = containerOffsetIndexSequence_;
    mounted.animationSequence = animationTargetSequence_;
    mounted.animationOffset = animationTargetOffset_;
    mounted.band.low = offsetBandLow_;
    mounted.band.high = offsetBandHigh_;
    return mounted;
  }

  double windowContainerHeight_{0.0};
  double windowContainerWidth_{0.0};
  double containerOffsetY_{0.0};
  double containerOffsetX_{0.0};
  double containerOffsetIndex_{-2.0};
  double containerOffsetIndexSequence_{0.0};
  /*
   * Where scrollToItem places its row in the viewport: 0 start, 0.5 center, 1 end.
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
   * start, 3 for end and 0 for none. Finger tracking stays native. There is no
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
   * as the user takes over. A short keep in place nudge can't get stuck and freeze
   * the window. The platforms set it from their drag state.
   */
  bool userScrolled_{false};

  /*
   * The gesture phase the host last reported: idle, dragging with a finger down, or
   * settling with momentum. Unlike userScrolled_ it lasts across commits between touch
   * frames. The core can tell a finger is still down even when the offset didn't move.
   * Keep it after userScrolled_ to match the Android constructor's init order.
   */
  double scrollPhase_{0.0};

  /*
   * Sticky header positions along the scroll axis from the layout pass, one entry per
   * sticky header in index order, empty for a plain list. The platforms pin the active
   * header from these each scroll frame. They never read a view frame that may be
   * transformed. Keep them after scrollPhase_ to match the Android constructor's init order.
   *
   * These lists are held by pointer on purpose. State gets copied every commit, every
   * layout and every iOS scroll frame, but the lists change only when rows move. Copying
   * snap offsets by value meant copying a 100k entry vector several times a frame.
   * A shared pointer makes a copy cheap and makes the change check a pointer compare.
   *
   * A null pointer means empty, and readers must treat both the same. Nothing changes a
   * list after it is published. Sharing it across copies and threads is safe.
   */
  std::shared_ptr<const std::vector<int>> stickyHeaderIndices_{};
  std::shared_ptr<const std::vector<double>> stickyHeaderOffsets_{};
  std::shared_ptr<const std::vector<double>> stickyHeaderSizes_{};

  /*
   * Snap points along the scroll axis from the layout pass, empty unless snapToItem is
   * set. The platforms land the scroll on the nearest one. Held by pointer, see above.
   */
  std::shared_ptr<const std::vector<double>> snapOffsets_{};

  /*
   * The id of the pending offset correction. The core sends it with the offset and the
   * view echoes it back. The core knows its own write by id instead of guessing by
   * distance. Zero means no correction or a report from the host. The dynamic and MapBuffer
   * forms carry it as a double. Keep it after snapOffsets_ to match the Android constructor's
   * init order.
   */
  std::uint64_t commitToken_{0};

  /*
   * The offset the core started from when it published a correction. The container
   * offset minus this is the correction as a delta. A commit can mount frames after the
   * report it was built on, and a moving view has gone further by then. Writing the
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
   * The scroll offsets the host can move through without sending a state update, from the
   * layout pass, see azimgd::shadowlist::OffsetBand. Low above high means send every frame,
   * which is also the start value. Keep these after concealGenerationAck_ to match the Android
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

  /*
   * The rest of the newest scroll command: how far past its view position the row rests, and
   * whether it animates to the core's estimate first. Keep these after liveScroll_ to match
   * the Android constructor's init order.
   */
  double containerOffsetIndexRowOffset_{0.0};
  bool containerOffsetIndexAnimated_{false};

  /*
   * Where the core estimates the newest animated command lands along the scroll axis, and its
   * sequence, from the layout pass. The host animates there, then sends the command again
   * without the animation.
   */
  double animationTargetSequence_{0.0};
  double animationTargetOffset_{0.0};

  /*
   * Bumped by the host to ask for the anchor at its offset. The component descriptor answers
   * once per new value with onAnchorState.
   */
  double anchorRequestSequence_{0.0};
};

}
