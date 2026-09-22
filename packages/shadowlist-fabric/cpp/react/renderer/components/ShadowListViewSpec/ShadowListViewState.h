#pragma once

#include <react/renderer/graphics/Float.h>

#include <memory>
#include <string>
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
    momentumYieldToken_(previousState.momentumYieldToken_) {
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
};

}
