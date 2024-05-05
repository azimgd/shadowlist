#pragma once

#include <folly/dynamic.h>
#include "ShadowListContainerState.h"

#ifdef ANDROID
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif

namespace facebook::react {

#ifdef ANDROID
inline folly::dynamic toDynamic(const ShadowListContainerState& state) {
  folly::dynamic newState = folly::dynamic::object();

  folly::dynamic newScrollPosition = folly::dynamic::object();
  newScrollPosition["x"] = state.scrollPosition.x;
  newScrollPosition["y"] = state.scrollPosition.y;
  newState["scrollPosition"] = newScrollPosition;

  folly::dynamic newScrollContainer = folly::dynamic::object();
  newScrollContainer["height"] = state.scrollContainer.height;
  newScrollContainer["width"] = state.scrollContainer.width;
  newState["scrollContainer"] = newScrollContainer;

  folly::dynamic newScrollContent = folly::dynamic::object();
  newScrollContent["height"] = state.scrollContent.height;
  newScrollContent["width"] = state.scrollContent.width;
  newState["scrollContent"] = newScrollContent;

  return newState;
}

inline MapBuffer toMapBuffer(const ShadowListContainerState& state) {
  auto builder = MapBufferBuilder();

  auto scrollPositionMapBuffer = MapBufferBuilder();
  scrollPositionMapBuffer.putDouble(CX_STATE_KEY_SCROLL_POSITION_X, state.scrollPosition.x);
  scrollPositionMapBuffer.putDouble(CX_STATE_KEY_SCROLL_POSITION_Y, state.scrollPosition.y);
  builder.putMapBuffer(CX_STATE_KEY_SCROLL_POSITION, scrollPositionMapBuffer.build());

  auto scrollContainerMapBuffer = MapBufferBuilder();
  scrollContainerMapBuffer.putDouble(CX_STATE_KEY_SCROLL_CONTAINER_WIDTH, state.scrollContainer.width);
  scrollContainerMapBuffer.putDouble(CX_STATE_KEY_SCROLL_CONTAINER_HEIGHT, state.scrollContainer.height);
  builder.putMapBuffer(CX_STATE_KEY_SCROLL_CONTAINER, scrollContainerMapBuffer.build());

  auto scrollContentMapBuffer = MapBufferBuilder();
  scrollContentMapBuffer.putDouble(CX_STATE_KEY_SCROLL_CONTENT_WIDTH, state.scrollContent.width);
  scrollContentMapBuffer.putDouble(CX_STATE_KEY_SCROLL_CONTENT_HEIGHT, state.scrollContent.height);
  builder.putMapBuffer(CX_STATE_KEY_SCROLL_CONTENT, scrollContentMapBuffer.build());

  return builder.build();
}
#endif

} // namespace facebook::react
