#include "SLContainerState.h"

namespace facebook::react {

SLContainerState::SLContainerState(
  SLFenwickTree childrenMeasurements,
  Point scrollPosition,
  Size scrollContainer,
  Size scrollContent,
  int visibleStartIndex,
  int visibleEndIndex,
  bool horizontal,
  int initialNumToRender) :
    childrenMeasurements(childrenMeasurements),
    scrollPosition(scrollPosition),
    scrollContainer(scrollContainer),
    scrollContent(scrollContent),
    visibleStartIndex(visibleStartIndex),
    visibleEndIndex(visibleEndIndex),
    horizontal(horizontal),
    initialNumToRender(initialNumToRender) {}

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
    calculateVisibleStartIndex(getScrollPosition(scrollPosition))
  )(
    "visibleEndIndex",
    calculateVisibleEndIndex(getScrollPosition(scrollPosition))
  )(
    "horizontal",
    horizontal
  )(
    "initialNumToRender",
    initialNumToRender
  );
}

MapBuffer SLContainerState::getMapBuffer() const {
  auto builder = MapBufferBuilder();
  builder.putInt(SLCONTAINER_STATE_VISIBLE_START_INDEX, calculateVisibleStartIndex(getScrollPosition(scrollPosition)));
  builder.putInt(SLCONTAINER_STATE_VISIBLE_END_INDEX, calculateVisibleEndIndex(getScrollPosition(scrollPosition)));
  builder.putDouble(SLCONTAINER_STATE_SCROLL_POSITION_LEFT, scrollPosition.x);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_POSITION_TOP, scrollPosition.y);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH, scrollContent.width);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT, scrollContent.height);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH, scrollContainer.width);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT, scrollContainer.height);
  builder.putBool(SLCONTAINER_STATE_HORIZONTAL, horizontal);
  builder.putInt(SLCONTAINER_STATE_INITIAL_NUM_TO_RENDER, initialNumToRender);
  return builder.build();
}
#endif

int SLContainerState::calculateVisibleStartIndex(const float visibleStartOffset) const {
  int offset = 5;
  int visibleStartIndex = childrenMeasurements.lower_bound(visibleStartOffset);
  int visibleEndIndexMin = 0;
  return std::max(visibleStartIndex - offset, visibleEndIndexMin);
}

int SLContainerState::calculateVisibleEndIndex(const float visibleStartOffset) const {
  int offset = 5;
  int visibleEndIndex = childrenMeasurements.lower_bound(visibleStartOffset + scrollContainer.height);
  int visibleEndIndexMax = childrenMeasurements.size();
  return std::min(visibleEndIndex + offset, visibleEndIndexMax);
}

Point SLContainerState::calculateScrollPositionOffset(const float visibleStartOffset) const {
  if (horizontal) {
    return Point{visibleStartOffset, scrollPosition.y};
  }
  return Point{scrollPosition.x, visibleStartOffset};
}

float SLContainerState::calculateContentSize() const {
  return childrenMeasurements.sum(childrenMeasurements.size());
}

float SLContainerState::getScrollPosition(const Point& scrollPosition) const {
  return horizontal ? scrollPosition.x : scrollPosition.y;
}

}
