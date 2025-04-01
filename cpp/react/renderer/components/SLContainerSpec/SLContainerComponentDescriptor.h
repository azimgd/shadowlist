#pragma once

#include "SLContainerShadowNode.h"
#include "SLRegistryManager.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

using namespace azimgd::shadowlist;

class SLContainerComponentDescriptor : public ConcreteComponentDescriptor<SLContainerShadowNode> {
  public:
  SLContainerComponentDescriptor(const ComponentDescriptorParameters& parameters) : ConcreteComponentDescriptor<SLContainerShadowNode>(parameters) {
    registryManager = std::make_shared<SLRegistryManager>();
  }

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);

    static_cast<SLContainerShadowNode&>(shadowNode).setRegistryManager(registryManager);
  }

  private:
  std::shared_ptr<SLRegistryManager> registryManager;
};

void SLContainerSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
