#include <react/renderer/core/ShadowNode.h>

#ifndef SLRegistryManager_h
#define SLRegistryManager_h

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;
    
class SLRegistryManager {
  public:
  using TemplateUniqueId = std::string;
  using ComponentUniqueId = std::string;
  
  void appendTemplate(TemplateUniqueId templateUniqueId, ShadowNode::Shared templateItem);
  ShadowNode::Shared getTemplate(TemplateUniqueId templateUniqueId);
  bool hasTemplate(TemplateUniqueId templateUniqueId) const;
  
  void appendComponent(TemplateUniqueId templateUniqueId, ComponentUniqueId componentUniqueId, ShadowNode::Shared componentItem);
  ShadowNode::Shared getComponent(ComponentUniqueId componentUniqueId);
  bool hasComponent(ComponentUniqueId componentUniqueId) const;

  void cleanup();

  std::unordered_map<TemplateUniqueId, ShadowNode::Shared> templatesRegistry{};
  std::unordered_map<ComponentUniqueId, ShadowNode::Unshared> componentsRegistry{};
};

}

#endif
