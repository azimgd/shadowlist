#include "SLContainerProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

SLContainerProps::SLContainerProps(
  const PropsParserContext &context,
  const SLContainerProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps),

  data(convertRawProp(context, rawProps, "data", sourceProps.data, {})),
  inverted(convertRawProp(context, rawProps, "inverted", sourceProps.inverted, false)),
  horizontal(convertRawProp(context, rawProps, "horizontal", sourceProps.horizontal, false)),
  initialNumToRender(convertRawProp(context, rawProps, "initialNumToRender", sourceProps.initialNumToRender, 10)),
  windowSize(convertRawProp(context, rawProps, "windowSize", sourceProps.windowSize, 2)),
  numColumns(convertRawProp(context, rawProps, "numColumns", sourceProps.numColumns, 1)),
  initialScrollIndex(convertRawProp(context, rawProps, "initialScrollIndex", sourceProps.initialScrollIndex, 0))
  {
    if (data.isArray())
    for (const auto& item : data) {
      if (item.isObject() && item.count("id") && item["id"].isString()) {
        uniqueIds.push_back(item["id"].asString());
      }
    }
  }

}
