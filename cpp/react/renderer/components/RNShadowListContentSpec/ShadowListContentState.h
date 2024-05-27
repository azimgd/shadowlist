#pragma once

#include "ShadowListFenwickTree.hpp"
#include <react/renderer/graphics/Point.h>
#include <react/renderer/graphics/Size.h>

#ifdef ANDROID
#include <folly/dynamic.h>
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif

namespace facebook::react {

class ShadowListContentState {
  public:
  ShadowListContentState(ShadowListFenwickTree contentViewMeasurements);
  ShadowListContentState() = default;
  
  /*
   * Binary tree, expensive for updates, cheap for reads
   */
  ShadowListFenwickTree contentViewMeasurements;
};

}
