#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>

namespace facebook::react {

class ShadowListItemProps final : public ViewProps {
  public:
  ShadowListItemProps() = default;
  ShadowListItemProps(const PropsParserContext& context, const ShadowListItemProps &sourceProps, const RawProps &rawProps);
};

}
