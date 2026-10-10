#include "ShadowListViewState.h"

namespace facebook::react {

#ifdef ANDROID
ShadowListViewState::ShadowListViewState(const ShadowListViewState& previousState, folly::dynamic data) :
  windowHeight_(data.count("windowHeight") ? data["windowHeight"].getDouble() : previousState.windowHeight_),
  windowWidth_(data.count("windowWidth") ? data["windowWidth"].getDouble() : previousState.windowWidth_),
  offsetY_(data.count("offsetY") ? data["offsetY"].getDouble() : previousState.offsetY_),
  offsetX_(data.count("offsetX") ? data["offsetX"].getDouble() : previousState.offsetX_),
  scrollIndex_(data.count("scrollIndex") ? data["scrollIndex"].getDouble() : previousState.scrollIndex_),
  scrollIndexSequence_(data.count("scrollIndexSequence") ? data["scrollIndexSequence"].getDouble() : previousState.scrollIndexSequence_),
  scrollIndexViewPosition_(data.count("scrollIndexViewPosition") ? data["scrollIndexViewPosition"].getDouble() : previousState.scrollIndexViewPosition_),
  contentHeight_(data.count("contentHeight") ? data["contentHeight"].getDouble() : previousState.contentHeight_),
  contentWidth_(data.count("contentWidth") ? data["contentWidth"].getDouble() : previousState.contentWidth_),
  startReachedEnabled_(data.count("startReachedEnabled") ? data["startReachedEnabled"].getBool() : previousState.startReachedEnabled_),
  endReachedEnabled_(data.count("endReachedEnabled") ? data["endReachedEnabled"].getBool() : previousState.endReachedEnabled_),
  offsetEnabled_(data.count("offsetEnabled") ? data["offsetEnabled"].getBool() : previousState.offsetEnabled_),
  dragEventSequence_(data.count("dragEventSequence") ? data["dragEventSequence"].getDouble() : previousState.dragEventSequence_),
  dragEventType_(data.count("dragEventType") ? data["dragEventType"].getDouble() : previousState.dragEventType_),
  dragSourceKey_(data.count("dragSourceKey") ? data["dragSourceKey"].getString() : previousState.dragSourceKey_),
  dragDestinationKey_(data.count("dragDestinationKey") ? data["dragDestinationKey"].getString() : previousState.dragDestinationKey_),
  userScrolled_(data.count("userScrolled") ? data["userScrolled"].getBool() : previousState.userScrolled_),
  scrollPhase_(data.count("scrollPhase") ? data["scrollPhase"].getDouble() : previousState.scrollPhase_),
  /*
   * Sticky header geometry only comes from the core. A partial update from
   * the Android view, like a scroll commit, carries it over unchanged.
   */
  stickyIndices_(previousState.stickyIndices_),
  stickyOffsets_(previousState.stickyOffsets_),
  stickySizes_(previousState.stickySizes_),
  snapOffsets_(previousState.snapOffsets_),
  commitToken_(
    data.count("commitToken") ? static_cast<std::uint64_t>(data["commitToken"].getDouble()) : previousState.commitToken_),
  /*
   * Carry the base over from the mounted state, like iOS does. A report that echoes a
   * correction keeps the base its first write started from. A republished correction
   * stays one running delta the host has partly applied.
   */
  offsetBaseX_(previousState.offsetBaseX_),
  offsetBaseY_(previousState.offsetBaseY_),
  // Android never conceals rows. Just carry these over.
  concealGeneration_(previousState.concealGeneration_),
  concealGenerationAck_(previousState.concealGenerationAck_),
  // The band comes from the layout pass and the live report is shared. Carry both over.
  offsetBandLow_(previousState.offsetBandLow_),
  offsetBandHigh_(previousState.offsetBandHigh_),
  hostSequence_(data.count("hostSequence") ? data["hostSequence"].asDouble() : previousState.hostSequence_),
  liveScroll_(previousState.liveScroll_),
  scrollIndexViewOffset_(data.count("scrollIndexViewOffset") ? data["scrollIndexViewOffset"].asDouble() : previousState.scrollIndexViewOffset_),
  scrollIndexAnimated_(data.count("scrollIndexAnimated") ? data["scrollIndexAnimated"].asBool() : previousState.scrollIndexAnimated_),
  // The estimate comes from the layout pass. Carry it over.
  animationTargetSequence_(previousState.animationTargetSequence_),
  animationTargetOffset_(previousState.animationTargetOffset_),
  anchorRequestSequence_(data.count("anchorRequestSequence") ? data["anchorRequestSequence"].asDouble() : previousState.anchorRequestSequence_) {
  if (data.count("stickyIndices") && data.count("stickyOffsets") && data.count("stickySizes")) {
    auto stickyIndices = std::make_shared<std::vector<int>>();
    auto stickyOffsets = std::make_shared<std::vector<double>>();
    auto stickySizes = std::make_shared<std::vector<double>>();
    for (const auto& value : data["stickyIndices"]) {
      stickyIndices->push_back((int)value.getInt());
    }
    for (const auto& value : data["stickyOffsets"]) {
      stickyOffsets->push_back(value.getDouble());
    }
    for (const auto& value : data["stickySizes"]) {
      stickySizes->push_back(value.getDouble());
    }
    /*
     * Turn empty back into null like the layout pass does. Otherwise a round trip
     * through Android hands back an empty list that the pointer check reads as a change.
     */
    stickyIndices_ = stickyIndices->empty() ? nullptr : std::move(stickyIndices);
    stickyOffsets_ = stickyOffsets->empty() ? nullptr : std::move(stickyOffsets);
    stickySizes_ = stickySizes->empty() ? nullptr : std::move(stickySizes);
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
  result["windowHeight"] = windowHeight_;
  result["windowWidth"] = windowWidth_;
  result["offsetY"] = offsetY_;
  result["offsetX"] = offsetX_;
  result["scrollIndex"] = scrollIndex_;
  result["scrollIndexSequence"] = scrollIndexSequence_;
  result["scrollIndexViewPosition"] = scrollIndexViewPosition_;
  result["scrollIndexViewOffset"] = scrollIndexViewOffset_;
  result["scrollIndexAnimated"] = scrollIndexAnimated_;
  result["anchorRequestSequence"] = anchorRequestSequence_;
  result["contentHeight"] = contentHeight_;
  result["contentWidth"] = contentWidth_;
  result["startReachedEnabled"] = startReachedEnabled_;
  result["endReachedEnabled"] = endReachedEnabled_;
  result["offsetEnabled"] = offsetEnabled_;
  result["userScrolled"] = userScrolled_;
  result["scrollPhase"] = scrollPhase_;
  result["dragEventSequence"] = dragEventSequence_;
  result["dragEventType"] = dragEventType_;
  result["dragSourceKey"] = dragSourceKey_;
  result["dragDestinationKey"] = dragDestinationKey_;

  // A null pointer means empty.
  folly::dynamic stickyIndices = folly::dynamic::array;
  if (stickyIndices_) {
    for (auto stickyIndex : *stickyIndices_) {
      stickyIndices.push_back(stickyIndex);
    }
  }
  folly::dynamic stickyOffsets = folly::dynamic::array;
  if (stickyOffsets_) {
    for (auto stickyOffset : *stickyOffsets_) {
      stickyOffsets.push_back((double)stickyOffset);
    }
  }
  folly::dynamic stickySizes = folly::dynamic::array;
  if (stickySizes_) {
    for (auto stickySize : *stickySizes_) {
      stickySizes.push_back((double)stickySize);
    }
  }
  result["stickyIndices"] = stickyIndices;
  result["stickyOffsets"] = stickyOffsets;
  result["stickySizes"] = stickySizes;

  folly::dynamic snapOffsets = folly::dynamic::array;
  if (snapOffsets_) {
    for (auto snapOffset : *snapOffsets_) {
      snapOffsets.push_back((double)snapOffset);
    }
  }
  result["snapOffsets"] = snapOffsets;
  result["commitToken"] = static_cast<double>(commitToken_);
  result["offsetBaseX"] = offsetBaseX_;
  result["offsetBaseY"] = offsetBaseY_;
  return result;
}

MapBuffer ShadowListViewState::getMapBuffer() const {
  MapBufferBuilder builder;
  builder.putDouble(ShadowListStateKey::CONTENT_WIDTH, contentWidth_);
  builder.putDouble(ShadowListStateKey::CONTENT_HEIGHT, contentHeight_);
  builder.putBool(ShadowListStateKey::OFFSET_ENABLED, offsetEnabled_);
  builder.putDouble(ShadowListStateKey::OFFSET_X, offsetX_);
  builder.putDouble(ShadowListStateKey::OFFSET_Y, offsetY_);
  builder.putDouble(ShadowListStateKey::COMMIT_TOKEN, static_cast<double>(commitToken_));
  builder.putDouble(ShadowListStateKey::OFFSET_BASE_X, offsetBaseX_);
  builder.putDouble(ShadowListStateKey::OFFSET_BASE_Y, offsetBaseY_);
  builder.putBool(ShadowListStateKey::USER_SCROLLED, userScrolled_);
  builder.putDouble(ShadowListStateKey::SCROLL_PHASE, scrollPhase_);
  builder.putDouble(ShadowListStateKey::COMMAND_SEQUENCE, scrollIndexSequence_);
  std::pair<std::uint64_t, std::uint64_t> versions{0, 0};
  if (liveScroll_) {
    ShadowListLiveScroll::registerHandle(liveScroll_);
    versions = liveScroll_->geometryVersions(stickyIndices_, stickyOffsets_, stickySizes_, snapOffsets_);
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
  offsetEnabled_ = patch.offsetEnabled;
  if (patch.hasCommand) {
    scrollIndex_ = patch.commandIndex;
    scrollIndexSequence_ = patch.commandSequence;
    scrollIndexViewPosition_ = patch.commandViewPosition;
    scrollIndexViewOffset_ = patch.commandViewOffset;
    scrollIndexAnimated_ = patch.commandAnimated;
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
