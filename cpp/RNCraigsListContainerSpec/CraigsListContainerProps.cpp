#include "CraigsListContainerProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

CraigsListContainerProps::CraigsListContainerProps(
  const PropsParserContext &context,
  const CraigsListContainerProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps),

  color(convertRawProp(context, rawProps, "color", sourceProps.color, {}))
  {}

}
