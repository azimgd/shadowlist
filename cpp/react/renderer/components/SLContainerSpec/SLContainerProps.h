#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>
#include "SLKeyExtractor.h"

#ifndef RCT_DEBUG
#include <iostream>
#ifdef ANDROID
#include <android/log.h>
#endif
#endif

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

class SLContainerProps final : public ViewProps {
  public:
  SLContainerProps() = default;
  SLContainerProps(const PropsParserContext& context, const SLContainerProps &sourceProps, const RawProps &rawProps);

  folly::dynamic data;
  std::vector<std::string> uniqueIds;
  bool inverted = false;
  bool horizontal = false;
  int initialNumToRender = 10;
  int windowSize = 2;
  int numColumns = 1;
  int initialScrollIndex = 0;
  };
}
