#pragma once

#include "SLContainerShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

class SLContainerComponentDescriptor : public ConcreteComponentDescriptor<SLContainerShadowNode> {
  using ConcreteComponentDescriptor::ConcreteComponentDescriptor;

  void adopt(ShadowNode& shadowNode) const override {
    auto& props = static_cast<const SLContainerShadowNode::ConcreteProps&>(*shadowNode.getProps());
    auto& state = static_cast<const SLContainerShadowNode::ConcreteState&>(*shadowNode.getState());
    auto stateData = state.getData();

    if (props.horizontal != stateData.horizontal) {
      stateData.horizontal = props.horizontal;
      state.updateState(std::move(stateData));
    }

    ConcreteComponentDescriptor::adopt(shadowNode);
  }
};

void SLContainerSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
