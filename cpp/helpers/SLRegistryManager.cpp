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
  return ShadowNode::Shared{};
}

bool SLRegistryManager::hasTemplate(TemplateUniqueId templateUniqueId) const {
  return false;
}

/*
 *
 */
void SLRegistryManager::appendComponent(TemplateUniqueId templateUniqueId, ComponentUniqueId componentUniqueId, ShadowNode::Shared componentItem) {
}

ShadowNode::Shared SLRegistryManager::getComponent(ComponentUniqueId componentUniqueId) {
  return ShadowNode::Shared{};
}

bool SLRegistryManager::hasComponent(ComponentUniqueId componentUniqueId) const {
  return false;
}

/*
 *
 */
void SLRegistryManager::cleanup() {
  componentsRegistry.clear();
  templatesRegistry.clear();
}

}
