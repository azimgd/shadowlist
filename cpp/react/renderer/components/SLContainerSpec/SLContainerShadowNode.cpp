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

  auto prevStateData = getStateData();
  auto nextStateData = getStateData();
  auto &props = getConcreteProps();

  // The order of operations are important here
  nextStateData.childrenMeasurements = calculateChildrenMeasurements(prevStateData, nextStateData);
  nextStateData.scrollContainer = calculateScrollContainer(prevStateData, nextStateData);
  nextStateData.scrollContent = calculateScrollContent(prevStateData, nextStateData);
  nextStateData.scrollPosition = calculateScrollPosition(prevStateData, nextStateData);

  nextStateData.horizontal = props.horizontal;
  nextStateData.initialNumToRender = props.initialNumToRender;

  nextStateData.visibleStartIndex = nextStateData.calculateVisibleStartIndex(
    nextStateData.getScrollPosition(nextStateData.scrollPosition)
  );
  nextStateData.visibleEndIndex = nextStateData.calculateVisibleEndIndex(
    nextStateData.getScrollPosition(nextStateData.scrollPosition)
  );

  setStateData(std::move(nextStateData));
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

SLFenwickTree SLContainerShadowNode::calculateChildrenMeasurements(const ConcreteStateData prevStateData, const ConcreteStateData nextStateData) {
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

Point SLContainerShadowNode::calculateScrollPosition(const ConcreteStateData prevStateData, const ConcreteStateData nextStateData) {
  auto &props = getConcreteProps();

  float verticalPosition;
  float horizontalPosition;
  
  if (props.inverted) {
    if (props.horizontal) {
      horizontalPosition = nextStateData.scrollContent.width - prevStateData.scrollContent.width + nextStateData.scrollPosition.x;
      verticalPosition = 0;
    } else {
      horizontalPosition = 0;
      verticalPosition = nextStateData.scrollContent.height - prevStateData.scrollContent.height + nextStateData.scrollPosition.y;
    }
  } else {
    if (props.horizontal) {
      horizontalPosition = nextStateData.scrollPosition.x;
      verticalPosition = 0;
    } else {
      horizontalPosition = 0;
      verticalPosition = nextStateData.scrollPosition.y;
    }
  }

  return Point{horizontalPosition, verticalPosition};
}

Size SLContainerShadowNode::calculateScrollContent(const ConcreteStateData prevStateData, const ConcreteStateData nextStateData) {
  auto &props = getConcreteProps();
  return props.horizontal ?
    Size{nextStateData.calculateContentSize(), getLayoutMetrics().frame.size.height}:
    Size{getLayoutMetrics().frame.size.width, nextStateData.calculateContentSize()};
}

Size SLContainerShadowNode::calculateScrollContainer(const ConcreteStateData prevStateData, const ConcreteStateData nextStateData) {
  return getLayoutMetrics().frame.size;
}

YogaLayoutableShadowNode& SLContainerShadowNode::shadowNodeFromContext(YGNodeConstRef yogaNode) {
  return dynamic_cast<YogaLayoutableShadowNode&>(*static_cast<ShadowNode*>(YGNodeGetContext(yogaNode)));
}

}
