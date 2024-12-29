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
  auto &props = getConcreteProps();
  auto nextStateData = getStateData();

  const auto viewportOffset = 1000;
  nextStateData.scrollPositionUpdated = false;

  auto containerShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>(*ShadowNode::emptySharedShadowNodeSharedList());
  const int elementsDataSize = props.data.size();
  const bool elementsDataPrepended = elementsDataSize && nextStateData.firstChildUniqueId.size() &&
    nextStateData.firstChildUniqueId != props.getElementValueByPath(props.data.front(), "id");
  const bool elementsDataAppended = elementsDataSize && nextStateData.lastChildUniqueId.size() &&
    nextStateData.lastChildUniqueId != props.getElementValueByPath(props.data.back(), "id");

  /*
   *
   */
  if (nextStateData.childrenMeasurementsTree.size() && props.data.size() != nextStateData.childrenMeasurementsTree.size()) {
    for (int elementDataIndex = 0; elementDataIndex < props.data.size(); ++elementDataIndex) {
      const nlohmann::json& elementData = props.getElementByIndex(elementDataIndex);
      auto elementDataUniqueKey = props.getElementValueByPath(elementData, "id");
      
      if (elementDataUniqueKey == nextStateData.firstChildUniqueId && !elementDataIndex) {
        break;
      }
      
      if (elementDataUniqueKey == nextStateData.firstChildUniqueId) {
        nextStateData.initialScrollIndex = elementDataIndex + 10;
      }
    }
  }
  
  /*
   *
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

  if (nextStateData.scrollPosition.y < nextStateData.scrollContainer.height) {
    if (std::max(nextStateData.initialScrollIndex - 10, 0) != nextStateData.initialScrollIndex) {
      nextStateData.initialScrollIndex = std::max(nextStateData.initialScrollIndex - 10, 0);
      nextStateData.scrollPositionUpdated = true;
    }

    if (!nextStateData.initialScrollIndex) {
      auto distanceFromStart = nextStateData.scrollPosition.y;
      getEventEmitter()->dispatchEvent("startReached", [distanceFromStart](jsi::Runtime &runtime) {
        auto $payload = jsi::Object(runtime);
        $payload.setProperty(runtime, "distanceFromStart", distanceFromStart);
        return $payload;
      });
    }
  }
  
  if (nextStateData.scrollContainer.height == getLayoutMetrics().frame.size.height && nextStateData.scrollPosition.y >= nextStateData.scrollContent.height - nextStateData.scrollContainer.height - 1) {
    auto distanceFromEnd = nextStateData.scrollContainer.height - getLayoutMetrics().frame.size.height;
    getEventEmitter()->dispatchEvent("endReached", [distanceFromEnd](jsi::Runtime &runtime) {
      auto $payload = jsi::Object(runtime);
      $payload.setProperty(runtime, "distanceFromEnd", distanceFromEnd);
      return $payload;
    });
  }

  /*
   * Transformer
   */
  auto transform = [&](int elementDataIndex) -> ComponentRegistryItem {
    const nlohmann::json& elementData = props.getElementByIndex(elementDataIndex);
    auto elementDataUniqueKey = props.getElementValueByPath(elementData, "id");

    auto elementShadowNodeComponentRegistryIt = elementShadowNodeComponentRegistry.find(elementDataUniqueKey);
    if (elementShadowNodeComponentRegistryIt == elementShadowNodeComponentRegistry.end()) {
      elementShadowNodeComponentRegistry[elementDataUniqueKey] = SLTemplate::cloneShadowNodeTree(elementData, elementShadowNodeTemplateRegistry["ListChildrenComponentUniqueId"].back());
    } else {
      elementShadowNodeComponentRegistry[elementDataUniqueKey] = elementShadowNodeComponentRegistry[elementDataUniqueKey]->clone({});
    }

    auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(elementShadowNodeComponentRegistry[elementDataUniqueKey]);

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

  int scrollContentAboveIndex = 0;
  float scrollContentAboveOffset = 0;
  auto scrollContentAboveComponents = std::views::iota(0, nextStateData.initialScrollIndex)
    | std::views::reverse
    | std::views::take(10)
    | std::views::reverse
    | std::views::transform(transform);

  int scrollContentBelowIndex = 0;
  float scrollContentBelowOffset = 0;
  auto scrollContentBelowComponents = std::views::iota(nextStateData.initialScrollIndex, elementsDataSize)
    | std::views::take_while([&](int i) {
      float scrollContentNextOffset = nextStateData.scrollPosition.y + viewportOffset;
      return scrollContentBelowOffset < scrollContentNextOffset;
    })
    | std::views::transform(transform);

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

  this->children_ = containerShadowNodeChildren;
  yogaNode_.setDirty(true);

  nextStateData.firstChildUniqueId = props.getElementValueByPath(props.data.front(), "id");
  nextStateData.lastChildUniqueId = props.getElementValueByPath(props.data.back(), "id");
  nextStateData.scrollContainer = getLayoutMetrics().frame.size;
  nextStateData.scrollContentUpdated = true;
  nextStateData.scrollContent.width = getLayoutMetrics().frame.size.width;
  nextStateData.scrollContent.height = nextStateData.childrenMeasurementsTree.sum(elementsDataSize);

  if (nextStateData.scrollPositionUpdated) {
    nextStateData.scrollPosition.x = 0;
    nextStateData.scrollPosition.y = scrollContentAboveOffset + nextStateData.scrollPosition.y;
  }

  setStateData(std::move(nextStateData));
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
