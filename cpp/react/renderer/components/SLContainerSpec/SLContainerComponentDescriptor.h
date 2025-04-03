#pragma once

#include "SLContainerShadowNode.h"
#include "SLRegistryManager.h"
#include "SLMeasurementsManager.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

using namespace azimgd::shadowlist;

class SLContainerComponentDescriptor : public ConcreteComponentDescriptor<SLContainerShadowNode> {
  public:
  SLContainerComponentDescriptor(const ComponentDescriptorParameters& parameters) : ConcreteComponentDescriptor<SLContainerShadowNode>(parameters) {
    registryManager = std::make_shared<SLRegistryManager>();
    measurementsManager = std::make_shared<SLMeasurementsManager>();
  }

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);

    auto& containerShadowNode = static_cast<SLContainerShadowNode&>(shadowNode);

    containerShadowNode.setRegistryManager(registryManager);
    containerShadowNode.setMeasurementsManager(measurementsManager);
  }

  private:
  std::shared_ptr<SLRegistryManager> registryManager;
  std::shared_ptr<SLMeasurementsManager> measurementsManager;
};

void SLContainerSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
