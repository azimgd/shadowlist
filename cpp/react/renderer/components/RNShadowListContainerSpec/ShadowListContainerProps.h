#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>

namespace facebook::react {

class ShadowListContainerProps final : public ViewProps {
  public:
  ShadowListContainerProps() = default;
  ShadowListContainerProps(const PropsParserContext& context, const ShadowListContainerProps &sourceProps, const RawProps &rawProps);

  bool inverted{false};
  bool horizontal{false};
  int initialScrollIndex{0};
  double onEndReachedThreshold{0};
  double onStartReachedThreshold{0};
};

}
