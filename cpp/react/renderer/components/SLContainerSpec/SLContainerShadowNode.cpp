#include "SLContainerShadowNode.h"
#include "SLTemplate.h"

namespace facebook::react {

extern const char SLContainerComponentName[] = "SLContainer";

std::vector<ShadowNode::Shared> elementShadowNodeTemplateRegistry{};
std::vector<std::string> elementShadowNodeOrderedIndices{};
std::unordered_map<std::string, ShadowNode::Unshared> elementShadowNodeComponentRegistry{};
SLFenwickTree elementShadowNodeMeasurements{};

void SLContainerShadowNode::layout(LayoutContext layoutContext) {
  int CONTAINER_ELEMENTS_SIZE = 1000;
  float CONTAINER_OFFSET = 1000;

  auto &props = getConcreteProps();
  auto nextStateData = getStateData();

  elementShadowNodeMeasurements.resize(CONTAINER_ELEMENTS_SIZE);
  elementShadowNodeOrderedIndices = {};

  /*
   * Create an empty list that will hold the visible container items
   */
  auto containerShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>(*ShadowNode::emptySharedShadowNodeSharedList());

  nlohmann::json elementData = { {"id", std::to_string(1)} };

  /*
   * Map data elements to the child elements of the container
   */
  for (int elementDataIndex = 0; elementDataIndex < CONTAINER_ELEMENTS_SIZE; ++elementDataIndex) {
    elementShadowNodeOrderedIndices.push_back(std::to_string(elementDataIndex));

    /*
     * Check if this element already exists or needs to be created
     * Create new element if it doesn't exist, otherwise use cached one
     */
    auto it = elementShadowNodeComponentRegistry.find(std::to_string(elementDataIndex));
    ShadowNode::Unshared elementShadowNodeCloned;

    if (it == elementShadowNodeComponentRegistry.end()) {
      elementShadowNodeCloned = SLTemplate::cloneShadowNodeTree(&elementData, elementShadowNodeTemplateRegistry[1]);
      elementShadowNodeComponentRegistry[std::to_string(elementDataIndex)] = elementShadowNodeCloned;
    } else {
      elementShadowNodeCloned = it->second;
    }

    /*
     * Calculate position and visibility boundaries
     */
    auto elementShadowNodeMeasurementsEndOffset = elementShadowNodeMeasurements.sum(elementDataIndex);
    auto visibleStartOffset = nextStateData.scrollPosition.y - CONTAINER_OFFSET;
    auto visibleEndOffset = getLayoutMetrics().frame.size.height + nextStateData.scrollPosition.y + CONTAINER_OFFSET;
    
    if (elementShadowNodeMeasurementsEndOffset < visibleEndOffset) {
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
  nextStateData.scrollContent = { .width = getLayoutMetrics().frame.size.width, .height = elementShadowNodeMeasurements.sum(elementShadowNodeMeasurements.size()) };
  nextStateData.childrenMeasurementsTree = elementShadowNodeMeasurements;
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
