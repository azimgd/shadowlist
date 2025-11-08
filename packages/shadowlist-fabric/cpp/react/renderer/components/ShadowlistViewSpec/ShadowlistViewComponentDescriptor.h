#pragma once

#include "ShadowlistViewShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>
#include <unordered_map>

namespace facebook::react {

struct ShadowlistCoreSharedInstance {
  std::shared_ptr<azimgd::shadowlist::Container> containerManager;
  std::shared_ptr<azimgd::shadowlist::Virtualizer> virtualizerManager;
  std::shared_ptr<ShadowlistViewShadowNode::ContainerSizeUpdateState> containerSizeUpdateState;
  std::shared_ptr<size_t> prependElementsSize;
  std::shared_ptr<double> prependElementsOffset;
  std::shared_ptr<double> prependedElementsOffset;
  std::shared_ptr<double> measuredElementsSize;
  std::string elementsHeadKey;
  std::string elementsTailKey;
  int prevVisibleStartIndex = -1;
  int prevVisibleEndIndex = -1;
};

/*
 * Descriptor for <ShadowlistView> component.
 */
class ShadowlistViewComponentDescriptor final : public ConcreteComponentDescriptor<ShadowlistViewShadowNode> {
  public:
  ShadowlistViewComponentDescriptor(const ComponentDescriptorParameters& parameters) :
    ConcreteComponentDescriptor<ShadowlistViewShadowNode>(parameters) {
  };

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);

    auto& shadowlistViewShadowNode = static_cast<ShadowlistViewShadowNode&>(shadowNode);
    auto tag = shadowNode.getTag();

    /*
     * Get or create per-instance state based on ShadowNode tag
     */
    if (shadowlistCoreSharedInstances_.find(tag) == shadowlistCoreSharedInstances_.end()) {
      ShadowlistCoreSharedInstance shadowlistCoreSharedInstance;
      shadowlistCoreSharedInstance.containerManager = std::make_shared<azimgd::shadowlist::Container>();
      shadowlistCoreSharedInstance.virtualizerManager = std::make_shared<azimgd::shadowlist::Virtualizer>();
      shadowlistCoreSharedInstance.containerSizeUpdateState = std::make_shared<ShadowlistViewShadowNode::ContainerSizeUpdateState>(ShadowlistViewShadowNode::ContainerSizeUpdateState::INITIALIZED);
      shadowlistCoreSharedInstance.prependElementsSize = std::make_shared<size_t>(0);
      shadowlistCoreSharedInstance.prependElementsOffset = std::make_shared<double>(0.0);
      shadowlistCoreSharedInstance.prependedElementsOffset = std::make_shared<double>(0.0);
      shadowlistCoreSharedInstance.measuredElementsSize = std::make_shared<double>(0.0);
      shadowlistCoreSharedInstances_[tag] = shadowlistCoreSharedInstance;
    }

    auto& shadowlistCoreSharedInstance = shadowlistCoreSharedInstances_[tag];

    shadowlistViewShadowNode.setContainerManager(shadowlistCoreSharedInstance.containerManager);
    shadowlistViewShadowNode.setVirtualizerManager(shadowlistCoreSharedInstance.virtualizerManager);
    shadowlistViewShadowNode.setContainerSizeUpdateState(shadowlistCoreSharedInstance.containerSizeUpdateState);
    shadowlistViewShadowNode.setPrependElementsSize(shadowlistCoreSharedInstance.prependElementsSize);
    shadowlistViewShadowNode.setPrependElementsOffset(shadowlistCoreSharedInstance.prependElementsOffset);
    shadowlistViewShadowNode.setPrependedElementsOffset(shadowlistCoreSharedInstance.prependedElementsOffset);
    shadowlistViewShadowNode.setMeasuredElementsSize(shadowlistCoreSharedInstance.measuredElementsSize);

    auto& shadowlistViewProps = static_cast<const ShadowlistViewShadowNode::ConcreteProps&>(*shadowNode.getProps());
    auto& shadowlistViewState = static_cast<const ShadowlistViewShadowNode::ConcreteState&>(*shadowNode.getState());
    auto& shadowlistViewEventEmitter = static_cast<const ShadowlistViewShadowNode::ConcreteEventEmitter&>(*shadowNode.getEventEmitter());

    auto shadowlistViewStateData = shadowlistViewState.getData();
    auto shadowlistViewLayoutMetrics = static_cast<YogaLayoutableShadowNode&>(shadowNode).getLayoutMetrics();

    if (shadowlistCoreSharedInstance.containerManager->onStartReachedCallback == nullptr) {
      shadowlistCoreSharedInstance.containerManager->onStartReachedCallback = [shadowlistViewEventEmitter]() -> void {
        shadowlistViewEventEmitter.onStartReached({});
      };
    }

    if (shadowlistCoreSharedInstance.containerManager->onEndReachedCallback == nullptr) {
      shadowlistCoreSharedInstance.containerManager->onEndReachedCallback = [shadowlistViewEventEmitter]() -> void {
        shadowlistViewEventEmitter.onEndReached({});
      };
    }

    if (shadowlistCoreSharedInstance.containerManager->nextRevision.elements.size() != shadowlistViewProps.elementsAllKeys.size()) {
      // @TODO: increment only for now, implement decrement as well
      auto prependElementsSize = shadowlistViewProps.elementsAllKeys.size() - shadowlistCoreSharedInstance.containerManager->nextRevision.elements.size();

      if (shadowlistCoreSharedInstance.elementsHeadKey.length() > 0 && shadowlistCoreSharedInstance.elementsHeadKey != shadowlistViewProps.elementsHeadKey) {
        shadowlistCoreSharedInstance.virtualizerManager->prependElements(shadowlistCoreSharedInstance.containerManager.get(), prependElementsSize);
        *shadowlistCoreSharedInstance.prependElementsSize += prependElementsSize;
      } else if (shadowlistCoreSharedInstance.elementsTailKey.length() > 0  && shadowlistCoreSharedInstance.elementsTailKey != shadowlistViewProps.elementsTailKey) {
        shadowlistCoreSharedInstance.virtualizerManager->appendElements(shadowlistCoreSharedInstance.containerManager.get(), prependElementsSize);
      } else {
        shadowlistCoreSharedInstance.containerManager->resizeElementsTail(shadowlistViewProps.elementsAllKeys.size());
      }

      shadowlistCoreSharedInstance.elementsHeadKey = shadowlistViewProps.elementsHeadKey;
      shadowlistCoreSharedInstance.elementsTailKey = shadowlistViewProps.elementsTailKey;
    }

    shadowlistCoreSharedInstance.containerManager->inverted = shadowlistViewProps.inverted;
    shadowlistCoreSharedInstance.containerManager->horizontal = shadowlistViewProps.horizontal;

    shadowlistCoreSharedInstance.containerManager->startRevision();

    shadowlistCoreSharedInstance.containerManager->setContainerOffsetX(shadowlistViewStateData.containerOffsetX_);
    shadowlistCoreSharedInstance.containerManager->setContainerOffsetY(shadowlistViewStateData.containerOffsetY_);
    shadowlistCoreSharedInstance.containerManager->setWindowContainerWidth(shadowlistViewLayoutMetrics.frame.size.width);
    shadowlistCoreSharedInstance.containerManager->setWindowContainerHeight(shadowlistViewLayoutMetrics.frame.size.height);

    shadowlistCoreSharedInstance.virtualizerManager->measure(shadowlistCoreSharedInstance.containerManager.get());
    shadowlistCoreSharedInstance.containerManager->endRevision();

    auto nextVisibleIndices = shadowlistCoreSharedInstance.containerManager->getVisibleIndices();
    int nextVisibleStartIndex = static_cast<int>(nextVisibleIndices.first);
    int nextVisibleEndIndex = static_cast<int>(nextVisibleIndices.second);

    if (shadowlistCoreSharedInstance.prevVisibleStartIndex != nextVisibleStartIndex || shadowlistCoreSharedInstance.prevVisibleEndIndex != nextVisibleEndIndex) {
      shadowlistViewEventEmitter.onVisibleIndicesChange({
        .visibleStartIndex = nextVisibleStartIndex,
        .visibleEndIndex = nextVisibleEndIndex,
      });

      shadowlistCoreSharedInstance.prevVisibleStartIndex = nextVisibleStartIndex;
      shadowlistCoreSharedInstance.prevVisibleEndIndex = nextVisibleEndIndex;
    }
  };

  private:
  mutable std::unordered_map<Tag, ShadowlistCoreSharedInstance> shadowlistCoreSharedInstances_;
};

void ShadowlistViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
