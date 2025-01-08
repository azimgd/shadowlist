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
constexpr static MapBuffer::Key SLCONTAINER_STATE_TEMPLATE_MEASUREMENTS_TREE_SIZE = 1;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_POSITION_LEFT = 2;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_POSITION_TOP = 3;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_POSITION_UPDATED = 4;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH = 5;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT = 6;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTAINER_UPDATED = 7;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH = 8;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT = 9;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTENT_UPDATED = 10;
constexpr static MapBuffer::Key SLCONTAINER_STATE_FIRST_CHILD_UNIQUE_ID = 11;
constexpr static MapBuffer::Key SLCONTAINER_STATE_LAST_CHILD_UNIQUE_ID = 12;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_INDEX = 13;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_INDEX_UPDATED = 14;

#endif

class SLContainerState {
  public:
  SLContainerState(
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
    bool scrollIndexUpdated);
  SLContainerState() = default;

  SLFenwickTree childrenMeasurementsTree;
  SLFenwickTree templateMeasurementsTree;
  Point scrollPosition;
  bool scrollPositionUpdated;
  Size scrollContainer;
  Size scrollContent;
  bool scrollContentUpdated;
  std::string firstChildUniqueId;
  std::string lastChildUniqueId;
  int scrollIndex;
  bool scrollIndexUpdated;

#ifdef ANDROID
  SLContainerState(SLContainerState const &previousState, folly::dynamic data) :
  childrenMeasurementsTree(previousState.childrenMeasurementsTree),
  templateMeasurementsTree(previousState.templateMeasurementsTree),
  scrollPosition({
    (Float)data["scrollPositionLeft"].getDouble(),
    (Float)data["scrollPositionTop"].getDouble()
  }),
  scrollPositionUpdated(previousState.scrollPositionUpdated),
  scrollContainer(previousState.scrollContainer),
  scrollContent(previousState.scrollContent),
  scrollContentUpdated(previousState.scrollContentUpdated),
  firstChildUniqueId(previousState.firstChildUniqueId),
  lastChildUniqueId(previousState.lastChildUniqueId),
  scrollIndex(previousState.scrollIndex),
  scrollIndexUpdated(previousState.scrollIndexUpdated) {};

  folly::dynamic getDynamic() const;
  MapBuffer getMapBuffer() const;
#endif
};

}
