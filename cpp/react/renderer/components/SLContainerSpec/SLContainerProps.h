#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>
#include "SLKeyExtractor.h"
#include "json.hpp"

namespace facebook::react {

class SLContainerProps final : public ViewProps {
  public:
  SLContainerProps() = default;
  SLContainerProps(const PropsParserContext& context, const SLContainerProps &sourceProps, const RawProps &rawProps);

  using SLContainerData = nlohmann::json::array_t;
  using SLContainerDataItem = nlohmann::json;
  using SLContainerDataItemPath = std::string;

  nlohmann::json data;
  std::vector<std::string> uniqueIds;
  bool inverted = false;
  bool horizontal = false;
  int initialNumToRender = 10;
  int initialScrollIndex = 0;

  const SLContainerDataItem& getElementByIndex(int index) const;
  static std::string getElementValueByPath(const SLContainerDataItem& element, const SLContainerDataItemPath& path) ;
  };
}
