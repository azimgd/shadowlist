#include <react/renderer/uimanager/UIManager.h>
#include "SLContainerProps.h"

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

class SLTemplate {
  public:
  static ShadowNode::Unshared cloneShadowNodeTree(
    const int& elementDataIndex,
    const SLContainerProps::SLContainerDataItem& elementData,
    const ShadowNode::Shared& shadowNode);
};

}
