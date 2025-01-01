#include "SLContainerState.h"

namespace facebook::react {

SLContainerState::SLContainerState(
  SLFenwickTree childrenMeasurementsTree,
  SLFenwickTree templateMeasurementsTree,
  Point scrollPosition,
  bool scrollPositionUpdated,
  Size scrollContainer,
  Size scrollContent,
  bool scrollContentUpdated,
  std::string firstChildUniqueId,
  std::string lastChildUniqueId,
  int scrollIndex) :
    childrenMeasurementsTree(childrenMeasurementsTree),
    templateMeasurementsTree(templateMeasurementsTree),
    scrollPosition(scrollPosition),
    scrollPositionUpdated(scrollPositionUpdated),
    scrollContainer(scrollContainer),
    scrollContent(scrollContent),
    scrollContentUpdated(scrollContentUpdated),
    firstChildUniqueId(firstChildUniqueId),
    lastChildUniqueId(lastChildUniqueId),
    scrollIndex(scrollIndex) {}

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
  return builder.build();
}
#endif

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
