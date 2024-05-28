#include "ShadowListContainerEventEmitter.h"

namespace facebook::react {

void ShadowListContainerEventEmitter::onVisibleChildrenUpdate(VisibleChildrenUpdate $event) const {
  dispatchEvent("visibleChildrenUpdate", [$event = std::move($event)](jsi::Runtime &runtime) {
    auto $payload = jsi::Object(runtime);
    $payload.setProperty(runtime, "visibleStartIndex", $event.visibleStartIndex);
    $payload.setProperty(runtime, "visibleEndIndex", $event.visibleEndIndex);
    $payload.setProperty(runtime, "visibleStartOffset", $event.visibleStartOffset);
    $payload.setProperty(runtime, "visibleEndOffset", $event.visibleEndOffset);
    $payload.setProperty(runtime, "headBlankStart", $event.headBlankStart);
    $payload.setProperty(runtime, "headBlankEnd", $event.headBlankEnd);
    $payload.setProperty(runtime, "tailBlankStart", $event.tailBlankStart);
    $payload.setProperty(runtime, "tailBlankEnd", $event.tailBlankEnd);
    return $payload;
  });
}

void ShadowListContainerEventEmitter::onEndReached(EndReached $event) const {
  dispatchEvent("endReached", [$event = std::move($event)](jsi::Runtime &runtime) {
    auto $payload = jsi::Object(runtime);
    $payload.setProperty(runtime, "distanceFromEnd", $event.distanceFromEnd);
    return $payload;
  });
}

void ShadowListContainerEventEmitter::onStartReached(StartReached $event) const {
  dispatchEvent("startReached", [$event = std::move($event)](jsi::Runtime &runtime) {
    auto $payload = jsi::Object(runtime);
    $payload.setProperty(runtime, "distanceFromStart", $event.distanceFromStart);
    return $payload;
  });
}
}
