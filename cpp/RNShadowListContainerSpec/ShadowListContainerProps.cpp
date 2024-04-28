#include "ShadowListContainerProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

ShadowListContainerProps::ShadowListContainerProps(
  const PropsParserContext &context,
  const ShadowListContainerProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps)
  {}

}
