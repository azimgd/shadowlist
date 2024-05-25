#include "ShadowListContentShadowNode.h"

namespace facebook::react {

extern const char ShadowListContentComponentName[] = "ShadowListContent";

/*
 * Native layout function
 */
void ShadowListContentShadowNode::layout(LayoutContext layoutContext) {
  ensureUnsealed();
  ConcreteShadowNode::layout(layoutContext);

  auto &props = getConcreteProps();
  auto state = getStateData();

  state.contentViewMeasurements = calculateContentViewMeasurements(
    layoutContext,
    props.horizontal,
    props.inverted
  );
  setStateData(std::move(state));
}

/*
 * Measure visible container, and all childs aka list
 */
ShadowListFenwickTree ShadowListContentShadowNode::calculateContentViewMeasurements(LayoutContext layoutContext, bool horizontal, bool inverted) {
  auto contentViewMeasurements = ShadowListFenwickTree(yogaNode_.getChildCount());

  for (std::size_t index = 0; index < yogaNode_.getChildCount(); ++index) {
    auto childYogaNode = yogaNode_.getChild(index);
    auto childNodeMetrics = shadowNodeFromContext(childYogaNode).getLayoutMetrics();
    
    if (horizontal) {
      contentViewMeasurements[index] = childNodeMetrics.frame.size.width;
    } else {
      contentViewMeasurements[index] = childNodeMetrics.frame.size.height;
    }
  }

  return contentViewMeasurements;
}

YogaLayoutableShadowNode& ShadowListContentShadowNode::shadowNodeFromContext(YGNodeConstRef yogaNode) {
  return dynamic_cast<YogaLayoutableShadowNode&>(*static_cast<ShadowNode*>(YGNodeGetContext(yogaNode)));
}
}
