#include "ShadowListContainerProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

ShadowListContainerProps::ShadowListContainerProps(
  const PropsParserContext &context,
  const ShadowListContainerProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps),

  inverted(convertRawProp(context, rawProps, "inverted", sourceProps.inverted, {false})),
  horizontal(convertRawProp(context, rawProps, "horizontal", sourceProps.horizontal, {false})),
  hasListHeaderComponent(convertRawProp(context, rawProps, "hasListHeaderComponent", sourceProps.hasListHeaderComponent, {false})),
  hasListFooterComponent(convertRawProp(context, rawProps, "hasListFooterComponent", sourceProps.hasListFooterComponent, {false})),
  initialScrollIndex(convertRawProp(context, rawProps, "initialScrollIndex", sourceProps.initialScrollIndex, {0})),
  onEndReachedThreshold(convertRawProp(context, rawProps, "onEndReachedThreshold", sourceProps.onEndReachedThreshold, {0}))
  {}

}
