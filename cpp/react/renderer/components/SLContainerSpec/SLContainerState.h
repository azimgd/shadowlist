#pragma once

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

  int calculateVisibleStartIndex(float visibleStartOffset) const;
  int calculateVisibleEndIndex(float visibleEndOffset) const;
  float calculateContentSize() const;

#ifdef ANDROID
  SLContainerState(SLContainerState const &previousState, folly::dynamic data) :
  childrenMeasurements(previousState.childrenMeasurements),
  scrollPosition({
    (Float)data["scrollPositionLeft"].getDouble(),
    (Float)data["scrollPositionTop"].getDouble()
  }),
  scrollContainer(previousState.scrollContainer),
  scrollContent(previousState.scrollContent),
  visibleStartIndex(
    calculateVisibleStartIndex(data["scrollPositionTop"].getDouble())
  ),
  visibleEndIndex(
    calculateVisibleEndIndex(data["scrollPositionTop"].getDouble() + previousState.scrollContainer.height)
  ) {};

  folly::dynamic getDynamic() const;
  MapBuffer getMapBuffer() const;
#endif
};

}
