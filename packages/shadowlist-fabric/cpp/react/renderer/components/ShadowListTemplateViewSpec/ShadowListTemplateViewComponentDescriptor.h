#pragma once

#include "ShadowListTemplateViewShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

/*
 * Descriptor for <ShadowListTemplateView> component.
 */
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
