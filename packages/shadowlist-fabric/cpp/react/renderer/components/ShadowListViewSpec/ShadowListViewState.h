#pragma once

#include <string>
#include <vector>

#include <react/renderer/graphics/Float.h>

#ifdef ANDROID
#include <folly/dynamic.h>
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif

namespace facebook::react {

/*
 * State for <ShadowListView> component.
 */
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
    /*
     * Sticky section-header geometry is produced by the C++ core (layout pass) and
     * only ever flows core -> view, so a partial update from the Android view
     * (e.g. a scroll commit) carries it forward unchanged.
     */
    stickyHeaderIndices_(previousState.stickyHeaderIndices_),
    stickyHeaderOffsets_(previousState.stickyHeaderOffsets_),
    stickyHeaderSizes_(previousState.stickyHeaderSizes_),
    snapOffsets_(previousState.snapOffsets_),
    commitToken_(data.count("commitToken") ? (Float)data["commitToken"].getDouble() : previousState.commitToken_) {
    if (data.count("stickyHeaderIndices") && data.count("stickyHeaderOffsets") && data.count("stickyHeaderSizes")) {
      stickyHeaderIndices_.clear();
      stickyHeaderOffsets_.clear();
      stickyHeaderSizes_.clear();
      for (const auto& value : data["stickyHeaderIndices"]) {
        stickyHeaderIndices_.push_back((int)value.getInt());
      }
      for (const auto& value : data["stickyHeaderOffsets"]) {
        stickyHeaderOffsets_.push_back((Float)value.getDouble());
      }
      for (const auto& value : data["stickyHeaderSizes"]) {
        stickyHeaderSizes_.push_back((Float)value.getDouble());
      }
    }
    if (data.count("snapOffsets")) {
      snapOffsets_.clear();
      for (const auto& value : data["snapOffsets"]) {
        snapOffsets_.push_back((Float)value.getDouble());
      }
    }
  };

  /* Serializes the state into folly::dynamic for the Android renderer. */
  folly::dynamic getDynamic() const {
    folly::dynamic result = folly::dynamic::object;
    result["windowContainerHeight"] = windowContainerHeight_;
    result["windowContainerWidth"] = windowContainerWidth_;
    result["containerOffsetY"] = containerOffsetY_;
    result["containerOffsetX"] = containerOffsetX_;
    result["containerOffsetIndex"] = containerOffsetIndex_;
    result["containerOffsetIndexSequence"] = containerOffsetIndexSequence_;
    result["totalContainerHeight"] = totalContainerHeight_;
    result["totalContainerWidth"] = totalContainerWidth_;
    result["startReachedEnabled"] = startReachedEnabled_;
    result["endReachedEnabled"] = endReachedEnabled_;
    result["containerOffsetEnabled"] = containerOffsetEnabled_;
    result["userScrolled"] = userScrolled_;
    result["dragEventSequence"] = dragEventSequence_;
    result["dragEventType"] = dragEventType_;
    result["dragFromKey"] = dragFromKey_;
    result["dragToKey"] = dragToKey_;

    folly::dynamic stickyHeaderIndices = folly::dynamic::array;
    for (auto stickyHeaderIndex : stickyHeaderIndices_) {
      stickyHeaderIndices.push_back(stickyHeaderIndex);
    }
    folly::dynamic stickyHeaderOffsets = folly::dynamic::array;
    for (auto stickyHeaderOffset : stickyHeaderOffsets_) {
      stickyHeaderOffsets.push_back((double)stickyHeaderOffset);
    }
    folly::dynamic stickyHeaderSizes = folly::dynamic::array;
    for (auto stickyHeaderSize : stickyHeaderSizes_) {
      stickyHeaderSizes.push_back((double)stickyHeaderSize);
    }
    result["stickyHeaderIndices"] = stickyHeaderIndices;
    result["stickyHeaderOffsets"] = stickyHeaderOffsets;
    result["stickyHeaderSizes"] = stickyHeaderSizes;

    folly::dynamic snapOffsets = folly::dynamic::array;
    for (auto snapOffset : snapOffsets_) {
      snapOffsets.push_back((double)snapOffset);
    }
    result["snapOffsets"] = snapOffsets;
    result["commitToken"] = commitToken_;
    return result;
  };
#endif

  double windowContainerHeight_{0.0};
  double windowContainerWidth_{0.0};
  double containerOffsetY_{0.0};
  double containerOffsetX_{0.0};
  double containerOffsetIndex_{-2.0};
  double containerOffsetIndexSequence_{0.0};
  double totalContainerHeight_{0.0};
  double totalContainerWidth_{0.0};
  bool startReachedEnabled_{true};
  bool endReachedEnabled_{true};
  bool containerOffsetEnabled_{false};

  /*
   * Drag-to-reorder signalling, written by the platform view as the native gesture
   * progresses and consumed by the component descriptor to emit the JS onDrag*
   * events exactly once per change. dragEventSequence_ is bumped on every drag event so
   * the descriptor can tell a fresh event from a carried-forward one (a plain scroll
   * commit leaves it unchanged). dragEventType_ is 1=start, 3=end (0=none); there is
   * no mid-drag event, the finger tracking and shuffle stay native and never reach
   * JS. dragFromKey_/dragToKey_ carry the keys for that event (the moved row
   * and its drop-target neighbour): JS resolves them against the current data
   * so an edit between gesture and drop can't reorder the wrong rows. Declared before
   * userScrolled_ so the Android constructor's member-init order matches.
   */
  double dragEventSequence_{0.0};
  double dragEventType_{0.0};
  std::string dragFromKey_{};
  std::string dragToKey_{};

  /*
   * True when the offset in this state came from a genuine user scroll gesture,
   * false when it is the view's resting position or an offset the core itself
   * applied. The core uses it to abandon an in-flight scroll correction the moment
   * the user takes over (see Virtualizer::update / FrameInput::userScrolled), so a
   * transient maintain-visible-content-position nudge cannot latch and freeze the
   * virtualization window. The integrations set it from the platform drag state.
   */
  bool userScrolled_{false};

  /*
   * Sticky section-header geometry along the scroll axis, produced by the core's
   * layout pass (one entry per sticky section header, ascending by index). The
   * integrations pin the active header on the UI thread per scroll frame from this,
   * mirroring Container::resolveStickyHeader, so the per-frame pin never reads a
   * (possibly transformed) view frame. Empty for a plain list. Declared after
   * userScrolled_ so the Android constructor's member-init order matches.
   */
  std::vector<int> stickyHeaderIndices_{};
  std::vector<Float> stickyHeaderOffsets_{};
  std::vector<Float> stickyHeaderSizes_{};

  /*
   * Resting snap offsets along the scroll axis (DIP), produced by the core's layout
   * pass. Empty unless snapToItem is set. The integrations snap the native scroll
   * view's landing position to the nearest of these.
   */
  std::vector<Float> snapOffsets_{};

  /*
   * Commit token: the id of the in-flight offset correction. Core -> view it rides on
   * the applied offset; view -> core it is echoed back so the core recognises its own
   * write exactly (by id), instead of by a pixel-distance heuristic. 0 = no correction
   * / a host-originated report. Carried as a double to match the other scalar state
   * fields; the integer id never approaches double's exact-integer range in practice.
   * Declared last so the Android constructor's member-init order matches.
   */
  double commitToken_{0.0};
};

}
