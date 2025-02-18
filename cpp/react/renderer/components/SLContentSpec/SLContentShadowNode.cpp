#include "SLContentShadowNode.h"

namespace facebook::react {

extern const char SLContentComponentName[] = "SLContent";

void SLContentShadowNode::layout(LayoutContext layoutContext) {
  ConcreteShadowNode::layout(layoutContext);
}

void SLContentShadowNode::appendChild(const ShadowNode::Shared& child) {
  ConcreteShadowNode::appendChild(child);
}

}
