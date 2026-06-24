#pragma once

#include "ShadowListElementViewShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

/*
 * Descriptor for <ShadowListElementView> component.
 */
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
