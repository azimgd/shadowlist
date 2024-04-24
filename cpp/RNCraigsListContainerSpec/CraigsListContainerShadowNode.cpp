#include "CraigsListContainerShadowNode.h"

namespace facebook::react {

extern const char CraigsListContainerComponentName[] = "CraigsListContainer";

/**
 * Native layout function
 */
void CraigsListContainerShadowNode::layout(LayoutContext layoutContext) {
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
    setStateData(std::move(state));
  }
    
  if (scrollContentTree_.sum(0, scrollContentTree_.size()) != state.scrollContentTree.sum(0, scrollContentTree_.size())) {
    state.scrollContentTree = scrollContentTree_;
    setStateData(std::move(state));
  }
}

/**
 * Measure visible container, and all childs aka list
 */
void CraigsListContainerShadowNode::calculateContainerMeasurements(LayoutContext layoutContext) {
  auto scrollContent = Rect{};
  auto scrollContentTree = CraigsListFenwickTree(yogaNode_.getChildCount());

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

/**
 * Measure layout metrics
 */
CraigsListContainerMetrics CraigsListContainerShadowNode::calculateLayoutMetrics() {
  auto state = getStateData();
  auto visibleStartPixels = std::max(0.0, state.scrollPosition.y);
  auto visibleEndPixels = std::min(state.scrollContent.height, state.scrollPosition.y + state.scrollContainer.height);

  int visibleStartIndex = state.scrollContentTree.lower_bound(visibleStartPixels);
  int visibleEndIndex = state.scrollContentTree.lower_bound(visibleEndPixels);

  int blankTopStartIndex = 0;
  int blankTopEndIndex = std::max(0, visibleStartIndex - 1);

  auto blankTopStartPixels = 0.0;
  auto blankTopEndPixels = state.scrollContentTree.sum(blankTopStartIndex, blankTopEndIndex);

  int blankBottomStartIndex = std::min(size_t(visibleEndIndex + 1), state.scrollContentTree.size());
  int blankBottomEndIndex = state.scrollContentTree.size();

  auto blankBottomStartPixels = state.scrollContentTree.sum(blankBottomStartIndex, state.scrollContentTree.size());
  auto blankBottomEndPixels = state.scrollContentTree.sum(0, state.scrollContentTree.size());

  return CraigsListContainerMetrics{
    visibleStartIndex,
    visibleEndIndex,
    visibleStartPixels,
    visibleEndPixels,
    blankTopStartIndex,
    blankTopEndIndex,
    blankTopStartPixels,
    blankTopEndPixels,
    blankBottomStartIndex,
    blankBottomEndIndex,
    blankBottomStartPixels,
    blankBottomEndPixels,
  };
}

/**
 * Debug string for layout metrics
 */
std::string CraigsListContainerShadowNode::calculateLayoutMetrics(CraigsListContainerMetrics metrics) {
  std::ostringstream oss;
  oss << "visibleStartIndex: " << metrics.visibleStartIndex << std::endl
      << "visibleEndIndex: " << metrics.visibleEndIndex << std::endl
      << "blankTopStartIndex: " << metrics.blankTopStartIndex << std::endl
      << "blankTopEndIndex: " << metrics.blankTopEndIndex << std::endl
      << "blankBottomStartIndex: " << metrics.blankBottomStartIndex << std::endl
      << "blankBottomEndIndex: " << metrics.blankBottomEndIndex << std::endl
      << "---------------" << std::endl << std::endl;

    return oss.str();
}

YogaLayoutableShadowNode& CraigsListContainerShadowNode::shadowNodeFromContext(YGNodeConstRef yogaNode) {
  return dynamic_cast<YogaLayoutableShadowNode&>(*static_cast<ShadowNode*>(YGNodeGetContext(yogaNode)));
}
}
