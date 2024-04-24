#include "CraigsListContainerShadowNode.h"
#include <iostream>

namespace facebook::react {

extern const char CraigsListContainerComponentName[] = "CraigsListContainer";

/**
  * Native layout function
  */
void CraigsListContainerShadowNode::layout(LayoutContext layoutContext) {
  ensureUnsealed();
  ConcreteShadowNode::layout(layoutContext);

  auto scrollContainer = getLayoutMetrics();
  auto scrollContent = calculateContainerMeasurements(layoutContext);
  auto state = getStateData();

  if (scrollContainer.frame.size != state.scrollContainer) {
    state.scrollContainer = scrollContainer.frame.size;
    setStateData(std::move(state));
  }
  
  if (scrollContent.size != state.scrollContent) {
    state.scrollContent = scrollContent.size;
    setStateData(std::move(state));
  }
}

/**
  * Measure visible container, and all childs aka list
  */
Rect CraigsListContainerShadowNode::calculateContainerMeasurements(LayoutContext layoutContext) {
  auto contentBoundingRect = Rect{};

  for (std::size_t index = 0; index < yogaNode_.getChildCount(); ++index) {
    auto childYogaNode = yogaNode_.getChild(index);
    auto& childNode = shadowNodeFromContext(childYogaNode);
    contentBoundingRect.unionInPlace(childNode.getLayoutMetrics().frame);
  }

  return contentBoundingRect;
}

YogaLayoutableShadowNode& CraigsListContainerShadowNode::shadowNodeFromContext(YGNodeConstRef yogaNode) {
  return dynamic_cast<YogaLayoutableShadowNode&>(*static_cast<ShadowNode*>(YGNodeGetContext(yogaNode)));
}
}
