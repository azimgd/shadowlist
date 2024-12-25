#include "SLContainerShadowNode.h"
#include "SLTemplate.h"
#include <iostream>

namespace facebook::react {

extern const char SLContainerComponentName[] = "SLContainer";

int scrolledBefore = 0;
bool dispatchedBefore = false;

std::vector<ShadowNode::Shared> elementShadowNodeTemplateRegistry{};
std::unordered_map<std::string, ShadowNode::Unshared> elementShadowNodeComponentRegistry{};
SLFenwickTree elementShadowNodeMeasurements{};
std::string elementShadowNodeOrderedIndicesFront;

int initialScrollIndex = 20;
int initialScrollAdjusted = false;
bool onStartReachedEventDispatched = false;

struct VirtualizedRegistry {
  int index;
  Float height;
  std::string elementDataUniqueKey;
};

void SLContainerShadowNode::layout(LayoutContext layoutContext) {
  auto &props = getConcreteProps();
  auto nextStateData = getStateData();

  auto containerShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>(*ShadowNode::emptySharedShadowNodeSharedList());

  /*
   *
   */
  if (elementShadowNodeMeasurements.size() && props.data.size() != elementShadowNodeMeasurements.size()) {
    for (int elementDataIndex = 0; elementDataIndex < props.data.size(); ++elementDataIndex) {
      const nlohmann::json& elementData = props.getElementByIndex(elementDataIndex);
      auto elementDataUniqueKey = props.getElementValueByPath(elementData, "id");
      
      if (elementDataUniqueKey == elementShadowNodeOrderedIndicesFront && !elementDataIndex) {
        break;
      }
      
      if (elementDataUniqueKey == elementShadowNodeOrderedIndicesFront) {
        initialScrollIndex = elementDataIndex + 10;
        onStartReachedEventDispatched = false;
      }
    }
  }

  const nlohmann::json& elementData = props.getElementByIndex(0);
  auto elementDataUniqueKey = props.getElementValueByPath(elementData, "id");
  elementShadowNodeOrderedIndicesFront = elementDataUniqueKey;
  
  /*
   *
   */
  const int size = props.data.size();
  
  if (elementShadowNodeMeasurements.size() && size > elementShadowNodeMeasurements.size()) {
    SLFenwickTree elementShadowNodeMeasurementsNext{};
    
    for (size_t i = 0; i < size - elementShadowNodeMeasurements.size(); ++i) {
      elementShadowNodeMeasurementsNext.push_back(0.0f);
    }
    for (float measurement : elementShadowNodeMeasurements) {
      elementShadowNodeMeasurementsNext.push_back(measurement);
    }

    elementShadowNodeMeasurements = elementShadowNodeMeasurementsNext;
  } else {
    elementShadowNodeMeasurements.resize(size);
  }

  if (!nextStateData.scrollPosition.y && initialScrollAdjusted) {
    initialScrollIndex = std::max(initialScrollIndex - 10, 0);
    initialScrollAdjusted = false;
    
    if (!initialScrollIndex && !onStartReachedEventDispatched) {
      auto distanceFromStart = nextStateData.scrollPosition.y;
      getEventEmitter()->dispatchEvent("startReached", [distanceFromStart](jsi::Runtime &runtime) {
        auto $payload = jsi::Object(runtime);
        $payload.setProperty(runtime, "distanceFromStart", distanceFromStart);
        return $payload;
      });
      onStartReachedEventDispatched = true;
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
  auto transform = [&](int elementDataIndex) -> VirtualizedRegistry {
    const nlohmann::json& elementData = props.getElementByIndex(elementDataIndex);
    auto elementDataUniqueKey = props.getElementValueByPath(elementData, "id");

    auto elementShadowNodeComponentRegistryIt = elementShadowNodeComponentRegistry.find(elementDataUniqueKey);
    if (elementShadowNodeComponentRegistryIt == elementShadowNodeComponentRegistry.end()) {
      elementShadowNodeComponentRegistry[elementDataUniqueKey] = SLTemplate::cloneShadowNodeTree(elementData, elementShadowNodeTemplateRegistry[1]);
    } else {
      elementShadowNodeComponentRegistry[elementDataUniqueKey] = elementShadowNodeComponentRegistry[elementDataUniqueKey]->clone({});
    }

    auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(elementShadowNodeComponentRegistry[elementDataUniqueKey]);

    if (elementShadowNodeLayoutable->getLayoutMetrics().frame.size.height == 0) {
      elementShadowNodeLayoutable->layoutTree(layoutContext, {});
    }

    elementShadowNodeMeasurements[elementDataIndex] = elementShadowNodeLayoutable->getLayoutMetrics().frame.size.height;

    return VirtualizedRegistry{
      elementDataIndex,
      elementShadowNodeLayoutable->getLayoutMetrics().frame.size.height,
      elementDataUniqueKey
    };
  };

  int scrollContentAboveIndex = 0;
  int scrollContentAboveOffset = 0;
  auto scrollContentAboveComponents = std::views::iota(0, initialScrollIndex)
    | std::views::reverse
    | std::views::take(10)
    | std::views::reverse
    | std::views::transform(transform);

  int scrollContentBelowIndex = 0;
  int scrollContentBelowOffset = 0;
  auto scrollContentBelowComponents = std::views::iota(initialScrollIndex, size)
    | std::views::take_while([&](int i) {
      int scrollContentNextOffset = nextStateData.scrollPosition.y + 1000;
      return scrollContentBelowOffset < scrollContentNextOffset;
    })
    | std::views::transform(transform);

  for (VirtualizedRegistry n : scrollContentAboveComponents) {
    auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(elementShadowNodeComponentRegistry[n.elementDataUniqueKey]);
    LayoutMetrics layoutMetrics = elementShadowNodeLayoutable->getLayoutMetrics();
    layoutMetrics.frame.origin.y = scrollContentAboveOffset + scrollContentBelowOffset;
    elementShadowNodeLayoutable->setLayoutMetrics(layoutMetrics);

    scrollContentAboveOffset += n.height;
    scrollContentAboveIndex = n.index;
    
    if (layoutMetrics.frame.origin.y <= nextStateData.scrollPosition.y + nextStateData.scrollContainer.height && (layoutMetrics.frame.origin.y + layoutMetrics.frame.size.height) >= nextStateData.scrollPosition.y) {
      containerShadowNodeChildren->push_back(elementShadowNodeComponentRegistry[n.elementDataUniqueKey]);
    }
  }

  for (VirtualizedRegistry n : scrollContentBelowComponents) {
    auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(elementShadowNodeComponentRegistry[n.elementDataUniqueKey]);
    LayoutMetrics layoutMetrics = elementShadowNodeLayoutable->getLayoutMetrics();
    layoutMetrics.frame.origin.y = scrollContentAboveOffset + scrollContentBelowOffset;
    elementShadowNodeLayoutable->setLayoutMetrics(layoutMetrics);

    scrollContentBelowOffset += n.height;
    scrollContentBelowIndex = n.index;
    
    if (layoutMetrics.frame.origin.y <= nextStateData.scrollPosition.y + nextStateData.scrollContainer.height && (layoutMetrics.frame.origin.y + layoutMetrics.frame.size.height) >= nextStateData.scrollPosition.y) {
      containerShadowNodeChildren->push_back(elementShadowNodeComponentRegistry[n.elementDataUniqueKey]);
    }
  }

  this->children_ = containerShadowNodeChildren;

  nextStateData.scrollContainer = getLayoutMetrics().frame.size;
  nextStateData.scrollContentUpdated = true;
  nextStateData.scrollContent.width = getLayoutMetrics().frame.size.width;
  nextStateData.scrollContent.height = elementShadowNodeMeasurements.sum(size);

  if (!initialScrollAdjusted) {
    nextStateData.scrollPositionUpdated = true;
    nextStateData.scrollPosition.x = 0;
    nextStateData.scrollPosition.y = scrollContentAboveOffset;
    initialScrollAdjusted = true;
  }

  setStateData(std::move(nextStateData));
}

void SLContainerShadowNode::appendChild(const ShadowNode::Shared& child) {
  elementShadowNodeTemplateRegistry.push_back(child);
}

void SLContainerShadowNode::replaceChild(
  const ShadowNode& oldChild,
  const ShadowNode::Shared& newChild,
  size_t suggestedIndex) {
  ConcreteShadowNode::replaceChild(oldChild, newChild, suggestedIndex);
}

}
