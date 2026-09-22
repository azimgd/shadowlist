#pragma once

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>

#include "ShadowListElementViewShadowNode.h"

namespace facebook::react {

class ShadowListElementViewComponentDescriptor final : public ConcreteComponentDescriptor<ShadowListElementViewShadowNode> {
public:
  ShadowListElementViewComponentDescriptor(const ComponentDescriptorParameters& parameters) :
    ConcreteComponentDescriptor<ShadowListElementViewShadowNode>(parameters) {
  };

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);
  };
};

void ShadowListElementViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
