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

#ifdef ANDROID
constexpr static MapBuffer::Key SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE = 0;
constexpr static MapBuffer::Key SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE_SIZE = 1;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_POSITION_LEFT = 4;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_POSITION_TOP = 5;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH = 6;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT = 7;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH = 8;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT = 9;
constexpr static MapBuffer::Key SLCONTAINER_STATE_HORIZONTAL = 10;
constexpr static MapBuffer::Key SLCONTAINER_STATE_INITIAL_NUM_TO_RENDER = 11;
#endif

class SLContainerState {
  public:
  SLContainerState(
    SLFenwickTree childrenMeasurementsTree,
    Point scrollPosition,
    Size scrollContainer,
    Size scrollContent,
    std::string firstChildUniqueId,
    std::string lastChildUniqueId,
    bool horizontal,
    int initialNumToRender);
  SLContainerState() = default;

  SLFenwickTree childrenMeasurementsTree;
  Point scrollPosition;
  Size scrollContainer;
  Size scrollContent;
  std::string firstChildUniqueId;
  std::string lastChildUniqueId;
  bool horizontal;
  int initialNumToRender;

  Point calculateScrollPositionOffset(const float visibleStartOffset) const;
  float calculateContentSize() const;
  float getScrollPosition(const Point& scrollPosition) const;

#ifdef ANDROID
  folly::dynamic childrenMeasurementsTreeToDynamic(SLFenwickTree childrenMeasurementsTree) const;
  MapBuffer childrenMeasurementsTreeToMapBuffer(SLFenwickTree childrenMeasurementsTree) const;
  SLFenwickTree childrenMeasurementsTreeFromDynamic(folly::dynamic childrenMeasurementsTree) const;
#endif

#ifdef ANDROID
  SLContainerState(SLContainerState const &previousState, folly::dynamic data) :
  childrenMeasurementsTree(
    childrenMeasurementsTreeFromDynamic(data["childrenMeasurementsTree"])
  ),
  scrollPosition({
    (Float)data["scrollPositionLeft"].getDouble(),
    (Float)data["scrollPositionTop"].getDouble()
  }),
  scrollContainer(previousState.scrollContainer),
  scrollContent(previousState.scrollContent),
  firstChildUniqueId(previousState.firstChildUniqueId),
  lastChildUniqueId(previousState.lastChildUniqueId),
  horizontal(previousState.horizontal),
  initialNumToRender(previousState.initialNumToRender) {};

  folly::dynamic getDynamic() const;
  MapBuffer getMapBuffer() const;
#endif
};

}
