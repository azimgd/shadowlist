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

void ShadowlistViewShadowNode::setMeasuredElementsSize(std::shared_ptr<double> measuredElementsSize) {
  this->measuredElementsSize_ = measuredElementsSize;
}

void ShadowlistViewShadowNode::layout(LayoutContext layoutContext) {
  ConcreteViewShadowNode::layout(layoutContext);

  /*
   * Identify and measure template views (header/footer/empty)
   */
  layoutContext.affectedNodes->clear();

  std::shared_ptr<const ShadowNode> headerNode = nullptr;
  std::shared_ptr<const ShadowNode> footerNode = nullptr;
  double headerSize = 0.0;
  double footerSize = 0.0;

  for (size_t i = 0; i < getChildren().size(); i++) {
    if (std::dynamic_pointer_cast<ShadowlistElementViewProps const>(getChildren()[i]->getProps())) {
      continue;
    }

    if (const auto templateProps = std::dynamic_pointer_cast<ShadowlistTemplateViewProps const>(getChildren()[i]->getProps())) {
      const auto templateViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode const>(getChildren()[i]);
      if (!templateViewNode) continue;

      const auto templateViewNodeLayoutMetrics = templateViewNode->getLayoutMetrics();
      const auto templateViewNodeSize = getConcreteProps().horizontal
        ? templateViewNodeLayoutMetrics.frame.size.width
        : templateViewNodeLayoutMetrics.frame.size.height;

      if (templateProps->templateType == "header" || templateProps->templateType == "empty") {
        headerNode = getChildren()[i];
        headerSize = templateViewNodeSize;
      } else if (templateProps->templateType == "footer") {
        footerNode = getChildren()[i];
        footerSize = templateViewNodeSize;
      }
    }
  }

  /*
   * Update layout metrics for all virtualized elements (with header offset)
   */
  for (size_t i = 0; i < getChildren().size(); i++) {
    if (const auto prevElementViewProps = std::dynamic_pointer_cast<ShadowlistElementViewProps const>(getChildren()[i]->getProps())) {
      auto elementViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode>(getChildren()[i]->clone({}));

      /*
       * Apply pre-calculated positions from the virtualizer
       */
      LayoutMetrics layoutMetrics = elementViewNode->getLayoutMetrics();

      if (this->containerManager_->columns > 1) {
        /*
         * Multi-column layout: set both X and Y positions and element width
         */
        const auto element = this->containerManager_->getElementAtIndex(prevElementViewProps->index);
        layoutMetrics.frame.origin.x = element.offsetX;
        layoutMetrics.frame.origin.y = element.offsetY + headerSize;
        layoutMetrics.frame.size.width = element.width;
      } else {
        /*
         * Single column layout: use orientation-based positioning with header offset
         */
        if (getConcreteProps().horizontal) {
          layoutMetrics.frame.origin.x = this->containerManager_->getElementOffset(prevElementViewProps->index) + headerSize;
          layoutMetrics.frame.origin.y = 0;
        } else {
          layoutMetrics.frame.origin.y = this->containerManager_->getElementOffset(prevElementViewProps->index) + headerSize;
          layoutMetrics.frame.origin.x = 0;
        }
      }

      elementViewNode->setLayoutMetrics(layoutMetrics);
      replaceChild(*getChildren()[i], elementViewNode);
      layoutContext.affectedNodes->push_back(elementViewNode.get());
    }
  }

  /*
   * Position header and footer template views
   */
  if (headerNode) {
    auto headerViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode>(headerNode->clone({}));
    LayoutMetrics headerMetrics = headerViewNode->getLayoutMetrics();

    headerMetrics.frame.origin.x = 0;
    headerMetrics.frame.origin.y = 0;

    headerViewNode->setLayoutMetrics(headerMetrics);
    replaceChild(*headerNode, headerViewNode);
    layoutContext.affectedNodes->push_back(headerViewNode.get());
  }

  if (footerNode) {
    auto footerViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode>(footerNode->clone({}));
    LayoutMetrics footerMetrics = footerViewNode->getLayoutMetrics();

    if (getConcreteProps().horizontal) {
      footerMetrics.frame.origin.x = this->containerManager_->nextRevision.totalContainerWidth + headerSize;
      footerMetrics.frame.origin.y = 0;
    } else {
      footerMetrics.frame.origin.y = this->containerManager_->nextRevision.totalContainerHeight + headerSize;
      footerMetrics.frame.origin.x = 0;
    }

    footerViewNode->setLayoutMetrics(footerMetrics);
    replaceChild(*footerNode, footerViewNode);
    layoutContext.affectedNodes->push_back(footerViewNode.get());
  }

  /*
   * Update container state and handle scroll offset adjustments
   * Include header and footer in total container size (only in scroll direction)
   */
  auto nextStateData = getStateData();
  auto totalContainerHeight = getConcreteProps().horizontal
    ? this->containerManager_->nextRevision.totalContainerHeight
    : this->containerManager_->nextRevision.totalContainerHeight + headerSize + footerSize;
  auto totalContainerWidth = getConcreteProps().horizontal
    ? this->containerManager_->nextRevision.totalContainerWidth + headerSize + footerSize
    : this->containerManager_->nextRevision.totalContainerWidth;

  /*
   * Initialize containerOffsetIndex from prop if provided (>= 0) and only once
   * State starts at -2.0 (initial), gets set to index, then resets to -1.0 (inactive)
   */
  if (getConcreteProps().containerOffsetIndex >= 0 && nextStateData.containerOffsetIndex_ == -2.0) {
    nextStateData.containerOffsetIndex_ = static_cast<double>(getConcreteProps().containerOffsetIndex);
  }

  if (*this->prependElementsSize_ == 0) {
    if (getConcreteProps().horizontal) {
      *this->prependedElementsOffset_ = nextStateData.containerOffsetX_;
    } else {
      *this->prependedElementsOffset_ = nextStateData.containerOffsetY_;
    }
  }

  if (
    totalContainerHeight != nextStateData.totalContainerHeight_ ||
    totalContainerWidth != nextStateData.totalContainerWidth_ ||
    nextStateData.containerOffsetIndex_ >= 0
    ) {
    nextStateData.totalContainerHeight_ = totalContainerHeight;
    nextStateData.totalContainerWidth_ = totalContainerWidth;

    /*
     * Position inverted lists at the bottom/right initially, adjusting scroll offset
     * as container dimensions stabilize during measurement
     * Skip this if containerOffsetIndex prop is provided (>= 0)
     * Use totalContainerHeight/Width which already includes header and footer
     */
    if (getConcreteProps().inverted && *this->containerSizeUpdateState_ != ContainerSizeUpdateState::UPDATED && getConcreteProps().containerOffsetIndex < 0) {
      if (getConcreteProps().horizontal) {
        nextStateData.containerOffsetX_ = totalContainerWidth - getLayoutMetrics().frame.size.width;
      } else {
        nextStateData.containerOffsetY_ = totalContainerHeight - getLayoutMetrics().frame.size.height;
      }
    }

    /*
     * Adjust scroll position to keep content visible when prepending elements
     */
    if (*this->prependElementsSize_ > 0) {
      if (getConcreteProps().horizontal) {
        nextStateData.containerOffsetX_ = *this->prependedElementsOffset_ + this->containerManager_->getElementOffset(*this->prependElementsSize_);
      } else {
        nextStateData.containerOffsetY_ = *this->prependedElementsOffset_ + this->containerManager_->getElementOffset(*this->prependElementsSize_);
      }
    }
    
    if (getConcreteProps().inverted) {
      if (getConcreteProps().horizontal) {
        nextStateData.containerOffsetX_ += *this->measuredElementsSize_;
      } else {
        nextStateData.containerOffsetY_ += *this->measuredElementsSize_;
      }
    }

    /*
     * Handle containerOffsetIndex for initial scroll position or scrollToIndex
     * Add header offset since elements are positioned after the header
     */
    if (nextStateData.containerOffsetIndex_ >= 0) {
      if (getConcreteProps().horizontal) {
        nextStateData.containerOffsetX_ = this->containerManager_->getElementOffset(nextStateData.containerOffsetIndex_) + headerSize;
      } else {
        nextStateData.containerOffsetY_ = this->containerManager_->getElementOffset(nextStateData.containerOffsetIndex_) + headerSize;
      }

      if (this->containerManager_->getElementAtIndex(nextStateData.containerOffsetIndex_).measured) {
        nextStateData.containerOffsetIndex_ = -1;

        /*
         * Mark container size as updated when using containerOffsetIndex on inverted list
         * This prevents default inverted positioning logic from interfering
         */
        if (getConcreteProps().inverted) {
          *this->containerSizeUpdateState_ = ContainerSizeUpdateState::UPDATED;
        }
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
   * Reset prepend offset tracking once elements are measured and positioned
   * This typically happens on the next render after prepended elements have been laid out
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
  
  /*
   * Reset measured elements size after applying to all elements
   */
  *this->measuredElementsSize_ = 0.0;
}

void ShadowlistViewShadowNode::appendChild(const std::shared_ptr<const ShadowNode>& nextElementShadowNode) {
  YogaLayoutableShadowNode::appendChild(nextElementShadowNode);
}

void ShadowlistViewShadowNode::replaceChild(
  const ShadowNode& prevElementShadowNode,
  const std::shared_ptr<const ShadowNode>& nextElementShadowNode,
  size_t suggestedIndex) {

  if (const auto elementViewProps = std::dynamic_pointer_cast<ShadowlistElementViewProps const>(nextElementShadowNode->getProps())) {
    const auto elementViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode const>(nextElementShadowNode);
    const auto elementViewNodeSize = elementViewNode->getLayoutMetrics().frame.size;

    /*
     * Get the old size before updating to calculate size difference
     */
    const auto prevElement = this->containerManager_->getElementAtIndex(elementViewProps->index);
    const auto prevSize = this->containerManager_->getElementSize(elementViewProps->index);

    this->virtualizerManager_->updateElementAtIndex(
      this->containerManager_.get(),
      elementViewProps->index,
      { .width = elementViewNodeSize.width, .height = elementViewNodeSize.height });

    /*
     * Calculate size difference and accumulate for scroll offset adjustment
     */
    double sizeDifference;
    if (getConcreteProps().horizontal) {
      sizeDifference = elementViewNodeSize.width - prevSize;
    } else {
      sizeDifference = elementViewNodeSize.height - prevSize;
    }

    *this->measuredElementsSize_ += sizeDifference;
  }

  YogaLayoutableShadowNode::replaceChild(prevElementShadowNode, nextElementShadowNode, suggestedIndex);
}

}
