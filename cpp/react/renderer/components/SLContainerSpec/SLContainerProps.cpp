#include "SLContainerProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

SLContainerProps::SLContainerProps(
  const PropsParserContext &context,
  const SLContainerProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps),

  data(convertRawProp(context, rawProps, "data", sourceProps.data, "[]")),
  inverted(convertRawProp(context, rawProps, "inverted", sourceProps.inverted, false)),
  horizontal(convertRawProp(context, rawProps, "horizontal", sourceProps.horizontal, false)),
  initialNumToRender(convertRawProp(context, rawProps, "initialNumToRender", sourceProps.initialNumToRender, 10)),
  numColumns(convertRawProp(context, rawProps, "numColumns", sourceProps.numColumns, 1)),
  initialScrollIndex(convertRawProp(context, rawProps, "initialScrollIndex", sourceProps.initialScrollIndex, 0))
  {
    try {
      uniqueIds = {};
      parsed = nlohmann::json::parse(data).get<nlohmann::json>();
    } catch (const nlohmann::json::parse_error& e) {
      parsed = nlohmann::json::array();
      std::cerr << "SLContainerProps data parse: " << e.what() << ", at: " << e.byte << std::endl;
    } catch (...) {
      parsed = nlohmann::json::array();
      std::cerr << "SLContainerProps data parse: unknown" << std::endl;
    }

    for (const auto& item : parsed) {
      if (item.contains("id") && item["id"].is_string()) {
        uniqueIds.push_back(item["id"].get<std::string>());
      }
    }
  }

const SLContainerProps::SLContainerDataItem& SLContainerProps::getElementByIndex(int index) const {
  if (index < 0 || index >= parsed.size()) {
    throw std::out_of_range("Index out of range");
  }
  return parsed[index];
}

std::string SLContainerProps::getElementValueByPath(const SLContainerDataItem& element, const SLContainerDataItemPath& path) {
  if (!element.contains(path)) return path;
  return element.at(path).get<std::string>();
}

}
