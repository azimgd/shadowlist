#include "ComponentDescriptors.h"
#include "SLContainerComponentDescriptor.h"
#include "SLContentComponentDescriptor.h"
#include "SLElementComponentDescriptor.h"

#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

void RNShadowlistSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry) {
  registry->add(concreteComponentDescriptorProvider<SLContainerComponentDescriptor>());
  registry->add(concreteComponentDescriptorProvider<SLContentComponentDescriptor>());
  registry->add(concreteComponentDescriptorProvider<SLElementComponentDescriptor>());
}

}
