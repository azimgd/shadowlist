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
   * Layoutable elements metrics adjustments
   */
  layoutContext.affectedNodes->clear();

  for (size_t i = 0; i < getChildren().size(); i++) {
    if (const auto elementViewProps = std::dynamic_pointer_cast<ShadowlistElementViewProps const>(getChildren()[i]->getProps())) {
      auto elementViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode>(getChildren()[i]->clone({}));
      LayoutMetrics layoutMetrics = elementViewNode->getLayoutMetrics();

      layoutMetrics.frame.origin.y = this->containerManager_->getElementAtIndex(elementViewProps->index).offsetY;
      layoutMetrics.frame.origin.x = this->containerManager_->getElementAtIndex(elementViewProps->index).offsetX;

      elementViewNode->setLayoutMetrics(layoutMetrics);
      replaceChild(*getChildren()[i], elementViewNode);
      layoutContext.affectedNodes->push_back(elementViewNode.get());
    }
  }

  /*
   * State updates
   */
  auto nextStateData = getStateData();
  auto totalContainerHeight = this->containerManager_->nextRevision.totalContainerHeight;
  auto totalContainerWidth = this->containerManager_->nextRevision.totalContainerWidth;

  if (*this->prependElementsSize_ == 0) {
    *this->prependedElementsOffset_ = nextStateData.containerOffsetY_;
  }

  if (totalContainerHeight != nextStateData.totalContainerHeight_ || totalContainerWidth != nextStateData.totalContainerWidth_) {
    nextStateData.totalContainerHeight_ = totalContainerHeight;
    nextStateData.totalContainerWidth_ = totalContainerWidth;

    /*
     * Initial positioning for inverted lists
     * Adjust scroll position until container dimensions stabilize
     */
    if (getConcreteProps().inverted && *this->containerSizeUpdateState_ != ContainerSizeUpdateState::UPDATED) {
      if (getConcreteProps().horizontal) {
        nextStateData.containerOffsetX_ = this->containerManager_->nextRevision.totalContainerWidth - getLayoutMetrics().frame.size.width;
      } else {
        nextStateData.containerOffsetY_ = this->containerManager_->nextRevision.totalContainerHeight - getLayoutMetrics().frame.size.height;
      }
    }

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

  if (*this->prependElementsSize_ != 0) {
    if (
      this->containerManager_->getVisibleIndices().second > *this->prependElementsSize_ ||
      this->containerManager_->getElementAtIndex(*this->prependElementsSize_ - 1).measured
    ) {
      *this->prependedElementsOffset_ = nextStateData.containerOffsetY_;
      *this->prependElementsSize_ = 0;
    }
  }
}

}
