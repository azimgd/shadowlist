#pragma once

#include <vector>

#include <react/renderer/graphics/Float.h>

#ifdef ANDROID
#include <folly/dynamic.h>
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif

namespace facebook::react {

/*
 * State for <ShadowlistView> component.
 */
class ShadowlistViewState final {
  public:
  ShadowlistViewState() = default;

  ShadowlistViewState(
    double windowContainerHeight,
    double windowContainerWidth,
    double containerOffsetY,
    double containerOffsetX,
    double containerOffsetIndex,
    double containerOffsetIndexNonce,
    double totalContainerHeight,
    double totalContainerWidth,
    bool startReachedEnabled,
    bool endReachedEnabled,
    bool containerOffsetEnabled,
    double dragEventNonce,
    double dragEventType,
    double dragFromIndex,
    double dragToIndex) :
    windowContainerHeight_(windowContainerHeight),
    windowContainerWidth_(windowContainerWidth),
    containerOffsetY_(containerOffsetY),
    containerOffsetX_(containerOffsetX),
    containerOffsetIndex_(containerOffsetIndex),
    containerOffsetIndexNonce_(containerOffsetIndexNonce),
    totalContainerHeight_(totalContainerHeight),
    totalContainerWidth_(totalContainerWidth),
    startReachedEnabled_(startReachedEnabled),
    endReachedEnabled_(endReachedEnabled),
    containerOffsetEnabled_(containerOffsetEnabled),
    dragEventNonce_(dragEventNonce),
    dragEventType_(dragEventType),
    dragFromIndex_(dragFromIndex),
    dragToIndex_(dragToIndex) {}

#ifdef ANDROID
  ShadowlistViewState(const ShadowlistViewState& previousState, folly::dynamic data) :
    windowContainerHeight_(data.count("windowContainerHeight") ? (Float)data["windowContainerHeight"].getDouble() : previousState.windowContainerHeight_),
    windowContainerWidth_(data.count("windowContainerWidth") ? (Float)data["windowContainerWidth"].getDouble() : previousState.windowContainerWidth_),
    containerOffsetY_(data.count("containerOffsetY") ? (Float)data["containerOffsetY"].getDouble() : previousState.containerOffsetY_),
    containerOffsetX_(data.count("containerOffsetX") ? (Float)data["containerOffsetX"].getDouble() : previousState.containerOffsetX_),
    containerOffsetIndex_(data.count("containerOffsetIndex") ? (Float)data["containerOffsetIndex"].getDouble() : previousState.containerOffsetIndex_),
    containerOffsetIndexNonce_(data.count("containerOffsetIndexNonce") ? (Float)data["containerOffsetIndexNonce"].getDouble() : previousState.containerOffsetIndexNonce_),
    totalContainerHeight_(data.count("totalContainerHeight") ? (Float)data["totalContainerHeight"].getDouble() : previousState.totalContainerHeight_),
    totalContainerWidth_(data.count("totalContainerWidth") ? (Float)data["totalContainerWidth"].getDouble() : previousState.totalContainerWidth_),
    startReachedEnabled_(data.count("startReachedEnabled") ? data["startReachedEnabled"].getBool() : previousState.startReachedEnabled_),
    endReachedEnabled_(data.count("endReachedEnabled") ? data["endReachedEnabled"].getBool() : previousState.endReachedEnabled_),
    containerOffsetEnabled_(data.count("containerOffsetEnabled") ? data["containerOffsetEnabled"].getBool() : previousState.containerOffsetEnabled_),
    dragEventNonce_(data.count("dragEventNonce") ? (Float)data["dragEventNonce"].getDouble() : previousState.dragEventNonce_),
    dragEventType_(data.count("dragEventType") ? (Float)data["dragEventType"].getDouble() : previousState.dragEventType_),
    dragFromIndex_(data.count("dragFromIndex") ? (Float)data["dragFromIndex"].getDouble() : previousState.dragFromIndex_),
    dragToIndex_(data.count("dragToIndex") ? (Float)data["dragToIndex"].getDouble() : previousState.dragToIndex_),
    userScrolled_(data.count("userScrolled") ? data["userScrolled"].getBool() : previousState.userScrolled_),
    /*
     * Sticky section-header geometry is produced by the C++ core (layout pass) and
     * only ever flows core -> view, so a partial update from the Android view
     * (e.g. a scroll commit) carries it forward unchanged.
     */
    stickyHeaderIndices_(previousState.stickyHeaderIndices_),
    stickyHeaderOffsets_(previousState.stickyHeaderOffsets_),
    stickyHeaderSizes_(previousState.stickyHeaderSizes_),
    snapOffsets_(previousState.snapOffsets_) {
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
    result["containerOffsetIndexNonce"] = containerOffsetIndexNonce_;
    result["totalContainerHeight"] = totalContainerHeight_;
    result["totalContainerWidth"] = totalContainerWidth_;
    result["startReachedEnabled"] = startReachedEnabled_;
    result["endReachedEnabled"] = endReachedEnabled_;
    result["containerOffsetEnabled"] = containerOffsetEnabled_;
    result["userScrolled"] = userScrolled_;
    result["dragEventNonce"] = dragEventNonce_;
    result["dragEventType"] = dragEventType_;
    result["dragFromIndex"] = dragFromIndex_;
    result["dragToIndex"] = dragToIndex_;

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
    return result;
  };
#endif

  double windowContainerHeight_{0.0};
  double windowContainerWidth_{0.0};
  double containerOffsetY_{0.0};
  double containerOffsetX_{0.0};
  double containerOffsetIndex_{-2.0};
  double containerOffsetIndexNonce_{0.0};
  double totalContainerHeight_{0.0};
  double totalContainerWidth_{0.0};
  bool startReachedEnabled_{true};
  bool endReachedEnabled_{true};
  bool containerOffsetEnabled_{false};

  /*
   * Drag-to-reorder signalling, written by the platform view as the native gesture
   * progresses and consumed by the component descriptor to emit the JS onDrag*
   * events exactly once per change. dragEventNonce_ is bumped on every drag event so
   * the descriptor can tell a fresh event from a carried-forward one (a plain scroll
   * commit leaves it unchanged). dragEventType_ is 1=start, 3=end (0=none); there is
   * no mid-drag event - the finger tracking and shuffle stay native and never reach
   * JS. dragFromIndex_/dragToIndex_ carry the element indices for that event. Declared
   * before userScrolled_ so the Android constructor's member-init order matches.
   */
  double dragEventNonce_{0.0};
  double dragEventType_{0.0};
  double dragFromIndex_{-1.0};
  double dragToIndex_{-1.0};

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
};

}
