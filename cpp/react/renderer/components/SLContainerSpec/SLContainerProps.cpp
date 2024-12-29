#include "SLContainerProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

nlohmann::json convertDataProp(
  const PropsParserContext& context,
  const RawProps& rawProps,
  const char* name,
  const nlohmann::json sourceValue,
  const nlohmann::json defaultValue) {
  try {
    std::string content = convertRawProp(context, rawProps, name, sourceValue, defaultValue);
    nlohmann::json json = nlohmann::json::parse(content);

    if (!json.is_array()) {
      throw std::runtime_error("data prop must be an array");
    }

    return json;
  } catch (...) {
    return nlohmann::json::array();
  }
}

std::vector<std::string> convertUniqueIdsProp(
  const PropsParserContext& context,
  const RawProps& rawProps,
  const char* name,
  const nlohmann::json sourceValue,
  const nlohmann::json defaultValue) {
  try {
    std::vector<std::string> uniqueIds;

    if (!defaultValue.is_array()) {
      throw std::runtime_error("data prop must be an array");
    }

    for (const auto& item : defaultValue) {
      if (item.contains("id") && item["id"].is_string()) {
        uniqueIds.push_back(item["id"].get<std::string>());
      }
    }
    
    return uniqueIds;
  } catch (...) {
    return std::vector<std::string>();
  }
}

SLContainerProps::SLContainerProps(
  const PropsParserContext &context,
  const SLContainerProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps),

  data(convertDataProp(context, rawProps, "data", sourceProps.data, nlohmann::json::array())),
  uniqueIds(convertUniqueIdsProp(context, rawProps, "uniqueIds", sourceProps.uniqueIds, data)),
  inverted(convertRawProp(context, rawProps, "inverted", sourceProps.inverted, {})),
  horizontal(convertRawProp(context, rawProps, "horizontal", sourceProps.horizontal, {})),
  initialNumToRender(convertRawProp(context, rawProps, "initialNumToRender", sourceProps.initialNumToRender, {})),
  initialScrollIndex(convertRawProp(context, rawProps, "initialScrollIndex", sourceProps.initialScrollIndex, {}))
  {}

const SLContainerProps::SLContainerDataItem& SLContainerProps::getElementByIndex(int index) const {
  if (index < 0 || index >= data.size()) {
    throw std::out_of_range("Index out of range");
  }
  return data[index];
}

std::string SLContainerProps::getElementValueByPath(const SLContainerDataItem& element, const SLContainerDataItemPath& path) {
  if (!element.contains(path)) return path;
  return element.at(path).get<std::string>();
}

}
