#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/debug/DebugStringConvertible.h>

namespace facebook::react {

class SLElementProps final : public ViewProps {
  public:
  SLElementProps() = default;
  SLElementProps(const PropsParserContext& context, const SLElementProps &sourceProps, const RawProps &rawProps);

#pragma mark - Props
  std::string uniqueId{};
  
#pragma mark - DebugStringConvertible

#if RN_DEBUG_STRING_CONVERTIBLE
  SharedDebugStringConvertibleList getDebugProps() const override;
#endif

};

}
