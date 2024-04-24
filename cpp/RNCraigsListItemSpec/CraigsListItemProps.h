#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>

namespace facebook::react {

class CraigsListItemProps final : public ViewProps {
  public:
  CraigsListItemProps() = default;
  CraigsListItemProps(const PropsParserContext& context, const CraigsListItemProps &sourceProps, const RawProps &rawProps);
};

}
