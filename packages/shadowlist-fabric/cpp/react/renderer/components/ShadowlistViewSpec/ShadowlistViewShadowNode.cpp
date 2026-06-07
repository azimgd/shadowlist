#include "ShadowlistViewShadowNode.h"

#include <mutex>
#include <vector>

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
  /*
   * The sticky section-header overlay (SectionList): an always-mounted template
   * whose content is the active section's header. Unlike a normal header it reserves
   * NO list space (it floats over the content), so it never contributes to
   * headerSize / total size or shifts element offsets - it is positioned at the
   * origin and pinned to the viewport entirely natively (see the integrations),
   * exactly like the regular sticky header. This is what keeps section headers
   * smooth: the pinned view is never virtualized, so it has no commit-cycle remount
   * latency.
   */
  std::shared_ptr<const ShadowNode> sectionHeaderNode = nullptr;
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

      if (templateProps->templateType == "sectionHeader") {
        sectionHeaderNode = getChildren()[i];
      } else if (templateProps->templateType == "header" || templateProps->templateType == "empty") {
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
   * Apply the freshly measured header/footer AND the actual window (viewport) size to
   * the core now and re-flow the element offsets in this same pass. update() runs from
   * adopt in the commit phase, before this node has been laid out, so it sees a zero
   * frame on the first render: a multi-column list then sizes every column to
   * windowWidth/columns == 0 (collapsed masonry), and a static, non-sticky header
   * overlaps the first rows. The corrected frame only reaches update() on a LATER
   * commit (e.g. a scroll), and the high-water visible band suppresses the re-render
   * that might trigger one - so a static list stays broken until the user scrolls.
   * Applying the real values here makes the first layout correct on its own.
   */
  const auto& windowFrameSize = getLayoutMetrics().frame.size;
  bool layoutInputsChanged =
    this->containerManager_->headerSize != headerSize ||
    this->containerManager_->footerSize != footerSize ||
    this->containerManager_->revision.windowContainerWidth != windowFrameSize.width ||
    this->containerManager_->revision.windowContainerHeight != windowFrameSize.height;

  if (layoutInputsChanged) {
    this->containerManager_->headerSize = headerSize;
    this->containerManager_->footerSize = footerSize;
    this->containerManager_->revision.windowContainerWidth = windowFrameSize.width;
    this->containerManager_->revision.windowContainerHeight = windowFrameSize.height;
    azimgd::shadowlist::Virtualizer::recomputeElementOffsets(this->containerManager_.get(), 0);

    /*
     * The header/footer/window just changed, which re-flowed every element offset (e.g.
     * the header measured one commit late shifts all rows down by its size). The
     * re-flow alone does not move the scroll view, so without re-asserting the offset
     * the host leaves contentOffset stale and the list appears scrolled past the header
     * (or, with a sticky header pinned at the top, the first rows sit under it). Mark
     * the offset corrected so this commit publishes applyContainerOffset and the host
     * re-asserts the core's resting offset (0 on first open). The MVCP header-size
     * compensation keeps that resting offset correct, so this re-assert is a no-op while
     * the user is scrolled (it re-applies the current offset) and only matters on the
     * header-measurement/resize commits.
     */
    this->containerManager_->containerOffsetCorrected = true;
  }

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
      const auto& element = this->containerManager_->getElementAtIndex(prevElementViewProps->index);

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

    /*
     * The header rests at the content start. A sticky header is pinned to the
     * viewport natively in the scroll callback (see the integrations), not here,
     * because the commit cycle is too slow to track scrolling smoothly.
     */
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
   * The sticky section-header overlay rests at the content origin; the integration
   * pins it to the viewport on the UI thread per scroll frame. It floats over the
   * content (no reserved space), so its position never feeds back into element
   * offsets or the total size.
   */
  if (sectionHeaderNode) {
    auto sectionHeaderViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode>(sectionHeaderNode->clone({}));
    LayoutMetrics sectionHeaderMetrics = sectionHeaderViewNode->getLayoutMetrics();
    sectionHeaderMetrics.frame.origin.x = 0;
    sectionHeaderMetrics.frame.origin.y = 0;
    sectionHeaderViewNode->setLayoutMetrics(sectionHeaderMetrics);
    replaceChild(*sectionHeaderNode, sectionHeaderViewNode);
    layoutContext.affectedNodes->push_back(sectionHeaderViewNode.get());
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

  /*
   * Gather the resting geometry (offset + size along the scroll axis) of each
   * sticky section header so the integration can pin the active one on the UI
   * thread per scroll frame (see ShadowlistViewState / the native pin), mirroring
   * Container::resolveStickyHeader. Only the core knows these offsets, and they
   * change as off-screen rows are measured, so they ride along on the state.
   */
  std::vector<int> stickyHeaderIndices;
  std::vector<Float> stickyHeaderOffsets;
  std::vector<Float> stickyHeaderSizes;
  std::size_t elementsSize = this->containerManager_->getElementsSize();
  /*
   * Inverted sticky section headers (pin to the viewport end) are an exotic
   * combination the core deliberately leaves resting (Container::resolveStickyHeader
   * bails for inverted), so publish no geometry here either - otherwise the native
   * pins, which have no inverted case, would pin the overlay to the wrong edge with
   * ascending (non-inverted) math. Empty geometry hides the overlay everywhere.
   */
  if (!this->containerManager_->inverted) {
    for (std::size_t stickyIndex : this->containerManager_->stickyIndices) {
      if (stickyIndex >= elementsSize) {
        continue;
      }
      stickyHeaderIndices.push_back(static_cast<int>(stickyIndex));
      stickyHeaderOffsets.push_back(static_cast<Float>(this->containerManager_->getElementOffset(stickyIndex)));
      stickyHeaderSizes.push_back(static_cast<Float>(this->containerManager_->getElementSize(stickyIndex)));
    }
  }
  bool stickyChanged =
    stickyHeaderIndices != nextStateData.stickyHeaderIndices_ ||
    stickyHeaderOffsets != nextStateData.stickyHeaderOffsets_ ||
    stickyHeaderSizes != nextStateData.stickyHeaderSizes_;

  /*
   * Resting snap offsets along the scroll axis (empty unless snapToItem is set).
   * Only the core knows element boundaries and they shift as off-screen rows are
   * measured, so they ride along on the state for the UI-thread snap.
   */
  std::vector<Float> snapOffsets;
  {
    auto coreSnapOffsets = this->containerManager_->getSnapOffsets();
    snapOffsets.reserve(coreSnapOffsets.size());
    for (double snapOffset : coreSnapOffsets) {
      snapOffsets.push_back(static_cast<Float>(snapOffset));
    }
  }
  bool snapChanged = snapOffsets != nextStateData.snapOffsets_;

  SL_LOG("layout: elementChildren=%zu hdr=%.1f ftr=%.1f stateOffset=(%.1f,%.1f) coreOffset=(%.1f,%.1f) total=(%.1f,%.1f) applyOffset=%d changed=%d",
    getChildren().size(), headerSize, footerSize,
    nextStateData.containerOffsetX_, nextStateData.containerOffsetY_,
    stateUpdate.containerOffsetX, stateUpdate.containerOffsetY,
    stateUpdate.totalContainerWidth, stateUpdate.totalContainerHeight,
    stateUpdate.applyContainerOffset ? 1 : 0, stateUpdate.changed ? 1 : 0);

  if (stateUpdate.changed || stickyChanged || snapChanged) {
    if (stateUpdate.changed) {
      nextStateData.containerOffsetX_ = stateUpdate.containerOffsetX;
      nextStateData.containerOffsetY_ = stateUpdate.containerOffsetY;
      nextStateData.totalContainerWidth_ = stateUpdate.totalContainerWidth;
      nextStateData.totalContainerHeight_ = stateUpdate.totalContainerHeight;
      nextStateData.containerOffsetEnabled_ = stateUpdate.applyContainerOffset;
    }
    if (stickyChanged) {
      nextStateData.stickyHeaderIndices_ = std::move(stickyHeaderIndices);
      nextStateData.stickyHeaderOffsets_ = std::move(stickyHeaderOffsets);
      nextStateData.stickyHeaderSizes_ = std::move(stickyHeaderSizes);
    }
    if (snapChanged) {
      nextStateData.snapOffsets_ = std::move(snapOffsets);
    }
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
    if (this->containerManager_ && this->virtualizerManager_) {
      std::lock_guard<std::recursive_mutex> lock(this->containerManager_->coreMutex);

      // Read the element count under the lock; a stale child can outrun it.
      if (elementViewProps->index < this->containerManager_->getElementsSize()) {
        const auto elementViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode const>(nextElementShadowNode);
        const auto elementViewNodeSize = elementViewNode->getLayoutMetrics().frame.size;

        this->virtualizerManager_->updateElementAtIndex(
          this->containerManager_.get(),
          elementViewProps->index,
          { .width = elementViewNodeSize.width, .height = elementViewNodeSize.height });
      }
    }
  }

  YogaLayoutableShadowNode::replaceChild(prevElementShadowNode, nextElementShadowNode, suggestedIndex);
}

}
