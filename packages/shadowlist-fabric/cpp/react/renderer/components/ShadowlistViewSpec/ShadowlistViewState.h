/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
    windowContainerHeight_{},
    windowContainerWidth_{},
    containerOffsetY_{},
    containerOffsetX_{},
    visibleStartIndex_{},
    visibleEndIndex_{},
    totalContainerHeight_{},
    totalContainerWidth_{}
    {};

  /*
   * Empty implementation for Android because it doesn't use this class.
   */
  folly::dynamic getDynamic() const {
    return {};
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

} // namespace facebook::react
