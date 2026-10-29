#include "ShadowListViewState.h"

#include "ShadowListMountDriver.h"

namespace facebook::react {

#ifdef ANDROID
namespace {

/*
 * The ScrollPatch in a state update's "patch" array, see ShadowListPatchSlot. Android never
 * conceals rows, and its acknowledgement carries over.
 */
azimgd::shadowlist::ScrollPatch scrollPatchFromSlots(const folly::dynamic& slots, double concealGenerationAck) {
  azimgd::shadowlist::ScrollPatch patch;
  if (!slots.isArray() || slots.size() < static_cast<std::size_t>(PATCH_SLOTS)) {
    return patch;
  }
  auto slot = [&slots](ShadowListPatchSlot index) { return slots[static_cast<std::size_t>(index)].asDouble(); };
  patch.report.offsetX = slot(PATCH_OFFSET_X);
  patch.report.offsetY = slot(PATCH_OFFSET_Y);
  patch.report.userScrolled = slot(PATCH_USER_SCROLLED) != 0.0;
  patch.report.scrollPhase = slot(PATCH_SCROLL_PHASE);
  patch.report.commitToken = static_cast<std::uint64_t>(slot(PATCH_COMMIT_TOKEN));
  patch.report.concealGenerationAck = concealGenerationAck;
  patch.report.sequence = static_cast<std::uint64_t>(slot(PATCH_SEQUENCE));
  patch.offsetEnabled = slot(PATCH_OFFSET_ENABLED) != 0.0;
  patch.hasCommand = slot(PATCH_HAS_COMMAND) != 0.0;
  patch.commandIndex = azimgd::shadowlist::scrollCommandIndexFromDouble(slot(PATCH_COMMAND_INDEX));
  patch.commandSequence = slot(PATCH_COMMAND_SEQUENCE);
  patch.commandViewPosition = slot(PATCH_COMMAND_VIEW_POSITION);
  patch.commandViewOffset = slot(PATCH_COMMAND_VIEW_OFFSET);
  patch.commandAnimated = slot(PATCH_COMMAND_ANIMATED) != 0.0;
  patch.hasAnchorRequest = slot(PATCH_HAS_ANCHOR_REQUEST) != 0.0;
  patch.anchorRequestSequence = slot(PATCH_ANCHOR_REQUEST_SEQUENCE);
  return patch;
}

/*
 * A list of numbers from the state, or null when empty, like the layout pass publishes it.
 * A round trip through Android then hands back no empty list the pointer check reads as a
 * change.
 */
template <typename Value>
std::shared_ptr<const std::vector<Value>> numbersFrom(const folly::dynamic& values) {
  if (!values.isArray() || values.empty()) {
    return nullptr;
  }
  auto numbers = std::make_shared<std::vector<Value>>();
  numbers->reserve(values.size());
  for (const auto& value : values) {
    numbers->push_back(static_cast<Value>(value.asDouble()));
  }
  return numbers;
}

}

/*
 * The Android view sends the scroll patch as one "patch" array of numbers, plus the few keys a
 * drag or an edge switch adds. Every other field carries over. Only the keys present are read.
 */
ShadowListViewState::ShadowListViewState(const ShadowListViewState& previousState, folly::dynamic data) :
  ShadowListViewState(previousState) {
  if (!data.isObject()) {
    return;
  }
  const folly::dynamic* stickyIndices = nullptr;
  const folly::dynamic* stickyOffsets = nullptr;
  const folly::dynamic* stickySizes = nullptr;
  for (const auto& [name, value] : data.items()) {
    if (!name.isString()) {
      continue;
    }
    const std::string& key = name.getString();
    if (key == "patch") {
      applyPatch(scrollPatchFromSlots(value, previousState.concealGenerationAck_));
    } else if (key == "startReachedEnabled") {
      startReachedEnabled_ = value.asBool();
    } else if (key == "endReachedEnabled") {
      endReachedEnabled_ = value.asBool();
    } else if (key == "dragEventSequence") {
      dragEventSequence_ = value.asDouble();
    } else if (key == "dragEventType") {
      dragEventType_ = value.asDouble();
    } else if (key == "dragSourceKey") {
      dragSourceKey_ = value.asString();
    } else if (key == "dragDestinationKey") {
      dragDestinationKey_ = value.asString();
    } else if (key == "dragDestinationListId") {
      dragDestinationListId_ = value.asString();
    } else if (key == "dragDestinationIndex") {
      dragDestinationIndex_ = value.asDouble();
    } else if (key == "windowHeight") {
      windowHeight_ = value.asDouble();
    } else if (key == "windowWidth") {
      windowWidth_ = value.asDouble();
    } else if (key == "offsetY") {
      offsetY_ = value.asDouble();
    } else if (key == "offsetX") {
      offsetX_ = value.asDouble();
    } else if (key == "scrollIndex") {
      scrollIndex_ = azimgd::shadowlist::scrollCommandIndexFromDouble(value.asDouble());
    } else if (key == "scrollIndexSequence") {
      scrollIndexSequence_ = value.asDouble();
    } else if (key == "scrollIndexViewPosition") {
      scrollIndexViewPosition_ = value.asDouble();
    } else if (key == "scrollIndexViewOffset") {
      scrollIndexViewOffset_ = value.asDouble();
    } else if (key == "scrollIndexAnimated") {
      scrollIndexAnimated_ = value.asBool();
    } else if (key == "anchorRequestSequence") {
      anchorRequestSequence_ = value.asDouble();
    } else if (key == "contentHeight") {
      contentHeight_ = value.asDouble();
    } else if (key == "contentWidth") {
      contentWidth_ = value.asDouble();
    } else if (key == "offsetEnabled") {
      offsetEnabled_ = value.asBool();
    } else if (key == "userScrolled") {
      userScrolled_ = value.asBool();
    } else if (key == "scrollPhase") {
      scrollPhase_ = value.asDouble();
    } else if (key == "commitToken") {
      commitToken_ = static_cast<std::uint64_t>(value.asDouble());
    } else if (key == "hostSequence") {
      hostSequence_ = value.asDouble();
    } else if (key == "stickyIndices") {
      stickyIndices = &value;
    } else if (key == "stickyOffsets") {
      stickyOffsets = &value;
    } else if (key == "stickySizes") {
      stickySizes = &value;
    } else if (key == "snapOffsets") {
      snapOffsets_ = numbersFrom<double>(value);
    }
  }
  // Sticky header geometry only comes from the core, and a partial update carries it over.
  if (stickyIndices && stickyOffsets && stickySizes) {
    stickyIndices_ = numbersFrom<int>(*stickyIndices);
    stickyOffsets_ = numbersFrom<double>(*stickyOffsets);
    stickySizes_ = numbersFrom<double>(*stickySizes);
  }
}

folly::dynamic ShadowListViewState::getDynamic() const {
  folly::dynamic result = folly::dynamic::object;
  result["windowHeight"] = windowHeight_;
  result["windowWidth"] = windowWidth_;
  result["offsetY"] = offsetY_;
  result["offsetX"] = offsetX_;
  result["scrollIndex"] = azimgd::shadowlist::scrollCommandIndexToDouble(scrollIndex_);
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
  result["dragDestinationListId"] = dragDestinationListId_;
  result["dragDestinationIndex"] = dragDestinationIndex_;

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
  if (mountDriver_) {
    ShadowListMountDriver::registerHandle(mountDriver_);
    mountDriver_->setMountedGeometry({stickyIndices_, stickyOffsets_, stickySizes_, snapOffsets_});
  }
  builder.putLong(ShadowListStateKey::MOUNT_HANDLE, mountDriver_ ? mountDriver_->getHandle() : 0);
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
