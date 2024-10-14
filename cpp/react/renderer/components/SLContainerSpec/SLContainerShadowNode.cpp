#include "SLContainerShadowNode.h"
#include "SLFenwickTree.hpp"
#include <iostream>

namespace facebook::react {

extern const char SLContainerComponentName[] = "ShadowlistView";

void SLContainerShadowNode::layout(LayoutContext layoutContext) {
  SLFenwickTree tree;
  ConcreteShadowNode::layout(layoutContext);
}

void SLContainerShadowNode::appendChild(const ShadowNode::Shared& child) {
  ConcreteShadowNode::appendChild(child);
}

void SLContainerShadowNode::replaceChild(
  const ShadowNode& oldChild,
  const ShadowNode::Shared& newChild,
  size_t suggestedIndex) {
  ConcreteShadowNode::replaceChild(oldChild, newChild, suggestedIndex);
}

}
