#pragma once

#include "ShadowlistViewShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

namespace facebook::react {

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

    /*
     * Lazily create the core for the initial node of a list. Clones carry these
     * instances forward (see the ShadowNode clone constructor), so a single core
     * is shared across a list's committed clones and freed when the node family
     * is destroyed - no descriptor-level registry that would leak one entry per
     * mounted list.
     */
    if (!shadowlistViewShadowNode.getContainerManager()) {
      shadowlistViewShadowNode.setContainerManager(std::make_shared<azimgd::shadowlist::Container>());
      shadowlistViewShadowNode.setVirtualizerManager(std::make_shared<azimgd::shadowlist::Virtualizer>());
      shadowlistViewShadowNode.setHeaderSize(std::make_shared<double>(0.0));
      shadowlistViewShadowNode.setFooterSize(std::make_shared<double>(0.0));
    }

    auto& shadowlistViewProps = static_cast<const ShadowlistViewShadowNode::ConcreteProps&>(*shadowNode.getProps());
    auto& shadowlistViewState = static_cast<const ShadowlistViewShadowNode::ConcreteState&>(*shadowNode.getState());
    auto& shadowlistViewEventEmitter = static_cast<const ShadowlistViewShadowNode::ConcreteEventEmitter&>(*shadowNode.getEventEmitter());

    auto shadowlistViewStateData = shadowlistViewState.getData();
    auto shadowlistViewLayoutMetrics = static_cast<YogaLayoutableShadowNode&>(shadowNode).getLayoutMetrics();

    auto containerManager = shadowlistViewShadowNode.getContainerManager().get();

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
    containerManager->onViewableIndicesChangeCallback = [shadowlistViewEventEmitter](std::size_t startIndex, std::size_t endIndex) -> void {
      shadowlistViewEventEmitter.onViewableIndicesChange({
        .viewableStartIndex = static_cast<int>(startIndex),
        .viewableEndIndex = static_cast<int>(endIndex),
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
     * index. The core resolves precedence and fires once per distinct target.
     */
    containerManager->requestScrollToIndex(
      shadowlistViewStateData.containerOffsetIndex_,
      shadowlistViewStateData.containerOffsetIndexNonce_,
      shadowlistViewProps.containerOffsetIndex);

    /*
     * Reconcile, measure and resolve scrolling in a single core call
     */
    azimgd::shadowlist::FrameInput input;
    input.keys = shadowlistViewProps.elementsAllKeys;
    input.containerOffsetX = shadowlistViewStateData.containerOffsetX_;
    input.containerOffsetY = shadowlistViewStateData.containerOffsetY_;
    input.windowContainerWidth = shadowlistViewLayoutMetrics.frame.size.width;
    input.windowContainerHeight = shadowlistViewLayoutMetrics.frame.size.height;
    input.headerSize = *shadowlistViewShadowNode.getHeaderSize();
    input.footerSize = *shadowlistViewShadowNode.getFooterSize();
    input.stickyHeader = shadowlistViewProps.stickyHeader;
    input.stickyFooter = shadowlistViewProps.stickyFooter;
    /*
     * Section-header element indices (SectionList). Skip negatives defensively; the
     * core expects an ascending list of valid indices.
     */
    input.stickyIndices.reserve(shadowlistViewProps.stickyHeaderIndices.size());
    for (auto stickyHeaderIndex : shadowlistViewProps.stickyHeaderIndices) {
      if (stickyHeaderIndex >= 0) {
        input.stickyIndices.push_back(static_cast<std::size_t>(stickyHeaderIndex));
      }
    }
    input.inverted = shadowlistViewProps.inverted;
    input.horizontal = shadowlistViewProps.horizontal;
    input.columns = shadowlistViewProps.columns > 0 ? static_cast<std::size_t>(shadowlistViewProps.columns) : 1;
    input.startReachedThreshold = shadowlistViewProps.startReachedThreshold;
    input.endReachedThreshold = shadowlistViewProps.endReachedThreshold;
    input.viewablePercentThreshold = shadowlistViewProps.viewablePercentThreshold;

    /*
     * A genuine user scroll abandons any in-flight scroll correction so the user
     * takes over instead of being snapped back. Without this the core's "yield to
     * the user" path never fires and a transient correction can latch and freeze
     * the visible window (blank list on deep scroll).
     */
    input.userScrolled = shadowlistViewStateData.userScrolled_;

    shadowlistViewShadowNode.getVirtualizerManager()->update(containerManager, input);
  };
};

void ShadowlistViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
