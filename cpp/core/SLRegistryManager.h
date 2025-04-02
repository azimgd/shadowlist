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
  
  void setTemplate(
    Tag containerTag,
    TemplateUniqueId templateUniqueId,
    ShadowNode::Shared templateItem);

  ShadowNode::Shared getTemplate(
    Tag containerTag,
    TemplateUniqueId templateUniqueId);

  bool hasTemplate(
    Tag containerTag,
    TemplateUniqueId templateUniqueId);
  
  void setComponent(
    Tag containerTag,
    TemplateUniqueId templateUniqueId,
    ComponentUniqueId componentUniqueId,
    ShadowNode::Unshared componentItem);

  ShadowNode::Unshared getComponent(
    Tag containerTag,
    ComponentUniqueId componentUniqueId);

  bool hasComponent(
    Tag containerTag,
    ComponentUniqueId componentUniqueId);

  void reset(Tag containerTag);

  std::unordered_map<Tag, std::unordered_map<TemplateUniqueId, ShadowNode::Shared>> templatesRegistry{};
  std::unordered_map<Tag, std::unordered_map<ComponentUniqueId, ShadowNode::Unshared>> componentsRegistry{};
};

}

#endif
