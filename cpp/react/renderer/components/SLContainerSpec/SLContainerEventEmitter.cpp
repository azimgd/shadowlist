#include "SLContainerEventEmitter.h"

namespace facebook::react {

void SLContainerEventEmitter::onVisibleChange(OnVisibleChange $event) const {
  dispatchEvent("visibleChange", [$event = std::move($event)](jsi::Runtime &runtime) {
    auto $payload = jsi::Object(runtime);
    $payload.setProperty(runtime, "visibleStartIndex", $event.visibleStartIndex);
    $payload.setProperty(runtime, "visibleEndIndex", $event.visibleEndIndex);
    return $payload;
  });
}

void SLContainerEventEmitter::onStartReached(OnStartReached $event) const {
  dispatchEvent("startReached", [$event = std::move($event)](jsi::Runtime &runtime) {
    auto $payload = jsi::Object(runtime);
    $payload.setProperty(runtime, "distanceFromStart", $event.distanceFromStart);
    return $payload;
  });
}

void SLContainerEventEmitter::onEndReached(OnEndReached $event) const {
  dispatchEvent("endReached", [$event = std::move($event)](jsi::Runtime &runtime) {
    auto $payload = jsi::Object(runtime);
    $payload.setProperty(runtime, "distanceFromEnd", $event.distanceFromEnd);
    return $payload;
  });
}
}
