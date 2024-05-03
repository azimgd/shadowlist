#pragma once

#include "ShadowNodes.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

using ShadowListContainerComponentDescriptor = ConcreteComponentDescriptor<ShadowListContainerShadowNode>;
using ShadowListItemComponentDescriptor = ConcreteComponentDescriptor<ShadowListItemShadowNode>;

}