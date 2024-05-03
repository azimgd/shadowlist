#pragma once

#include "ShadowListContainerShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/graphics/RectangleEdges.h>

namespace facebook::react {

class ShadowListContainerComponentDescriptor : public ConcreteComponentDescriptor<ShadowListContainerShadowNode> {
  using ConcreteComponentDescriptor::ConcreteComponentDescriptor;
};

void RNShadowListContainerSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
