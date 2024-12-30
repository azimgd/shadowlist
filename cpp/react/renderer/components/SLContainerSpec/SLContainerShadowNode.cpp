#include "SLContainerShadowNode.h"
#include "SLTemplate.h"
#include <iostream>

namespace facebook::react {

extern const char SLContainerComponentName[] = "SLContainer";

std::unordered_map<std::string, std::vector<ShadowNode::Shared>> elementShadowNodeTemplateRegistry{};
std::unordered_map<std::string, ShadowNode::Unshared> elementShadowNodeComponentRegistry{};

struct ComponentRegistryItem {
  int index;
  Float height;
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

  /*
   * Find the index of the first child element that has not been measured
   */
  if (nextStateData.childrenMeasurementsTree.size() && props.data.size() != nextStateData.childrenMeasurementsTree.size()) {
    for (int elementDataIndex = 0; elementDataIndex < props.data.size(); ++elementDataIndex) {
      auto elementDataUniqueKey = props.uniqueIds[elementDataIndex];
      
      if (elementDataUniqueKey == nextStateData.firstChildUniqueId && !elementDataIndex) {
        break;
      }
      
      if (elementDataUniqueKey == nextStateData.firstChildUniqueId) {
        nextStateData.scrollIndex = elementDataIndex + 10;
      }
    }
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
   * Adjust the scroll index when the user is near the top of the container to load more data,
   * and updates the scrollPositionUpdated flag to keep track of changes in the list position,
   * ensuring the visible content stays in the correct place
   */
  if (nextStateData.scrollPosition.y < nextStateData.scrollContainer.height) {
    if (std::max(nextStateData.scrollIndex - 10, 0) != nextStateData.scrollIndex) {
      nextStateData.scrollIndex = std::max(nextStateData.scrollIndex - 10, 0);
      nextStateData.scrollPositionUpdated = true;
    }

    if (!nextStateData.scrollIndex) {
      int distanceFromStart = nextStateData.scrollPosition.y;
      getConcreteEventEmitter().onStartReached({ .distanceFromStart = distanceFromStart });
    }
  }

  if (nextStateData.scrollContainer.height == getLayoutMetrics().frame.size.height && nextStateData.scrollPosition.y >= nextStateData.scrollContent.height - nextStateData.scrollContainer.height - 1) {
    int distanceFromEnd = nextStateData.scrollContainer.height - getLayoutMetrics().frame.size.height;
    getConcreteEventEmitter().onEndReached({ .distanceFromEnd = distanceFromEnd });
  }

  /*
   * Transforms the element's data into a cached reference to the element instance
   * Clones a new shadow node from a template and adds it to the registry if not already present
   * Measures the layout and stores the height in a measurement tree
   */
  auto transform = [&](int elementDataIndex) -> ComponentRegistryItem {
    auto elementDataUniqueKey = props.uniqueIds[elementDataIndex];

    auto elementShadowNodeComponentRegistryIt = elementShadowNodeComponentRegistry.find(elementDataUniqueKey);
    if (elementShadowNodeComponentRegistryIt == elementShadowNodeComponentRegistry.end()) {
      const nlohmann::json& elementData = props.getElementByIndex(elementDataIndex);
      elementShadowNodeComponentRegistry[elementDataUniqueKey] = SLTemplate::cloneShadowNodeTree(elementData, elementShadowNodeTemplateRegistry["ListChildrenComponentUniqueId"].back());
    } else {
      elementShadowNodeComponentRegistry[elementDataUniqueKey] = elementShadowNodeComponentRegistry[elementDataUniqueKey]->clone({});
    }

    auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(elementShadowNodeComponentRegistry[elementDataUniqueKey]);

    // Prevent re-measuring if the height is already defined, as layouting is expensive
    if (elementShadowNodeLayoutable->getLayoutMetrics().frame.size.height == 0) {
      LayoutConstraints layoutConstraints = {};
      layoutConstraints.minimumSize.width = getLayoutMetrics().frame.size.width;
      layoutConstraints.maximumSize.width = getLayoutMetrics().frame.size.width;
      elementShadowNodeLayoutable->layoutTree(layoutContext, layoutConstraints);
    }

    nextStateData.childrenMeasurementsTree[elementDataIndex] = elementShadowNodeLayoutable->getLayoutMetrics().frame.size.height;

    return ComponentRegistryItem{
      elementDataIndex,
      elementShadowNodeLayoutable->getLayoutMetrics().frame.size.height,
      elementDataUniqueKey
    };
  };

  /*
   * Calculate sequence of indices above and below the current scroll index
   */
  int scrollContentAboveIndex = 0;
  float scrollContentAboveOffset = 0;
  auto scrollContentAboveComponents = std::views::iota(0, nextStateData.scrollIndex)
    | std::views::reverse
    | std::views::take(10)
    | std::views::reverse
    | std::views::transform(transform);

  /*
   * Start from the scroll index and continues until it reaches the end or the next item is
   * no longer visible on the screen
   */
  int scrollContentBelowIndex = 0;
  float scrollContentBelowOffset = 0;
  auto scrollContentBelowComponents = std::views::iota(nextStateData.scrollIndex, elementsDataSize)
    | std::views::take_while([&](int i) {
      float scrollContentNextOffset = nextStateData.scrollPosition.y + viewportOffset;
      return scrollContentBelowOffset < scrollContentNextOffset;
    })
    | std::views::transform(transform);

  /*
   * Iterate through the components above the scroll content, update their position,
   * and add them to the container if they are visible in the current viewport
   */
  for (ComponentRegistryItem componentRegistryItem : scrollContentAboveComponents) {
    auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(elementShadowNodeComponentRegistry[componentRegistryItem.elementDataUniqueKey]);
    LayoutMetrics layoutMetrics = elementShadowNodeLayoutable->getLayoutMetrics();
    layoutMetrics.frame.origin.y = scrollContentAboveOffset + scrollContentBelowOffset;
    elementShadowNodeLayoutable->setLayoutMetrics(layoutMetrics);

    scrollContentAboveOffset += componentRegistryItem.height;
    scrollContentAboveIndex = componentRegistryItem.index;
    
    if (layoutMetrics.frame.origin.y <= (nextStateData.scrollPosition.y + nextStateData.scrollContainer.height + viewportOffset) &&
      (layoutMetrics.frame.origin.y + layoutMetrics.frame.size.height) >= (nextStateData.scrollPosition.y - viewportOffset)) {
      containerShadowNodeChildren->push_back(elementShadowNodeComponentRegistry[componentRegistryItem.elementDataUniqueKey]);
    }
  }

  /*
   * Go through the components below the scroll content, update their position,
   * and add them to the container if they are visible in the current viewport
   */
  for (ComponentRegistryItem componentRegistryItem : scrollContentBelowComponents) {
    auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(elementShadowNodeComponentRegistry[componentRegistryItem.elementDataUniqueKey]);
    LayoutMetrics layoutMetrics = elementShadowNodeLayoutable->getLayoutMetrics();
    layoutMetrics.frame.origin.y = scrollContentAboveOffset + scrollContentBelowOffset;
    elementShadowNodeLayoutable->setLayoutMetrics(layoutMetrics);

    scrollContentBelowOffset += componentRegistryItem.height;
    scrollContentBelowIndex = componentRegistryItem.index;
    
    if (layoutMetrics.frame.origin.y <= (nextStateData.scrollPosition.y + nextStateData.scrollContainer.height + viewportOffset) &&
      (layoutMetrics.frame.origin.y + layoutMetrics.frame.size.height) >= (nextStateData.scrollPosition.y - viewportOffset)) {
      containerShadowNodeChildren->push_back(elementShadowNodeComponentRegistry[componentRegistryItem.elementDataUniqueKey]);
    }
  }

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
  nextStateData.scrollContent.height = nextStateData.childrenMeasurementsTree.sum(elementsDataSize);

  /*
   * Update the scroll position when new items are prepended to the top of the list
   */
  if (nextStateData.scrollPositionUpdated) {
    nextStateData.scrollPosition.x = 0;
    nextStateData.scrollPosition.y = scrollContentAboveOffset + nextStateData.scrollPosition.y;
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

}
