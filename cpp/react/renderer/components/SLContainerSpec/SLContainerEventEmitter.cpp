#include "SLContainerEventEmitter.h"

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

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

void SLContainerEventEmitter::onScroll(OnScroll $event) const {
  dispatchEvent("onScroll", [$event = std::move($event)](jsi::Runtime &runtime) {
    auto $payload = jsi::Object(runtime);
    auto $contentSize = jsi::Object(runtime);
    $contentSize.setProperty(runtime, "width", $event.contentSize.width);
    $contentSize.setProperty(runtime, "height", $event.contentSize.height);
    $payload.setProperty(runtime, "contentSize", $contentSize);
    auto $contentOffset = jsi::Object(runtime);
    $contentOffset.setProperty(runtime, "x", $event.contentOffset.x);
    $contentOffset.setProperty(runtime, "y", $event.contentOffset.y);
    $payload.setProperty(runtime, "contentOffset", $contentOffset);
    return $payload;
  });
}
}
