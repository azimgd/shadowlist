#include "ShadowListContainerShadowNode.h"

namespace facebook::react {

extern const char ShadowListContainerComponentName[] = "ShadowListContainer";

/**
 * Native layout function
 */
void ShadowListContainerShadowNode::layout(LayoutContext layoutContext) {
  ensureUnsealed();
  ConcreteShadowNode::layout(layoutContext);

  calculateContainerMeasurements(layoutContext);

  auto state = getStateData();

  if (scrollContainer_.size != state.scrollContainer) {
    state.scrollContainer = scrollContainer_.size;
    setStateData(std::move(state));
  }
  
  if (scrollContent_.size != state.scrollContent) {
    state.scrollContent = scrollContent_.size;
    state.scrollContentTree = scrollContentTree_;
    setStateData(std::move(state));
  }
}

/**
 * Measure visible container, and all childs aka list
 */
void ShadowListContainerShadowNode::calculateContainerMeasurements(LayoutContext layoutContext) {
  auto scrollContent = Rect{};
  auto scrollContentTree = ShadowListFenwickTree(yogaNode_.getChildCount());

  for (std::size_t index = 0; index < yogaNode_.getChildCount(); ++index) {
    auto childYogaNode = yogaNode_.getChild(index);
    auto childNodeMetrics = shadowNodeFromContext(childYogaNode).getLayoutMetrics();
    scrollContent.unionInPlace(childNodeMetrics.frame);
    scrollContentTree[index] = childNodeMetrics.frame.size.height;
  }

  scrollContent_ = scrollContent;
  scrollContainer_ = getLayoutMetrics().frame;
  scrollContentTree_ = scrollContentTree;
}

YogaLayoutableShadowNode& ShadowListContainerShadowNode::shadowNodeFromContext(YGNodeConstRef yogaNode) {
  return dynamic_cast<YogaLayoutableShadowNode&>(*static_cast<ShadowNode*>(YGNodeGetContext(yogaNode)));
}
}
