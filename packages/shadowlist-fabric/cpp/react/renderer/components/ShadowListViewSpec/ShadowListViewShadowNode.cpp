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
 * Android merges its reports onto the newest state instead, so it never hides rows.
 */
#ifdef __APPLE__
constexpr bool CONCEAL_UNSETTLED_ROWS = true;
#else
constexpr bool CONCEAL_UNSETTLED_ROWS = false;
#endif

// Show a hidden row anyway after this many layout passes, in case corrections never settle.
constexpr std::size_t MAX_CONCEALED_LAYOUT_PASSES = 8;

/*
 * Build the row's props with opacity 0. Props can't be copied, so parse them from raw.
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
  this->containerManager_ = source.containerManager_;
  this->geometryCache_ = source.geometryCache_;
  this->nativeEngine_ = source.nativeEngine_;
  this->nativeKeys_ = source.nativeKeys_;
}

void ShadowListViewShadowNode::setContainerManager(std::shared_ptr<azimgd::shadowlist::Container> containerManager) {
  this->containerManager_ = containerManager;
}

void ShadowListViewShadowNode::setGeometryCache(std::shared_ptr<ShadowListViewGeometryCache> geometryCache) {
  this->geometryCache_ = geometryCache;
}

bool ShadowListViewShadowNode::ownsLayoutableChild(const YogaLayoutableShadowNode& child) const {
  /*
   * yogaNode_ is protected, so it can't be read on another node directly. A member pointer
   * named through this class can, which is the plain C++ way to reach it.
   */
  constexpr auto yogaNodeMember = &ShadowListViewShadowNode::yogaNode_;
  return (child.*yogaNodeMember).getOwner() == &this->yogaNode_;
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
   * in place. Such a row is ours alone, so write the core's origin the same way instead of
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
   * Opacity isn't a Yoga style, so changing it keeps the row's layout intact.
   * A clone that only moves the row passes React's reference along. A hiding clone must
   * not, or React's next update of the row would start from the hidden props.
   */
  auto nextChild = std::static_pointer_cast<YogaLayoutableShadowNode>(
    child->clone({.props = nextProps, .runtimeShadowNodeReference = nextProps == nullptr}));
  nextChild->setLayoutMetrics(layoutMetrics);
  /*
   * Pass the child index, or replaceChild searches the children for every row.
   * The first pass already took the sizes, so don't report the frame we just wrote.
   * That would fight the column layout in a multi column list.
   * Keep the old child alive past this commit, see replacedChildren_.
   */
  this->replacedChildren_.push_back(child);
  this->suppressElementSizeFeedback_ = true;
  replaceChild(*child, nextChild, childIndex);
  this->suppressElementSizeFeedback_ = false;
  if (layoutContext.affectedNodes != nullptr) {
    layoutContext.affectedNodes->push_back(nextChild.get());
  }
}

void ShadowListViewShadowNode::layout(LayoutContext layoutContext) {
  ConcreteViewShadowNode::layout(layoutContext);

  if (!this->containerManager_ || !this->geometryCache_) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(this->containerManager_->coreMutex);

  // The commit that held raw pointers to these children is long done, so let them go.
  this->replacedChildren_.clear();

  /*
   * Find and measure the header, footer and empty templates.
   *
   * Warning: affectedNodes belongs to the whole surface for this commit, and onLayout fires
   * for everything in it afterward. Never clear it, or other nodes lose their onLayout.
   * It can also be null, so check it before use like the rest of the framework does.
   */

  /*
   * A template child and where it sits in the children, or no node when it isn't mounted.
   * Raw pointers are fine, the children keep them alive for the whole pass.
   */
  struct TemplateSlot {
    const YogaLayoutableShadowNode* node = nullptr;
    std::size_t childIndex = 0;
  };
  TemplateSlot headerSlot;
  TemplateSlot footerSlot;
  /*
   * The SectionList sticky header overlay, an always mounted template showing the
   * current section's header. It floats over the content and takes no list space, so it
   * never changes sizes or row offsets. It sits at the origin and the platform pins it.
   * Since it is never virtualized, it never waits on a remount, which keeps it smooth.
   */
  TemplateSlot sectionHeaderSlot;
  /*
   * Separate from headerSlot because an empty list mounts both the header and the
   * empty template, and sharing one slot would overwrite the real header.
   */
  TemplateSlot emptySlot;
  double headerSize = 0.0;
  double footerSize = 0.0;
  bool horizontal = getConcreteProps().horizontal;

  /*
   * Sort the children once. Rows are stored with their current core index and their
   * layoutable node so the two passes below don't repeat the casts or the key lookup.
   */
  struct MountedElement {
    std::size_t childIndex;
    std::size_t elementIndex;
    const YogaLayoutableShadowNode* node;
  };
  std::vector<MountedElement> mountedElements;
  mountedElements.reserve(getChildren().size());

  for (std::size_t childIndex = 0; childIndex < getChildren().size(); ++childIndex) {
    const auto& child = getChildren()[childIndex];
    const facebook::react::Props* childProps = child->getProps().get();
    if (const auto elementViewProps = dynamic_cast<const ShadowListElementViewProps*>(childProps)) {
      /*
       * Look the row up by key, since a child committed before a prepend, insert or
       * reorder has an old index. Skip it if the key is gone. Use the index only when
       * there is no key.
       */
      std::size_t elementIndex = elementViewProps->elementKey.empty()
        ? static_cast<std::size_t>(elementViewProps->index)
        : this->containerManager_->findElementIndexByKey(elementViewProps->elementKey);
      const auto elementViewNode = dynamic_cast<const YogaLayoutableShadowNode*>(child.get());
      if (elementViewNode != nullptr && elementIndex < this->containerManager_->getElementsSize()) {
        mountedElements.push_back({childIndex, elementIndex, elementViewNode});
      }
      continue;
    }

    if (const auto templateProps = dynamic_cast<const ShadowListTemplateViewProps*>(childProps)) {
      const auto templateViewNode = dynamic_cast<const YogaLayoutableShadowNode*>(child.get());
      if (templateViewNode == nullptr) {
        continue;
      }

      const auto& templateViewNodeLayoutMetrics = templateViewNode->getLayoutMetrics();
      const auto templateViewNodeSize = horizontal
        ? templateViewNodeLayoutMetrics.frame.size.width
        : templateViewNodeLayoutMetrics.frame.size.height;

      if (templateProps->templateType == "sectionHeader") {
        sectionHeaderSlot = {templateViewNode, childIndex};
      } else if (templateProps->templateType == "header") {
        headerSlot = {templateViewNode, childIndex};
        headerSize = templateViewNodeSize;
      } else if (templateProps->templateType == "empty") {
        emptySlot = {templateViewNode, childIndex};
      } else if (templateProps->templateType == "footer") {
        footerSlot = {templateViewNode, childIndex};
        footerSize = templateViewNodeSize;
      }
    }
  }

  /*
   * Give the core the measured header, footer and window size now and reflow the rows.
   * update() runs before this node is laid out, so on the first render it sees a zero
   * frame. Masonry columns then collapse to zero width and a plain header covers the
   * first rows, and nothing fixes it until the user scrolls. Doing it here makes the
   * first layout right. The next update() reads these values back from the core.
   */
  const auto& windowFrameSize = getLayoutMetrics().frame.size;
  bool layoutInputsChanged =
    this->containerManager_->headerSize != headerSize ||
    this->containerManager_->footerSize != footerSize ||
    this->containerManager_->revision.windowContainerWidth != windowFrameSize.width ||
    this->containerManager_->revision.windowContainerHeight != windowFrameSize.height;

  if (layoutInputsChanged) {
    double previousHeaderSize = this->containerManager_->headerSize;
    double previousWindowSize = this->containerManager_->getWindowContainerSize();
    /*
     * Row offsets only depend on the header and, for columns, on the window's cross size.
     * A window growing or shrinking along the scroll axis, like a chat composer resizing
     * the list, or a footer change moves no row, so skip walking every row for those.
     */
    bool rowsMove = previousHeaderSize != headerSize ||
      (this->containerManager_->horizontal
        ? this->containerManager_->revision.windowContainerHeight != windowFrameSize.height
        : this->containerManager_->revision.windowContainerWidth != windowFrameSize.width);
    this->containerManager_->headerSize = headerSize;
    this->containerManager_->footerSize = footerSize;
    this->containerManager_->revision.windowContainerWidth = windowFrameSize.width;
    this->containerManager_->revision.windowContainerHeight = windowFrameSize.height;
    if (rowsMove) {
      azimgd::shadowlist::Virtualizer::recomputeElementOffsets(this->containerManager_.get(), 0);
    }
    /*
     * update() ran with the old header size and doesn't know the rows moved. Settle the
     * header change now, or the rows jump for one frame, like when a header spinner
     * toggles during a prepend.
     */
    azimgd::shadowlist::Virtualizer::applyHeaderSizeChange(this->containerManager_.get(), previousHeaderSize);
    // A chat resting at its bottom keeps it as the composer resizes the list.
    azimgd::shadowlist::Virtualizer::applyWindowSizeChange(this->containerManager_.get(), previousWindowSize);

    /*
     * Every row just moved, but the scroll view didn't. Without writing the offset again
     * the list looks scrolled past the header, or the first rows sit under a sticky one.
     * Mark it corrected so the host writes the core's offset, 0 on first open. While the
     * user is scrolled this just writes the current offset again.
     */
    this->containerManager_->containerOffsetCorrected = true;
  }

  /*
   * First pass: give the core every mounted row's new size before reading any position.
   * applyElementSize only records the size, then one commitElementSizes reflows from the
   * lowest changed row, so there is one reflow per layout instead of one per row.
   */
  std::size_t lowestChangedIndex = azimgd::shadowlist::UNDEFINED_INDEX;
  for (const auto& mounted : mountedElements) {
    const auto& measuredSize = mounted.node->getLayoutMetrics().frame.size;
    azimgd::shadowlist::Size feedSize{measuredSize.width, measuredSize.height};

    if (this->containerManager_->columns > 1) {
      /*
       * In a multi column list the column sets the row's cross size, not the measurement.
       * Keep the core's value for that axis and only take the scroll axis size.
       */
      const auto& element = this->containerManager_->getElementAtIndex(mounted.elementIndex);
      if (horizontal) {
        feedSize.height = element.height;
      } else {
        feedSize.width = element.width;
      }
    }

    bool firstMeasurement = !this->containerManager_->getElementAtIndex(mounted.elementIndex).measured;
    bool changed = azimgd::shadowlist::Virtualizer::applyElementSize(
      this->containerManager_.get(), mounted.elementIndex, feedSize);
    if (firstMeasurement) {
      this->firstMeasuredTags_.push_back(mounted.node->getTag());
    }

    if (changed && mounted.elementIndex < lowestChangedIndex) {
      lowestChangedIndex = mounted.elementIndex;
    }
  }
  if (lowestChangedIndex != azimgd::shadowlist::UNDEFINED_INDEX) {
    azimgd::shadowlist::Virtualizer::commitElementSizes(this->containerManager_.get(), lowestChangedIndex);
  }

  // Update the total size once for the whole batch. The footer and content size need it.
  azimgd::shadowlist::Virtualizer::recomputeTotalSize(this->containerManager_.get());

  auto& geometry = *this->geometryCache_;

  /*
   * Row hiding, see ShadowListViewGeometryCache::concealedRows. The correction this pass
   * publishes is final here, resolveStateUpdate below only reads it.
   */
  const auto& inputStateData = getStateData();
  bool correcting = this->containerManager_->containerOffsetCorrected;
  auto concealAck = static_cast<std::uint64_t>(inputStateData.concealGenerationAck_);

  /*
   * A correction keeps its anchor row still only once the host mounts it. Hide the newly
   * measured rows above the anchor, since they are what moved it. A fixed offset
   * correction, like pinning to the bottom, has no anchor and hides nothing.
   */
  std::size_t concealBeforeIndex = 0;
  if (CONCEAL_UNSETTLED_ROWS && correcting && !this->firstMeasuredTags_.empty()) {
    const auto* compensationAnchor = this->containerManager_->compensationAnchor();
    if (compensationAnchor != nullptr && !compensationAnchor->key.empty()) {
      std::size_t anchorIndex = this->containerManager_->findElementIndexByKey(compensationAnchor->key);
      if (anchorIndex != azimgd::shadowlist::UNDEFINED_INDEX) {
        concealBeforeIndex = anchorIndex;
      }
    }
  }
  // Sorted, so each row below checks it with a binary search instead of a scan.
  if (concealBeforeIndex > 0) {
    std::sort(this->firstMeasuredTags_.begin(), this->firstMeasuredTags_.end());
  }
  std::uint64_t nextConcealGeneration = geometry.concealGeneration + 1;
  std::vector<Tag> stillConcealedTags;

  /*
   * Second pass: place each row where the core says. Offsets already include the header.
   * A row that didn't move still has the right frame, so only touch the rows that moved.
   * Cloning every row each scroll frame was a lot of wasted work, see placeChild.
   */
  for (const auto& mounted : mountedElements) {
    const auto& previousChild = getChildren()[mounted.childIndex];
    const auto& previousLayoutableChild = *mounted.node;

    /*
     * The props this row should get, or null to keep its own. A hidden row shows again
     * once the host echoes its generation with no correction pending, or after too many
     * passes. Until then, new props or original props from React get hidden again too.
     */
    const Tag tag = previousChild->getTag();
    std::shared_ptr<const facebook::react::Props> nextProps = nullptr;
    auto concealedRow = geometry.concealedRows.find(tag);
    if (concealedRow != geometry.concealedRows.end()) {
      auto& row = concealedRow->second;
      ++row.layoutPasses;
      bool settled = (concealAck >= row.generation && !correcting) || row.layoutPasses > MAX_CONCEALED_LAYOUT_PASSES;
      if (!settled && previousChild->getProps() != row.sourceProps && previousChild->getProps() != row.concealedProps) {
        row.sourceProps = previousChild->getProps();
        row.concealedProps = concealedPropsForRow(*previousChild);
        settled = row.concealedProps == nullptr;
      }
      if (settled) {
        SL_LOG("  reveal: tag=%d gen=%llu ack=%llu passes=%zu correcting=%d",
          tag, static_cast<unsigned long long>(row.generation), static_cast<unsigned long long>(concealAck),
          row.layoutPasses, correcting ? 1 : 0);
        if (row.concealedProps != nullptr && previousChild->getProps() == row.concealedProps) {
          nextProps = row.sourceProps;
        }
        geometry.concealedRows.erase(concealedRow);
      } else {
        stillConcealedTags.push_back(tag);
        if (previousChild->getProps() != row.concealedProps) {
          nextProps = row.concealedProps;
        }
      }
    } else if (mounted.elementIndex < concealBeforeIndex &&
               std::binary_search(this->firstMeasuredTags_.begin(), this->firstMeasuredTags_.end(), tag)) {
      auto concealedProps = concealedPropsForRow(*previousChild);
      if (concealedProps != nullptr) {
        SL_LOG("  conceal: tag=%d index=%zu anchorIndex=%zu gen=%llu",
          tag, mounted.elementIndex, concealBeforeIndex, static_cast<unsigned long long>(nextConcealGeneration));
        geometry.concealGeneration = nextConcealGeneration;
        geometry.concealedRows.insert_or_assign(
          tag, ShadowListViewGeometryCache::ConcealedRow{previousChild->getProps(), concealedProps, nextConcealGeneration, 0});
        stillConcealedTags.push_back(tag);
        nextProps = std::move(concealedProps);
      }
    }

    LayoutMetrics layoutMetrics = previousLayoutableChild.getLayoutMetrics();
    const LayoutMetrics previousLayoutMetrics = layoutMetrics;
    const auto& element = this->containerManager_->getElementAtIndex(mounted.elementIndex);

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

    if (layoutMetrics == previousLayoutMetrics && nextProps == nullptr) {
      continue;
    }

    placeChild(previousChild, previousLayoutableChild, layoutMetrics, nextProps, mounted.childIndex, layoutContext);
  }

  // Forget hidden rows that are no longer mounted.
  if (geometry.concealedRows.size() > stillConcealedTags.size()) {
    std::sort(stillConcealedTags.begin(), stillConcealedTags.end());
    for (auto iterator = geometry.concealedRows.begin(); iterator != geometry.concealedRows.end();) {
      bool stillConcealed = std::binary_search(stillConcealedTags.begin(), stillConcealedTags.end(), iterator->first);
      iterator = stillConcealed ? std::next(iterator) : geometry.concealedRows.erase(iterator);
    }
  }

  // Move a template to origin, only if it actually moved, same as the rows.
  auto placeTemplate = [&](const TemplateSlot& slot, Point origin) {
    LayoutMetrics templateMetrics = slot.node->getLayoutMetrics();
    if (templateMetrics.frame.origin == origin) {
      return;
    }
    templateMetrics.frame.origin = origin;
    placeChild(getChildren()[slot.childIndex], *slot.node, templateMetrics, nullptr, slot.childIndex, layoutContext);
  };

  /*
   * The header sits at the start. A sticky header is pinned natively while scrolling,
   * because commits are too slow to follow the scroll smoothly.
   */
  if (headerSlot.node != nullptr) {
    placeTemplate(headerSlot, {0, 0});
  }

  // The empty template sits right after the header, where the rows would start.
  if (emptySlot.node != nullptr) {
    /*
     * Float is float on Android and double on Apple, so cast to avoid a narrowing
     * error that only one platform reports.
     */
    auto emptyOffset = static_cast<Float>(headerSize);
    placeTemplate(emptySlot, horizontal ? Point{emptyOffset, 0} : Point{0, emptyOffset});
  }

  if (footerSlot.node != nullptr) {
    auto footerOffset = static_cast<Float>(this->containerManager_->getFooterOffset(footerSize));
    placeTemplate(footerSlot, horizontal ? Point{footerOffset, 0} : Point{0, footerOffset});
  }

  /*
   * The section header overlay sits at the origin and the platform pins it while
   * scrolling. It floats over the content, so it never changes row offsets or sizes.
   */
  if (sectionHeaderSlot.node != nullptr) {
    placeTemplate(sectionHeaderSlot, {0, 0});
  }

  /*
   * Work out what to publish. The core decides if the content size changed and if it
   * wants to move the scroll view. The offset is only written then, so we never fight
   * the user's own scrolling.
   */
  auto nextStateData = getStateData();
  auto stateUpdate = this->containerManager_->resolveStateUpdate(
    nextStateData.containerOffsetX_,
    nextStateData.containerOffsetY_,
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);

  /*
   * Collect each sticky header's offset and size so the platform can pin the current
   * one while scrolling. Only the core knows them, so they go out on the state.
   *
   * This runs every scroll frame and walks every row, so only rebuild when the row
   * geometry actually moved. None of it depends on the scroll offset.
   */
  double windowSize = this->containerManager_->getWindowContainerSize();
  double totalSize = this->containerManager_->horizontal
    ? this->containerManager_->revision.totalContainerWidth
    : this->containerManager_->revision.totalContainerHeight;

  bool geometryStale =
    geometry.geometryVersion != this->containerManager_->geometryVersion ||
    geometry.snapToItem != this->containerManager_->snapToItem ||
    geometry.snapAlignment != this->containerManager_->snapAlignment ||
    geometry.inverted != this->containerManager_->inverted ||
    geometry.horizontal != this->containerManager_->horizontal ||
    geometry.windowSize != windowSize ||
    geometry.totalSize != totalSize ||
    geometry.sourceStickyIndices != this->containerManager_->stickyIndices;

  if (geometryStale) {
    geometry.geometryVersion = this->containerManager_->geometryVersion;
    geometry.snapToItem = this->containerManager_->snapToItem;
    geometry.snapAlignment = this->containerManager_->snapAlignment;
    geometry.inverted = this->containerManager_->inverted;
    geometry.horizontal = this->containerManager_->horizontal;
    geometry.windowSize = windowSize;
    geometry.totalSize = totalSize;
    geometry.sourceStickyIndices = this->containerManager_->stickyIndices;

    /*
     * Build plain vectors, then adoptIfChanged keeps the old pointer when nothing changed.
     * Measuring new rows while scrolling bumps the version every frame, but the results are
     * usually the same, and reusing the pointer avoids a pointless state update.
     */
    std::vector<int> stickyHeaderIndices;
    std::vector<Float> stickyHeaderOffsets;
    std::vector<Float> stickyHeaderSizes;

    auto adoptIfChanged = [](auto& cached, auto&& next) {
      using ValueT = typename std::decay_t<decltype(next)>::value_type;
      if (next.empty()) {
        // Null means empty, and needs no allocation.
        cached = nullptr;
        return;
      }
      if (cached && *cached == next) {
        return;
      }
      cached = std::make_shared<const std::vector<ValueT>>(std::move(next));
    };

    std::size_t elementsSize = this->containerManager_->getElementsSize();
    /*
     * Sticky headers in an inverted list aren't supported. The native pinning has no
     * inverted case and would pin to the wrong edge, so publish nothing, which hides the overlay.
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

    adoptIfChanged(geometry.stickyHeaderIndices, std::move(stickyHeaderIndices));
    adoptIfChanged(geometry.stickyHeaderOffsets, std::move(stickyHeaderOffsets));
    adoptIfChanged(geometry.stickyHeaderSizes, std::move(stickyHeaderSizes));

    /*
     * Snap points, empty unless snapToItem is set. Only the core knows where rows start,
     * so they go out on the state for the platform to snap to.
     */
    const auto& coreSnapOffsets = this->containerManager_->getSnapOffsets();
    std::vector<Float> snapOffsets;
    snapOffsets.reserve(coreSnapOffsets.size());
    for (double snapOffset : coreSnapOffsets) {
      snapOffsets.push_back(static_cast<Float>(snapOffset));
    }
    adoptIfChanged(geometry.snapOffsets, std::move(snapOffsets));
  }

  // Compare pointers. The cache only takes a new one when the values changed, so this stays cheap.
  bool stickyChanged =
    geometry.stickyHeaderIndices != nextStateData.stickyHeaderIndices_ ||
    geometry.stickyHeaderOffsets != nextStateData.stickyHeaderOffsets_ ||
    geometry.stickyHeaderSizes != nextStateData.stickyHeaderSizes_;

  bool snapChanged = geometry.snapOffsets != nextStateData.snapOffsets_;

  // The newest hide while any row is hidden, or 0 when nothing waits on the host.
  double concealGeneration = geometry.concealedRows.empty() ? 0.0 : static_cast<double>(geometry.concealGeneration);
  bool concealChanged = nextStateData.concealGeneration_ != concealGeneration;

  // The offsets the host can scroll through without a state update, see ShadowListOffsetBand.h.
  auto offsetBand = shadowListOffsetBand(*this);
  bool bandChanged = !shadowListOffsetBandPublished(nextStateData, offsetBand);

  SL_LOG("layout: elementChildren=%zu hdr=%.1f ftr=%.1f stateOffset=(%.1f,%.1f) coreOffset=(%.1f,%.1f) total=(%.1f,%.1f) applyOffset=%d changed=%d",
    getChildren().size(), headerSize, footerSize,
    nextStateData.containerOffsetX_, nextStateData.containerOffsetY_,
    stateUpdate.containerOffsetX, stateUpdate.containerOffsetY,
    stateUpdate.totalContainerWidth, stateUpdate.totalContainerHeight,
    stateUpdate.applyContainerOffset ? 1 : 0, stateUpdate.changed ? 1 : 0);

  if (stateUpdate.changed || stickyChanged || snapChanged || concealChanged || bandChanged) {
    if (stateUpdate.changed) {
      /*
       * Record where the correction started, see ShadowListViewState::containerOffsetBaseX_.
       * If the state already carries this correction's token, keep the first base, so the
       * host never applies the same part of the correction twice.
       */
      bool continuesCorrection = stateUpdate.commitToken != 0 &&
        static_cast<std::uint64_t>(nextStateData.commitToken_) == stateUpdate.commitToken;
      if (!continuesCorrection) {
        nextStateData.containerOffsetBaseX_ = nextStateData.containerOffsetX_;
        nextStateData.containerOffsetBaseY_ = nextStateData.containerOffsetY_;
      }
      nextStateData.containerOffsetX_ = stateUpdate.containerOffsetX;
      nextStateData.containerOffsetY_ = stateUpdate.containerOffsetY;
      nextStateData.totalContainerWidth_ = stateUpdate.totalContainerWidth;
      nextStateData.totalContainerHeight_ = stateUpdate.totalContainerHeight;
      nextStateData.containerOffsetEnabled_ = stateUpdate.applyContainerOffset;
      /*
       * Send the token with the offset so the host can echo it back and the core knows its
       * own write. 0 when no offset was written.
       */
      nextStateData.commitToken_ = static_cast<double>(stateUpdate.commitToken);
      /*
       * A ShadowListNative scroll command tells the host to stop momentum and write the offset.
       * See ShadowListNativeEngine::setMomentumYieldToken.
       */
      if (this->nativeEngine_ && stateUpdate.applyContainerOffset && stateUpdate.commitToken != 0 &&
          stateUpdate.commitToken == this->nativeEngine_->momentumYieldToken()) {
        nextStateData.momentumYieldToken_ = static_cast<double>(stateUpdate.commitToken);
        nextStateData.userScrolled_ = false;
        nextStateData.scrollPhase_ = SCROLL_PHASE_IDLE;
      }
    }
    /*
     * Copy, don't move, since the next layout reuses the cache. It's just a shared pointer
     * and the list never changes once published, so sharing it is safe.
     */
    if (stickyChanged) {
      nextStateData.stickyHeaderIndices_ = geometry.stickyHeaderIndices;
      nextStateData.stickyHeaderOffsets_ = geometry.stickyHeaderOffsets;
      nextStateData.stickyHeaderSizes_ = geometry.stickyHeaderSizes;
    }
    if (snapChanged) {
      nextStateData.snapOffsets_ = geometry.snapOffsets;
    }
    nextStateData.concealGeneration_ = concealGeneration;
    nextStateData.offsetBandLow_ = offsetBand.low;
    nextStateData.offsetBandHigh_ = offsetBand.high;
    setStateData(std::move(nextStateData));
  }

  this->firstMeasuredTags_.clear();

  // For ShadowListNative, keep the laid out rows and mount more if they don't fill the viewport.
  if (this->nativeEngine_) {
    this->nativeEngine_->didLayout(*this, *this->containerManager_);
  }
}

void ShadowListViewShadowNode::replaceChild(
  const ShadowNode& previousElementShadowNode,
  const std::shared_ptr<const ShadowNode>& nextElementShadowNode,
  std::size_t suggestedIndex) {

  /*
   * Send measured row sizes to the core. Look rows up by key, or a child behind a prepend
   * would write its size onto whatever row now has its old index. Take the core lock,
   * since this can run at the same time as the commit phase.
   */
  if (this->suppressElementSizeFeedback_) {
    // layout() already gave the core these sizes, so skip the frame it just wrote.
  } else if (const auto elementViewProps = dynamic_cast<const ShadowListElementViewProps*>(nextElementShadowNode->getProps().get())) {
    if (this->containerManager_) {
      std::lock_guard<std::recursive_mutex> lock(this->containerManager_->coreMutex);

      // Look up the index under the lock, since a stale child can arrive before the data catches up.
      std::size_t elementIndex = elementViewProps->elementKey.empty()
        ? static_cast<std::size_t>(elementViewProps->index)
        : this->containerManager_->findElementIndexByKey(elementViewProps->elementKey);
      const auto elementViewNode = dynamic_cast<const YogaLayoutableShadowNode*>(nextElementShadowNode.get());
      const auto elementViewNodeSize = elementViewNode
        ? elementViewNode->getLayoutMetrics().frame.size
        : Size{};
      /*
       * A zero size means Yoga hasn't laid the row out yet. Recording 0 would collapse the
       * row and the content under the reader would jump by its size. Let the layout pass
       * measure it from the real frame instead.
       */
      bool laidOut = (this->containerManager_->horizontal ? elementViewNodeSize.width : elementViewNodeSize.height) > 0.0;
      if (elementIndex < this->containerManager_->getElementsSize() && elementViewNode && laidOut) {
        bool firstMeasurement = !this->containerManager_->getElementAtIndex(elementIndex).measured;

        azimgd::shadowlist::Virtualizer::updateElementAtIndex(
          this->containerManager_.get(),
          elementIndex,
          {.width = elementViewNodeSize.width, .height = elementViewNodeSize.height});

        if (firstMeasurement) {
          this->firstMeasuredTags_.push_back(nextElementShadowNode->getTag());
        }
      }
    }
  }

  YogaLayoutableShadowNode::replaceChild(previousElementShadowNode, nextElementShadowNode, suggestedIndex);
}

}
