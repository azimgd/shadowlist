#include "SLContainerShadowNode.h"

#define MEASURE_CHILDREN(childrenMeasurements, childNodeMetrics, isHorizontal) \
  if (isHorizontal) { \
    childrenMeasurements[index] = childNodeMetrics.frame.size.width; \
  } else { \
    childrenMeasurements[index] = childNodeMetrics.frame.size.height; \
  }

namespace facebook::react {

extern const char SLContainerComponentName[] = "SLContainer";

void SLContainerShadowNode::layout(LayoutContext layoutContext) {
  ConcreteShadowNode::layout(layoutContext);

  auto stateData = getStateData();
  auto &props = getConcreteProps();

  // The order of operations are important here
  stateData.childrenMeasurements = calculateChildrenMeasurements(stateData);
  stateData.scrollContainer = calculateScrollContainer(stateData);
  stateData.scrollContent = calculateScrollContent(stateData);
  stateData.scrollPosition = calculateScrollPosition(stateData);

  stateData.horizontal = props.horizontal;
  stateData.initialNumToRender = props.initialNumToRender;

  stateData.visibleStartIndex = stateData.calculateVisibleStartIndex(
    stateData.getScrollPosition(stateData.scrollPosition)
  );
  stateData.visibleEndIndex = stateData.calculateVisibleEndIndex(
    stateData.getScrollPosition(stateData.scrollPosition)
  );

  setStateData(std::move(stateData));
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

SLFenwickTree SLContainerShadowNode::calculateChildrenMeasurements(ConcreteStateData stateData) {
  auto &props = getConcreteProps();

  int childCount = yogaNode_.getChildCount();
  SLFenwickTree childrenMeasurements(childCount);

  for (int index = 0; index < childCount; ++index) {
    auto childYogaNode = yogaNode_.getChild(index);
    auto childNodeMetrics = shadowNodeFromContext(childYogaNode).getLayoutMetrics();
    MEASURE_CHILDREN(childrenMeasurements, childNodeMetrics, props.horizontal);
  }

  return childrenMeasurements;
}

Point SLContainerShadowNode::calculateScrollPosition(ConcreteStateData stateData) {
  auto &props = getConcreteProps();

  if (props.initialScrollIndex == 0) {
    return stateData.scrollPosition;
  }

  if (props.initialScrollIndex >= stateData.childrenMeasurements.size()) {
    return stateData.scrollPosition;
  }

  if (!props.horizontal && !props.inverted) {
    return Point{0, stateData.childrenMeasurements.sum(props.initialScrollIndex)};
  }

  if (props.horizontal && !props.inverted) {
    return Point{stateData.childrenMeasurements.sum(props.initialScrollIndex), 0};
  }

  if (!props.horizontal && props.inverted) {
    float position = std::min(
      (float)(stateData.scrollContent.height - stateData.scrollContainer.height),
      (float)(stateData.childrenMeasurements.sum(stateData.childrenMeasurements.size() - 1 - props.initialScrollIndex))
    );
    return Point{0, position};
  }

  if (props.horizontal && props.inverted) {
    float position = std::min(
      (float)(stateData.scrollContent.width - stateData.scrollContainer.width),
      (float)(stateData.childrenMeasurements.sum(stateData.childrenMeasurements.size() - 1 - props.initialScrollIndex))
    );
    return Point{position, 0};
  }

  return stateData.scrollPosition;
}

Size SLContainerShadowNode::calculateScrollContent(ConcreteStateData stateData) {
  auto &props = getConcreteProps();
  return props.horizontal ?
    Size{stateData.calculateContentSize(), getLayoutMetrics().frame.size.height}:
    Size{getLayoutMetrics().frame.size.width, stateData.calculateContentSize()};
}

Size SLContainerShadowNode::calculateScrollContainer(ConcreteStateData stateData) {
  return getLayoutMetrics().frame.size;
}

YogaLayoutableShadowNode& SLContainerShadowNode::shadowNodeFromContext(YGNodeConstRef yogaNode) {
  return dynamic_cast<YogaLayoutableShadowNode&>(*static_cast<ShadowNode*>(YGNodeGetContext(yogaNode)));
}

}
