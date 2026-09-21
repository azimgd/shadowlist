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
 * Values of ShadowListViewState::scrollPhase_, mirrored by the Android host's
 * ShadowListView.SCROLL_PHASE_* constants and mapped to ScrollPhase by the component
 * descriptor.
 */
constexpr double SCROLL_PHASE_IDLE = 0.0;
constexpr double SCROLL_PHASE_DRAGGING = 1.0;
constexpr double SCROLL_PHASE_SETTLING = 2.0;

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
     * Sticky section-header geometry is produced by the C++ core (layout pass) and
     * only ever flows core -> view, so a partial update from the Android view
     * (e.g. a scroll commit) carries it forward unchanged.
     */
    stickyHeaderIndices_(previousState.stickyHeaderIndices_),
    stickyHeaderOffsets_(previousState.stickyHeaderOffsets_),
    stickyHeaderSizes_(previousState.stickyHeaderSizes_),
    snapOffsets_(previousState.snapOffsets_),
    commitToken_(data.count("commitToken") ? (Float)data["commitToken"].getDouble() : previousState.commitToken_),
    /*
     * Carried from the mounted state, as the iOS host's state copy does: a report echoing a
     * correction's token keeps the base its first write started from, so a republish of that
     * correction stays one cumulative delta the host has partly applied already.
     */
    containerOffsetBaseX_(previousState.containerOffsetBaseX_),
    containerOffsetBaseY_(previousState.containerOffsetBaseY_),
    // Row concealment is not enabled on Android (see ShadowListViewShadowNode::layout); carry it.
    concealGeneration_(previousState.concealGeneration_),
    concealGenerationAck_(previousState.concealGenerationAck_),
    // Core -> view only (see momentumYieldToken_); carried.
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
       * Normalise empty back to null, the same way the layout pass publishes it, so a
       * round trip through the Android renderer cannot hand the shadow node a non-null
       * empty collection that its pointer comparison would read as a change.
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

  // Serializes the state into folly::dynamic for the Android renderer.
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

    // A null pointer means empty; see the member declarations.
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
   * Where the scrollToIndex command rests its row in the viewport (0 start, 0.5 centre,
   * 1 end). Carried with containerOffsetIndex_ by the platform views.
   */
  double containerOffsetIndexViewPosition_{0.0};
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
   * The live gesture phase the host last reported: SCROLL_PHASE_IDLE,
   * SCROLL_PHASE_DRAGGING (a finger is down) or SCROLL_PHASE_SETTLING (momentum is
   * running). Unlike userScrolled_, which describes where THIS offset came from, the
   * phase persists across the commits that land between touch frames, so the core can
   * tell "a finger is still on the list" on a frame whose offset did not move (see
   * FrameInput::scrollPhase). A double like the other scalar fields. Declared after
   * userScrolled_ so the Android constructor's member-init order matches.
   */
  double scrollPhase_{0.0};

  /*
   * Sticky section-header geometry along the scroll axis, produced by the core's
   * layout pass (one entry per sticky section header, ascending by index). The
   * integrations pin the active header on the UI thread per scroll frame from this,
   * so the per-frame pin never reads a
   * (possibly transformed) view frame. Empty for a plain list. Declared after
   * scrollPhase_ so the Android constructor's member-init order matches.
   *
   * HELD BY POINTER, NOT BY VALUE. State is copied constantly -- adopt() copies it once
   * per commit, layout() again, and the iOS scroll delegate once per scroll frame on the
   * main thread -- while these collections change only when the element geometry moves.
   * By value, snapOffsets_ alone (one entry per row, see Container::getSnapOffsets) meant
   * copying a 100k-element vector several times per frame for a large snapping list.
   * Shared, immutable and refcounted, a state copy is a few atomic increments, and the
   * "did this change?" test the layout pass runs every frame becomes a pointer comparison
   * instead of an O(rows) element-wise one.
   *
   * A NULL pointer means empty; readers must treat the two as identical. Nothing mutates
   * a pointee after it is published, so sharing one across state copies (and across
   * threads) is safe.
   */
  std::shared_ptr<const std::vector<int>> stickyHeaderIndices_{};
  std::shared_ptr<const std::vector<Float>> stickyHeaderOffsets_{};
  std::shared_ptr<const std::vector<Float>> stickyHeaderSizes_{};

  /*
   * Resting snap offsets along the scroll axis (DIP), produced by the core's layout
   * pass. Empty unless snapToItem is set. The integrations snap the native scroll
   * view's landing position to the nearest of these. Held by pointer for the reason
   * above -- this is the collection that made it necessary.
   */
  std::shared_ptr<const std::vector<Float>> snapOffsets_{};

  /*
   * Commit token: the id of the in-flight offset correction. Core -> view it rides on
   * the applied offset; view -> core it is echoed back so the core recognises its own
   * write exactly (by id), instead of by a pixel-distance heuristic. 0 = no correction
   * / a host-originated report. Carried as a double to match the other scalar state
   * fields; the integer id never approaches double's exact-integer range in practice.
   * Declared after snapOffsets_ so the Android constructor's member-init order matches.
   */
  double commitToken_{0.0};

  /*
   * The offset the core started from when the layout pass published a correction: the
   * reported offset in the state it read. containerOffsetX_/Y_ minus this is the
   * correction as a delta. A commit can mount frames after the report it was built on,
   * and a view still moving on its own has travelled on by then; writing the absolute
   * offset throws that travel away and the content jumps. Both hosts apply the delta to
   * the live offset instead while the view is moving (a finger dragging, momentum, the iOS
   * scroll-to-top animation) and for an operation correction computed from a gesture report.
   * Meaningful only on a state whose containerOffsetEnabled_ the layout pass set. Host
   * reports carry the mounted state's value (the Android partial-update constructor copies
   * it). Declared after commitToken_ so the Android constructor's member-init order matches.
   */
  double containerOffsetBaseX_{0.0};
  double containerOffsetBaseY_{0.0};

  /*
   * Row concealment handshake (see ShadowListViewGeometryCache::concealedRows).
   *
   * concealGeneration_ flows core -> view: the generation of the newest concealment the layout
   * pass published, 0 when no row is concealed. concealGenerationAck_ flows view -> core: the
   * concealGeneration_ of the mounted state a host report was built on. A report acking a
   * generation proves the host mounted that state, and with it the offset correction the
   * concealment waits for. The layout pass copies the ack through untouched, so it never claims
   * more than the host has mounted. Declared after containerOffsetBaseY_ so the Android
   * constructor's member-init order matches.
   */
  double concealGeneration_{0.0};
  double concealGenerationAck_{0.0};

  /*
   * Core -> view: the commit token of a correction that is a scroll command issued in C++ (a
   * ShadowListNative scrollToIndex/scrollToEnd/scrollToStart), 0 when none. A host mounting a
   * correction with this token stops any momentum first and writes the offset, as it does for
   * its own scroll commands (which stop momentum when issued). Declared after
   * concealGenerationAck_ so the Android constructor's member-init order matches.
   */
  double momentumYieldToken_{0.0};
};

}
