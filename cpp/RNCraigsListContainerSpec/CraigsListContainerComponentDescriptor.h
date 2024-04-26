#pragma once

#include "CraigsListContainerShadowNode.h"
#include "CraigsListItemShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/graphics/RectangleEdges.h>

namespace facebook::react {

class CraigsListContainerComponentDescriptor : public ConcreteComponentDescriptor<CraigsListContainerShadowNode> {
  using ConcreteComponentDescriptor::ConcreteComponentDescriptor;
};

void RNCraigsListContainerSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
