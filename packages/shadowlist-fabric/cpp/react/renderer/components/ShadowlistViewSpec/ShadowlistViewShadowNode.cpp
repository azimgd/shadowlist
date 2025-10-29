#include "ShadowlistViewShadowNode.h"
#include <iostream>

namespace facebook::react {

void ShadowlistViewShadowNode::setContainerManager(std::shared_ptr<azimgd::shadowlist::Container> containerManager) {
  this->containerManager_ = containerManager;
}
void ShadowlistViewShadowNode::setVirtualizerManager(std::shared_ptr<azimgd::shadowlist::Virtualizer> virtualizerManager) {
  this->virtualizerManager_ = virtualizerManager;
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
  auto visibleStartIndex = this->containerManager_->getVisibleIndices().first;
  auto visibleEndIndex = this->containerManager_->getVisibleIndices().second;

  if (visibleStartIndex != nextStateData.visibleStartIndex_ || visibleEndIndex != nextStateData.visibleEndIndex_) {
    nextStateData.totalContainerHeight_ = this->containerManager_->nextRevision.totalContainerHeight;
    nextStateData.totalContainerWidth_ = this->containerManager_->nextRevision.totalContainerWidth;
    nextStateData.containerOffsetY_ = this->containerManager_->nextRevision.containerOffsetY + this->containerManager_->nextRevision.mvcpDiffHeight;
    nextStateData.containerOffsetX_ = this->containerManager_->nextRevision.containerOffsetY + this->containerManager_->nextRevision.mvcpDiffWidth;
    nextStateData.visibleStartIndex_ = this->containerManager_->getVisibleIndices().first;
    nextStateData.visibleEndIndex_ = this->containerManager_->getVisibleIndices().second;

    this->containerManager_->nextRevision.mvcpDiffWidth = 0;
    this->containerManager_->nextRevision.mvcpDiffHeight = 0;

    setStateData(std::move(nextStateData));
  }
}

}
