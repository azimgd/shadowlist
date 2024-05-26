#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>

namespace facebook::react {

class ShadowListContentProps final : public ViewProps {
  public:
  ShadowListContentProps() = default;
  ShadowListContentProps(const PropsParserContext& context, const ShadowListContentProps &sourceProps, const RawProps &rawProps);

  bool inverted{false};
  bool horizontal{false};
  bool hasListHeaderComponent{false};
  bool hasListFooterComponent{false};
  int initialScrollIndex{0};
  double onEndReachedThreshold{0};
  double onStartReachedThreshold{0};
};

}
