#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>

namespace facebook::react {

class SLElementProps final : public ViewProps {
  public:
  SLElementProps() = default;
  SLElementProps(const PropsParserContext& context, const SLElementProps &sourceProps, const RawProps &rawProps);

#pragma mark - Props
  int index;
  std::string uniqueId;
};

}
