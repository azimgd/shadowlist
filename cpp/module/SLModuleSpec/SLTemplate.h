#include <react/renderer/uimanager/UIManager.h>
#include <react/renderer/components/text/RawTextShadowNode.h>
#include "json.hpp"
#include "SLKeyExtractor.h"

namespace facebook::react {

class SLTemplate {
  public:
  static ShadowNode::Shared cloneShadowNodeTree(
    jsi::Runtime *runtime,
    nlohmann::json* elementData,
    const ShadowNode::Shared& shadowNode);
};

}
