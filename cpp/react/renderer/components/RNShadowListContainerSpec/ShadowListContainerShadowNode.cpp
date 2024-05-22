#include "ShadowListContainerShadowNode.h"

namespace facebook::react {

extern const char ShadowListContainerComponentName[] = "ShadowListContainer";

/*
 * Native layout function
 */
void ShadowListContainerShadowNode::layout(LayoutContext layoutContext) {
  ensureUnsealed();
  ConcreteShadowNode::layout(layoutContext);

  auto &props = getConcreteProps();
  auto state = getStateData();

  calculateContainerMeasurements(
    layoutContext,
    props.horizontal,
    props.inverted
  );

  if (scrollContainer_.size != state.scrollContainer) {
    state.scrollContainer = scrollContainer_.size;
  }
  
  if (scrollContent_.size != state.scrollContent) {
    state.scrollContent = scrollContent_.size;
    state.scrollContentTree = scrollContentTree_;
  }
  
  if (props.initialScrollIndex && props.horizontal) {
    state.scrollPosition = Point{state.calculateItemOffset(props.initialScrollIndex), 0};
  } else if (props.initialScrollIndex) {
    state.scrollPosition = Point{0, state.calculateItemOffset(props.initialScrollIndex)};
  } else if (props.inverted && props.horizontal) {
    state.scrollPosition = Point{scrollContent_.size.width - scrollContainer_.size.width, 0};
  } else if (props.inverted) {
    state.scrollPosition = Point{0, scrollContent_.size.height - scrollContainer_.size.height};
  } else {
    state.scrollPosition = Point{0, 0};
  }

  setStateData(std::move(state));

  getConcreteEventEmitter().onBatchLayout({
    .size = static_cast<int>(scrollContentTree_.size())
  });
}

/*
 * Measure visible container, and all childs aka list
 */
void ShadowListContainerShadowNode::calculateContainerMeasurements(LayoutContext layoutContext, bool horizontal, bool inverted) {
  auto scrollContent = Rect{};
  auto scrollContentTree = ShadowListFenwickTree(yogaNode_.getChildCount());

  for (std::size_t index = 0; index < yogaNode_.getChildCount(); ++index) {
    auto childYogaNode = yogaNode_.getChild(index);
    auto childNodeMetrics = shadowNodeFromContext(childYogaNode).getLayoutMetrics();
    scrollContent.unionInPlace(childNodeMetrics.frame);
    scrollContentTree[index] = Scrollable::getScrollContentItemSize(childNodeMetrics.frame.size, horizontal);
  }

  scrollContent_ = scrollContent;
  scrollContainer_ = getLayoutMetrics().frame;
  scrollContentTree_ = scrollContentTree;
}

YogaLayoutableShadowNode& ShadowListContainerShadowNode::shadowNodeFromContext(YGNodeConstRef yogaNode) {
  return dynamic_cast<YogaLayoutableShadowNode&>(*static_cast<ShadowNode*>(YGNodeGetContext(yogaNode)));
}
}
