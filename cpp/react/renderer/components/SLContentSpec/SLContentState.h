#pragma once

#include <react/renderer/graphics/Point.h>
#include <react/renderer/graphics/Size.h>

#ifdef ANDROID
#include <folly/dynamic.h>
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif


namespace facebook::react {

class SLContentState {
  public:
  SLContentState() = default;

#ifdef ANDROID
  SLContentState(const SLContentState& previousState, folly::dynamic data) {};
  folly::dynamic getDynamic() const {
    return {};
  };
  MapBuffer getMapBuffer() const {
    return MapBufferBuilder::EMPTY();
  };
#endif
};

}
