#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>
#include "SLKeyExtractor.h"
#include "json.hpp"

#ifndef RCT_DEBUG
#include <iostream>
#ifdef ANDROID
#include <android/log.h>
#endif
#endif

namespace facebook::react {

class SLContainerProps final : public ViewProps {
  public:
  SLContainerProps() = default;
  SLContainerProps(const PropsParserContext& context, const SLContainerProps &sourceProps, const RawProps &rawProps);

  using SLContainerData = nlohmann::json::array_t;
  using SLContainerDataItem = nlohmann::json;
  using SLContainerDataItemPath = std::string;

  std::string data;
  nlohmann::json parsed;
  std::vector<std::string> uniqueIds;
  bool inverted = false;
  bool horizontal = false;
  int initialNumToRender = 10;
  int numColumns = 1;
  int initialScrollIndex = 0;

  const SLContainerDataItem& getElementByIndex(int index) const;
  static std::string getElementValueByPath(const SLContainerDataItem& element, const SLContainerDataItemPath& path) ;
  };
}
