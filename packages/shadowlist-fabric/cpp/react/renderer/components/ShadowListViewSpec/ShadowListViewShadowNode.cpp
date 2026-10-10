#include "ShadowListViewShadowNode.h"

#include "ShadowListOffsetBand.h"

#include <folly/dynamic.h>
#include <react/renderer/core/ComponentDescriptor.h>
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/RawProps.h>

#include <algorithm>
#include <mutex>
#include <vector>

namespace facebook::react {

namespace {

/*
 * Only hide rows on hosts that echo back the generation of the state they mounted.
 * Android never hides rows because it merges its reports onto the newest state instead.
 */
#ifdef __APPLE__
constexpr bool CONCEAL_UNSETTLED_ROWS = true;
#else
constexpr bool CONCEAL_UNSETTLED_ROWS = false;
#endif

/*
 * Build the row's props with opacity 0. Props can't be copied. Parse them from raw.
 * Returns null without a ContextContainer.
 */
std::shared_ptr<const Props> concealedPropsForRow(const ShadowNode& rowShadowNode) {
  const auto contextContainer = rowShadowNode.getContextContainer();
  if (!contextContainer) {
    return nullptr;
  }
  PropsParserContext propsParserContext{rowShadowNode.getSurfaceId(), *contextContainer};
  return rowShadowNode.getComponentDescriptor().cloneProps(
    propsParserContext,
    rowShadowNode.getProps(),
    RawProps(folly::dynamic::object("opacity", 0)));
}

}

ShadowListViewShadowNode::ShadowListViewShadowNode(
  const ShadowNode& sourceShadowNode,
  const ShadowNodeFragment& fragment) :
  ConcreteViewShadowNode(sourceShadowNode, fragment) {
  // Share the source's core so every clone of a list uses one Container.
  const auto& source = static_cast<const ShadowListViewShadowNode&>(sourceShadowNode);
  containerManager_ = source.containerManager_;
  geometryCache_ = source.geometryCache_;
}

void ShadowListViewShadowNode::setContainerManager(std::shared_ptr<azimgd::shadowlist::Container> containerManager) {
  containerManager_ = containerManager;
}

void ShadowListViewShadowNode::setGeometryCache(std::shared_ptr<ShadowListViewGeometryCache> geometryCache) {
  geometryCache_ = geometryCache;
}

bool ShadowListViewShadowNode::ownsLayoutableChild(const YogaLayoutableShadowNode& child) const {
  /*
   * yogaNode_ is protected and can't be read on another node directly. A member pointer
   * named through this class can, which is the plain C++ way to reach it.
   */
  constexpr auto yogaNodeMember = &ShadowListViewShadowNode::yogaNode_;
  return (child.*yogaNodeMember).getOwner() == &yogaNode_;
}

void ShadowListViewShadowNode::placeChild(
  const std::shared_ptr<const ShadowNode>& child,
  const YogaLayoutableShadowNode& layoutableChild,
  const LayoutMetrics& layoutMetrics,
  const std::shared_ptr<const facebook::react::Props>& nextProps,
  std::size_t childIndex,
  LayoutContext& layoutContext) {
  /*
   * The base layout pass cloned every row it laid out for this commit and wrote its frame
   * in place. Such a row is ours alone. Write the core's origin the same way instead of
   * cloning it a second time. React's reference already moved to it with Yoga's clone.
   * Listing it in affectedNodes again is harmless, onLayout drops repeated frames.
   */
  if (nextProps == nullptr && ownsLayoutableChild(layoutableChild)) {
    const_cast<YogaLayoutableShadowNode&>(layoutableChild).setLayoutMetrics(layoutMetrics);
    if (layoutContext.affectedNodes != nullptr) {
      layoutContext.affectedNodes->push_back(&layoutableChild);
    }
    return;
  }

  /*
   * Opacity isn't a Yoga style. Changing it keeps the row's layout intact.
   * A clone that only moves the row passes React's reference along. A hiding clone must
   * not, or React's next update of the row would start from the hidden props.
   */
  auto nextChild = std::static_pointer_cast<YogaLayoutableShadowNode>(
    child->clone({.props = nextProps, .runtimeShadowNodeReference = nextProps == nullptr}));
  nextChild->setLayoutMetrics(layoutMetrics);
  /*
   * Pass the child index, or replaceChild searches the children for every row.
   * The first pass already took the sizes. Don't report the frame we just wrote.
   * That would fight the column layout in a multi column list.
   * Keep the previous child alive past this commit, see replacedChildren_.
   */
  replacedChildren_.push_back(child);
  suppressElementSizeFeedback_ = true;
  replaceChild(*child, nextChild, childIndex);
  suppressElementSizeFeedback_ = false;
  if (layoutContext.affectedNodes != nullptr) {
    layoutContext.affectedNodes->push_back(nextChild.get());
  }
}

void ShadowListViewShadowNode::layout(LayoutContext layoutContext) {
  ConcreteViewShadowNode::layout(layoutContext);

  if (!containerManager_ || !geometryCache_) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(containerManager_->coreMutex);
  auto& core = *containerManager_;

  // The commit that held raw pointers to these children is long done. Let them go.
  replacedChildren_.clear();

  /*
   * Find and measure the header, footer and empty templates.
   *
   * Warning: affectedNodes belongs to the whole surface for this commit, and onLayout fires
   * for everything in it afterward. Never clear it, or other nodes lose their onLayout.
   * It can also be null. Check it before use like the rest of the framework does.
   */
  bool horizontal = getConcreteProps().horizontal;
  LayoutSlots slots;
  measureChildren(core, horizontal, slots);
  placeElements(core, slots.mountedElements, horizontal, layoutContext);
  placeTemplates(core, slots, horizontal, layoutContext);
  publishLayoutState(core, slots.headerSize, slots.footerSize);

  firstMeasuredTags_.clear();
}

void ShadowListViewShadowNode::measureChildren(azimgd::shadowlist::Container& core, bool horizontal, LayoutSlots& slots) {
  slots.mountedElements.reserve(getChildren().size());
  std::vector<azimgd::shadowlist::MeasuredRow> measuredRows;
  measuredRows.reserve(getChildren().size());

  for (std::size_t childIndex = 0; childIndex < getChildren().size(); ++childIndex) {
    const auto& child = getChildren()[childIndex];
    const facebook::react::Props* childProps = child->getProps().get();
    if (const auto elementViewProps = dynamic_cast<const ShadowListElementViewProps*>(childProps)) {
      /*
       * Look the row up by key, since a child committed before a prepend, insert or reorder
       * has a stale index. Skip it if the key is gone. Use the index only when there is no key.
       */
      std::size_t elementIndex = elementViewProps->elementKey.empty()
        ? static_cast<std::size_t>(elementViewProps->index)
        : core.indexOfKey(elementViewProps->elementKey);
      const auto elementViewNode = dynamic_cast<const YogaLayoutableShadowNode*>(child.get());
      if (elementViewNode != nullptr && elementIndex < core.getRowCount()) {
        slots.mountedElements.push_back({childIndex, elementIndex, elementViewNode});
        const auto& measuredSize = elementViewNode->getLayoutMetrics().frame.size;
        measuredRows.push_back({elementIndex, measuredSize.width, measuredSize.height,
          static_cast<std::uint64_t>(elementViewNode->getTag())});
      }
      continue;
    }

    if (const auto templateProps = dynamic_cast<const ShadowListTemplateViewProps*>(childProps)) {
      const auto templateViewNode = dynamic_cast<const YogaLayoutableShadowNode*>(child.get());
      if (templateViewNode == nullptr) {
        continue;
      }
      const auto& templateFrameSize = templateViewNode->getLayoutMetrics().frame.size;
      const auto templateViewNodeSize = horizontal ? templateFrameSize.width : templateFrameSize.height;
      if (templateProps->templateType == "sectionHeader") {
        slots.sectionHeaderSlot = {templateViewNode, childIndex};
      } else if (templateProps->templateType == "header") {
        slots.headerSlot = {templateViewNode, childIndex};
        slots.headerSize = templateViewNodeSize;
      } else if (templateProps->templateType == "empty") {
        slots.emptySlot = {templateViewNode, childIndex};
        slots.emptySize = templateViewNodeSize;
      } else if (templateProps->templateType == "footer") {
        slots.footerSlot = {templateViewNode, childIndex};
        slots.footerSize = templateViewNodeSize;
      }
    }
  }

  // Header, footer and window into the core, then every mounted row's size in one reflow.
  const auto& windowFrameSize = getLayoutMetrics().frame.size;
  /*
   * The empty template sits after the header and only mounts without rows. The core counts it
   * with the header so the footer goes below it and the content size holds it.
   */
  azimgd::shadowlist::applyLayoutInputs(
    core, slots.headerSize + slots.emptySize, slots.footerSize, windowFrameSize.width, windowFrameSize.height);
  std::vector<std::uint64_t> firstMeasured;
  azimgd::shadowlist::applyMeasuredRows(core, measuredRows, horizontal, firstMeasured);
  for (auto tag : firstMeasured) {
    firstMeasuredTags_.push_back(static_cast<Tag>(tag));
  }
}

void ShadowListViewShadowNode::placeElements(
  azimgd::shadowlist::Container& core,
  const std::vector<MountedElement>& mountedElements,
  bool horizontal,
  LayoutContext& layoutContext) {
  auto& geometry = *geometryCache_;
  auto& concealed = geometry.concealedRows;

  /*
   * Row hiding, see azimgd::shadowlist::ConcealTracker. The correction this pass publishes is
   * final here, resolveStateUpdate below only reads it.
   */
  const auto& inputStateData = getStateData();
  bool correcting = core.offsetCorrected;
  auto concealAck = static_cast<std::uint64_t>(inputStateData.concealGenerationAck_);
  std::size_t concealBeforeIndex = CONCEAL_UNSETTLED_ROWS
    ? azimgd::shadowlist::ConcealTracker<ShadowListViewGeometryCache::ConcealedProps>::hideBeforeIndex(
        core, correcting, !firstMeasuredTags_.empty())
    : 0;
  // Sorted. Each row below checks it with a binary search instead of a scan.
  if (concealBeforeIndex > 0) {
    std::sort(firstMeasuredTags_.begin(), firstMeasuredTags_.end());
  }
  concealed.beginPass();
  std::vector<std::uint64_t> stillConcealedTags;

  /*
   * Place each row where the core says. Only touch the rows that moved, the others still
   * have the right frame. Cloning every row each scroll frame would waste a lot of
   * work, see placeChild.
   */
  for (const auto& mounted : mountedElements) {
    const auto& previousChild = getChildren()[mounted.childIndex];
    const auto& previousLayoutableChild = *mounted.node;

    /*
     * The props this row should get, or null to keep its own. A hidden row shows again once
     * the host echoes its generation with no correction pending, or after too many passes.
     * Until then, new props or original props from React get hidden again too.
     */
    const Tag tag = previousChild->getTag();
    std::shared_ptr<const facebook::react::Props> nextProps = nullptr;
    if (auto* row = concealed.find(static_cast<std::uint64_t>(tag))) {
      auto& props = row->payload;
      bool settled = concealed.settle(*row, concealAck, correcting);
      if (!settled && previousChild->getProps() != props.sourceProps && previousChild->getProps() != props.concealedProps) {
        props.sourceProps = previousChild->getProps();
        props.concealedProps = concealedPropsForRow(*previousChild);
        settled = props.concealedProps == nullptr;
      }
      if (settled) {
        SL_LOG("  reveal: tag=%d gen=%llu ack=%llu passes=%zu correcting=%d",
          tag, static_cast<unsigned long long>(row->generation), static_cast<unsigned long long>(concealAck),
          row->layoutPasses, correcting ? 1 : 0);
        if (props.concealedProps != nullptr && previousChild->getProps() == props.concealedProps) {
          nextProps = props.sourceProps;
        }
        concealed.show(static_cast<std::uint64_t>(tag));
      } else {
        stillConcealedTags.push_back(static_cast<std::uint64_t>(tag));
        if (previousChild->getProps() != props.concealedProps) {
          nextProps = props.concealedProps;
        }
      }
    } else if (mounted.elementIndex < concealBeforeIndex &&
               std::binary_search(firstMeasuredTags_.begin(), firstMeasuredTags_.end(), tag)) {
      auto concealedProps = concealedPropsForRow(*previousChild);
      if (concealedProps != nullptr) {
        [[maybe_unused]] auto generation = concealed.hide(static_cast<std::uint64_t>(tag), {previousChild->getProps(), concealedProps});
        SL_LOG("  conceal: tag=%d index=%zu anchorIndex=%zu gen=%llu",
          tag, mounted.elementIndex, concealBeforeIndex, static_cast<unsigned long long>(generation));
        stillConcealedTags.push_back(static_cast<std::uint64_t>(tag));
        nextProps = std::move(concealedProps);
      }
    }

    LayoutMetrics layoutMetrics = previousLayoutableChild.getLayoutMetrics();
    const LayoutMetrics previousLayoutMetrics = layoutMetrics;
    auto frame = azimgd::shadowlist::rowFrame(core, mounted.elementIndex, horizontal);
    layoutMetrics.frame.origin.x = frame.x;
    layoutMetrics.frame.origin.y = frame.y;
    if (frame.setsWidth) {
      layoutMetrics.frame.size.width = frame.width;
    }

    if (layoutMetrics == previousLayoutMetrics && nextProps == nullptr) {
      continue;
    }

    placeChild(previousChild, previousLayoutableChild, layoutMetrics, nextProps, mounted.childIndex, layoutContext);
  }

  // Forget hidden rows that are no longer mounted.
  concealed.forgetExcept(stillConcealedTags);
}

void ShadowListViewShadowNode::placeTemplates(
  azimgd::shadowlist::Container& core,
  const LayoutSlots& slots,
  bool horizontal,
  LayoutContext& layoutContext) {
  // Move a template along the scroll axis, only if it actually moved, same as the rows.
  auto templates = azimgd::shadowlist::templateOffsets(core, slots.headerSize, slots.footerSize);
  auto placeTemplate = [&](const TemplateSlot& slot, double offset) {
    if (slot.node == nullptr) {
      return;
    }
    /*
     * Float is float on Android and double on Apple. The cast avoids a narrowing error that
     * only one platform reports.
     */
    auto along = static_cast<Float>(offset);
    Point origin = horizontal ? Point{along, 0} : Point{0, along};
    LayoutMetrics templateMetrics = slot.node->getLayoutMetrics();
    if (templateMetrics.frame.origin == origin) {
      return;
    }
    templateMetrics.frame.origin = origin;
    placeChild(getChildren()[slot.childIndex], *slot.node, templateMetrics, nullptr, slot.childIndex, layoutContext);
  };
  /*
   * A sticky header is pinned natively while scrolling, because commits are too slow to
   * follow the scroll smoothly. The same goes for the section header overlay.
   */
  placeTemplate(slots.headerSlot, templates.header);
  placeTemplate(slots.emptySlot, templates.empty);
  placeTemplate(slots.footerSlot, templates.footer);
  placeTemplate(slots.sectionHeaderSlot, templates.sectionHeader);
}

void ShadowListViewShadowNode::publishLayoutState(azimgd::shadowlist::Container& core, double headerSize, double footerSize) {
  auto& geometry = *geometryCache_;
  auto& concealed = geometry.concealedRows;

  /*
   * Work out what to publish. The core decides if the content size changed and if it wants
   * to move the scroll view. The offset is only written then. We never fight the user.
   */
  auto nextStateData = getStateData();
  auto stateUpdate = core.resolveStateUpdate(
    nextStateData.containerOffsetX_,
    nextStateData.containerOffsetY_,
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);

  // Sticky header and snap positions, rebuilt only when the row geometry moved.
  auto& published = geometry.published;
  published.refresh(core);

  // Compare pointers. This stays cheap because the cache only takes a new one when the values changed.
  bool stickyChanged =
    published.stickyIndices != nextStateData.stickyHeaderIndices_ ||
    published.stickyOffsets != nextStateData.stickyHeaderOffsets_ ||
    published.stickySizes != nextStateData.stickyHeaderSizes_;
  bool snapChanged = published.snapOffsets != nextStateData.snapOffsets_;

  double concealGeneration = concealed.getPublishedGeneration();
  bool concealChanged = nextStateData.concealGeneration_ != concealGeneration;

  // The offsets the host can scroll through without a state update, see ShadowListOffsetBand.h.
  auto offsetBand = shadowListOffsetBand(*this);
  bool bandChanged = !shadowListOffsetBandPublished(nextStateData, offsetBand);

  SL_LOG("layout: elementChildren=%zu hdr=%.1f ftr=%.1f stateOffset=(%.1f,%.1f) coreOffset=(%.1f,%.1f) total=(%.1f,%.1f) applyOffset=%d changed=%d",
    getChildren().size(), headerSize, footerSize,
    nextStateData.containerOffsetX_, nextStateData.containerOffsetY_,
    stateUpdate.offsetX, stateUpdate.offsetY,
    stateUpdate.contentWidth, stateUpdate.contentHeight,
    stateUpdate.applyOffset ? 1 : 0, stateUpdate.changed ? 1 : 0);

  // A new animated command's estimate, see ShadowListViewGeometryCache::animationSequence.
  bool animationChanged = nextStateData.animationTargetSequence_ != geometry.animationSequence;

  if (stateUpdate.changed || stickyChanged || snapChanged || concealChanged || bandChanged || animationChanged) {
    auto scrollState = nextStateData.scrollState();
    if (azimgd::shadowlist::publishStateUpdate(scrollState, stateUpdate)) {
      nextStateData.setScrollState(scrollState);
    }
    /*
     * Copy, don't move, since the next layout reuses the cache. It's just a shared pointer
     * and the list never changes once published. Sharing it is safe.
     */
    if (stickyChanged) {
      nextStateData.stickyHeaderIndices_ = published.stickyIndices;
      nextStateData.stickyHeaderOffsets_ = published.stickyOffsets;
      nextStateData.stickyHeaderSizes_ = published.stickySizes;
    }
    if (snapChanged) {
      nextStateData.snapOffsets_ = published.snapOffsets;
    }
    nextStateData.concealGeneration_ = concealGeneration;
    nextStateData.offsetBandLow_ = offsetBand.low;
    nextStateData.offsetBandHigh_ = offsetBand.high;
    nextStateData.animationTargetSequence_ = geometry.animationSequence;
    nextStateData.animationTargetOffset_ = geometry.animationOffset;
    setStateData(std::move(nextStateData));
  }

  /*
   * Tell JS when the content size changes, header and footer included, like ScrollView's
   * onContentSizeChange. Once per size for the whole list, not per clone, and only while JS
   * listens.
   */
  double contentWidth = stateUpdate.contentWidth;
  double contentHeight = stateUpdate.contentHeight;
  if (getConcreteProps().contentSizeEventEnabled &&
      (contentWidth != geometry.emittedContentWidth || contentHeight != geometry.emittedContentHeight)) {
    geometry.emittedContentWidth = contentWidth;
    geometry.emittedContentHeight = contentHeight;
    ShadowListViewEventEmitter::OnContentSizeChange event;
    event.width = contentWidth;
    event.height = contentHeight;
    getConcreteEventEmitter().onContentSizeChange(event);
  }
}

void ShadowListViewShadowNode::replaceChild(
  const ShadowNode& previousElementShadowNode,
  const std::shared_ptr<const ShadowNode>& nextElementShadowNode,
  std::size_t suggestedIndex) {
  /*
   * Send measured row sizes to the core. Look rows up by key, or a child behind a prepend
   * would write its size onto whatever row now has its previous index. Take the core lock,
   * since this can run at the same time as the commit phase.
   */
  if (suppressElementSizeFeedback_) {
    // layout() already gave the core these sizes. Skip the frame it just wrote.
  } else if (const auto elementViewProps = dynamic_cast<const ShadowListElementViewProps*>(nextElementShadowNode->getProps().get())) {
    if (containerManager_) {
      std::lock_guard<std::recursive_mutex> lock(containerManager_->coreMutex);

      // Look up the index under the lock, since a stale child can arrive before the data catches up.
      std::size_t elementIndex = elementViewProps->elementKey.empty()
        ? static_cast<std::size_t>(elementViewProps->index)
        : containerManager_->indexOfKey(elementViewProps->elementKey);
      const auto elementViewNode = dynamic_cast<const YogaLayoutableShadowNode*>(nextElementShadowNode.get());
      const auto elementViewNodeSize = elementViewNode
        ? elementViewNode->getLayoutMetrics().frame.size
        : Size{};
      /*
       * A zero size means Yoga hasn't laid the row out yet. Recording 0 would collapse the
       * row and the content under the reader would jump by its size. Let the layout pass
       * measure it from the real frame instead.
       */
      bool laidOut = (containerManager_->horizontal ? elementViewNodeSize.width : elementViewNodeSize.height) > 0.0;
      if (elementIndex < containerManager_->getRowCount() && elementViewNode && laidOut) {
        bool firstMeasurement = !containerManager_->getRowAtIndex(elementIndex).measured;

        azimgd::shadowlist::Virtualizer::updateRowAtIndex(
          *containerManager_,
          elementIndex,
          {.width = elementViewNodeSize.width, .height = elementViewNodeSize.height});

        if (firstMeasurement) {
          firstMeasuredTags_.push_back(nextElementShadowNode->getTag());
        }
      }
    }
  }

  YogaLayoutableShadowNode::replaceChild(previousElementShadowNode, nextElementShadowNode, suggestedIndex);
}

}
