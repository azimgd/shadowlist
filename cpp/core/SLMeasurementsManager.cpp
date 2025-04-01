#include "SLMeasurementsManager.h"
#include <react/renderer/components/view/YogaLayoutableShadowNode.h>

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

/*
 *
 */
void SLMeasurementsManager::appendTemplate(TemplateUniqueId templateUniqueId, TemplateMeasurements templateMeasurements) {
  templatesMeasurements[templateUniqueId] = templateMeasurements;
  templatesMeasurementsTotal += templateMeasurements;
}

SLMeasurementsManager::TemplateMeasurements SLMeasurementsManager::getTemplate(TemplateUniqueId templateUniqueId) {
  return templatesMeasurements[templateUniqueId];
}

/*
 *
 */
void SLMeasurementsManager::appendComponent(ComponentUniqueId componentUniqueId, ComponentMeasurements componentMeasurements) {
  componentsMeasurements[componentUniqueId] = componentMeasurements;
  componentsMeasurementsTotal += componentMeasurements;
}

SLMeasurementsManager::ComponentMeasurements SLMeasurementsManager::getComponent(ComponentUniqueId componentUniqueId) {
  return componentsMeasurements[componentUniqueId];
}

/*
 *
 */
void SLMeasurementsManager::cleanup() {
  templatesMeasurements.clear();
  templatesMeasurementsTotal = 0;
}

}
