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

  void appendTemplate(TemplateUniqueId templateUniqueId, TemplateMeasurements templateMeasurements);
  TemplateMeasurements getTemplate(TemplateUniqueId templateUniqueId);
  size_t getTemplatesSize();

  void appendComponent(ComponentUniqueId componentUniqueId, ComponentMeasurements componentMeasurements);
  ComponentMeasurements getComponent(ComponentUniqueId componentUniqueId);
  size_t getComponentsSize();

  void cleanup();

  std::unordered_map<TemplateUniqueId, TemplateMeasurements> templatesMeasurements{};
  TemplateMeasurements templatesMeasurementsTotal;
  std::unordered_map<ComponentUniqueId, ComponentMeasurements> componentsMeasurements{};
  ComponentMeasurements componentsMeasurementsTotal;
};

}

#endif
