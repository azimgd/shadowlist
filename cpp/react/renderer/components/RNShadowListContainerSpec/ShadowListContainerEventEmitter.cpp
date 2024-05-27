#include "ShadowListContainerEventEmitter.h"

namespace facebook::react {

void ShadowListContainerEventEmitter::onVisibleChange(VisibleMetrics $event) const {
  dispatchEvent("visibleChange", [$event = std::move($event)](jsi::Runtime &runtime) {
    auto $payload = jsi::Object(runtime);
    $payload.setProperty(runtime, "start", $event.start);
    $payload.setProperty(runtime, "end", $event.end);
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
