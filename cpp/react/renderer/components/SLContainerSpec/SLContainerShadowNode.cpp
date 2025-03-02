#include "SLContainerShadowNode.h"
#include "SLContentShadowNode.h"
#include "SLElementShadowNode.h"
#include "SLRuntimeManager.h"
#include "SLTemplate.h"
#include "Offsetter.h"

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

std::unordered_map<Tag, std::unordered_map<std::string, std::vector<ShadowNode::Shared>>> elementShadowNodeTemplateRegistry{};
std::unordered_map<Tag, std::unordered_map<std::string, ShadowNode::Unshared>> elementShadowNodeComponentRegistry{};

extern const char SLContainerComponentName[] = "SLContainer";

struct ComponentRegistryItem {
  int index;
  Float size;
  std::string elementDataUniqueKey;
};

void SLContainerShadowNode::layout(LayoutContext layoutContext) {
  #ifndef RCT_DEBUG
  auto start = std::chrono::high_resolution_clock::now();
  #endif

  auto &templateRegistry = elementShadowNodeTemplateRegistry[getTag()];
  auto &componentRegistry = elementShadowNodeComponentRegistry[getTag()];

  auto &props = getConcreteProps();
  auto nextStateData = getStateData();

  #ifdef ANDROID
  __android_log_print(ANDROID_LOG_VERBOSE, "shadowlist", "SLContainerShadowNode onLayout %d", nextStateData.scrollIndex);
  #endif

  const int elementsDataSize = props.parsed.size();
  
  int viewportOffset;
  if (props.horizontal) {
    viewportOffset = props.windowSize * getLayoutMetrics().frame.size.width;
  } else {
    viewportOffset = props.windowSize * getLayoutMetrics().frame.size.height;
  }

  nextStateData.scrollPositionUpdated = false;
  nextStateData.scrollContainer = getLayoutMetrics().frame.size;

  if (!nextStateData.childrenMeasurementsTree.size() && props.initialScrollIndex) {
    nextStateData.scrollIndex = props.initialScrollIndex;
    nextStateData.scrollPositionUpdated = true;
    nextStateData.scrollIndexUpdated = true;
  }

  if (!nextStateData.childrenMeasurementsTree.size() && props.inverted) {
    nextStateData.scrollIndex = props.parsed.size();
    nextStateData.scrollPositionUpdated = true;
    nextStateData.scrollIndexUpdated = true;
  }

  /*
   * Store the child nodes for the container
   */
  auto contentShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>();
  OnViewableItemsChanged contentShadowNodeItems = {};

  bool elementsDataPrepended = elementsDataSize && nextStateData.firstChildUniqueId.size() && props.uniqueIds.size() &&
    nextStateData.firstChildUniqueId != props.uniqueIds.front();
  bool elementsDataAppended = elementsDataSize && nextStateData.lastChildUniqueId.size() && props.uniqueIds.size() &&
    nextStateData.lastChildUniqueId != props.uniqueIds.back();

  if (elementsDataPrepended) {
    SLRuntimeManager::getInstance().shiftIndices(elementsDataSize - nextStateData.childrenMeasurementsTree.size());
    nextStateData.scrollIndex = elementsDataSize - nextStateData.childrenMeasurementsTree.size();
    nextStateData.scrollPositionUpdated = true;
  }

  /*
   * Guard case when list was emptied
   */
  if (nextStateData.scrollIndex > props.uniqueIds.size()) {
    nextStateData.scrollIndex = 0;
    nextStateData.scrollPositionUpdated = true;
    nextStateData.scrollIndexUpdated = true;
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

    auto elementShadowNodeComponentRegistryIt = componentRegistry.find(elementDataUniqueKey);
    if (elementShadowNodeComponentRegistryIt == componentRegistry.end()) {
      const nlohmann::json& elementData = props.getElementByIndex(elementDataIndex);
      auto templateKey = SLContainerProps::getElementValueByPath(elementData, "__shadowlist_template_id");
      componentRegistry[elementDataUniqueKey] = SLTemplate::cloneShadowNodeTree(elementDataIndex, elementData, templateRegistry[templateKey].back());
    } else {
      componentRegistry[elementDataUniqueKey] = componentRegistry[elementDataUniqueKey]->clone({});
    }

    // Prevent re-measuring if the height is already defined, as layouting is expensive
    auto elementSize = layoutElement(layoutContext, componentRegistry[elementDataUniqueKey], props.numColumns);
    nextStateData.childrenMeasurementsTree[elementDataIndex] = getRelativeSizeFromSize(elementSize.frame.size);

    return ComponentRegistryItem{
      elementDataIndex,
      nextStateData.childrenMeasurementsTree[elementDataIndex],
      elementDataUniqueKey
    };
  };

  auto transformTemplateComponent = [&](std::string elementDataUniqueKey, int templateDataIndex) -> ComponentRegistryItem {
    componentRegistry[elementDataUniqueKey] = templateRegistry[elementDataUniqueKey].back()->clone({});

    // Prevent re-measuring if the height is already defined, as layouting is expensive
    auto elementSize = layoutElement(layoutContext, componentRegistry[elementDataUniqueKey], 0);
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
  if (props.inverted) {
    transformTemplateComponent("ListFooterComponentUniqueId", 0);
    contentShadowNodeChildren->push_back(componentRegistry["ListFooterComponentUniqueId"]);
  } else {
    transformTemplateComponent("ListHeaderComponentUniqueId", 0);
    contentShadowNodeChildren->push_back(componentRegistry["ListHeaderComponentUniqueId"]);
  }

  /*
   * Calculate sequence of indices above and below the current scroll index
   */
  int scrollContentAboveIndex = 0;
  int scrollContentAboveBound = std::min(nextStateData.scrollIndex, (int) props.uniqueIds.size());
  Offsetter scrollContentAboveOffset{props.numColumns, nextStateData.templateMeasurementsTree[0]};
  auto scrollContentAboveComponents = std::views::iota(0, scrollContentAboveBound)
    | std::views::transform(transformElementComponent);

  /*
   * Start from the scroll index and continues until it reaches the end or the next item is
   * no longer visible on the screen
   */
  int scrollContentBelowIndex = 0;
  int scrollContentBelowBound = std::min(elementsDataSize, (int) props.uniqueIds.size());
  Offsetter scrollContentBelowOffset{props.numColumns};
  auto scrollContentBelowComponents = std::views::iota(nextStateData.scrollIndex, scrollContentBelowBound)
    | std::views::take_while([&](int i) {
      float scrollContentNextOffset = getRelativePointFromPoint(nextStateData.scrollPosition) + viewportOffset;
      return scrollContentBelowOffset.get(i % props.numColumns) < scrollContentNextOffset;
    })
    | std::views::transform(transformElementComponent);

  /*
   * Iterate through the components above the scroll content, update their position,
   * and add them to the container if they are visible in the current viewport
   */
  for (ComponentRegistryItem componentRegistryItem : scrollContentAboveComponents) {
    auto elementMetrics = adjustElement({
      .x = (componentRegistryItem.index % props.numColumns) * (nextStateData.scrollContainer.width / props.numColumns),
      .y = scrollContentAboveOffset.get(componentRegistryItem.index % props.numColumns) + scrollContentBelowOffset.get(componentRegistryItem.index % props.numColumns)
    }, componentRegistry[componentRegistryItem.elementDataUniqueKey]);

    scrollContentAboveOffset.add(componentRegistryItem.index % props.numColumns, componentRegistryItem.size);
    scrollContentAboveIndex = componentRegistryItem.index;

    int scrollPosition = nextStateData.scrollPositionUpdated ? (
      scrollContentAboveOffset.get(componentRegistryItem.index % props.numColumns) + getRelativePointFromPoint(nextStateData.scrollPosition) - nextStateData.templateMeasurementsTree[0]
    ) : getRelativePointFromPoint(nextStateData.scrollPosition);

    if (getRelativePointFromPoint(elementMetrics.frame.origin) <= (scrollPosition + getRelativeSizeFromSize(nextStateData.scrollContainer) + viewportOffset) &&
      (getRelativePointFromPoint(elementMetrics.frame.origin) + getRelativeSizeFromSize(elementMetrics.frame.size)) >= (scrollPosition - viewportOffset)) {
      
      if (true) {
        contentShadowNodeChildren->push_back(componentRegistry[componentRegistryItem.elementDataUniqueKey]);
      }

      contentShadowNodeItems.viewableItems.push_back({
        .key = componentRegistryItem.elementDataUniqueKey,
        .index = componentRegistryItem.index,
        .isViewable = true,
        .origin = {
          .x = elementMetrics.frame.origin.x,
          .y = elementMetrics.frame.origin.y,
        },
        .size = {
          .width = elementMetrics.frame.size.width,
          .height = elementMetrics.frame.size.height,
        },
      });
    }
  }

  /*
   * Go through the components below the scroll content, update their position,
   * and add them to the container if they are visible in the current viewport
   */
  for (ComponentRegistryItem componentRegistryItem : scrollContentBelowComponents) {
    auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(componentRegistry[componentRegistryItem.elementDataUniqueKey]);
    auto elementMetrics = adjustElement({
      .x = (componentRegistryItem.index % props.numColumns) * (nextStateData.scrollContainer.width / props.numColumns),
      .y = scrollContentAboveOffset.get(componentRegistryItem.index % props.numColumns) + scrollContentBelowOffset.get(componentRegistryItem.index % props.numColumns)
    }, componentRegistry[componentRegistryItem.elementDataUniqueKey]);

    scrollContentBelowOffset.add(componentRegistryItem.index % props.numColumns, componentRegistryItem.size);
    scrollContentBelowIndex = componentRegistryItem.index;

    int scrollPosition = nextStateData.scrollPositionUpdated ? (
      scrollContentAboveOffset.get(componentRegistryItem.index % props.numColumns) + getRelativePointFromPoint(nextStateData.scrollPosition) - nextStateData.templateMeasurementsTree[0]
    ) : getRelativePointFromPoint(nextStateData.scrollPosition);

    if (getRelativePointFromPoint(elementMetrics.frame.origin) <= (scrollPosition + getRelativeSizeFromSize(nextStateData.scrollContainer) + viewportOffset) &&
      (getRelativePointFromPoint(elementMetrics.frame.origin) + getRelativeSizeFromSize(elementMetrics.frame.size)) >= (scrollPosition - viewportOffset)) {
      
      if (true) {
        contentShadowNodeChildren->push_back(componentRegistry[componentRegistryItem.elementDataUniqueKey]);
      }

      contentShadowNodeItems.viewableItems.push_back({
        .key = componentRegistryItem.elementDataUniqueKey,
        .index = componentRegistryItem.index,
        .isViewable = true,
        .origin = {
          .x = elementMetrics.frame.origin.x,
          .y = elementMetrics.frame.origin.y,
        },
        .size = {
          .width = elementMetrics.frame.size.width,
          .height = elementMetrics.frame.size.height,
        },
      });
    }
  }

  if (!props.uniqueIds.size()) {
    auto templateRegistryItem = transformTemplateComponent("ListEmptyComponentUniqueId", 1);
    contentShadowNodeChildren->push_back(componentRegistry["ListEmptyComponentUniqueId"]);

    adjustElement({
      .x = 0,
      .y = scrollContentAboveOffset.max() + scrollContentBelowOffset.max()
    }, componentRegistry["ListEmptyComponentUniqueId"]);

    scrollContentBelowOffset.add(0, templateRegistryItem.size);
  }

  /*
   * Render and adjust origin of Footer template
   */
  if (props.inverted) {
    transformTemplateComponent("ListHeaderComponentUniqueId", 1);
    contentShadowNodeChildren->push_back(componentRegistry["ListHeaderComponentUniqueId"]);

    adjustElement({
      .x = 0,
      .y = scrollContentAboveOffset.max() + scrollContentBelowOffset.max()
    }, componentRegistry["ListHeaderComponentUniqueId"]);
  } else {
    transformTemplateComponent("ListFooterComponentUniqueId", 1);
    contentShadowNodeChildren->push_back(componentRegistry["ListFooterComponentUniqueId"]);

    adjustElement({
      .x = 0,
      .y = scrollContentAboveOffset.max() + scrollContentBelowOffset.max()
    }, componentRegistry["ListFooterComponentUniqueId"]);
  }
  
  if (props.uniqueIds.size()) {
    nextStateData.firstChildUniqueId = props.uniqueIds.front();
    nextStateData.lastChildUniqueId = props.uniqueIds.back();
  } else {
    nextStateData.firstChildUniqueId = {};
    nextStateData.lastChildUniqueId = {};
  }

  float scrollContentOffset = (
    scrollContentAboveOffset.max() +
    scrollContentBelowOffset.max() +
    nextStateData.templateMeasurementsTree[1]
  );
  if (props.horizontal) {
    nextStateData.scrollContent.height = nextStateData.scrollContainer.height;
    nextStateData.scrollContent.width = std::max((Float) scrollContentOffset, nextStateData.scrollContent.width);
    nextStateData.scrollContentUpdated = true;
  } else if (!props.horizontal) {
    nextStateData.scrollContent.width = nextStateData.scrollContainer.width;
    nextStateData.scrollContent.height = std::max((Float) scrollContentOffset, nextStateData.scrollContent.height);
    nextStateData.scrollContentUpdated = true;
  }

  /*
   * Update the scroll position when new items are prepended to the top of the list
   */
  if (!nextStateData.scrollIndexUpdated && nextStateData.scrollPositionUpdated && props.horizontal) {
    nextStateData.scrollPosition.y = 0;
    nextStateData.scrollPosition.x = (
      scrollContentAboveOffset.max() + getRelativePointFromPoint(nextStateData.scrollPosition) - nextStateData.templateMeasurementsTree[0]
    );
  } else if (!nextStateData.scrollIndexUpdated && nextStateData.scrollPositionUpdated && !props.horizontal) {
    nextStateData.scrollPosition.x = 0;
    nextStateData.scrollPosition.y = (
      scrollContentAboveOffset.max() + getRelativePointFromPoint(nextStateData.scrollPosition) - nextStateData.templateMeasurementsTree[0]
    );
  }

  if (nextStateData.scrollIndexUpdated && nextStateData.scrollPositionUpdated && props.horizontal) {
    nextStateData.scrollPosition.y = 0;
    nextStateData.scrollPosition.x = scrollContentAboveOffset.max();
    nextStateData.scrollIndexUpdated = false;
  } else if (nextStateData.scrollIndexUpdated && nextStateData.scrollPositionUpdated && !props.horizontal) {
    nextStateData.scrollPosition.x = 0;
    nextStateData.scrollPosition.y = scrollContentAboveOffset.max();
    nextStateData.scrollIndexUpdated = false;
  }

  /*
   * Dynamic overlay
   */
  if (nextStateData.scrollContentCompleted) {
    transformTemplateComponent("ListDynamicComponentUniqueId", 1);
    contentShadowNodeChildren->push_back(componentRegistry["ListDynamicComponentUniqueId"]);

    adjustElement({
      .x = 0,
      .y = 0
    }, componentRegistry["ListDynamicComponentUniqueId"]);

    getConcreteEventEmitter().onViewableItemsChanged({
      .viewableItems = contentShadowNodeItems.viewableItems
    });
  }

  /*
   * Update children and mark the container as dirty to trigger a layout update on state change
   */
  auto contentShadowNode = getChildren()[0].get();
  ConcreteShadowNode::replaceChild(*contentShadowNode, contentShadowNode->clone({
    .children = contentShadowNodeChildren
  }));
  dirtyLayout();

  getConcreteEventEmitter().onVisibleChange({
    .visibleStartIndex = scrollContentBelowIndex,
    .visibleEndIndex = scrollContentAboveIndex,
  });

  getConcreteEventEmitter().onScroll({
    .contentSize = nextStateData.scrollContent,
    .contentOffset = nextStateData.scrollPosition,
  });

  setStateData(std::move(nextStateData));

  #ifndef RCT_DEBUG
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::cout << "SLContainerShadowNode onLayout: " << std::fixed << std::setprecision(2) << duration.count() << " ms" << std::endl;
  #endif
}

void SLContainerShadowNode::appendChild(const ShadowNode::Shared& child) {
  if (dynamic_cast<const SLElementShadowNode*>(child.get())) {
    auto &templateRegistry = elementShadowNodeTemplateRegistry[getTag()];
    auto uniqueId = static_cast<const SLElementProps&>(*child->getProps()).uniqueId;
    templateRegistry[uniqueId].push_back(child);
  } else {
    ConcreteShadowNode::appendChild(child);
  }
}

void SLContainerShadowNode::replaceChild(
  const ShadowNode& oldChild,
  const ShadowNode::Shared& newChild,
  size_t suggestedIndex) {
  ConcreteShadowNode::replaceChild(oldChild, newChild, suggestedIndex);
}

LayoutMetrics SLContainerShadowNode::layoutElement(LayoutContext layoutContext, ShadowNode::Unshared shadowNode, int numColumns) {
  auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(shadowNode);

  if (getRelativeSizeFromSize(elementShadowNodeLayoutable->getLayoutMetrics().frame.size)) {
    return elementShadowNodeLayoutable->getLayoutMetrics();
  }

  LayoutConstraints layoutConstraints = {};

  if (numColumns > 0) {
    layoutConstraints.minimumSize.width = getLayoutMetrics().frame.size.width / numColumns;
    layoutConstraints.maximumSize.width = getLayoutMetrics().frame.size.width / numColumns;
  } else {
    layoutConstraints.minimumSize.width = getLayoutMetrics().frame.size.width;
    layoutConstraints.maximumSize.width = getLayoutMetrics().frame.size.width;
  }

  elementShadowNodeLayoutable->layoutTree(layoutContext, layoutConstraints);

  return elementShadowNodeLayoutable->getLayoutMetrics();
}

LayoutMetrics SLContainerShadowNode::adjustElement(Point origin, ShadowNode::Unshared shadowNode) {
  auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(shadowNode);
  LayoutMetrics layoutMetrics = elementShadowNodeLayoutable->getLayoutMetrics();

  if (getConcreteProps().horizontal) {
    layoutMetrics.frame.origin.y = origin.x;
    layoutMetrics.frame.origin.x = origin.y;
  } else {
    layoutMetrics.frame.origin.y = origin.y;
    layoutMetrics.frame.origin.x = origin.x;
  }
  elementShadowNodeLayoutable->setLayoutMetrics(layoutMetrics);

  return layoutMetrics;
}

LayoutMetrics SLContainerShadowNode::resizeElement(Size size, ShadowNode::Unshared shadowNode) {
  auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(shadowNode);
  LayoutMetrics layoutMetrics = elementShadowNodeLayoutable->getLayoutMetrics();

  layoutMetrics.frame.size.width = size.width;
  layoutMetrics.frame.size.height = size.height;
  elementShadowNodeLayoutable->setLayoutMetrics(layoutMetrics);

  return layoutMetrics;
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
