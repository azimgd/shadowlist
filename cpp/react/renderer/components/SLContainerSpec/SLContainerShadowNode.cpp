#include "SLContainerShadowNode.h"
#include "SLContentShadowNode.h"
#include "SLElementShadowNode.h"
#include "SLRuntimeManager.h"
#include "SLTemplate.h"
#include "Offsetter.h"

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

extern const char SLContainerComponentName[] = "SLContainer";

struct ComponentRegistryItem {
  int index;
  Float size;
  std::string elementDataUniqueKey;
};

void SLContainerShadowNode::setRegistryManager(std::shared_ptr<SLRegistryManager> registry) {
  registryManager = std::move(registry);
}

void SLContainerShadowNode::resetRegistryManager() {
  registryManager->cleanup(getTag());
}

void SLContainerShadowNode::setMeasurementsManager(std::shared_ptr<SLMeasurementsManager> measurements) {
  measurementsManager = std::move(measurements);
}

void SLContainerShadowNode::resetMeasurementsManager() {
  measurementsManager->cleanup(getTag());
}

void SLContainerShadowNode::layout(LayoutContext layoutContext) {
  #ifndef RCT_DEBUG
  auto start = std::chrono::high_resolution_clock::now();
  #endif

  auto &props = getConcreteProps();
  auto nextStateData = getStateData();

  nextStateData.registryManager = registryManager;

  /*
   * The first state update on iOS doesn't trigger :updateState like it does on Android
   * as a result, flags like scrollPositionUpdated and scrollContentUpdate aren't applied correctly
   * to fix this, we ignore the first layout phase and gracefully trigger a second layout event
   */
  #if TARGET_OS_IPHONE
  if (getState()->getRevision() == State::initialRevisionValue) {
    setStateData(std::move(nextStateData));
    return;
  }
  bool isInitialState = getState()->getRevision() == State::initialRevisionValue + 1;
  #else
  bool isInitialState = getState()->getRevision() == State::initialRevisionValue;
  #endif

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

  if (isInitialState && props.initialScrollIndex) {
    nextStateData.scrollIndex = props.initialScrollIndex;
    nextStateData.scrollPositionUpdated = true;
    nextStateData.scrollIndexUpdated = true;
  }

  if (isInitialState && props.inverted) {
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

  if (elementsDataPrepended) {
    SLRuntimeManager::getInstance().shiftIndices(elementsDataSize - measurementsManager->getComponentsSize(getTag()));
    nextStateData.scrollIndex = elementsDataSize - measurementsManager->getComponentsSize(getTag());
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
  } else {
  }

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
    auto &elementData = props.getElementByIndex(elementDataIndex);
    auto templateKey = SLContainerProps::getElementValueByPath(elementData, "__shadowlist_template_id");

    ShadowNode::Unshared componentItem;

    if (!registryManager->hasComponent(getTag(), elementDataUniqueKey)) {
      auto templateItem = registryManager->getTemplate(getTag(), templateKey);
      componentItem = SLTemplate::cloneShadowNodeTree(elementDataIndex, elementData, templateItem);
      registryManager->appendComponent(getTag(), templateKey, elementDataUniqueKey, componentItem);
    } else {
      componentItem = registryManager->getComponent(getTag(), elementDataUniqueKey)->clone({});
      registryManager->appendComponent(getTag(), templateKey, elementDataUniqueKey, componentItem);
    }

    // Prevent re-measuring if the height is already defined, as layouting is expensive
    auto elementSize = layoutElement(layoutContext, componentItem, props.numColumns);
    auto componentSize = getRelativeSizeFromSize(elementSize.frame.size);

    measurementsManager->appendComponent(getTag(), elementDataUniqueKey, componentSize);

    return ComponentRegistryItem{
      elementDataIndex,
      componentSize,
      elementDataUniqueKey
    };
  };

  auto transformTemplateComponent = [&](std::string elementDataUniqueKey, int templateDataIndex) -> ComponentRegistryItem {
    auto componentItem = registryManager->getTemplate(getTag(), elementDataUniqueKey)->clone({});
    registryManager->appendComponent(getTag(), elementDataUniqueKey, elementDataUniqueKey, componentItem);

    // Prevent re-measuring if the height is already defined, as layouting is expensive
    auto elementSize = layoutElement(layoutContext, componentItem, 0);
    auto componentSize = getRelativeSizeFromSize(elementSize.frame.size);
    
    measurementsManager->appendTemplate(getTag(), elementDataUniqueKey, componentSize);

    return ComponentRegistryItem{
      -1,
      componentSize,
      elementDataUniqueKey
    };
  };

  /*
   * Render and adjust origin of Header template
   */
  if (props.inverted) {
    auto componentUniqueId = "ListFooterComponentUniqueId";
    transformTemplateComponent(componentUniqueId, 0);
    auto componentItem = registryManager->getComponent(getTag(), componentUniqueId);
    contentShadowNodeChildren->push_back(componentItem);
  } else {
    auto componentUniqueId = "ListHeaderComponentUniqueId";
    transformTemplateComponent(componentUniqueId, 0);
    auto componentItem = registryManager->getComponent(getTag(), componentUniqueId);
    contentShadowNodeChildren->push_back(componentItem);
  }

  /*
   * Calculate sequence of indices above and below the current scroll index
   */
  int scrollContentAboveIndex = 0;
  int scrollContentAboveBound = std::min(nextStateData.scrollIndex, (int) props.uniqueIds.size());
  Offsetter scrollContentAboveOffset{props.numColumns, measurementsManager->getTemplate(getTag(), "ListHeaderComponentUniqueId")};
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
    auto componentItem = registryManager->getComponent(getTag(), componentRegistryItem.elementDataUniqueKey);
    auto elementMetrics = adjustElement({
      .x = (componentRegistryItem.index % props.numColumns) * (nextStateData.scrollContainer.width / props.numColumns),
      .y = scrollContentAboveOffset.get(componentRegistryItem.index % props.numColumns) + scrollContentBelowOffset.get(componentRegistryItem.index % props.numColumns)
    }, componentItem);

    scrollContentAboveOffset.add(componentRegistryItem.index % props.numColumns, componentRegistryItem.size);
    scrollContentAboveIndex = componentRegistryItem.index;

    int scrollPosition = nextStateData.scrollPositionUpdated ? (
      scrollContentAboveOffset.get(componentRegistryItem.index % props.numColumns) + getRelativePointFromPoint(nextStateData.scrollPosition) - measurementsManager->getTemplate(getTag(), "ListHeaderComponentUniqueId")
    ) : getRelativePointFromPoint(nextStateData.scrollPosition);

    if (getRelativePointFromPoint(elementMetrics.frame.origin) <= (scrollPosition + getRelativeSizeFromSize(nextStateData.scrollContainer) + viewportOffset) &&
      (getRelativePointFromPoint(elementMetrics.frame.origin) + getRelativeSizeFromSize(elementMetrics.frame.size)) >= (scrollPosition - viewportOffset)) {
      
      if (true) {
        contentShadowNodeChildren->push_back(componentItem);
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
    auto componentItem = registryManager->getComponent(getTag(), componentRegistryItem.elementDataUniqueKey);
    auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(componentItem);
    auto elementMetrics = adjustElement({
      .x = (componentRegistryItem.index % props.numColumns) * (nextStateData.scrollContainer.width / props.numColumns),
      .y = scrollContentAboveOffset.get(componentRegistryItem.index % props.numColumns) + scrollContentBelowOffset.get(componentRegistryItem.index % props.numColumns)
    }, componentItem);

    scrollContentBelowOffset.add(componentRegistryItem.index % props.numColumns, componentRegistryItem.size);
    scrollContentBelowIndex = componentRegistryItem.index;

    int scrollPosition = nextStateData.scrollPositionUpdated ? (
      scrollContentAboveOffset.get(componentRegistryItem.index % props.numColumns) + getRelativePointFromPoint(nextStateData.scrollPosition) - measurementsManager->getTemplate(getTag(), "ListHeaderComponentUniqueId")
    ) : getRelativePointFromPoint(nextStateData.scrollPosition);

    if (getRelativePointFromPoint(elementMetrics.frame.origin) <= (scrollPosition + getRelativeSizeFromSize(nextStateData.scrollContainer) + viewportOffset) &&
      (getRelativePointFromPoint(elementMetrics.frame.origin) + getRelativeSizeFromSize(elementMetrics.frame.size)) >= (scrollPosition - viewportOffset)) {
      
      if (true) {
        contentShadowNodeChildren->push_back(componentItem);
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
    auto componentUniqueId = "ListEmptyComponentUniqueId";
    auto templateRegistryItem = transformTemplateComponent(componentUniqueId, 0);
    auto componentItem = registryManager->getComponent(getTag(), componentUniqueId);
    contentShadowNodeChildren->push_back(componentItem);

    adjustElement({
      .x = 0,
      .y = scrollContentAboveOffset.max() + scrollContentBelowOffset.max()
    }, componentItem);

    scrollContentBelowOffset.add(0, templateRegistryItem.size);
  }

  /*
   * Render and adjust origin of Footer template
   */
  if (props.inverted) {
    auto componentUniqueId = "ListHeaderComponentUniqueId";
    transformTemplateComponent(componentUniqueId, 1);
    auto componentItem = registryManager->getComponent(getTag(), componentUniqueId);
    contentShadowNodeChildren->push_back(componentItem);

    adjustElement({
      .x = 0,
      .y = scrollContentAboveOffset.max() + scrollContentBelowOffset.max()
    }, componentItem);
  } else {
    auto componentUniqueId = "ListFooterComponentUniqueId";
    transformTemplateComponent(componentUniqueId, 1);
    auto componentItem = registryManager->getComponent(getTag(), componentUniqueId);
    contentShadowNodeChildren->push_back(componentItem);

    adjustElement({
      .x = 0,
      .y = scrollContentAboveOffset.max() + scrollContentBelowOffset.max()
    }, componentItem);
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
    measurementsManager->getTemplate(getTag(), "ListFooterComponentUniqueId")
  );
  if (props.horizontal) {
    auto nextScrollContentWidth = std::max((Float) scrollContentOffset, nextStateData.scrollContent.width);
    nextStateData.scrollContentUpdated = nextScrollContentWidth != nextStateData.scrollContent.width;
    nextStateData.scrollContent.height = nextStateData.scrollContainer.height;
    nextStateData.scrollContent.width = nextScrollContentWidth;
  } else if (!props.horizontal) {
    auto nextScrollContentHeight = std::max((Float) scrollContentOffset, nextStateData.scrollContent.height);
    nextStateData.scrollContentUpdated = nextScrollContentHeight != nextStateData.scrollContent.height;
    nextStateData.scrollContent.width = nextStateData.scrollContainer.width;
    nextStateData.scrollContent.height = nextScrollContentHeight;
  }

  /*
   * Update the scroll position when new items are prepended to the top of the list
   */
  if (!nextStateData.scrollIndexUpdated && nextStateData.scrollPositionUpdated && props.horizontal) {
    nextStateData.scrollPosition.y = 0;
    nextStateData.scrollPosition.x = (
      scrollContentAboveOffset.max() + getRelativePointFromPoint(nextStateData.scrollPosition) - measurementsManager->getTemplate(getTag(), "ListHeaderComponentUniqueId")
    );
  } else if (!nextStateData.scrollIndexUpdated && nextStateData.scrollPositionUpdated && !props.horizontal) {
    nextStateData.scrollPosition.x = 0;
    nextStateData.scrollPosition.y = (
      scrollContentAboveOffset.max() + getRelativePointFromPoint(nextStateData.scrollPosition) - measurementsManager->getTemplate(getTag(), "ListHeaderComponentUniqueId")
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
    auto componentUniqueId = "ListDynamicComponentUniqueId";
    transformTemplateComponent(componentUniqueId, 1);
    auto componentItem = registryManager->getComponent(getTag(), componentUniqueId);
    contentShadowNodeChildren->push_back(componentItem);

    adjustElement({
      .x = 0,
      .y = 0
    }, componentItem);

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
    auto uniqueId = static_cast<const SLElementProps&>(*child->getProps()).uniqueId;
    registryManager->appendTemplate(getTag(), uniqueId, child);
  } else {
    ConcreteShadowNode::appendChild(child);
  }
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
