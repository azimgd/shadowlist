#include "ShadowListItemProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

ShadowListItemProps::ShadowListItemProps(
  const PropsParserContext &context,
  const ShadowListItemProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps)
  {}

}
