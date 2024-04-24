#pragma once

#ifdef ANDROID
#include <folly/dynamic.h>
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif

namespace facebook::react {

class CraigsListContainerState {
  public:
  CraigsListContainerState() = default;

#ifdef ANDROID
  CraigsListContainerState(CraigsListContainerState const &previousState, folly::dynamic data){};
  folly::dynamic getDynamic() const {
    return {};
  };
  MapBuffer getMapBuffer() const {
    return MapBufferBuilder::EMPTY();
  };
#endif

};

}