#include <react/renderer/uimanager/UIManager.h>
#include "SLContainerProps.h"

namespace facebook::react {

class SLTemplate {
  public:
  static ShadowNode::Shared cloneShadowNodeTree(
    jsi::Runtime *runtime,
    SLContainerProps::SLContainerDataItem* elementData,
    const ShadowNode::Shared& shadowNode);
};

}
