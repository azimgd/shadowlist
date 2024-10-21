#pragma once
#include "SLFenwickTree.hpp"

#ifdef ANDROID
#include <folly/dynamic.h>
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif

namespace facebook::react {

#ifdef ANDROID
constexpr static MapBuffer::Key CX_STATE_KEY_CHILDREN_MEASUREMENTS = 0;
#endif

class SLContainerState {
  public:
  SLContainerState(SLFenwickTree childrenMeasurements);
  SLContainerState() = default;

  SLFenwickTree childrenMeasurements;

#ifdef ANDROID
  SLContainerState(SLContainerState const &previousState, folly::dynamic data) :
  childrenMeasurements(previousState.childrenMeasurements) {};
  folly::dynamic getDynamic() const {
    return {};
  };
#endif
};

}
