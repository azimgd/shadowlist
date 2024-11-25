#include "SLContainerState.h"

namespace facebook::react {

SLContainerState::SLContainerState(
  SLFenwickTree childrenMeasurementsTree,
  Point scrollPosition,
  Size scrollContainer,
  Size scrollContent,
  std::string firstChildUniqueId,
  std::string lastChildUniqueId,
  bool horizontal,
  int initialNumToRender) :
    childrenMeasurementsTree(childrenMeasurementsTree),
    scrollPosition(scrollPosition),
    scrollContainer(scrollContainer),
    scrollContent(scrollContent),
    firstChildUniqueId(firstChildUniqueId),
    lastChildUniqueId(lastChildUniqueId),
    horizontal(horizontal),
    initialNumToRender(initialNumToRender) {}

#ifdef ANDROID
folly::dynamic SLContainerState::getDynamic() const {
  return folly::dynamic::object(
    "childrenMeasurementsTree",
    childrenMeasurementsTreeToDynamic(childrenMeasurementsTree)
  )(
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
    "horizontal",
    horizontal
  )(
    "initialNumToRender",
    initialNumToRender
  )(
    "firstChildUniqueId",
    firstChildUniqueId
  )(
    "lastChildUniqueId",
    lastChildUniqueId
  );
}

MapBuffer SLContainerState::getMapBuffer() const {
  auto builder = MapBufferBuilder();
  builder.putMapBuffer(SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE, childrenMeasurementsTreeToMapBuffer(childrenMeasurementsTree));
  builder.putInt(SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE_SIZE, childrenMeasurementsTree.size());
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

Point SLContainerState::calculateScrollPositionOffset(const float visibleStartOffset) const {
  if (horizontal) {
    return Point{visibleStartOffset, scrollPosition.y};
  }
  return Point{scrollPosition.x, visibleStartOffset};
}

float SLContainerState::calculateContentSize() const {
  return childrenMeasurementsTree.sum(childrenMeasurementsTree.size());
}

float SLContainerState::getScrollPosition(const Point& scrollPosition) const {
  return horizontal ? scrollPosition.x : scrollPosition.y;
}

#ifdef ANDROID
folly::dynamic SLContainerState::childrenMeasurementsTreeToDynamic(SLFenwickTree childrenMeasurementsTree) const {
  folly::dynamic childrenMeasurementsNext = folly::dynamic::array();
  for (size_t i = 0; i < childrenMeasurementsTree.size(); ++i) {
    folly::dynamic measurement = static_cast<float>(childrenMeasurementsTree.at(i));
    childrenMeasurementsNext.push_back(measurement);
  }
  return childrenMeasurementsNext;
}

MapBuffer SLContainerState::childrenMeasurementsTreeToMapBuffer(SLFenwickTree childrenMeasurementsTree) const {
  auto childrenMeasurementsNext = MapBufferBuilder();
  for (size_t i = 0; i < childrenMeasurementsTree.size(); ++i) {
    childrenMeasurementsNext.putDouble(i, childrenMeasurementsTree.at(i));
  }
  return childrenMeasurementsNext.build();
}

SLFenwickTree SLContainerState::childrenMeasurementsTreeFromDynamic(folly::dynamic childrenMeasurementsTree) const {
  SLFenwickTree childrenMeasurementsNext;
  for (size_t i = 0; i < childrenMeasurementsTree.size(); ++i) {
    childrenMeasurementsNext[i] = childrenMeasurementsTree[i].getDouble();
  }
  return childrenMeasurementsNext;
}
#endif

}
