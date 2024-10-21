#pragma once

#include <algorithm>
#include <react/renderer/graphics/Point.h>
#include <react/renderer/graphics/Size.h>
#include "SLFenwickTree.hpp"

#ifdef ANDROID
#include <folly/dynamic.h>
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif

namespace facebook::react {

class SLContainerState {
  public:
  SLContainerState(
    SLFenwickTree childrenMeasurements,
    Point scrollPosition,
    Size scrollContainer,
    Size scrollContent,
    int visibleStartIndex,
    int visibleEndIndex);
  SLContainerState() = default;

  SLFenwickTree childrenMeasurements;
  Point scrollPosition;
  Size scrollContainer;
  Size scrollContent;
  int visibleStartIndex;
  int visibleEndIndex;

  int calculateVisibleStartIndex(float visibleStartOffset);
  int calculateVisibleEndIndex(float visibleEndOffset);
  float calculateContentSize();
#ifdef ANDROID
  SLContainerState(SLContainerState const &previousState, folly::dynamic data) :
  childrenMeasurements(previousState.childrenMeasurements) {};

  folly::dynamic getDynamic() const;
#endif
};

}
