#include "SLElementProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>
#include <react/renderer/debug/debugStringConvertibleUtils.h>

namespace facebook::react {

SLElementProps::SLElementProps(
  const PropsParserContext &context,
  const SLElementProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps),

  uniqueId(convertRawProp(context, rawProps, "uniqueId", sourceProps.uniqueId, {""}))
    {}

#pragma mark - DebugStringConvertible

#if RN_DEBUG_STRING_CONVERTIBLE
SharedDebugStringConvertibleList SLElementProps::getDebugProps() const {
  return SharedDebugStringConvertibleList{
    debugStringConvertibleItem("uniqueId", uniqueId)};
}
#endif

}
