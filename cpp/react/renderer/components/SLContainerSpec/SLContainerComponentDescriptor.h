#pragma once

#include "SLContainerShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

class SLContainerComponentDescriptor : public ConcreteComponentDescriptor<SLContainerShadowNode> {
  using ConcreteComponentDescriptor::ConcreteComponentDescriptor;

  void adopt(ShadowNode& shadowNode) const override {
    /*
     * Temporary fix to force the triggering of the shadow node's layout phase
     * as for some reason the shadow node doesn't re-layout upon a state update
     */
    auto& containerShadowNodeLayoutable = static_cast<YogaLayoutableShadowNode&>(shadowNode);
    containerShadowNodeLayoutable.setSize({500, 500});

    ConcreteComponentDescriptor::adopt(shadowNode);
  }
};

void SLContainerSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
