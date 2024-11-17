#include "SLElementComponentDescriptor.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

void SLElementSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry) {
  registry->add(concreteComponentDescriptorProvider<SLElementComponentDescriptor>());
}

}
