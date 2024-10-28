#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>

namespace facebook::react {

class SLContainerProps final : public ViewProps {
  public:
  SLContainerProps() = default;
  SLContainerProps(const PropsParserContext& context, const SLContainerProps &sourceProps, const RawProps &rawProps);
};

}
