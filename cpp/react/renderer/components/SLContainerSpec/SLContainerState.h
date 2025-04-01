#pragma once

#include <react/renderer/graphics/Point.h>
#include <react/renderer/graphics/Size.h>
#include "SLRegistryManager.h"

#ifdef ANDROID
#include <folly/dynamic.h>
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;
using facebook::react::Size;
using facebook::react::Point;

#ifdef ANDROID
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_POSITION_LEFT = 2;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_POSITION_TOP = 3;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_POSITION_UPDATED = 4;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH = 5;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT = 6;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTAINER_UPDATED = 7;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH = 8;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT = 9;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTENT_UPDATED = 10;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_CONTENT_COMPLETED = 15;
constexpr static MapBuffer::Key SLCONTAINER_STATE_FIRST_CHILD_UNIQUE_ID = 11;
constexpr static MapBuffer::Key SLCONTAINER_STATE_LAST_CHILD_UNIQUE_ID = 12;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_INDEX = 13;
constexpr static MapBuffer::Key SLCONTAINER_STATE_SCROLL_INDEX_UPDATED = 14;

#endif

class SLContainerState {
  public:
  SLContainerState(
    Point scrollPosition,
    bool scrollPositionUpdated,
    Size scrollContainer,
    Size scrollContent,
    bool scrollContentUpdated,
    bool scrollContentCompleted,
    std::string firstChildUniqueId,
    std::string lastChildUniqueId,
    int scrollIndex,
    bool scrollIndexUpdated,
    std::weak_ptr<SLRegistryManager> registryManager);
  SLContainerState() = default;

  Point scrollPosition;
  bool scrollPositionUpdated;
  Size scrollContainer;
  Size scrollContent;
  bool scrollContentUpdated;
  bool scrollContentCompleted;
  std::string firstChildUniqueId;
  std::string lastChildUniqueId;
  int scrollIndex;
  bool scrollIndexUpdated;
  std::weak_ptr<SLRegistryManager> registryManager;

#ifdef ANDROID
  SLContainerState(SLContainerState const &previousState, folly::dynamic data) :
  scrollPosition({
    (Float)data["scrollPositionLeft"].getDouble(),
    (Float)data["scrollPositionTop"].getDouble()
  }),
  scrollPositionUpdated(previousState.scrollPositionUpdated),
  scrollContainer(previousState.scrollContainer),
  scrollContent(previousState.scrollContent),
  scrollContentUpdated(previousState.scrollContentUpdated),
  scrollContentCompleted(data["scrollContentCompleted"].getBool()),
  firstChildUniqueId(previousState.firstChildUniqueId),
  lastChildUniqueId(previousState.lastChildUniqueId),
  scrollIndex(previousState.scrollIndex),
  scrollIndexUpdated(previousState.scrollIndexUpdated) {};

  folly::dynamic getDynamic() const;
  MapBuffer getMapBuffer() const;
#endif
};

}
