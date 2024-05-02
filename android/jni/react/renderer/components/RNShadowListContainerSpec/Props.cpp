#include <react/renderer/components/RNShadowListContainerSpec/Props.h>
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

ShadowListContainerProps::ShadowListContainerProps(
    const PropsParserContext &context,
    const ShadowListContainerProps &sourceProps,
    const RawProps &rawProps): ViewProps(context, sourceProps, rawProps),

    inverted(convertRawProp(context, rawProps, "inverted", sourceProps.inverted, {false}))
      {}
ShadowListItemProps::ShadowListItemProps(
    const PropsParserContext &context,
    const ShadowListItemProps &sourceProps,
    const RawProps &rawProps): ViewProps(context, sourceProps, rawProps)

    
      {}

}
