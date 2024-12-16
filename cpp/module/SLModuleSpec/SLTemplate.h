#include <react/renderer/uimanager/UIManager.h>
#include "SLContainerProps.h"

namespace facebook::react {

class SLTemplate {
  public:
  static ShadowNode::Unshared cloneShadowNodeTree(
    SLContainerProps::SLContainerDataItem* elementData,
    const ShadowNode::Shared& shadowNode);
};

}
