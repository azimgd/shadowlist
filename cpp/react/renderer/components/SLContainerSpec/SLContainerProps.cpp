#include "SLContainerProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

nlohmann::json convertJsonRawProp(
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

SLContainerProps::SLContainerProps(
  const PropsParserContext &context,
  const SLContainerProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps),

  data(convertJsonRawProp(context, rawProps, "data", sourceProps.data, nlohmann::json::array())),
  inverted(convertRawProp(context, rawProps, "inverted", sourceProps.inverted, {})),
  horizontal(convertRawProp(context, rawProps, "horizontal", sourceProps.horizontal, {})),
  initialNumToRender(convertRawProp(context, rawProps, "initialNumToRender", sourceProps.initialNumToRender, {})),
  initialScrollIndex(convertRawProp(context, rawProps, "initialScrollIndex", sourceProps.initialScrollIndex, {}))
  {}

SLContainerProps::SLContainerDataItem* SLContainerProps::getDataItem(int index) {
  auto elementDataPointer = nlohmann::json::json_pointer("/" + std::to_string(index));
  return &data[elementDataPointer];
}

std::string SLContainerProps::getDataItemContent(nlohmann::json *dataItem, std::string path) {
  auto dataItemContentPath = SLKeyExtractor::extractKey(path);
  if (!dataItemContentPath.has_value()) {
    return path;
  }

  auto dataItemContent = nlohmann::json::json_pointer("/" + dataItemContentPath.value());
  if (!dataItem->contains(dataItemContent) || !dataItem->at(dataItemContent).is_string()) {
    return path;
  }

  return dataItem->at(dataItemContent).get<std::string>();
}

}
