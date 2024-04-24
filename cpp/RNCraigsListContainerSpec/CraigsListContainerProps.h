#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>

namespace facebook::react {

class CraigsListContainerProps final : public ViewProps {
  public:
  CraigsListContainerProps() = default;
  CraigsListContainerProps(const PropsParserContext& context, const CraigsListContainerProps &sourceProps, const RawProps &rawProps);
};

}
