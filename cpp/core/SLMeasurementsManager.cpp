#include "SLMeasurementsManager.h"
#include <react/renderer/components/view/YogaLayoutableShadowNode.h>

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

/*
 *
 */
void SLMeasurementsManager::setTemplate(
  Tag containerTag,
  TemplateUniqueId templateUniqueId,
  TemplateMeasurements templateMeasurements) {
  templatesMeasurements[containerTag][templateUniqueId] = templateMeasurements;
  templatesMeasurementsTotal[containerTag] += templateMeasurements;
}

SLMeasurementsManager::TemplateMeasurements SLMeasurementsManager::getTemplate(
  Tag containerTag,
  TemplateUniqueId templateUniqueId) {
  return templatesMeasurements[containerTag][templateUniqueId];
}

size_t SLMeasurementsManager::getTemplatesSize(
  Tag containerTag) {
  return templatesMeasurements[containerTag].size();
}

/*
 *
 */
void SLMeasurementsManager::setComponent(
  Tag containerTag,
  ComponentUniqueId componentUniqueId,
  ComponentMeasurements componentMeasurements) {
  componentsMeasurements[containerTag][componentUniqueId] = componentMeasurements;
  componentsMeasurementsTotal[containerTag] += componentMeasurements;
}

SLMeasurementsManager::ComponentMeasurements SLMeasurementsManager::getComponent(
  Tag containerTag,
  ComponentUniqueId componentUniqueId) {
  return componentsMeasurements[containerTag][componentUniqueId];
}

size_t SLMeasurementsManager::getComponentsSize(
  Tag containerTag) {
  return componentsMeasurements[containerTag].size();
}

/*
 *
 */
void SLMeasurementsManager::reset(Tag containerTag) {
  templatesMeasurements[containerTag].clear();
  templatesMeasurementsTotal[containerTag] = 0;
}

}
