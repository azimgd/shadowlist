#pragma once

#include "ShadowListFenwickTree.hpp"
#include "Scrollable.h"
#include <react/renderer/graphics/Point.h>
#include <react/renderer/graphics/Size.h>

#ifdef ANDROID
#include <folly/dynamic.h>
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif

namespace facebook::react {

#ifdef ANDROID
constexpr static MapBuffer::Key CX_STATE_KEY_SCROLL_POSITION = 0;
constexpr static MapBuffer::Key CX_STATE_KEY_SCROLL_CONTAINER = 1;
constexpr static MapBuffer::Key CX_STATE_KEY_SCROLL_CONTENT = 2;

constexpr static MapBuffer::Key CX_STATE_KEY_SCROLL_POSITION_X = 0;
constexpr static MapBuffer::Key CX_STATE_KEY_SCROLL_POSITION_Y = 1;

constexpr static MapBuffer::Key CX_STATE_KEY_SCROLL_CONTAINER_WIDTH = 0;
constexpr static MapBuffer::Key CX_STATE_KEY_SCROLL_CONTAINER_HEIGHT = 1;

constexpr static MapBuffer::Key CX_STATE_KEY_SCROLL_CONTENT_WIDTH = 0;
constexpr static MapBuffer::Key CX_STATE_KEY_SCROLL_CONTENT_HEIGHT = 1;
#endif


struct ShadowListContainerLayoutMetrics {
  double height;
};

struct ShadowListContainerExtendedMetrics {
  int visibleStartIndex;
  int visibleEndIndex;
  
  double visibleStartPixels;
  double visibleEndPixels;
  
  int blankTopStartIndex;
  int blankTopEndIndex;
  
  double blankTopStartPixels;
  double blankTopEndPixels;
  
  int blankBottomStartIndex;
  int blankBottomEndIndex;
  
  double blankBottomStartPixels;
  double blankBottomEndPixels;
};

class ShadowListContainerState {
  public:
  ShadowListContainerState(
    Point scrollPosition,
    Size scrollContainer,
    Size scrollContent,
    ShadowListFenwickTree scrollContentTree);
  ShadowListContainerState() = default;

  /*
   * Keep track of container dimensions which a visible area,
   * content dimensions which is a long list from start to bottom,
   * and a scroll position which is an offset from top.
   */
  Point scrollPosition;
  Size scrollContainer;
  Size scrollContent;
  
  /*
   * Binary tree, expensive for updates, cheap for reads
   */
  ShadowListFenwickTree scrollContentTree;
  
  /*
   * Measure layout and children metrics
   */
  ShadowListContainerExtendedMetrics calculateExtendedMetrics(
    Point scrollPosition,
    bool horizontal,
    bool inverted) const;
  ShadowListContainerLayoutMetrics calculateLayoutMetrics() const;
  float calculateItemOffset(int index) const;
  int countTree() const;

#ifdef ANDROID
  ShadowListContainerState(ShadowListContainerState const &previousState, folly::dynamic data){};

  folly::dynamic getDynamic() const {
    folly::dynamic newState = folly::dynamic::object();

    folly::dynamic newScrollPosition = folly::dynamic::object();
    newScrollPosition["x"] = this->scrollPosition.x;
    newScrollPosition["y"] = this->scrollPosition.y;
    newState["scrollPosition"] = newScrollPosition;

    folly::dynamic newScrollContainer = folly::dynamic::object();
    newScrollContainer["height"] = this->scrollContainer.height;
    newScrollContainer["width"] = this->scrollContainer.width;
    newState["scrollContainer"] = newScrollContainer;

    folly::dynamic newScrollContent = folly::dynamic::object();
    newScrollContent["height"] = this->scrollContent.height;
    newScrollContent["width"] = this->scrollContent.width;
    newState["scrollContent"] = newScrollContent;

    return newState;
  };

  MapBuffer getMapBuffer() const {
    auto builder = MapBufferBuilder();

    auto scrollPositionMapBuffer = MapBufferBuilder();
    scrollPositionMapBuffer.putDouble(CX_STATE_KEY_SCROLL_POSITION_X, this->scrollPosition.x);
    scrollPositionMapBuffer.putDouble(CX_STATE_KEY_SCROLL_POSITION_Y, this->scrollPosition.y);
    builder.putMapBuffer(CX_STATE_KEY_SCROLL_POSITION, scrollPositionMapBuffer.build());

    auto scrollContainerMapBuffer = MapBufferBuilder();
    scrollContainerMapBuffer.putDouble(CX_STATE_KEY_SCROLL_CONTAINER_WIDTH, this->scrollContainer.width);
    scrollContainerMapBuffer.putDouble(CX_STATE_KEY_SCROLL_CONTAINER_HEIGHT, this->scrollContainer.height);
    builder.putMapBuffer(CX_STATE_KEY_SCROLL_CONTAINER, scrollContainerMapBuffer.build());

    auto scrollContentMapBuffer = MapBufferBuilder();
    scrollContentMapBuffer.putDouble(CX_STATE_KEY_SCROLL_CONTENT_WIDTH, this->scrollContent.width);
    scrollContentMapBuffer.putDouble(CX_STATE_KEY_SCROLL_CONTENT_HEIGHT, this->scrollContent.height);
    builder.putMapBuffer(CX_STATE_KEY_SCROLL_CONTENT, scrollContentMapBuffer.build());

    return builder.build();
  };
#endif

};

}
