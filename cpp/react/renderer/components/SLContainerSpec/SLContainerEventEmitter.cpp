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

}
