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

  /*
   * Header/footer sizes measured by the ShadowNode during layout, fed into the
   * next frame's Virtualizer::update so the core can position elements
   */
  std::shared_ptr<double> headerSize;
  std::shared_ptr<double> footerSize;

  /*
   * Last seen scrollToIndex prop value, so the command fires once per change
   */
  int prevContainerOffsetIndex = -1;
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
      shadowlistCoreSharedInstance.headerSize = std::make_shared<double>(0.0);
      shadowlistCoreSharedInstance.footerSize = std::make_shared<double>(0.0);
      shadowlistCoreSharedInstances_[tag] = shadowlistCoreSharedInstance;
    }

    auto& shadowlistCoreSharedInstance = shadowlistCoreSharedInstances_[tag];

    shadowlistViewShadowNode.setContainerManager(shadowlistCoreSharedInstance.containerManager);
    shadowlistViewShadowNode.setVirtualizerManager(shadowlistCoreSharedInstance.virtualizerManager);
    shadowlistViewShadowNode.setHeaderSize(shadowlistCoreSharedInstance.headerSize);
    shadowlistViewShadowNode.setFooterSize(shadowlistCoreSharedInstance.footerSize);

    auto& shadowlistViewProps = static_cast<const ShadowlistViewShadowNode::ConcreteProps&>(*shadowNode.getProps());
    auto& shadowlistViewState = static_cast<const ShadowlistViewShadowNode::ConcreteState&>(*shadowNode.getState());
    auto& shadowlistViewEventEmitter = static_cast<const ShadowlistViewShadowNode::ConcreteEventEmitter&>(*shadowNode.getEventEmitter());

    auto shadowlistViewStateData = shadowlistViewState.getData();
    auto shadowlistViewLayoutMetrics = static_cast<YogaLayoutableShadowNode&>(shadowNode).getLayoutMetrics();

    auto containerManager = shadowlistCoreSharedInstance.containerManager.get();

    /*
     * Forward core events to the event emitter. The core deduplicates so we
     * only need to translate the payloads here.
     */
    containerManager->onStartReachedCallback = [shadowlistViewEventEmitter]() -> void {
      shadowlistViewEventEmitter.onStartReached({});
    };
    containerManager->onEndReachedCallback = [shadowlistViewEventEmitter]() -> void {
      shadowlistViewEventEmitter.onEndReached({});
    };
    containerManager->onVisibleIndicesChangeCallback = [shadowlistViewEventEmitter](std::size_t startIndex, std::size_t endIndex) -> void {
      shadowlistViewEventEmitter.onVisibleIndicesChange({
        .visibleStartIndex = static_cast<int>(startIndex),
        .visibleEndIndex = static_cast<int>(endIndex),
      });
    };
    containerManager->onScrollCallback = [shadowlistViewEventEmitter](double containerOffsetX, double containerOffsetY) -> void {
      shadowlistViewEventEmitter.onScroll({
        .contentOffsetX = containerOffsetX,
        .contentOffsetY = containerOffsetY,
      });
    };

    /*
     * Resolve a pending scrollToIndex. The imperative command writes the target
     * into state (containerOffsetIndex_); the declarative prop provides an initial
     * index. Fire the core command once whenever the resolved target changes.
     */
    int scrollToIndexTarget = -1;
    if (shadowlistViewStateData.containerOffsetIndex_ >= 0) {
      scrollToIndexTarget = static_cast<int>(shadowlistViewStateData.containerOffsetIndex_);
    } else if (shadowlistViewProps.containerOffsetIndex >= 0) {
      scrollToIndexTarget = shadowlistViewProps.containerOffsetIndex;
    }
    if (scrollToIndexTarget >= 0 && scrollToIndexTarget != shadowlistCoreSharedInstance.prevContainerOffsetIndex) {
      containerManager->scrollToIndex(static_cast<std::size_t>(scrollToIndexTarget));
    }
    shadowlistCoreSharedInstance.prevContainerOffsetIndex = scrollToIndexTarget;

    /*
     * Reconcile, measure and resolve scrolling in a single core call
     */
    azimgd::shadowlist::FrameInput input;
    input.keys = shadowlistViewProps.elementsAllKeys;
    input.containerOffsetX = shadowlistViewStateData.containerOffsetX_;
    input.containerOffsetY = shadowlistViewStateData.containerOffsetY_;
    input.windowContainerWidth = shadowlistViewLayoutMetrics.frame.size.width;
    input.windowContainerHeight = shadowlistViewLayoutMetrics.frame.size.height;
    input.headerSize = *shadowlistCoreSharedInstance.headerSize;
    input.footerSize = *shadowlistCoreSharedInstance.footerSize;
    input.inverted = shadowlistViewProps.inverted;
    input.horizontal = shadowlistViewProps.horizontal;
    input.columns = shadowlistViewProps.columns > 0 ? static_cast<std::size_t>(shadowlistViewProps.columns) : 1;

    shadowlistCoreSharedInstance.virtualizerManager->update(containerManager, input);
  };

  private:
  mutable std::unordered_map<Tag, ShadowlistCoreSharedInstance> shadowlistCoreSharedInstances_;
};

void ShadowlistViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
