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
    int visibleEndIndex,
    bool horizontal,
    int initialNumToRender);
  SLContainerState() = default;

  SLFenwickTree childrenMeasurements;
  Point scrollPosition;
  Size scrollContainer;
  Size scrollContent;
  int visibleStartIndex;
  int visibleEndIndex;
  bool horizontal;
  int initialNumToRender;

  int calculateVisibleStartIndex(const float visibleStartOffset) const;
  int calculateVisibleEndIndex(const float visibleStartOffset) const;
  float calculateContentSize() const;
  float getScrollPosition(const Point& scrollPosition) const;

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
    calculateVisibleEndIndex(data["scrollPositionTop"].getDouble())
  ),
  horizontal(previousState.horizontal),
  initialNumToRender(previousState.initialNumToRender) {};

  folly::dynamic getDynamic() const;
  MapBuffer getMapBuffer() const;
#endif
};

}
