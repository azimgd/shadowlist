#include "CraigsListContainerEventEmitter.h"

namespace facebook::react {

void CraigsListContainerEventEmitter::onVisibleChange(VisibleMetrics $event) const {
  dispatchEvent("visibleChange", [$event=std::move($event)](jsi::Runtime &runtime) {
    auto $payload = jsi::Object(runtime);
    $payload.setProperty(runtime, "start", $event.start);
    $payload.setProperty(runtime, "end", $event.end);
    return $payload;
  });
}

}
