#include "ShadowListItemComponentDescriptor.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

void RNShadowListItemSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry) {
  registry->add(concreteComponentDescriptorProvider<ShadowListItemComponentDescriptor>());
}

}
