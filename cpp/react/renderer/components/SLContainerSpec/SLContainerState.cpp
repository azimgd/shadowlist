#include "SLContainerState.h"

namespace facebook::react {

SLContainerState::SLContainerState(
  SLFenwickTree childrenMeasurements,
  Point scrollPosition,
  Size scrollContainer,
  Size scrollContent,
  int visibleStartIndex,
  int visibleEndIndex) :
    childrenMeasurements(childrenMeasurements),
    scrollPosition(scrollPosition),
    scrollContainer(scrollContainer),
    scrollContent(scrollContent),
    visibleStartIndex(visibleStartIndex),
    visibleEndIndex(visibleEndIndex) {}

#ifdef ANDROID
folly::dynamic SLContainerState::getDynamic() const {
  return folly::dynamic::object(
    "scrollContentWidth",
    scrollContent.width
  )(
    "scrollContentHeight",
    scrollContent.height
  )(
    "scrollContainerWidth",
    scrollContainer.width
  )(
    "scrollContainerHeight",
    scrollContainer.height
  )(
    "scrollPositionLeft",
    scrollPosition.x
  )(
    "scrollPositionTop",
    scrollPosition.y
  )(
    "visibleStartIndex",
    calculateVisibleStartIndex(scrollPosition.y)
  )(
    "visibleEndIndex",
    calculateVisibleEndIndex(scrollPosition.y)
  );
}

MapBuffer SLContainerState::getMapBuffer() const {
  auto builder = MapBufferBuilder();
  builder.putInt(0, visibleStartIndex);
  builder.putInt(1, visibleEndIndex);
  builder.putDouble(2, scrollPosition.y);
  builder.putDouble(3, scrollPosition.x);
  builder.putDouble(4, scrollContent.width);
  builder.putDouble(5, scrollContent.height);
  builder.putDouble(6, scrollContainer.width);
  builder.putDouble(7, scrollContainer.height);
  return builder.build();
}
#endif

int SLContainerState::calculateVisibleStartIndex(float visibleStartOffset) const {
  int offset = 5;
  int visibleStartIndex = childrenMeasurements.lower_bound(visibleStartOffset);
  int visibleEndIndexMin = 0;
  return std::max(visibleStartIndex - offset, visibleEndIndexMin);
}

int SLContainerState::calculateVisibleEndIndex(float visibleStartOffset) const {
  int offset = 5;
  int visibleEndIndex = childrenMeasurements.lower_bound(visibleStartOffset + scrollContainer.height);
  int visibleEndIndexMax = childrenMeasurements.size();
  return std::min(visibleEndIndex + offset, visibleEndIndexMax);
}

float SLContainerState::calculateContentSize() const {
  return childrenMeasurements.sum(childrenMeasurements.size());
}

}
