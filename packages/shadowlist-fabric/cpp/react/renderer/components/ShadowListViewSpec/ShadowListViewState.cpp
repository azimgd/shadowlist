#include "ShadowListViewState.h"

namespace facebook::react {

#ifdef ANDROID
ShadowListViewState::ShadowListViewState(const ShadowListViewState& previousState, folly::dynamic data) :
  windowContainerHeight_(data.count("windowContainerHeight") ? data["windowContainerHeight"].getDouble() : previousState.windowContainerHeight_),
  windowContainerWidth_(data.count("windowContainerWidth") ? data["windowContainerWidth"].getDouble() : previousState.windowContainerWidth_),
  containerOffsetY_(data.count("containerOffsetY") ? data["containerOffsetY"].getDouble() : previousState.containerOffsetY_),
  containerOffsetX_(data.count("containerOffsetX") ? data["containerOffsetX"].getDouble() : previousState.containerOffsetX_),
  containerOffsetIndex_(data.count("containerOffsetIndex") ? data["containerOffsetIndex"].getDouble() : previousState.containerOffsetIndex_),
  containerOffsetIndexSequence_(data.count("containerOffsetIndexSequence") ? data["containerOffsetIndexSequence"].getDouble() : previousState.containerOffsetIndexSequence_),
  containerOffsetIndexViewPosition_(data.count("containerOffsetIndexViewPosition") ? data["containerOffsetIndexViewPosition"].getDouble() : previousState.containerOffsetIndexViewPosition_),
  totalContainerHeight_(data.count("totalContainerHeight") ? data["totalContainerHeight"].getDouble() : previousState.totalContainerHeight_),
  totalContainerWidth_(data.count("totalContainerWidth") ? data["totalContainerWidth"].getDouble() : previousState.totalContainerWidth_),
  startReachedEnabled_(data.count("startReachedEnabled") ? data["startReachedEnabled"].getBool() : previousState.startReachedEnabled_),
  endReachedEnabled_(data.count("endReachedEnabled") ? data["endReachedEnabled"].getBool() : previousState.endReachedEnabled_),
  containerOffsetEnabled_(data.count("containerOffsetEnabled") ? data["containerOffsetEnabled"].getBool() : previousState.containerOffsetEnabled_),
  dragEventSequence_(data.count("dragEventSequence") ? data["dragEventSequence"].getDouble() : previousState.dragEventSequence_),
  dragEventType_(data.count("dragEventType") ? data["dragEventType"].getDouble() : previousState.dragEventType_),
  dragFromKey_(data.count("dragFromKey") ? data["dragFromKey"].getString() : previousState.dragFromKey_),
  dragToKey_(data.count("dragToKey") ? data["dragToKey"].getString() : previousState.dragToKey_),
  userScrolled_(data.count("userScrolled") ? data["userScrolled"].getBool() : previousState.userScrolled_),
  scrollPhase_(data.count("scrollPhase") ? data["scrollPhase"].getDouble() : previousState.scrollPhase_),
  /*
   * Sticky header geometry only comes from the core. A partial update from
   * the Android view, like a scroll commit, carries it over unchanged.
   */
  stickyHeaderIndices_(previousState.stickyHeaderIndices_),
  stickyHeaderOffsets_(previousState.stickyHeaderOffsets_),
  stickyHeaderSizes_(previousState.stickyHeaderSizes_),
  snapOffsets_(previousState.snapOffsets_),
  commitToken_(
    data.count("commitToken") ? static_cast<std::uint64_t>(data["commitToken"].getDouble()) : previousState.commitToken_),
  /*
   * Carry the base over from the mounted state, like iOS does. A report that echoes a
   * correction keeps the base its first write started from. A republished correction
   * stays one running delta the host has partly applied.
   */
  containerOffsetBaseX_(previousState.containerOffsetBaseX_),
  containerOffsetBaseY_(previousState.containerOffsetBaseY_),
  // Android never conceals rows. Just carry these over.
  concealGeneration_(previousState.concealGeneration_),
  concealGenerationAck_(previousState.concealGenerationAck_),
  // The band comes from the layout pass and the live report is shared. Carry both over.
  offsetBandLow_(previousState.offsetBandLow_),
  offsetBandHigh_(previousState.offsetBandHigh_),
  hostSequence_(data.count("hostSequence") ? data["hostSequence"].asDouble() : previousState.hostSequence_),
  liveScroll_(previousState.liveScroll_),
  containerOffsetIndexRowOffset_(data.count("containerOffsetIndexRowOffset") ? data["containerOffsetIndexRowOffset"].asDouble() : previousState.containerOffsetIndexRowOffset_),
  containerOffsetIndexAnimated_(data.count("containerOffsetIndexAnimated") ? data["containerOffsetIndexAnimated"].asBool() : previousState.containerOffsetIndexAnimated_),
  // The estimate comes from the layout pass. Carry it over.
  animationTargetSequence_(previousState.animationTargetSequence_),
  animationTargetOffset_(previousState.animationTargetOffset_),
  anchorRequestSequence_(data.count("anchorRequestSequence") ? data["anchorRequestSequence"].asDouble() : previousState.anchorRequestSequence_) {
  if (data.count("stickyHeaderIndices") && data.count("stickyHeaderOffsets") && data.count("stickyHeaderSizes")) {
    auto stickyHeaderIndices = std::make_shared<std::vector<int>>();
    auto stickyHeaderOffsets = std::make_shared<std::vector<double>>();
    auto stickyHeaderSizes = std::make_shared<std::vector<double>>();
    for (const auto& value : data["stickyHeaderIndices"]) {
      stickyHeaderIndices->push_back((int)value.getInt());
    }
    for (const auto& value : data["stickyHeaderOffsets"]) {
      stickyHeaderOffsets->push_back(value.getDouble());
    }
    for (const auto& value : data["stickyHeaderSizes"]) {
      stickyHeaderSizes->push_back(value.getDouble());
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
    auto snapOffsets = std::make_shared<std::vector<double>>();
    for (const auto& value : data["snapOffsets"]) {
      snapOffsets->push_back(value.getDouble());
    }
    snapOffsets_ = snapOffsets->empty() ? nullptr : std::move(snapOffsets);
  }
}

folly::dynamic ShadowListViewState::getDynamic() const {
  folly::dynamic result = folly::dynamic::object;
  result["windowContainerHeight"] = windowContainerHeight_;
  result["windowContainerWidth"] = windowContainerWidth_;
  result["containerOffsetY"] = containerOffsetY_;
  result["containerOffsetX"] = containerOffsetX_;
  result["containerOffsetIndex"] = containerOffsetIndex_;
  result["containerOffsetIndexSequence"] = containerOffsetIndexSequence_;
  result["containerOffsetIndexViewPosition"] = containerOffsetIndexViewPosition_;
  result["containerOffsetIndexRowOffset"] = containerOffsetIndexRowOffset_;
  result["containerOffsetIndexAnimated"] = containerOffsetIndexAnimated_;
  result["anchorRequestSequence"] = anchorRequestSequence_;
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
  result["commitToken"] = static_cast<double>(commitToken_);
  result["containerOffsetBaseX"] = containerOffsetBaseX_;
  result["containerOffsetBaseY"] = containerOffsetBaseY_;
  return result;
}

MapBuffer ShadowListViewState::getMapBuffer() const {
  MapBufferBuilder builder;
  builder.putDouble(ShadowListStateKey::TOTAL_WIDTH, totalContainerWidth_);
  builder.putDouble(ShadowListStateKey::TOTAL_HEIGHT, totalContainerHeight_);
  builder.putBool(ShadowListStateKey::OFFSET_ENABLED, containerOffsetEnabled_);
  builder.putDouble(ShadowListStateKey::OFFSET_X, containerOffsetX_);
  builder.putDouble(ShadowListStateKey::OFFSET_Y, containerOffsetY_);
  builder.putDouble(ShadowListStateKey::COMMIT_TOKEN, static_cast<double>(commitToken_));
  builder.putDouble(ShadowListStateKey::OFFSET_BASE_X, containerOffsetBaseX_);
  builder.putDouble(ShadowListStateKey::OFFSET_BASE_Y, containerOffsetBaseY_);
  builder.putBool(ShadowListStateKey::USER_SCROLLED, userScrolled_);
  builder.putDouble(ShadowListStateKey::SCROLL_PHASE, scrollPhase_);
  builder.putDouble(ShadowListStateKey::COMMAND_SEQUENCE, containerOffsetIndexSequence_);
  std::pair<std::uint64_t, std::uint64_t> versions{0, 0};
  if (liveScroll_) {
    ShadowListLiveScroll::registerHandle(liveScroll_);
    versions = liveScroll_->geometryVersions(stickyHeaderIndices_, stickyHeaderOffsets_, stickyHeaderSizes_, snapOffsets_);
  }
  builder.putLong(ShadowListStateKey::STICKY_VERSION, static_cast<std::int64_t>(versions.first));
  builder.putLong(ShadowListStateKey::SNAP_VERSION, static_cast<std::int64_t>(versions.second));
  builder.putDouble(ShadowListStateKey::BAND_LOW, offsetBandLow_);
  builder.putDouble(ShadowListStateKey::BAND_HIGH, offsetBandHigh_);
  builder.putLong(ShadowListStateKey::LIVE_HANDLE, liveScroll_ ? liveScroll_->getHandle() : 0);
  builder.putDouble(ShadowListStateKey::CONCEAL_GENERATION, concealGeneration_);
  builder.putDouble(ShadowListStateKey::ANIMATION_SEQUENCE, animationTargetSequence_);
  builder.putDouble(ShadowListStateKey::ANIMATION_OFFSET, animationTargetOffset_);
  return builder.build();
}
#endif

void ShadowListViewState::applyPatch(const azimgd::shadowlist::ScrollPatch& patch) {
  applyLiveReport(patch.report);
  containerOffsetEnabled_ = patch.offsetEnabled;
  if (patch.hasCommand) {
    containerOffsetIndex_ = patch.commandIndex;
    containerOffsetIndexSequence_ = patch.commandSequence;
    containerOffsetIndexViewPosition_ = patch.commandViewPosition;
    containerOffsetIndexRowOffset_ = patch.commandViewOffset;
    containerOffsetIndexAnimated_ = patch.commandAnimated;
  }
  if (patch.hasAnchorRequest) {
    anchorRequestSequence_ = patch.anchorRequestSequence;
  }
  if (patch.hasStartReachedEnabled) {
    startReachedEnabled_ = patch.startReachedEnabled;
  }
  if (patch.hasEndReachedEnabled) {
    endReachedEnabled_ = patch.endReachedEnabled;
  }
}

}
