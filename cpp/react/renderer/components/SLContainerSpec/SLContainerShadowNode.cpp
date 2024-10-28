#include "SLContainerShadowNode.h"

namespace facebook::react {

extern const char SLContainerComponentName[] = "SLContainer";

void SLContainerShadowNode::layout(LayoutContext layoutContext) {
  ConcreteShadowNode::layout(layoutContext);

  auto state = getStateData();
  state.childrenMeasurements = measureChildren();
  state.scrollPosition = Point{0, 0};
  state.scrollContainer = getLayoutMetrics().frame.size;
  state.scrollContent = Size{getContentBounds().size.width, state.calculateContentSize()};
  setStateData(std::move(state));
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

SLFenwickTree SLContainerShadowNode::measureChildren() {
  int childCount = yogaNode_.getChildCount();
  SLFenwickTree childrenMeasurements(childCount);

  for (int index = 0; index < childCount; ++index) {
    auto childYogaNode = yogaNode_.getChild(index);
    auto childNodeMetrics = shadowNodeFromContext(childYogaNode).getLayoutMetrics();
    childrenMeasurements[index] = childNodeMetrics.frame.size.height;
  }

  return childrenMeasurements;
}

YogaLayoutableShadowNode& SLContainerShadowNode::shadowNodeFromContext(YGNodeConstRef yogaNode) {
  return dynamic_cast<YogaLayoutableShadowNode&>(*static_cast<ShadowNode*>(YGNodeGetContext(yogaNode)));
}

}
