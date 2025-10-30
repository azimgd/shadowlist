#pragma once

#include <react/renderer/imagemanager/ImageRequest.h>
#include <react/renderer/imagemanager/ImageRequestParams.h>
#include <react/renderer/imagemanager/primitives.h>

#ifdef ANDROID
#include <folly/dynamic.h>
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif

namespace facebook::react {

/*
 * State for <Image> component.
 */
class ShadowlistViewState final {
  public:
  ShadowlistViewState() = default;

  ShadowlistViewState(
    double windowContainerHeight,
    double windowContainerWidth,
    double containerOffsetY,
    double containerOffsetX,
    size_t visibleStartIndex,
    size_t visibleEndIndex,
    double totalContainerHeight,
    double totalContainerWidth) :
    windowContainerHeight_(windowContainerHeight),
    windowContainerWidth_(windowContainerWidth),
    containerOffsetY_(containerOffsetY),
    containerOffsetX_(containerOffsetX),
    visibleStartIndex_(visibleStartIndex),
    visibleEndIndex_(visibleEndIndex),
    totalContainerHeight_(totalContainerHeight),
    totalContainerWidth_(totalContainerWidth) {}

#ifdef ANDROID
  ShadowlistViewState(const ShadowlistViewState& previousState, folly::dynamic data) :
    windowContainerHeight_((Float)data["windowContainerHeight"].getDouble()),
    windowContainerWidth_((Float)data["windowContainerWidth"].getDouble()),
    containerOffsetY_((Float)data["containerOffsetY"].getDouble()),
    containerOffsetX_((Float)data["containerOffsetX"].getDouble()),
    visibleStartIndex_(static_cast<size_t>(data["visibleStartIndex"].getInt())),
    visibleEndIndex_(static_cast<size_t>(data["visibleEndIndex"].getInt())),
    totalContainerHeight_((Float)data["totalContainerHeight"].getDouble()),
    totalContainerWidth_((Float)data["totalContainerWidth"].getDouble())
    {};

  /*
   * Empty implementation for Android because it doesn't use this class.
   */
  folly::dynamic getDynamic() const {
    folly::dynamic result = folly::dynamic::object;
    result["windowContainerHeight"] = windowContainerHeight_;
    result["windowContainerWidth"] = windowContainerWidth_;
    result["containerOffsetY"] = containerOffsetY_;
    result["containerOffsetX"] = containerOffsetX_;
    result["visibleStartIndex"] = static_cast<int64_t>(visibleStartIndex_);
    result["visibleEndIndex"] = static_cast<int64_t>(visibleEndIndex_);
    result["totalContainerHeight"] = totalContainerHeight_;
    result["totalContainerWidth"] = totalContainerWidth_;
    return result;
  };
#endif
  
  double windowContainerHeight_{0.0};
  double windowContainerWidth_{0.0};
  double containerOffsetY_{0.0};
  double containerOffsetX_{0.0};
  size_t visibleStartIndex_{};
  size_t visibleEndIndex_{};
  double totalContainerHeight_{0.0};
  double totalContainerWidth_{0.0};
};

}
