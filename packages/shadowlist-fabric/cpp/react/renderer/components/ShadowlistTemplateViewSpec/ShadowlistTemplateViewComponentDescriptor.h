#pragma once

#include "ShadowlistTemplateViewShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

/*
 * Descriptor for <ShadowlistTemplateView> component.
 */
class ShadowlistTemplateViewComponentDescriptor final : public ConcreteComponentDescriptor<ShadowlistTemplateViewShadowNode> {
  public:
  ShadowlistTemplateViewComponentDescriptor(const ComponentDescriptorParameters& parameters) :
    ConcreteComponentDescriptor<ShadowlistTemplateViewShadowNode>(parameters) {
  };

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);
  };
};

void ShadowlistTemplateViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
