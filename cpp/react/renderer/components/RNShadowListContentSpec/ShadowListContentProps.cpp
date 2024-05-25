#include "ShadowListContentProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

ShadowListContentProps::ShadowListContentProps(
  const PropsParserContext &context,
  const ShadowListContentProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps),

  inverted(convertRawProp(context, rawProps, "inverted", sourceProps.inverted, {false})),
  horizontal(convertRawProp(context, rawProps, "horizontal", sourceProps.horizontal, {false}))
  {}

}
