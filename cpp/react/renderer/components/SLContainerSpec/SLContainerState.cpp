#include "SLContainerState.h"

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;
using facebook::react::Size;
using facebook::react::Point;

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
  builder.putBool(SLCONTAINER_STATE_SCROLL_POSITION_UPDATED, scrollPositionUpdated);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH, scrollContainer.width);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT, scrollContainer.height);
  builder.putBool(SLCONTAINER_STATE_SCROLL_CONTAINER_UPDATED, true);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH, scrollContent.width);
  builder.putDouble(SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT, scrollContent.height);
  builder.putBool(SLCONTAINER_STATE_SCROLL_CONTENT_UPDATED, scrollContentUpdated);
  builder.putString(SLCONTAINER_STATE_FIRST_CHILD_UNIQUE_ID, firstChildUniqueId);
  builder.putString(SLCONTAINER_STATE_LAST_CHILD_UNIQUE_ID, lastChildUniqueId);
  builder.putInt(SLCONTAINER_STATE_SCROLL_INDEX, scrollIndex);
  builder.putBool(SLCONTAINER_STATE_SCROLL_INDEX_UPDATED, scrollIndexUpdated);

  return builder.build();
}
#endif

}
