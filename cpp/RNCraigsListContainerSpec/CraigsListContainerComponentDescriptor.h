#pragma once

#include "CraigsListContainerShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

using CraigsListContainerComponentDescriptor = ConcreteComponentDescriptor<CraigsListContainerShadowNode>;

void RNCraigsListContainerSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
