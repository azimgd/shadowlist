#include "SLRegistryManager.h"
#include <react/renderer/components/view/YogaLayoutableShadowNode.h>

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

/*
 *
 */
void SLRegistryManager::setTemplate(
  Tag containerTag,
  TemplateUniqueId templateUniqueId,
  ShadowNode::Shared templateItem) {
  templatesRegistry[containerTag][templateUniqueId] = templateItem;
}

ShadowNode::Shared SLRegistryManager::getTemplate(
  Tag containerTag,
  TemplateUniqueId templateUniqueId) {
  return templatesRegistry[containerTag][templateUniqueId];
}

bool SLRegistryManager::hasTemplate(
  Tag containerTag,
  TemplateUniqueId templateUniqueId) {
  return templatesRegistry[containerTag].find(templateUniqueId) != templatesRegistry[containerTag].end();
}

/*
 *
 */
void SLRegistryManager::setComponent(
  Tag containerTag,
  TemplateUniqueId templateUniqueId,
  ComponentUniqueId componentUniqueId,
  ShadowNode::Unshared componentItem) {
  componentsRegistry[containerTag][componentUniqueId] = componentItem;
}

ShadowNode::Unshared SLRegistryManager::getComponent(
  Tag containerTag,
  ComponentUniqueId componentUniqueId) {
  return componentsRegistry[containerTag][componentUniqueId];
}

bool SLRegistryManager::hasComponent(
  Tag containerTag,
  ComponentUniqueId componentUniqueId) {
  return componentsRegistry[containerTag].find(componentUniqueId) != componentsRegistry[containerTag].end();
}

/*
 *
 */
void SLRegistryManager::reset(Tag containerTag) {
  componentsRegistry[containerTag].clear();
  templatesRegistry[containerTag].clear();
}

}
