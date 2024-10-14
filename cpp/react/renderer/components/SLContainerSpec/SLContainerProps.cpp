#include "SLContainerProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

SLContainerProps::SLContainerProps(
  const PropsParserContext &context,
  const SLContainerProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps),

  color(convertRawProp(context, rawProps, "color", sourceProps.color, {}))
  {}
}
