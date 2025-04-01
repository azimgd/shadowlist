#include "SLRegistryManager.h"
#include <react/renderer/components/view/YogaLayoutableShadowNode.h>

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

/*
 *
 */
void SLRegistryManager::appendTemplate(TemplateUniqueId templateUniqueId, ShadowNode::Shared templateItem) {
}

ShadowNode::Shared SLRegistryManager::getTemplate(TemplateUniqueId templateUniqueId) {
  return templatesRegistry[templateUniqueId];
}

bool SLRegistryManager::hasTemplate(TemplateUniqueId templateUniqueId) const {
  return templatesRegistry.find(templateUniqueId) != templatesRegistry.end();
}

/*
 *
 */
void SLRegistryManager::appendComponent(TemplateUniqueId templateUniqueId, ComponentUniqueId componentUniqueId, ShadowNode::Unshared componentItem) {
  componentsRegistry[componentUniqueId] = componentItem;
}

ShadowNode::Unshared SLRegistryManager::getComponent(ComponentUniqueId componentUniqueId) {
  return componentsRegistry[componentUniqueId];
}

bool SLRegistryManager::hasComponent(ComponentUniqueId componentUniqueId) const {
  return componentsRegistry.find(componentUniqueId) != componentsRegistry.end();
}

/*
 *
 */
void SLRegistryManager::cleanup() {
  componentsRegistry.clear();
  templatesRegistry.clear();
}

}
