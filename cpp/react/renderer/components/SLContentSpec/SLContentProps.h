#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/debug/DebugStringConvertible.h>

namespace facebook::react {

class SLContentProps final : public ViewProps {
  public:
  SLContentProps() = default;
  SLContentProps(const PropsParserContext& context, const SLContentProps &sourceProps, const RawProps &rawProps);

#pragma mark - Props
  
#pragma mark - DebugStringConvertible

#if RN_DEBUG_STRING_CONVERTIBLE
  SharedDebugStringConvertibleList getDebugProps() const override;
#endif

};

}
