#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>

namespace facebook::react {

class ShadowListContainerProps final : public ViewProps {
 public:
  ShadowListContainerProps() = default;
  ShadowListContainerProps(const PropsParserContext& context, const ShadowListContainerProps &sourceProps, const RawProps &rawProps);

#pragma mark - Props

  bool inverted{false};
};

class ShadowListItemProps final : public ViewProps {
 public:
  ShadowListItemProps() = default;
  ShadowListItemProps(const PropsParserContext& context, const ShadowListItemProps &sourceProps, const RawProps &rawProps);

#pragma mark - Props

  
};

}
