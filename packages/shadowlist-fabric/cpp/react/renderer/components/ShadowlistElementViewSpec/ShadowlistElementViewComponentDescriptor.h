#pragma once

#include "ShadowlistElementViewShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

/*
 * Descriptor for <ShadowlistElementView> component.
 */
class ShadowlistElementViewComponentDescriptor final : public ConcreteComponentDescriptor<ShadowlistElementViewShadowNode> {
  public:
  ShadowlistElementViewComponentDescriptor(const ComponentDescriptorParameters& parameters) :
    ConcreteComponentDescriptor<ShadowlistElementViewShadowNode>(parameters) {
  };

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);
  };
};

void ShadowlistElementViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
