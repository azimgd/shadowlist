#include "ShadowlistViewShadowNode.h"

namespace facebook::react {

void ShadowlistViewShadowNode::setContainerManager(std::shared_ptr<azimgd::shadowlist::Container> containerManager) {
  this->containerManager_ = containerManager;
}

void ShadowlistViewShadowNode::setVirtualizerManager(std::shared_ptr<azimgd::shadowlist::Virtualizer> virtualizerManager) {
  this->virtualizerManager_ = virtualizerManager;
}

void ShadowlistViewShadowNode::setHeaderSize(std::shared_ptr<double> headerSize) {
  this->headerSize_ = headerSize;
}

void ShadowlistViewShadowNode::setFooterSize(std::shared_ptr<double> footerSize) {
  this->footerSize_ = footerSize;
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
  bool horizontal = getConcreteProps().horizontal;

  for (size_t i = 0; i < getChildren().size(); i++) {
    if (std::dynamic_pointer_cast<ShadowlistElementViewProps const>(getChildren()[i]->getProps())) {
      continue;
    }

    if (const auto templateProps = std::dynamic_pointer_cast<ShadowlistTemplateViewProps const>(getChildren()[i]->getProps())) {
      const auto templateViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode const>(getChildren()[i]);
      if (!templateViewNode) continue;

      const auto templateViewNodeLayoutMetrics = templateViewNode->getLayoutMetrics();
      const auto templateViewNodeSize = horizontal
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
   * Feed the measured header/footer sizes back so the next Virtualizer::update
   * positions elements after the header and includes both in the total size
   */
  *this->headerSize_ = headerSize;
  *this->footerSize_ = footerSize;

  /*
   * Apply pre-calculated positions from the virtualizer (offsets already include the header)
   */
  for (size_t i = 0; i < getChildren().size(); i++) {
    if (const auto prevElementViewProps = std::dynamic_pointer_cast<ShadowlistElementViewProps const>(getChildren()[i]->getProps())) {
      auto elementViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode>(getChildren()[i]->clone({}));

      LayoutMetrics layoutMetrics = elementViewNode->getLayoutMetrics();
      const auto element = this->containerManager_->getElementAtIndex(prevElementViewProps->index);

      if (this->containerManager_->columns > 1) {
        layoutMetrics.frame.origin.x = element.offsetX;
        layoutMetrics.frame.origin.y = element.offsetY;
        layoutMetrics.frame.size.width = element.width;
      } else if (horizontal) {
        layoutMetrics.frame.origin.x = element.offsetX;
        layoutMetrics.frame.origin.y = 0;
      } else {
        layoutMetrics.frame.origin.y = element.offsetY;
        layoutMetrics.frame.origin.x = 0;
      }

      elementViewNode->setLayoutMetrics(layoutMetrics);
      replaceChild(*getChildren()[i], elementViewNode);
      layoutContext.affectedNodes->push_back(elementViewNode.get());
    }
  }

  /*
   * Position header (at the origin) and footer (after the content)
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

    if (horizontal) {
      footerMetrics.frame.origin.x = this->containerManager_->nextRevision.totalContainerWidth - footerSize;
      footerMetrics.frame.origin.y = 0;
    } else {
      footerMetrics.frame.origin.y = this->containerManager_->nextRevision.totalContainerHeight - footerSize;
      footerMetrics.frame.origin.x = 0;
    }

    footerViewNode->setLayoutMetrics(footerMetrics);
    replaceChild(*footerNode, footerViewNode);
    layoutContext.affectedNodes->push_back(footerViewNode.get());
  }

  /*
   * Push the core resolved scroll offset and total container size into state.
   * containerOffsetEnabled is only set when the core moved the offset (scrollToIndex,
   * maintain-visible-content-position, inverted initial position) so we don't fight
   * the user's own scrolling.
   */
  auto nextStateData = getStateData();

  double totalContainerWidth = this->containerManager_->nextRevision.totalContainerWidth;
  double totalContainerHeight = this->containerManager_->nextRevision.totalContainerHeight;
  double containerOffsetX = this->containerManager_->nextRevision.containerOffsetX;
  double containerOffsetY = this->containerManager_->nextRevision.containerOffsetY;

  bool offsetChanged =
    containerOffsetX != nextStateData.containerOffsetX_ ||
    containerOffsetY != nextStateData.containerOffsetY_;
  bool sizeChanged =
    totalContainerWidth != nextStateData.totalContainerWidth_ ||
    totalContainerHeight != nextStateData.totalContainerHeight_;

  if (offsetChanged || sizeChanged) {
    nextStateData.totalContainerWidth_ = totalContainerWidth;
    nextStateData.totalContainerHeight_ = totalContainerHeight;
    nextStateData.containerOffsetX_ = containerOffsetX;
    nextStateData.containerOffsetY_ = containerOffsetY;
    nextStateData.containerOffsetEnabled_ = offsetChanged;
    setStateData(std::move(nextStateData));
  }
}

void ShadowlistViewShadowNode::appendChild(const std::shared_ptr<const ShadowNode>& nextElementShadowNode) {
  YogaLayoutableShadowNode::appendChild(nextElementShadowNode);
}

void ShadowlistViewShadowNode::replaceChild(
  const ShadowNode& prevElementShadowNode,
  const std::shared_ptr<const ShadowNode>& nextElementShadowNode,
  size_t suggestedIndex) {

  /*
   * Feed natively measured element sizes back into the virtualizer
   */
  if (const auto elementViewProps = std::dynamic_pointer_cast<ShadowlistElementViewProps const>(nextElementShadowNode->getProps())) {
    const auto elementViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode const>(nextElementShadowNode);
    const auto elementViewNodeSize = elementViewNode->getLayoutMetrics().frame.size;

    this->virtualizerManager_->updateElementAtIndex(
      this->containerManager_.get(),
      elementViewProps->index,
      { .width = elementViewNodeSize.width, .height = elementViewNodeSize.height });
  }

  YogaLayoutableShadowNode::replaceChild(prevElementShadowNode, nextElementShadowNode, suggestedIndex);
}

}
