#include "SLElementShadowNode.h"

namespace facebook::react {

extern const char SLElementComponentName[] = "SLElement";

void SLElementShadowNode::layout(LayoutContext layoutContext) {
  ConcreteShadowNode::layout(layoutContext);
}

void SLElementShadowNode::appendChild(const ShadowNode::Shared& child) {
  ConcreteShadowNode::appendChild(child);
}

}
