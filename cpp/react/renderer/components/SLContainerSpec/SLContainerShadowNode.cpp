#include "SLContainerShadowNode.h"
#include "SLTemplate.h"

namespace facebook::react {

extern const char SLContainerComponentName[] = "SLContainer";

std::vector<ShadowNode::Shared> elementShadowNodeTemplateRegistry{};
std::vector<std::string> elementShadowNodeOrderedIndices{};
std::unordered_map<std::string, ShadowNode::Unshared> elementShadowNodeComponentRegistry{};
SLFenwickTree elementShadowNodeMeasurements{};

void SLContainerShadowNode::layout(LayoutContext layoutContext) {
  float CONTAINER_OFFSET = 1000;

  auto &props = getConcreteProps();
  auto nextStateData = getStateData();

  /*
   * Calculate the difference in the data list (append or prepend) by comparing it to the previous state
   */
  bool elementDataPrepended = false;
  int elementDataPrependedSize = 0;
  bool elementDataAppended = false;
  int elementDataAppendedSize = 0;
  
  if (props.data.size() > 0 && elementShadowNodeOrderedIndices.size() > 0) {
    elementDataPrepended = elementShadowNodeOrderedIndices.front() != props.getElementValueByPath(props.data.front(), "id");
    elementDataAppended = elementShadowNodeOrderedIndices.back() != props.getElementValueByPath(props.data.back(), "id");
  }

  // Finding the index of the first prepended element in the data list
  if (elementDataPrepended) {
    for (int elementDataIndex = 0; elementDataIndex < props.data.size(); ++elementDataIndex) {
      const nlohmann::json& elementData = props.getElementByIndex(elementDataIndex);
      auto elementDataUniqueKey = props.getElementValueByPath(elementData, "id");
      
      if (elementDataUniqueKey == elementShadowNodeOrderedIndices.front()) {
        elementDataPrependedSize = elementDataIndex;
        break;
      }
    }
  }

  // Finding the index of the last appended element in the data list
  if (elementDataAppended) {
    for (int elementDataIndex = props.data.size() - 1; elementDataIndex >= 0; --elementDataIndex) {
      const nlohmann::json& elementData = props.getElementByIndex(elementDataIndex);
      auto elementDataUniqueKey = props.getElementValueByPath(elementData, "id");
      
      if (elementDataUniqueKey == elementShadowNodeOrderedIndices.back()) {
        elementDataAppendedSize = elementDataIndex;
        break;
      }
    }
  }

  elementShadowNodeMeasurements.resize(props.data.size());
  elementShadowNodeOrderedIndices = {};

  /*
   * Create an empty list that will hold the visible container items
   */
  auto containerShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>(*ShadowNode::emptySharedShadowNodeSharedList());

  /*
   * Map data elements to the child elements of the container
   */
  for (int elementDataIndex = 0; elementDataIndex < props.data.size(); ++elementDataIndex) {
    const nlohmann::json& elementData = props.getElementByIndex(elementDataIndex);
    auto elementDataUniqueKey = props.getElementValueByPath(elementData, "id");

    elementShadowNodeOrderedIndices.push_back(elementDataUniqueKey);

    /*
     * Check if this element already exists or needs to be created
     * Create new element if it doesn't exist, otherwise use cached one
     */
    auto it = elementShadowNodeComponentRegistry.find(elementDataUniqueKey);
    ShadowNode::Unshared elementShadowNodeCloned;

    if (it == elementShadowNodeComponentRegistry.end()) {
      elementShadowNodeCloned = SLTemplate::cloneShadowNodeTree(elementData, elementShadowNodeTemplateRegistry[1]);
      elementShadowNodeComponentRegistry[elementDataUniqueKey] = elementShadowNodeCloned;
    } else {
      elementShadowNodeCloned = it->second;
    }

    /*
     * Calculate position and visibility boundaries
     */
    auto elementShadowNodeMeasurementsEndOffset = elementShadowNodeMeasurements.sum(elementDataIndex);
    auto visibleStartOffset = nextStateData.scrollPosition.y - CONTAINER_OFFSET;
    auto visibleEndOffset = getLayoutMetrics().frame.size.height + nextStateData.scrollPosition.y + CONTAINER_OFFSET;

    if (elementShadowNodeMeasurementsEndOffset < visibleEndOffset || elementDataPrependedSize > elementDataIndex) {
      LayoutConstraints layoutConstraints;
      layoutConstraints.layoutDirection = facebook::react::LayoutDirection::LeftToRight;
      layoutConstraints.maximumSize.width = getLayoutMetrics().frame.size.width;

      /*
       * Update element layout if not already fixed, position the element and cache it's metrics
       */
      auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(elementShadowNodeCloned);
      if (!elementShadowNodeLayoutable->getSealed()) {
        elementShadowNodeLayoutable->layoutTree(layoutContext, layoutConstraints);

        LayoutMetrics layoutMetrics = elementShadowNodeLayoutable->getLayoutMetrics();
        layoutMetrics.frame.origin.y = elementShadowNodeMeasurementsEndOffset;
        elementShadowNodeLayoutable->setLayoutMetrics(layoutMetrics);

        elementShadowNodeMeasurements[elementDataIndex] = layoutMetrics.frame.size.height;
      }
    }

    /*
     * Add element to container if it's measured and within visible area
     */
    const auto elementShadowNodeHasBeenMeasured = elementShadowNodeMeasurements[elementDataIndex] > 0;
    if (elementShadowNodeHasBeenMeasured && elementShadowNodeMeasurementsEndOffset >= visibleStartOffset && elementShadowNodeMeasurementsEndOffset <= visibleEndOffset) {
      containerShadowNodeChildren->push_back(elementShadowNodeCloned);
    }
  }

  this->children_ = containerShadowNodeChildren;

  nextStateData.scrollContainer = getLayoutMetrics().frame.size;
  nextStateData.scrollContent = {
    .width = getLayoutMetrics().frame.size.width,
    .height = elementShadowNodeMeasurements.sum(elementShadowNodeMeasurements.size()) };
  nextStateData.childrenMeasurementsTree = elementShadowNodeMeasurements;
  setStateData(std::move(nextStateData));

  /*
   * Dispatches events if the scroll position is near the start or end of a container.
   */
  if (nextStateData.scrollPosition.y < CONTAINER_OFFSET && elementDataPrependedSize == 0) {
    auto distanceFromStart = nextStateData.scrollPosition.y;
    getEventEmitter()->dispatchEvent("startReached", [distanceFromStart](jsi::Runtime &runtime) {
      auto $payload = jsi::Object(runtime);
      $payload.setProperty(runtime, "distanceFromStart", distanceFromStart);
      return $payload;
    });
  }

  if (nextStateData.scrollContent.height - nextStateData.scrollPosition.y < CONTAINER_OFFSET && elementDataAppendedSize == 0) {
    auto distanceFromEnd = nextStateData.scrollContent.height - nextStateData.scrollPosition.y;
    getEventEmitter()->dispatchEvent("endReached", [distanceFromEnd](jsi::Runtime &runtime) {
      auto $payload = jsi::Object(runtime);
      $payload.setProperty(runtime, "distanceFromEnd", distanceFromEnd);
      return $payload;
    });
  }
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
