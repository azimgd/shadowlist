#include "ShadowlistViewShadowNode.h"

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
  auto totalContainerHeight = this->containerManager_->nextRevision.totalContainerHeight;
  auto totalContainerWidth = this->containerManager_->nextRevision.totalContainerWidth;

  if (totalContainerHeight != nextStateData.totalContainerHeight_ || totalContainerWidth != nextStateData.totalContainerWidth_) {
    nextStateData.totalContainerHeight_ = totalContainerHeight;
    nextStateData.totalContainerWidth_ = totalContainerWidth;

    setStateData(std::move(nextStateData));
  }

  /*
   * Set initial scroll position for inverted lists
   */
  auto stateData = getStateData();
  if (!stateData.containerOffsetUpdated_) {
    if (getConcreteProps().inverted) {
      if (getConcreteProps().horizontal) {
        stateData.containerOffsetX_ = this->containerManager_->nextRevision.totalContainerWidth - getLayoutMetrics().frame.size.width;
      } else {
        stateData.containerOffsetY_ = this->containerManager_->nextRevision.totalContainerHeight - getLayoutMetrics().frame.size.height;
      }

      setStateData(std::move(stateData));
    }
  }
}

}
