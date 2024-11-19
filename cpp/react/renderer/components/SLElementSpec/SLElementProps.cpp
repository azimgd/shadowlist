#include "SLElementProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

SLElementProps::SLElementProps(
  const PropsParserContext &context,
  const SLElementProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps),

  index(convertRawProp(context, rawProps, "index", sourceProps.index, {0})),
  uniqueId(convertRawProp(context, rawProps, "uniqueId", sourceProps.uniqueId, ""))
    {}

}
