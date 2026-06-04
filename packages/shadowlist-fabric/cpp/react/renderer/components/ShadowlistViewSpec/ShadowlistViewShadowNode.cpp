#include "ShadowlistViewShadowNode.h"

#include <mutex>

namespace facebook::react {

ShadowlistViewShadowNode::ShadowlistViewShadowNode(
  const ShadowNode& sourceShadowNode,
  const ShadowNodeFragment& fragment) :
  ConcreteViewShadowNode(sourceShadowNode, fragment) {
  /*
   * Carry the shared core instances forward from the source so every clone of a
   * list shares one Container/Virtualizer, freed when the node family dies
   */
  const auto& source = static_cast<const ShadowlistViewShadowNode&>(sourceShadowNode);
  this->containerManager_ = source.containerManager_;
  this->virtualizerManager_ = source.virtualizerManager_;
  this->headerSize_ = source.headerSize_;
  this->footerSize_ = source.footerSize_;
}

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

  if (!this->containerManager_ || !this->virtualizerManager_) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(this->containerManager_->coreMutex);

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
      if (prevElementViewProps->index >= this->containerManager_->getElementsSize()) {
        continue;
      }

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
   * Refresh the total once now that the measured sizes for this batch of children
   * have been fed back, instead of rescanning the whole list per measured child.
   * The footer position below and the published content size depend on it.
   */
  azimgd::shadowlist::Virtualizer::recomputeTotalSize(this->containerManager_.get());

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
      footerMetrics.frame.origin.x = this->containerManager_->getFooterOffset(footerSize);
      footerMetrics.frame.origin.y = 0;
    } else {
      footerMetrics.frame.origin.y = this->containerManager_->getFooterOffset(footerSize);
      footerMetrics.frame.origin.x = 0;
    }

    footerViewNode->setLayoutMetrics(footerMetrics);
    replaceChild(*footerNode, footerViewNode);
    layoutContext.affectedNodes->push_back(footerViewNode.get());
  }

  /*
   * Resolve the frame into the values to publish. The core decides whether the
   * content size changed and whether it wants to move the scroll view (only then
   * is the offset applied, so we never fight the user's own scrolling).
   */
  auto nextStateData = getStateData();
  auto stateUpdate = this->containerManager_->resolveStateUpdate(
    nextStateData.containerOffsetX_,
    nextStateData.containerOffsetY_,
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);

  SL_LOG("layout: elementChildren=%zu hdr=%.1f ftr=%.1f stateOffset=(%.1f,%.1f) coreOffset=(%.1f,%.1f) total=(%.1f,%.1f) applyOffset=%d changed=%d",
    getChildren().size(), headerSize, footerSize,
    nextStateData.containerOffsetX_, nextStateData.containerOffsetY_,
    stateUpdate.containerOffsetX, stateUpdate.containerOffsetY,
    stateUpdate.totalContainerWidth, stateUpdate.totalContainerHeight,
    stateUpdate.applyContainerOffset ? 1 : 0, stateUpdate.changed ? 1 : 0);

  if (stateUpdate.changed) {
    nextStateData.containerOffsetX_ = stateUpdate.containerOffsetX;
    nextStateData.containerOffsetY_ = stateUpdate.containerOffsetY;
    nextStateData.totalContainerWidth_ = stateUpdate.totalContainerWidth;
    nextStateData.totalContainerHeight_ = stateUpdate.totalContainerHeight;
    nextStateData.containerOffsetEnabled_ = stateUpdate.applyContainerOffset;
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
   * Feed natively measured element sizes back into the virtualizer. Guard the
   * index against the reconciled element count (a stale child can outrun it) and
   * take the core lock since this can run concurrently with the commit phase.
   */
  if (const auto elementViewProps = std::dynamic_pointer_cast<ShadowlistElementViewProps const>(nextElementShadowNode->getProps())) {
    if (this->containerManager_ && this->virtualizerManager_ &&
        elementViewProps->index < this->containerManager_->getElementsSize()) {
      std::lock_guard<std::recursive_mutex> lock(this->containerManager_->coreMutex);

      const auto elementViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode const>(nextElementShadowNode);
      const auto elementViewNodeSize = elementViewNode->getLayoutMetrics().frame.size;

      this->virtualizerManager_->updateElementAtIndex(
        this->containerManager_.get(),
        elementViewProps->index,
        { .width = elementViewNodeSize.width, .height = elementViewNodeSize.height });
    }
  }

  YogaLayoutableShadowNode::replaceChild(prevElementShadowNode, nextElementShadowNode, suggestedIndex);
}

}
