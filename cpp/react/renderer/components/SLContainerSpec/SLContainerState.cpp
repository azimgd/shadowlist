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
  int scrollIndex,
  bool scrollIndexUpdated) :
    childrenMeasurementsTree(childrenMeasurementsTree),
    templateMeasurementsTree(templateMeasurementsTree),
    scrollPosition(scrollPosition),
    scrollPositionUpdated(scrollPositionUpdated),
    scrollContainer(scrollContainer),
    scrollContent(scrollContent),
    scrollContentUpdated(scrollContentUpdated),
    firstChildUniqueId(firstChildUniqueId),
    lastChildUniqueId(lastChildUniqueId),
    scrollIndex(scrollIndex),
    scrollIndexUpdated(scrollIndexUpdated) {}

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
    "firstChildUniqueId",
    firstChildUniqueId
  )(
    "lastChildUniqueId",
    lastChildUniqueId
  );
}

MapBuffer SLContainerState::getMapBuffer() const {
  auto builder = MapBufferBuilder();
  builder.putDouble(SLCONTAINER_STATE_SCROLL_POSITION_LEFT, scrollPosition.x);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_POSITION_TOP, scrollPosition.y);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH, scrollContent.width);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT, scrollContent.height);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH, scrollContainer.width);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT, scrollContainer.height);
  return builder.build();
}
#endif

}
