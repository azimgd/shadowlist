#include "SLContainerShadowNode.h"
#include "SLTemplate.h"
#include <iostream>

namespace facebook::react {

extern const char SLContainerComponentName[] = "SLContainer";

std::unordered_map<std::string, std::vector<ShadowNode::Shared>> elementShadowNodeTemplateRegistry{};
std::unordered_map<std::string, ShadowNode::Unshared> elementShadowNodeComponentRegistry{};

struct ComponentRegistryItem {
  int index;
  Float size;
  std::string elementDataUniqueKey;
};

void SLContainerShadowNode::layout(LayoutContext layoutContext) {
  #ifndef RCT_DEBUG
  auto start = std::chrono::high_resolution_clock::now();
  #endif

  auto &props = getConcreteProps();
  auto nextStateData = getStateData();

  const int elementsDataSize = props.data.size();
  const int viewportOffset = 1000;

  nextStateData.scrollPositionUpdated = false;

  /*
   * Store the child nodes for the container
   */
  auto containerShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>();

  bool elementsDataPrepended = elementsDataSize && nextStateData.firstChildUniqueId.size() &&
    nextStateData.firstChildUniqueId != props.uniqueIds.front();
  bool elementsDataAppended = elementsDataSize && nextStateData.lastChildUniqueId.size() &&
    nextStateData.lastChildUniqueId != props.uniqueIds.back();

  if (elementsDataPrepended) {
    nextStateData.scrollIndex = elementsDataSize - nextStateData.childrenMeasurementsTree.size();
    nextStateData.scrollPositionUpdated = true;
  }

  /*
   * If data is prepended, extend the measurements tree from the beginning 
   * to match the new data size. If data is appended, resize the tree at the end
   */
  if (elementsDataPrepended) {
    SLFenwickTree childrenMeasurementsTreeNext{};

    for (auto i = 0; i < elementsDataSize - nextStateData.childrenMeasurementsTree.size(); ++i) {
      childrenMeasurementsTreeNext.push_back(0.0f);
    }
    for (auto measurement : nextStateData.childrenMeasurementsTree) {
      childrenMeasurementsTreeNext.push_back(measurement);
    }

    nextStateData.childrenMeasurementsTree = childrenMeasurementsTreeNext;
  } else {
    nextStateData.childrenMeasurementsTree.resize(elementsDataSize);
  }
  
  /*
   * Static templates measurements, incl. Header, Empty, Footer
   */
  nextStateData.templateMeasurementsTree.resize(3);

  /*
   * Adjust the scroll index when the user is near the top of the container to load more data,
   * and updates the scrollPositionUpdated flag to keep track of changes in the list position,
   * ensuring the visible content stays in the correct place
   */
  if (!nextStateData.scrollPositionUpdated && getRelativePointFromPoint(nextStateData.scrollPosition) < getRelativeSizeFromSize(nextStateData.scrollContainer)) {
    int distanceFromStart = getRelativePointFromPoint(nextStateData.scrollPosition);
    getConcreteEventEmitter().onStartReached({ .distanceFromStart = distanceFromStart });
  }

  if (getRelativeSizeFromSize(nextStateData.scrollContainer) == getRelativeSizeFromSize(getLayoutMetrics().frame.size) && getRelativePointFromPoint(nextStateData.scrollPosition) >= getRelativeSizeFromSize(nextStateData.scrollContent) - getRelativeSizeFromSize(nextStateData.scrollContainer) - 1) {
    int distanceFromEnd = getRelativeSizeFromSize(nextStateData.scrollContainer) - getRelativeSizeFromSize(getLayoutMetrics().frame.size);
    getConcreteEventEmitter().onEndReached({ .distanceFromEnd = distanceFromEnd });
  }

  /*
   * Transforms the element's data into a cached reference to the element instance
   * Clones a new shadow node from a template and adds it to the registry if not already present
   * Measures the layout and stores the height in a measurement tree
   */
  auto transformElementComponent = [&](int elementDataIndex) -> ComponentRegistryItem {
    auto elementDataUniqueKey = props.uniqueIds[elementDataIndex];

    auto elementShadowNodeComponentRegistryIt = elementShadowNodeComponentRegistry.find(elementDataUniqueKey);
    if (elementShadowNodeComponentRegistryIt == elementShadowNodeComponentRegistry.end()) {
      const nlohmann::json& elementData = props.getElementByIndex(elementDataIndex);
      elementShadowNodeComponentRegistry[elementDataUniqueKey] = SLTemplate::cloneShadowNodeTree(elementData, elementShadowNodeTemplateRegistry["ListChildrenComponentUniqueId"].back());
    } else {
      elementShadowNodeComponentRegistry[elementDataUniqueKey] = elementShadowNodeComponentRegistry[elementDataUniqueKey]->clone({});
    }

    // Prevent re-measuring if the height is already defined, as layouting is expensive
    auto elementSize = layoutTree(layoutContext, elementShadowNodeComponentRegistry[elementDataUniqueKey]);
    nextStateData.childrenMeasurementsTree[elementDataIndex] = getRelativeSizeFromSize(elementSize.frame.size);

    return ComponentRegistryItem{
      elementDataIndex,
      nextStateData.childrenMeasurementsTree[elementDataIndex],
      elementDataUniqueKey
    };
  };

  auto transformTemplateComponent = [&](std::string elementDataUniqueKey, int templateDataIndex) -> ComponentRegistryItem {
    auto elementShadowNodeComponentRegistryIt = elementShadowNodeComponentRegistry.find(elementDataUniqueKey);
    if (elementShadowNodeComponentRegistryIt == elementShadowNodeComponentRegistry.end()) {
      const nlohmann::json& elementData = {};
      elementShadowNodeComponentRegistry[elementDataUniqueKey] = SLTemplate::cloneShadowNodeTree(elementData, elementShadowNodeTemplateRegistry[elementDataUniqueKey].back());
    } else {
      elementShadowNodeComponentRegistry[elementDataUniqueKey] = elementShadowNodeComponentRegistry[elementDataUniqueKey]->clone({});
    }

    auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(elementShadowNodeComponentRegistry[elementDataUniqueKey]);

    // Prevent re-measuring if the height is already defined, as layouting is expensive
    auto elementSize = layoutTree(layoutContext, elementShadowNodeComponentRegistry[elementDataUniqueKey]);
    nextStateData.templateMeasurementsTree[templateDataIndex] = getRelativeSizeFromSize(elementSize.frame.size);

    return ComponentRegistryItem{
      -1,
      nextStateData.templateMeasurementsTree[templateDataIndex],
      elementDataUniqueKey
    };
  };

  /*
   * Render and adjust origin of Header template
   */
  transformTemplateComponent("ListHeaderComponentUniqueId", 0);
  containerShadowNodeChildren->push_back(elementShadowNodeComponentRegistry["ListHeaderComponentUniqueId"]);

  /*
   * Calculate sequence of indices above and below the current scroll index
   */
  int scrollContentAboveIndex = 0;
  float scrollContentAboveOffset = nextStateData.templateMeasurementsTree[0];
  auto scrollContentAboveComponents = std::views::iota(0, nextStateData.scrollIndex)
    | std::views::transform(transformElementComponent);

  /*
   * Start from the scroll index and continues until it reaches the end or the next item is
   * no longer visible on the screen
   */
  int scrollContentBelowIndex = 0;
  float scrollContentBelowOffset = 0;
  auto scrollContentBelowComponents = std::views::iota(nextStateData.scrollIndex, elementsDataSize)
    | std::views::take_while([&](int i) {
      float scrollContentNextOffset = getRelativePointFromPoint(nextStateData.scrollPosition) + viewportOffset;
      return scrollContentBelowOffset < scrollContentNextOffset;
    })
    | std::views::transform(transformElementComponent);

  /*
   * Iterate through the components above the scroll content, update their position,
   * and add them to the container if they are visible in the current viewport
   */
  for (ComponentRegistryItem componentRegistryItem : scrollContentAboveComponents) {
    auto elementMetrics = adjustTree(
      { .y = scrollContentAboveOffset + scrollContentBelowOffset },
      elementShadowNodeComponentRegistry[componentRegistryItem.elementDataUniqueKey]);

    scrollContentAboveOffset += componentRegistryItem.size;
    scrollContentAboveIndex = componentRegistryItem.index;
    
    int scrollPosition = nextStateData.scrollPositionUpdated ? (
      scrollContentAboveOffset + getRelativePointFromPoint(nextStateData.scrollPosition) - nextStateData.templateMeasurementsTree[0]
    ) : getRelativePointFromPoint(nextStateData.scrollPosition);
    
    if (getRelativePointFromPoint(elementMetrics.frame.origin) <= (scrollPosition + getRelativeSizeFromSize(nextStateData.scrollContainer) + viewportOffset) &&
      (getRelativePointFromPoint(elementMetrics.frame.origin) + getRelativeSizeFromSize(elementMetrics.frame.size)) >= (scrollPosition - viewportOffset)) {
      containerShadowNodeChildren->push_back(elementShadowNodeComponentRegistry[componentRegistryItem.elementDataUniqueKey]);
    }
  }

  /*
   * Go through the components below the scroll content, update their position,
   * and add them to the container if they are visible in the current viewport
   */
  for (ComponentRegistryItem componentRegistryItem : scrollContentBelowComponents) {
    auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(elementShadowNodeComponentRegistry[componentRegistryItem.elementDataUniqueKey]);
    auto elementMetrics = adjustTree(
      { .y = scrollContentAboveOffset + scrollContentBelowOffset },
      elementShadowNodeComponentRegistry[componentRegistryItem.elementDataUniqueKey]);

    scrollContentBelowOffset += componentRegistryItem.size;
    scrollContentBelowIndex = componentRegistryItem.index;
    
    int scrollPosition = nextStateData.scrollPositionUpdated ? (
      scrollContentAboveOffset + getRelativePointFromPoint(nextStateData.scrollPosition) - nextStateData.templateMeasurementsTree[0]
    ) : getRelativePointFromPoint(nextStateData.scrollPosition);
    
    if (getRelativePointFromPoint(elementMetrics.frame.origin) <= (scrollPosition + getRelativeSizeFromSize(nextStateData.scrollContainer) + viewportOffset) &&
      (getRelativePointFromPoint(elementMetrics.frame.origin) + getRelativeSizeFromSize(elementMetrics.frame.size)) >= (scrollPosition - viewportOffset)) {
      containerShadowNodeChildren->push_back(elementShadowNodeComponentRegistry[componentRegistryItem.elementDataUniqueKey]);
    }
  }

  /*
   * Render and adjust origin of Footer template
   */
  transformTemplateComponent("ListFooterComponentUniqueId", 1);
  containerShadowNodeChildren->push_back(elementShadowNodeComponentRegistry["ListFooterComponentUniqueId"]);
  
  adjustTree(
    { .y = scrollContentAboveOffset + scrollContentBelowOffset },
    elementShadowNodeComponentRegistry["ListFooterComponentUniqueId"]);

  /*
   * Update children and mark the container as dirty to trigger a layout update on state change
   */
  this->children_ = containerShadowNodeChildren;
  yogaNode_.setDirty(true);

  nextStateData.firstChildUniqueId = props.uniqueIds.front();
  nextStateData.lastChildUniqueId = props.uniqueIds.back();
  nextStateData.scrollContainer = getLayoutMetrics().frame.size;
  nextStateData.scrollContentUpdated = true;
  nextStateData.scrollContent.width = getLayoutMetrics().frame.size.width;
  nextStateData.scrollContent.height = (
    nextStateData.childrenMeasurementsTree.sum(elementsDataSize) +
    nextStateData.templateMeasurementsTree.sum(2)
  );

  /*
   * Update the scroll position when new items are prepended to the top of the list
   */
  if (nextStateData.scrollPositionUpdated) {
    nextStateData.scrollPosition.x = 0;
    nextStateData.scrollPosition.y = scrollContentAboveOffset + getRelativePointFromPoint(nextStateData.scrollPosition) - nextStateData.templateMeasurementsTree[0];
  }

  getConcreteEventEmitter().onVisibleChange({
    .visibleStartIndex = scrollContentBelowIndex,
    .visibleEndIndex = scrollContentAboveIndex,
  });

  setStateData(std::move(nextStateData));
  
  #ifndef RCT_DEBUG
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::cout << "SLContainerShadowNode onLayout: " << std::fixed << std::setprecision(2) << duration.count() << " ms" << std::endl;
  #endif
}

void SLContainerShadowNode::appendChild(const ShadowNode::Shared& child) {
  auto uniqueId = static_cast<const SLElementProps&>(*child->getProps()).uniqueId;
  elementShadowNodeTemplateRegistry[uniqueId].push_back(child);
}

void SLContainerShadowNode::replaceChild(
  const ShadowNode& oldChild,
  const ShadowNode::Shared& newChild,
  size_t suggestedIndex) {
  ConcreteShadowNode::replaceChild(oldChild, newChild, suggestedIndex);
}

LayoutMetrics SLContainerShadowNode::layoutTree(LayoutContext layoutContext, ShadowNode::Unshared shadowNode) {
  auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(shadowNode);

  if (getRelativeSizeFromSize(elementShadowNodeLayoutable->getLayoutMetrics().frame.size)) {
    return elementShadowNodeLayoutable->getLayoutMetrics();
  }

  LayoutConstraints layoutConstraints = {};
  layoutConstraints.minimumSize.width = getLayoutMetrics().frame.size.width;
  layoutConstraints.maximumSize.width = getLayoutMetrics().frame.size.width;
  elementShadowNodeLayoutable->layoutTree(layoutContext, layoutConstraints);
  
  return elementShadowNodeLayoutable->getLayoutMetrics();
}

LayoutMetrics SLContainerShadowNode::adjustTree(Point origin, ShadowNode::Unshared shadowNode) {
  auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(shadowNode);
  LayoutMetrics layoutMetrics = elementShadowNodeLayoutable->getLayoutMetrics();
  layoutMetrics.frame.origin = origin;
  elementShadowNodeLayoutable->setLayoutMetrics(layoutMetrics);
  
  return elementShadowNodeLayoutable->getLayoutMetrics();
}

float SLContainerShadowNode::getRelativeSizeFromSize(Size size) {
  if (getConcreteProps().horizontal) {
    return size.width;
  } else {
    return size.height;
  }
}

float SLContainerShadowNode::getRelativePointFromPoint(Point point) {
  if (getConcreteProps().horizontal) {
    return point.x;
  } else {
    return point.y;
  }
}

}
