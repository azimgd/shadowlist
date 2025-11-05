#include "ShadowlistViewShadowNode.h"

namespace facebook::react {

void ShadowlistViewShadowNode::setContainerManager(std::shared_ptr<azimgd::shadowlist::Container> containerManager) {
  this->containerManager_ = containerManager;
}

void ShadowlistViewShadowNode::setVirtualizerManager(std::shared_ptr<azimgd::shadowlist::Virtualizer> virtualizerManager) {
  this->virtualizerManager_ = virtualizerManager;
}

void ShadowlistViewShadowNode::setContainerSizeUpdateState(std::shared_ptr<ContainerSizeUpdateState> containerSizeUpdateState) {
  this->containerSizeUpdateState_ = containerSizeUpdateState;
}

void ShadowlistViewShadowNode::setPrependElementsSize(std::shared_ptr<size_t> prependElementsSize) {
  this->prependElementsSize_ = prependElementsSize;
}

void ShadowlistViewShadowNode::setPrependElementsOffset(std::shared_ptr<double> prependElementsOffset) {
  this->prependElementsOffset_ = prependElementsOffset;
}

void ShadowlistViewShadowNode::setPrependedElementsOffset(std::shared_ptr<double> prependedElementsOffset) {
  this->prependedElementsOffset_ = prependedElementsOffset;
}

void ShadowlistViewShadowNode::layout(LayoutContext layoutContext) {
  ConcreteViewShadowNode::layout(layoutContext);

  /*
   * Update layout metrics for all virtualized elements
   */
  layoutContext.affectedNodes->clear();

  for (size_t i = 0; i < getChildren().size(); i++) {
    if (const auto prevElementViewProps = std::dynamic_pointer_cast<ShadowlistElementViewProps const>(getChildren()[i]->getProps())) {
      /*
       * Set opacity to prevent visual flash when prepending items
       * Without this, prepended elements briefly render at (0,0) before being positioned
       */
      auto nextElementViewProps = std::make_shared<ShadowlistElementViewProps>();
      nextElementViewProps->opacity = 1;
      nextElementViewProps->index = prevElementViewProps->index;

      auto elementViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode>(getChildren()[i]->clone({
        .props = nextElementViewProps
      }));

      /*
       * Apply pre-calculated positions from the virtualizer
       */
      LayoutMetrics layoutMetrics = elementViewNode->getLayoutMetrics();

      if (getConcreteProps().horizontal) {
        layoutMetrics.frame.origin.x = this->containerManager_->getElementAtIndex(prevElementViewProps->index).offsetX;
        layoutMetrics.frame.origin.y = 0;
      } else {
        layoutMetrics.frame.origin.y = this->containerManager_->getElementAtIndex(prevElementViewProps->index).offsetY;
        layoutMetrics.frame.origin.x = 0;
      }

      elementViewNode->setLayoutMetrics(layoutMetrics);
      replaceChild(*getChildren()[i], elementViewNode);
      layoutContext.affectedNodes->push_back(elementViewNode.get());
    }
  }

  /*
   * Update container state and handle scroll offset adjustments
   */
  auto nextStateData = getStateData();
  auto totalContainerHeight = this->containerManager_->nextRevision.totalContainerHeight;
  auto totalContainerWidth = this->containerManager_->nextRevision.totalContainerWidth;

  if (*this->prependElementsSize_ == 0) {
    if (getConcreteProps().horizontal) {
      *this->prependedElementsOffset_ = nextStateData.containerOffsetX_;
    } else {
      *this->prependedElementsOffset_ = nextStateData.containerOffsetY_;
    }
  }

  if (totalContainerHeight != nextStateData.totalContainerHeight_ || totalContainerWidth != nextStateData.totalContainerWidth_) {
    nextStateData.totalContainerHeight_ = totalContainerHeight;
    nextStateData.totalContainerWidth_ = totalContainerWidth;

    /*
     * Position inverted lists at the bottom/right initially, adjusting scroll offset
     * as container dimensions stabilize during measurement
     */
    if (getConcreteProps().inverted && *this->containerSizeUpdateState_ != ContainerSizeUpdateState::UPDATED) {
      if (getConcreteProps().horizontal) {
        nextStateData.containerOffsetX_ = this->containerManager_->nextRevision.totalContainerWidth - getLayoutMetrics().frame.size.width;
      } else {
        nextStateData.containerOffsetY_ = this->containerManager_->nextRevision.totalContainerHeight - getLayoutMetrics().frame.size.height;
      }
    }

    /*
     * Adjust scroll position to keep content visible when prepending elements
     */
    if (*this->prependElementsSize_ > 0) {
      if (getConcreteProps().horizontal) {
        nextStateData.containerOffsetX_ = *this->prependedElementsOffset_ + this->containerManager_->getElementAtIndex(*this->prependElementsSize_).offsetX;
      } else {
        nextStateData.containerOffsetY_ = *this->prependedElementsOffset_ + this->containerManager_->getElementAtIndex(*this->prependElementsSize_).offsetY;
      }
    }

    setStateData(std::move(nextStateData));
  } else {
    if (*this->containerSizeUpdateState_ == ContainerSizeUpdateState::INITIALIZED) {
      if (this->containerManager_->getElementAtIndex(this->containerManager_->nextRevision.elements.size() - 1).measured) {
        *this->containerSizeUpdateState_ = ContainerSizeUpdateState::UPDATED;
      }
    }
  }
  
  /*
   * Reset prepend offset tracking once items are measured and positioned
   * This typically happens on the next render after prepended items have been laid out
   */
  if (*this->prependElementsSize_ != 0) {
    if (
      getConcreteProps().inverted && (
      this->containerManager_->getVisibleIndices().second > *this->prependElementsSize_ ||
      this->containerManager_->getElementAtIndex(*this->prependElementsSize_ - 1).measured
    )) {
      if (getConcreteProps().horizontal) {
        *this->prependedElementsOffset_ = nextStateData.containerOffsetX_;
      } else {
        *this->prependedElementsOffset_ = nextStateData.containerOffsetY_;
      }
      *this->prependElementsSize_ = 0;
    }

    if (
      !getConcreteProps().inverted && (
      this->containerManager_->getVisibleIndices().first > *this->prependElementsSize_ ||
      this->containerManager_->getElementAtIndex(*this->prependElementsSize_ - 1).measured
    )) {
      if (getConcreteProps().horizontal) {
        *this->prependedElementsOffset_ = nextStateData.containerOffsetX_;
      } else {
        *this->prependedElementsOffset_ = nextStateData.containerOffsetY_;
      }
      *this->prependElementsSize_ = 0;
    }
  }
}

}
