#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>

namespace facebook::react {

class ShadowListContentProps final : public ViewProps {
  public:
  ShadowListContentProps() = default;
  ShadowListContentProps(const PropsParserContext& context, const ShadowListContentProps &sourceProps, const RawProps &rawProps);
};

}
