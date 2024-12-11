#include "SLContainerProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

SLContainerProps::SLContainerProps(
  const PropsParserContext &context,
  const SLContainerProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps),

  data(convertRawProp(context, rawProps, "data", sourceProps.data, {})),
  inverted(convertRawProp(context, rawProps, "inverted", sourceProps.inverted, {})),
  horizontal(convertRawProp(context, rawProps, "horizontal", sourceProps.horizontal, {})),
  initialNumToRender(convertRawProp(context, rawProps, "initialNumToRender", sourceProps.initialNumToRender, {})),
  initialScrollIndex(convertRawProp(context, rawProps, "initialScrollIndex", sourceProps.initialScrollIndex, {}))
  {}
}
