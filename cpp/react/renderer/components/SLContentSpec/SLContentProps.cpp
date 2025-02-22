#include "SLContentProps.h"
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>
#include <react/renderer/debug/debugStringConvertibleUtils.h>

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

SLContentProps::SLContentProps(
  const PropsParserContext &context,
  const SLContentProps &sourceProps,
  const RawProps &rawProps): ViewProps(context, sourceProps, rawProps) {}

#pragma mark - DebugStringConvertible

#if RN_DEBUG_STRING_CONVERTIBLE
SharedDebugStringConvertibleList SLContentProps::getDebugProps() const {
  return SharedDebugStringConvertibleList{};
}
#endif

}
