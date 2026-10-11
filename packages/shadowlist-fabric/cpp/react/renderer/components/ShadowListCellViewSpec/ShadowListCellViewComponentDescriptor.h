#pragma once

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>

#include "ShadowListCellViewShadowNode.h"

namespace facebook::react {

class ShadowListCellViewComponentDescriptor final : public ConcreteComponentDescriptor<ShadowListCellViewShadowNode> {
public:
  ShadowListCellViewComponentDescriptor(const ComponentDescriptorParameters& parameters) :
    ConcreteComponentDescriptor<ShadowListCellViewShadowNode>(parameters) {}

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);
  }
};

void ShadowListCellViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
