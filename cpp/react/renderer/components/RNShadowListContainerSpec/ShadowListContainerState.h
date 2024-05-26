#pragma once

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
    Size scrollContent);
  ShadowListContainerState() = default;

  /*
   * Keep track of container dimensions which a visible area,
   * content dimensions which is a long list from start to bottom,
   * and a scroll position which is an offset from top.
   */
  Point scrollPosition;
  Size scrollContainer;
  Size scrollContent;
};

}
