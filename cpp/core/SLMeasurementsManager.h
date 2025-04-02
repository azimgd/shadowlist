#include <react/renderer/core/ShadowNode.h>

#ifndef SLMeasurementsManager_h
#define SLMeasurementsManager_h

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;
    
class SLMeasurementsManager {
  public:
  using TemplateUniqueId = std::string;
  using TemplateMeasurements = float;
  using ComponentUniqueId = std::string;
  using ComponentMeasurements = float;

  void setTemplate(
    Tag containerTag,
    TemplateUniqueId templateUniqueId,
    TemplateMeasurements templateMeasurements);

  TemplateMeasurements getTemplate(
    Tag containerTag,
    TemplateUniqueId templateUniqueId);

  size_t getTemplatesSize(Tag containerTag);

  void setComponent(
    Tag containerTag,
    ComponentUniqueId componentUniqueId,
    ComponentMeasurements componentMeasurements);

  ComponentMeasurements getComponent(
    Tag containerTag,
    ComponentUniqueId componentUniqueId);

  size_t getComponentsSize(Tag containerTag);

  void reset(Tag containerTag);

  std::unordered_map<Tag, std::unordered_map<TemplateUniqueId, TemplateMeasurements>> templatesMeasurements{};
  std::unordered_map<Tag, TemplateMeasurements> templatesMeasurementsTotal;
  std::unordered_map<Tag, std::unordered_map<ComponentUniqueId, ComponentMeasurements>> componentsMeasurements{};
  std::unordered_map<Tag, ComponentMeasurements> componentsMeasurementsTotal;
};

}

#endif
