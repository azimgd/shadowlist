#pragma once

#include <react/renderer/components/RNShadowListContainerSpec/ShadowNodes.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

using ShadowListContainerComponentDescriptor = ConcreteComponentDescriptor<ShadowListContainerShadowNode>;
using ShadowListItemComponentDescriptor = ConcreteComponentDescriptor<ShadowListItemShadowNode>;

void RNShadowListContainerSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
