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
    
    containerManager_->offsetInitAdjustmentCallback = [](azimgd::shadowlist::Revision revision) -> void {};
    containerManager_->offsetMvcpAdjustmentCallback = [](azimgd::shadowlist::Revision revision) -> void {};
    containerManager_->sizeAdjustmentCallback = [](azimgd::shadowlist::Revision revision) -> void {};
    containerManager_->measurementCallback = [](std::size_t index) -> std::pair<double, double> {
      return {120.0, 120.0};
    };
  };

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);

    auto& shadowlistViewShadowNode = static_cast<ShadowlistViewShadowNode&>(shadowNode);
    shadowlistViewShadowNode.setContainerManager(containerManager_);
    shadowlistViewShadowNode.setVirtualizerManager(virtualizerManager_);
    
    auto& shadowlistViewProps = static_cast<const ShadowlistViewShadowNode::ConcreteProps&>(*shadowNode.getProps());
    auto& shadowlistViewState = static_cast<const ShadowlistViewShadowNode::ConcreteState&>(*shadowNode.getState());
    auto& shadowlistViewStateData = shadowlistViewState.getData();
    
    auto& shadowlistViewChildren = shadowNode.getChildren();
    auto shadowlistViewLayoutMetrics = static_cast<YogaLayoutableShadowNode&>(shadowNode).getLayoutMetrics();
    
    if (this->containerManager_->nextRevision.elements.size() != shadowlistViewProps.elementsAllKeys.size()) {
      // @TODO: increment only for now, implement decrement as well
      auto elementsSizeDiff = shadowlistViewProps.elementsAllKeys.size() - this->containerManager_->nextRevision.elements.size();

      this->containerManager_->resizeElementsTail(shadowlistViewProps.elementsAllKeys.size());
      
      if (this->elementsHeadKey_.length() > 0 && this->elementsHeadKey_ != shadowlistViewProps.elementsHeadKey) {
        this->virtualizerManager_->prependElements(this->containerManager_.get(), elementsSizeDiff);
      } else if (this->elementsTailKey_.length() > 0  && this->elementsTailKey_ != shadowlistViewProps.elementsTailKey) {
        this->virtualizerManager_->appendElements(this->containerManager_.get(), elementsSizeDiff);
      }
      
      elementsHeadKey_ = shadowlistViewProps.elementsHeadKey;
      elementsTailKey_ = shadowlistViewProps.elementsTailKey;
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

        this->virtualizerManager_->updateElementAtIndex(
          this->containerManager_.get(),
          elementViewProps->index,
          this->containerManager_->nextRevision,
          {
            .width = elementViewNode->getLayoutMetrics().frame.size.width,
            .height = elementViewNode->getLayoutMetrics().frame.size.height,
          });
      }
    }

    this->virtualizerManager_->measure(this->containerManager_.get());
    this->containerManager_->endRevision();
  };

  private:
  mutable std::string elementsHeadKey_;
  mutable std::string elementsTailKey_;
  std::shared_ptr<azimgd::shadowlist::Container> containerManager_;
  std::shared_ptr<azimgd::shadowlist::Virtualizer> virtualizerManager_;
};


void ShadowlistViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

} // namespace facebook::react
