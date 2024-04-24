#pragma once
#include <iostream>

#include "CraigsListContainerShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/graphics/RectangleEdges.h>

namespace facebook::react {

class CraigsListContainerComponentDescriptor : public ConcreteComponentDescriptor<CraigsListContainerShadowNode> {
  using ConcreteComponentDescriptor::ConcreteComponentDescriptor;

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);
    auto& layoutableShadowNode = static_cast<CraigsListContainerShadowNode&>(shadowNode);
    auto& stateData = static_cast<const CraigsListContainerShadowNode::ConcreteState&>(*shadowNode.getState()).getData();
    
    std::cout << layoutableShadowNode.calculateLayoutMetrics(layoutableShadowNode.calculateLayoutMetrics()) << std::endl;
  }
};

void RNCraigsListContainerSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
