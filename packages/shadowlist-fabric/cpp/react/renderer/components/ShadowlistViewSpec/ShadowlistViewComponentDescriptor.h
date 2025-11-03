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
    containerManager_ = std::make_shared<azimgd::shadowlist::Container>();
    virtualizerManager_ = std::make_shared<azimgd::shadowlist::Virtualizer>();
    containerSizeUpdateState_ = std::make_shared<ShadowlistViewShadowNode::ContainerSizeUpdateState>(ShadowlistViewShadowNode::ContainerSizeUpdateState::INITIALIZED);
    prependElementsSize_ = std::make_shared<size_t>(0);
  };

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);

    auto& shadowlistViewShadowNode = static_cast<ShadowlistViewShadowNode&>(shadowNode);
    shadowlistViewShadowNode.setContainerManager(containerManager_);
    shadowlistViewShadowNode.setVirtualizerManager(virtualizerManager_);
    shadowlistViewShadowNode.setContainerSizeUpdateState(containerSizeUpdateState_);
    shadowlistViewShadowNode.setPrependElementsSize(prependElementsSize_);
    
    auto& shadowlistViewProps = static_cast<const ShadowlistViewShadowNode::ConcreteProps&>(*shadowNode.getProps());
    auto& shadowlistViewState = static_cast<const ShadowlistViewShadowNode::ConcreteState&>(*shadowNode.getState());
    auto& shadowlistViewEventEmitter = static_cast<const ShadowlistViewShadowNode::ConcreteEventEmitter&>(*shadowNode.getEventEmitter());
    
    auto shadowlistViewStateData = shadowlistViewState.getData();
    auto& shadowlistViewChildren = shadowNode.getChildren();
    auto shadowlistViewLayoutMetrics = static_cast<YogaLayoutableShadowNode&>(shadowNode).getLayoutMetrics();
    
    if (containerManager_->onStartReachedCallback == nullptr) {
      containerManager_->onStartReachedCallback = [shadowlistViewEventEmitter]() -> void {
        shadowlistViewEventEmitter.onStartReached({});
      };
    }

    if (containerManager_->onEndReachedCallback == nullptr) {
      containerManager_->onEndReachedCallback = [shadowlistViewEventEmitter]() -> void {
        shadowlistViewEventEmitter.onEndReached({});
      };
    }

    if (this->containerManager_->nextRevision.elements.size() != shadowlistViewProps.elementsAllKeys.size()) {
      // @TODO: increment only for now, implement decrement as well
      auto prependElementsSize = shadowlistViewProps.elementsAllKeys.size() - this->containerManager_->nextRevision.elements.size();

      if (this->elementsHeadKey_.length() > 0 && this->elementsHeadKey_ != shadowlistViewProps.elementsHeadKey) {
        this->virtualizerManager_->prependElements(this->containerManager_.get(), prependElementsSize);
        *this->prependElementsSize_ = prependElementsSize;
      } else if (this->elementsTailKey_.length() > 0  && this->elementsTailKey_ != shadowlistViewProps.elementsTailKey) {
        this->virtualizerManager_->appendElements(this->containerManager_.get(), prependElementsSize);
      } else {
        this->containerManager_->resizeElementsTail(shadowlistViewProps.elementsAllKeys.size());
      }

      this->elementsHeadKey_ = shadowlistViewProps.elementsHeadKey;
      this->elementsTailKey_ = shadowlistViewProps.elementsTailKey;
    }
    
    this->containerManager_->inverted = shadowlistViewProps.inverted;
    this->containerManager_->horizontal = shadowlistViewProps.horizontal;
    
    this->containerManager_->startRevision();
    
    this->containerManager_->setContainerOffsetX(shadowlistViewStateData.containerOffsetX_);
    this->containerManager_->setContainerOffsetY(shadowlistViewStateData.containerOffsetY_);
    this->containerManager_->setWindowContainerWidth(shadowlistViewLayoutMetrics.frame.size.width);
    this->containerManager_->setWindowContainerHeight(shadowlistViewLayoutMetrics.frame.size.height);

    /*
     * Revision elements metrics adjustments
     */
    for (size_t i = 0; i < shadowlistViewChildren.size(); i++) {
      if (const auto elementViewProps = std::dynamic_pointer_cast<ShadowlistElementViewProps const>(shadowlistViewChildren[i]->getProps())) {
        const auto elementViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode const>(shadowlistViewChildren[i]);
        const auto elementViewNodeSize = elementViewNode->getLayoutMetrics().frame.size;

        this->virtualizerManager_->updateElementAtIndex(
          this->containerManager_.get(),
          elementViewProps->index,
          { .width = elementViewNodeSize.width, .height = elementViewNodeSize.height });
      }
    }

    this->virtualizerManager_->measure(this->containerManager_.get());
    this->containerManager_->endRevision();

    auto nextVisibleIndices = this->containerManager_->getVisibleIndices();
    int nextVisibleStartIndex = static_cast<int>(nextVisibleIndices.first);
    int nextVisibleEndIndex = static_cast<int>(nextVisibleIndices.second);

    if (prevVisibleStartIndex_ != nextVisibleStartIndex || prevVisibleEndIndex_ != nextVisibleEndIndex) {
      shadowlistViewEventEmitter.onVisibleIndicesChange({
        .visibleStartIndex = nextVisibleStartIndex,
        .visibleEndIndex = nextVisibleEndIndex,
      });

      prevVisibleStartIndex_ = nextVisibleStartIndex;
      prevVisibleEndIndex_ = nextVisibleEndIndex;
    }
  };

  private:
  mutable std::string elementsHeadKey_;
  mutable std::string elementsTailKey_;
  mutable int prevVisibleStartIndex_ = -1;
  mutable int prevVisibleEndIndex_ = -1;
  std::shared_ptr<azimgd::shadowlist::Container> containerManager_;
  std::shared_ptr<azimgd::shadowlist::Virtualizer> virtualizerManager_;
  std::shared_ptr<ShadowlistViewShadowNode::ContainerSizeUpdateState> containerSizeUpdateState_;
  std::shared_ptr<size_t> prependElementsSize_;
};


void ShadowlistViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
