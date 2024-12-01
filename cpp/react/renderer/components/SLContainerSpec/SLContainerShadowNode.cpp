#include "SLContainerShadowNode.h"

namespace facebook::react {

extern const char SLContainerComponentName[] = "SLContainer";

void SLContainerShadowNode::layout(LayoutContext layoutContext) {
  ConcreteShadowNode::layout(layoutContext);

  auto prevStateData = getStateData();
  auto nextStateData = getStateData();
  auto &props = getConcreteProps();

  // The order of operations are important here
  nextStateData.firstChildUniqueId = calculateFirstChildUniqueId(prevStateData, nextStateData);
  nextStateData.lastChildUniqueId = calculateLastChildUniqueId(prevStateData, nextStateData);

  nextStateData.childrenMeasurementsTree = calculateChildrenMeasurementsTree(prevStateData, nextStateData);
  nextStateData.scrollContainer = calculateScrollContainer(prevStateData, nextStateData);
  nextStateData.scrollContent = calculateScrollContent(prevStateData, nextStateData);
  nextStateData.scrollPosition = calculateScrollPosition(prevStateData, nextStateData);

  nextStateData.horizontal = props.horizontal;
  nextStateData.initialNumToRender = props.initialNumToRender;

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

SLFenwickTree SLContainerShadowNode::calculateChildrenMeasurementsTree(const ConcreteStateData prevStateData, const ConcreteStateData nextStateData) {
  auto &props = getConcreteProps();

  int childCount = yogaNode_.getChildCount();
  SLFenwickTree childrenMeasurementsTree(childCount);

  for (int index = 0; index < childCount; ++index) {
    auto &childNode = yogaNodeFromContext(yogaNode_.getChild(index));
    auto nextStyle = yogaNode_.getChild(index)->style();
    nextStyle.setPositionType(yoga::PositionType::Absolute);

    if (props.horizontal) {
      if (
        childNode.getLayoutMetrics().frame.size.width == 0 &&
        prevStateData.childrenMeasurementsTree.size() >= index
      ) {
        childrenMeasurementsTree[index] = prevStateData.childrenMeasurementsTree[index];
      } else {
        childrenMeasurementsTree[index] = childNode.getLayoutMetrics().frame.size.width;
      }

      nextStyle.setPosition(yoga::Edge::Left, yoga::value::points(childrenMeasurementsTree.sum(index)));
      nextStyle.setPosition(yoga::Edge::Top, yoga::value::points(0));
      nextStyle.setPosition(yoga::Edge::Bottom, yoga::value::points(0));
    } else {
      if (
        childNode.getLayoutMetrics().frame.size.height == 0 &&
        prevStateData.childrenMeasurementsTree.size() >= index
      ) {
        childrenMeasurementsTree[index] = prevStateData.childrenMeasurementsTree[index];
      } else {
        childrenMeasurementsTree[index] = childNode.getLayoutMetrics().frame.size.height;
      }
      
      nextStyle.setPosition(yoga::Edge::Top, yoga::value::points(childrenMeasurementsTree.sum(index)));
      nextStyle.setPosition(yoga::Edge::Left, yoga::value::points(0));
      nextStyle.setPosition(yoga::Edge::Right, yoga::value::points(0));
    }

    yogaNode_.getChild(index)->setStyle(nextStyle);
    yogaNode_.getChild(index)->setDirty(true);
  }

  return childrenMeasurementsTree;
}

Point SLContainerShadowNode::calculateScrollPosition(const ConcreteStateData prevStateData, const ConcreteStateData nextStateData) {
  auto &props = getConcreteProps();

  bool appended = (
    prevStateData.firstChildUniqueId.size() > 0 &&
    prevStateData.firstChildUniqueId == nextStateData.firstChildUniqueId &&
    prevStateData.lastChildUniqueId != nextStateData.lastChildUniqueId);
  bool prepended = (
    prevStateData.firstChildUniqueId.size() > 0 &&
    prevStateData.firstChildUniqueId != nextStateData.firstChildUniqueId &&
    prevStateData.lastChildUniqueId == nextStateData.lastChildUniqueId);

  int headerFooter = 1;
  int scrollPositionDiff = 0;
  float verticalPosition = 0;
  float horizontalPosition = 0;
  float initialScrollPosition = nextStateData.childrenMeasurementsTree.sum(props.initialScrollIndex + headerFooter);

  float scrollContentHorizontalDiff = nextStateData.scrollContent.width - prevStateData.scrollContent.width;
  float scrollContentVerticalDiff = nextStateData.scrollContent.height - prevStateData.scrollContent.height;

  if (props.inverted) {
    if (props.horizontal) {
      if (props.initialScrollIndex > 0 && !prepended && !appended) {
        horizontalPosition = initialScrollPosition;
      } else if (prepended) {
        horizontalPosition = scrollContentHorizontalDiff + scrollPositionDiff;
      } else if (appended) {
        horizontalPosition = scrollPositionDiff;
      } else {
        horizontalPosition = nextStateData.scrollContent.width - nextStateData.scrollContainer.width;
      }
    } else {
      if (props.initialScrollIndex > 0 && !prepended && !appended) {
        verticalPosition = initialScrollPosition;
      } else if (prepended) {
        verticalPosition = scrollContentVerticalDiff + scrollPositionDiff;
      } else if (appended) {
        verticalPosition = scrollPositionDiff;
      } else {
        verticalPosition = nextStateData.scrollContent.height - nextStateData.scrollContainer.height;
      }
    }
  } else {
    if (props.horizontal) {
      if (props.initialScrollIndex > 0 && !prepended && !appended) {
        horizontalPosition = initialScrollPosition;
      } else if (appended) {
        horizontalPosition = scrollPositionDiff;
      } else if (prepended) {
        horizontalPosition = scrollContentHorizontalDiff + scrollPositionDiff;
      }
    } else {
      if (props.initialScrollIndex > 0 && !prepended && !appended) {
        verticalPosition = initialScrollPosition;
      } else if (appended) {
        verticalPosition = scrollPositionDiff;
      } else if (prepended) {
        verticalPosition = scrollContentVerticalDiff + scrollPositionDiff;
      }
    }
  }

  return Point{horizontalPosition, verticalPosition};
}

Size SLContainerShadowNode::calculateScrollContent(const ConcreteStateData prevStateData, const ConcreteStateData nextStateData) {
  auto &props = getConcreteProps();

  Size contentSize;
  auto headerFooter = 0;
  auto &headerChildNode = yogaNodeFromContext(yogaNode_.getChild(0));
  auto &footerChildNode = yogaNodeFromContext(yogaNode_.getChild(yogaNode_.getChildCount() - 1));

  if (props.horizontal) {
    contentSize = Size{nextStateData.calculateContentSize(), getLayoutMetrics().frame.size.height};
    headerFooter += headerChildNode.getLayoutMetrics().frame.size.width;
    headerFooter += footerChildNode.getLayoutMetrics().frame.size.width;
  } else {
    contentSize = Size{getLayoutMetrics().frame.size.width, nextStateData.calculateContentSize()};
    headerFooter += headerChildNode.getLayoutMetrics().frame.size.height;
    headerFooter += footerChildNode.getLayoutMetrics().frame.size.height;
  }

  return contentSize;
}

std::string SLContainerShadowNode::calculateFirstChildUniqueId(const ConcreteStateData prevStateData, const ConcreteStateData nextStateData) {
  return std::to_string(getChildren().at(1)->getTag());
}

std::string SLContainerShadowNode::calculateLastChildUniqueId(const ConcreteStateData prevStateData, const ConcreteStateData nextStateData) {
  return std::to_string(getChildren().at(getChildren().size() - 2)->getTag());
}

Size SLContainerShadowNode::calculateScrollContainer(const ConcreteStateData prevStateData, const ConcreteStateData nextStateData) {
  return getLayoutMetrics().frame.size;
}

YogaLayoutableShadowNode& SLContainerShadowNode::yogaNodeFromContext(YGNodeConstRef yogaNode) {
  return dynamic_cast<YogaLayoutableShadowNode&>(*static_cast<ShadowNode*>(YGNodeGetContext(yogaNode)));
}

}
