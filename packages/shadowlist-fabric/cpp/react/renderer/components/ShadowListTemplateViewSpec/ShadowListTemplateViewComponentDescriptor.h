#pragma once

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>

#include "ShadowListTemplateViewShadowNode.h"

namespace facebook::react {

class ShadowListTemplateViewComponentDescriptor final : public ConcreteComponentDescriptor<ShadowListTemplateViewShadowNode> {
public:
  ShadowListTemplateViewComponentDescriptor(const ComponentDescriptorParameters& parameters) :
    ConcreteComponentDescriptor<ShadowListTemplateViewShadowNode>(parameters) {
  };

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);
  };
};

void ShadowListTemplateViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
