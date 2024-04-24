#include "CraigsListItemProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

CraigsListItemProps::CraigsListItemProps(
  const PropsParserContext &context,
  const CraigsListItemProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps)
  {}

}
