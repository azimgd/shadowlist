#include "ShadowListViewShadowNode.h"

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
 * Rows are concealed only on hosts that ack the generation of the state they mounted (see
 * ShadowListViewState::concealGenerationAck_). The Android host merges its reports onto the
 * newest state rather than the mounted one, so it cannot make that claim and never conceals.
 */
#ifdef __APPLE__
constexpr bool CONCEAL_UNSETTLED_ROWS = true;
#else
constexpr bool CONCEAL_UNSETTLED_ROWS = false;
#endif

/*
 * Layout passes a row may stay concealed through before it is revealed regardless. Every pass
 * is a commit, so this only matters when corrections keep landing without settling.
 */
constexpr std::size_t MAX_CONCEALED_LAYOUT_PASSES = 8;

/*
 * Parse `opacity: 0` onto a row's props, through the parse-from-raw path (props are not
 * copy-constructible). Null without a ContextContainer.
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
  /*
   * Carry the shared core instances forward from the source so every clone of a
   * list shares one Container, freed when the node family dies.
   */
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

void ShadowListViewShadowNode::layout(LayoutContext layoutContext) {
  ConcreteViewShadowNode::layout(layoutContext);

  if (!this->containerManager_ || !this->geometryCache_) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(this->containerManager_->coreMutex);

  /*
   * Release the children retained by the previous layout: the commit that recorded their
   * raw pointers in affectedNodes completed long ago (see replacedChildren_).
   */
  this->replacedChildren_.clear();

  /*
   * Identify and measure template views (header/footer/empty)
   *
   * affectedNodes is NOT this node's scratch space: ShadowTree::commit() allocates it
   * once per commit for the WHOLE surface and passes it down through the entire
   * recursive Yoga layout, then fires onLayout afterward for every node it collects
   * (see YogaLayoutableShadowNode::layout, which appends here rather than clearing).
   * Clearing it wipes every sibling/ancestor's entry recorded earlier in this same
   * commit, silently dropping their onLayout. It is also documented as nullable
   * (LayoutContext.h) and every other call site in the framework guards it before use
   * -- do the same below instead of dereferencing unconditionally.
   */

  std::shared_ptr<const ShadowNode> headerNode = nullptr;
  std::shared_ptr<const ShadowNode> footerNode = nullptr;
  /*
   * The sticky section-header overlay (SectionList): an always-mounted template
   * whose content is the active section's header. Unlike a normal header it reserves
   * NO list space (it floats over the content), so it never contributes to
   * headerSize / total size or shifts element offsets; it is positioned at the
   * origin and pinned to the viewport entirely natively (see the integrations),
   * exactly like the regular sticky header. This is what keeps section headers
   * smooth: the pinned view is never virtualized, so it has no commit-cycle remount
   * latency.
   */
  std::shared_ptr<const ShadowNode> sectionHeaderNode = nullptr;
  /*
   * Kept separate from headerNode: ShadowList mounts `header` and `empty` at the same
   * time when data is empty, so folding empty into the header would overwrite the real
   * header's node and size.
   */
  std::shared_ptr<const ShadowNode> emptyNode = nullptr;
  double headerSize = 0.0;
  double footerSize = 0.0;
  bool horizontal = getConcreteProps().horizontal;

  /*
   * One classification pass over the children. Element rows are collected here with the
   * core index their key currently resolves to, so the two passes below (measurement
   * intake, then placement) never repeat the dynamic_pointer_cast or the key lookup.
   */
  struct MountedElement {
    std::size_t childIndex;
    std::size_t elementIndex;
  };
  std::vector<MountedElement> mountedElements;
  mountedElements.reserve(getChildren().size());

  for (std::size_t childIndex = 0; childIndex < getChildren().size(); ++childIndex) {
    if (const auto elementViewProps = std::dynamic_pointer_cast<const ShadowListElementViewProps>(getChildren()[childIndex]->getProps())) {
      /*
       * Resolve the mounted view to its CURRENT core element by key: a child committed
       * before a prepend/insert/reorder carries a stale index. findElementIndexByKey
       * returns the row's live position, or UNDEFINED_INDEX if the key is gone (drop the
       * frame for it). Fall back to the index only when no key is supplied.
       */
      std::size_t elementIndex = elementViewProps->elementKey.empty()
        ? static_cast<std::size_t>(elementViewProps->index)
        : this->containerManager_->findElementIndexByKey(elementViewProps->elementKey);
      if (elementIndex < this->containerManager_->getElementsSize()) {
        mountedElements.push_back({childIndex, elementIndex});
      }
      continue;
    }

    if (const auto templateProps = std::dynamic_pointer_cast<const ShadowListTemplateViewProps>(getChildren()[childIndex]->getProps())) {
      const auto templateViewNode = std::dynamic_pointer_cast<const YogaLayoutableShadowNode>(getChildren()[childIndex]);
      if (!templateViewNode) {
        continue;
      }

      const auto templateViewNodeLayoutMetrics = templateViewNode->getLayoutMetrics();
      const auto templateViewNodeSize = horizontal
        ? templateViewNodeLayoutMetrics.frame.size.width
        : templateViewNodeLayoutMetrics.frame.size.height;

      if (templateProps->templateType == "sectionHeader") {
        sectionHeaderNode = getChildren()[childIndex];
      } else if (templateProps->templateType == "header") {
        headerNode = getChildren()[childIndex];
        headerSize = templateViewNodeSize;
      } else if (templateProps->templateType == "empty") {
        emptyNode = getChildren()[childIndex];
      } else if (templateProps->templateType == "footer") {
        footerNode = getChildren()[childIndex];
        footerSize = templateViewNodeSize;
      }
    }
  }

  /*
   * Apply the freshly measured header/footer AND the actual window (viewport) size to
   * the core now and reflow the element offsets in this same pass. update() runs from
   * adopt in the commit phase, before this node has been laid out, so it sees a zero
   * frame on the first render: a multi-column list then sizes every column to
   * windowWidth/columns == 0 (collapsed masonry), and a static, non-sticky header
   * overlaps the first rows. The corrected frame only reaches update() on a LATER
   * commit (e.g. a scroll), and the high-water visible band suppresses the re-render
   * that might trigger one, so a static list stays broken until the user scrolls.
   * Applying the real values here makes the first layout correct on its own. The next
   * Virtualizer::update reads the header/footer back from the core, so they carry over.
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
    this->containerManager_->headerSize = headerSize;
    this->containerManager_->footerSize = footerSize;
    this->containerManager_->revision.windowContainerWidth = windowFrameSize.width;
    this->containerManager_->revision.windowContainerHeight = windowFrameSize.height;
    azimgd::shadowlist::Virtualizer::recomputeElementOffsets(this->containerManager_.get(), 0);
    /*
     * update() for this commit ran with the previous header size, so its anchor correction
     * does not know the rows just moved. Settle the header change now: publishing the offset
     * it computed would show the rows shifted by the change until the next commit resolved
     * the correction again (a one-frame jump when a loading spinner in the header toggles
     * with a prepend).
     */
    azimgd::shadowlist::Virtualizer::applyHeaderSizeChange(this->containerManager_.get(), previousHeaderSize);
    // A chat resting at its bottom keeps it as the composer resizes the list.
    azimgd::shadowlist::Virtualizer::applyWindowSizeChange(this->containerManager_.get(), previousWindowSize);

    /*
     * The header/footer/window just changed, which reflowed every element offset (e.g.
     * the header measured one commit late shifts all rows down by its size). The
     * reflow alone does not move the scroll view, so without reasserting the offset
     * the host leaves contentOffset stale and the list appears scrolled past the header
     * (or, with a sticky header pinned at the top, the first rows sit under it). Mark
     * the offset corrected so this commit publishes applyContainerOffset and the host
     * reasserts the core's resting offset (0 on first open). The MVCP header-size
     * compensation keeps that resting offset correct, so this reassert is a no-op while
     * the user is scrolled (it reapplies the current offset) and only matters on the
     * header-measurement/resize commits.
     */
    this->containerManager_->containerOffsetCorrected = true;
  }

  /*
   * Pass 1: intake. Hand the core every mounted row's freshly measured size BEFORE any
   * position is read, and reflow the affected suffix exactly once.
   *
   * applyElementSize only records a size and reports whether the geometry moved; one
   * commitElementSizes from the lowest changed row then reflows the suffix, so a layout
   * with M mounted rows costs one reflow rather than M. Every row is positioned from
   * geometry that already accounts for all of this pass's measurements.
   */
  std::size_t lowestChangedIndex = azimgd::shadowlist::UNDEFINED_INDEX;
  for (const auto& mounted : mountedElements) {
    const auto elementViewNode =
      std::dynamic_pointer_cast<const YogaLayoutableShadowNode>(getChildren()[mounted.childIndex]);
    if (!elementViewNode) {
      continue;
    }

    const auto& measuredSize = elementViewNode->getLayoutMetrics().frame.size;
    azimgd::shadowlist::Size feedSize{measuredSize.width, measuredSize.height};

    if (this->containerManager_->columns > 1) {
      /*
       * A multi-column row's cross-axis extent is owned by the track layout
       * (recomputeElementOffsets forces it to the track size), not by measurement. Keep
       * the core's value for that axis and take only the scroll-axis measurement,
       * otherwise every frame reports a cross size the very next reflow overwrites.
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
      this->firstMeasuredTags_.push_back(getChildren()[mounted.childIndex]->getTag());
    }

    if (changed && mounted.elementIndex < lowestChangedIndex) {
      lowestChangedIndex = mounted.elementIndex;
    }
  }
  if (lowestChangedIndex != azimgd::shadowlist::UNDEFINED_INDEX) {
    azimgd::shadowlist::Virtualizer::commitElementSizes(this->containerManager_.get(), lowestChangedIndex);
  }

  /*
   * Refresh the total once now that the measured sizes for this batch of children
   * have been fed back, instead of rescanning the whole list per measured child.
   * The footer position below and the published content size depend on it.
   */
  azimgd::shadowlist::Virtualizer::recomputeTotalSize(this->containerManager_.get());

  auto& geometry = *this->geometryCache_;

  /*
   * Row concealment (see ShadowListViewGeometryCache::concealedRows). The correction this pass
   * publishes is final by now: resolveStateUpdate below only reads it.
   */
  const auto& inputStateData = getStateData();
  bool correcting = this->containerManager_->containerOffsetCorrected;
  auto concealAck = static_cast<std::uint64_t>(inputStateData.concealGenerationAck_);

  /*
   * A correction anchored to a row keeps that row still only once the host mounts it. Rows
   * before the anchor measured for the first time in this cycle are what moved it; conceal
   * them. A fixed-offset correction (bottom pin, scroll to end) has no anchor row and conceals
   * nothing. The anchor is the one commitElementSizes compensates for.
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
  std::uint64_t nextConcealGeneration = geometry.concealGeneration + 1;
  std::vector<Tag> stillConcealedTags;

  /*
   * Pass 2: placement. Apply the virtualizer's positions (offsets already include the
   * header).
   *
   * Cloning a child is how a new frame is written into the tree, but Yoga only overwrites
   * a child's layout metrics when it actually re-laid it out, so a row whose position has
   * not moved since the last commit still holds the frame we want. Cloning it anyway
   * meant a clone, a shadow-node replacement, a Yoga child replacement and a spurious
   * affectedNodes entry per mounted row on every single scroll frame, for no change.
   * Compare first and only rewrite the rows that actually move.
   */
  for (const auto& mounted : mountedElements) {
    const auto& prevChild = getChildren()[mounted.childIndex];
    const auto prevLayoutableChild = std::dynamic_pointer_cast<const YogaLayoutableShadowNode>(prevChild);
    if (!prevLayoutableChild) {
      continue;
    }

    /*
     * The props this row should carry, or null to keep its own. A concealed row is revealed once
     * the host acked its generation with no correction in flight, or after too many passes.
     * While it stays concealed, a React commit that handed its original props back is concealed
     * again, and new props from a re-render are concealed too.
     */
    const Tag tag = prevChild->getTag();
    std::shared_ptr<const facebook::react::Props> nextProps = nullptr;
    auto concealedRow = geometry.concealedRows.find(tag);
    if (concealedRow != geometry.concealedRows.end()) {
      auto& row = concealedRow->second;
      ++row.layoutPasses;
      bool settled = (concealAck >= row.generation && !correcting) || row.layoutPasses > MAX_CONCEALED_LAYOUT_PASSES;
      if (!settled && prevChild->getProps() != row.sourceProps && prevChild->getProps() != row.concealedProps) {
        row.sourceProps = prevChild->getProps();
        row.concealedProps = concealedPropsForRow(*prevChild);
        settled = row.concealedProps == nullptr;
      }
      if (settled) {
        SL_LOG("  reveal: tag=%d gen=%llu ack=%llu passes=%zu correcting=%d",
          tag, static_cast<unsigned long long>(row.generation), static_cast<unsigned long long>(concealAck),
          row.layoutPasses, correcting ? 1 : 0);
        if (row.concealedProps != nullptr && prevChild->getProps() == row.concealedProps) {
          nextProps = row.sourceProps;
        }
        geometry.concealedRows.erase(concealedRow);
      } else {
        stillConcealedTags.push_back(tag);
        if (prevChild->getProps() != row.concealedProps) {
          nextProps = row.concealedProps;
        }
      }
    } else if (mounted.elementIndex < concealBeforeIndex &&
               std::find(this->firstMeasuredTags_.begin(), this->firstMeasuredTags_.end(), tag) != this->firstMeasuredTags_.end()) {
      auto concealedProps = concealedPropsForRow(*prevChild);
      if (concealedProps != nullptr) {
        SL_LOG("  conceal: tag=%d index=%zu anchorIndex=%zu gen=%llu",
          tag, mounted.elementIndex, concealBeforeIndex, static_cast<unsigned long long>(nextConcealGeneration));
        geometry.concealGeneration = nextConcealGeneration;
        geometry.concealedRows.insert_or_assign(
          tag, ShadowListViewGeometryCache::ConcealedRow{prevChild->getProps(), concealedProps, nextConcealGeneration, 0});
        stillConcealedTags.push_back(tag);
        nextProps = std::move(concealedProps);
      }
    }

    LayoutMetrics layoutMetrics = prevLayoutableChild->getLayoutMetrics();
    const LayoutMetrics prevLayoutMetrics = layoutMetrics;
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

    if (layoutMetrics == prevLayoutMetrics && nextProps == nullptr) {
      continue;
    }

    /*
     * Opacity is not a Yoga style, so a props clone leaves the row's Yoga node clean and its
     * laid-out frame intact.
     */
    /*
     * A clone that only moves the row hands React's reference over, so React's node keeps a
     * laid-out frame. One that conceals the row does not: on a JS-thread commit React's
     * reference would move to it, and React's next update of the row would build on the
     * concealed props.
     */
    auto elementViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode>(
      prevChild->clone({.props = nextProps, .runtimeShadowNodeReference = nextProps == nullptr}));
    elementViewNode->setLayoutMetrics(layoutMetrics);
    /*
     * The child's own position is known here, and both ShadowNode::replaceChild and
     * YogaLayoutableShadowNode::replaceChild fall back to a linear search without it --
     * two scans of the child vector per replaced row, i.e. quadratic across the loop.
     *
     * Pass 1 above is the authority on this batch's measurements, so the size-feedback
     * side of the replaceChild override is suppressed here: re-reporting the frame we
     * just wrote (whose cross-axis size the track layout owns, not the measurement)
     * would fight the layout for a multi-column list on every frame.
     */
    // Keep the outgoing child alive past this commit; see replacedChildren_.
    this->replacedChildren_.push_back(prevChild);
    this->suppressElementSizeFeedback_ = true;
    replaceChild(*prevChild, elementViewNode, mounted.childIndex);
    this->suppressElementSizeFeedback_ = false;
    if (layoutContext.affectedNodes != nullptr) {
      layoutContext.affectedNodes->push_back(elementViewNode.get());
    }
  }

  // Forget concealed rows that are no longer mounted: unmounted, or their key left the list.
  if (geometry.concealedRows.size() > stillConcealedTags.size()) {
    for (auto iterator = geometry.concealedRows.begin(); iterator != geometry.concealedRows.end();) {
      bool stillConcealed =
        std::find(stillConcealedTags.begin(), stillConcealedTags.end(), iterator->first) != stillConcealedTags.end();
      iterator = stillConcealed ? std::next(iterator) : geometry.concealedRows.erase(iterator);
    }
  }

  /*
   * Move a template view to `origin`, cloning it into the tree only if it is not already
   * there. Same reasoning as the row placement above: a template that has not moved since
   * the last commit still carries the frame we want, so re-cloning it every scroll frame
   * buys nothing and costs a clone, a shadow-node replacement, a Yoga child replacement
   * and a spurious onLayout entry.
   */
  auto placeTemplate = [&](const std::shared_ptr<const ShadowNode>& templateNode, Point origin) {
    const auto prevTemplateNode = std::dynamic_pointer_cast<const YogaLayoutableShadowNode>(templateNode);
    if (!prevTemplateNode) {
      return;
    }

    LayoutMetrics templateMetrics = prevTemplateNode->getLayoutMetrics();
    if (templateMetrics.frame.origin == origin) {
      return;
    }

    templateMetrics.frame.origin = origin;
    auto templateViewNode = std::dynamic_pointer_cast<YogaLayoutableShadowNode>(templateNode->clone({}));
    templateViewNode->setLayoutMetrics(templateMetrics);
    // Keep the outgoing template alive past this commit; see replacedChildren_.
    this->replacedChildren_.push_back(templateNode);
    replaceChild(*templateNode, templateViewNode);
    if (layoutContext.affectedNodes != nullptr) {
      layoutContext.affectedNodes->push_back(templateViewNode.get());
    }
  };

  /*
   * The header rests at the content start. A sticky header is pinned to the
   * viewport natively in the scroll callback (see the integrations), not here,
   * because the commit cycle is too slow to track scrolling smoothly.
   */
  if (headerNode) {
    placeTemplate(headerNode, {0, 0});
  }

  /*
   * The empty template rests where content would start: just after the header
   * (at the origin when there is none). It contributes nothing to headerSize.
   */
  if (emptyNode) {
    /*
     * Point holds Float, which is `float` on Android and `double` on 64-bit Apple targets,
     * so these casts are what keep the narrowing explicit rather than a -Wc++11-narrowing
     * error that only one of the two platforms reports.
     */
    auto emptyOffset = static_cast<Float>(headerSize);
    placeTemplate(emptyNode, horizontal ? Point{emptyOffset, 0} : Point{0, emptyOffset});
  }

  if (footerNode) {
    auto footerOffset = static_cast<Float>(this->containerManager_->getFooterOffset(footerSize));
    placeTemplate(footerNode, horizontal ? Point{footerOffset, 0} : Point{0, footerOffset});
  }

  /*
   * The sticky section-header overlay rests at the content origin; the integration
   * pins it to the viewport on the UI thread per scroll frame. It floats over the
   * content (no reserved space), so its position never feeds back into element
   * offsets or the total size.
   */
  if (sectionHeaderNode) {
    placeTemplate(sectionHeaderNode, {0, 0});
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
   * thread per scroll frame (see ShadowListViewState / the native pin). Only the core knows these offsets, and they
   * change as off-screen rows are measured, so they ride along on the state.
   *
   * Rebuild the publishable geometry only when the element geometry it derives from has
   * actually moved. This runs on every layout, i.e. on every scroll frame, and both the
   * snap targets (one per row) and the sticky header table are O(rows) to build and
   * allocate. None of it depends on the scroll offset, which is the thing that changed
   * on almost all of those frames.
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
     * Build into plain vectors, then hand each to adoptIfChanged, which keeps the EXISTING
     * shared pointer when the contents are identical. Geometry is versioned by any offset
     * reflow, so a scroll that measures fresh rows invalidates this cache every frame even
     * though the sticky table (and, for most lists, the snap targets) usually come out
     * exactly the same. Reusing the pointer in that case is what stops those frames from
     * publishing a state update nobody needs.
     */
    std::vector<int> stickyHeaderIndices;
    std::vector<Float> stickyHeaderOffsets;
    std::vector<Float> stickyHeaderSizes;

    auto adoptIfChanged = [](auto& cached, auto&& next) {
      using ValueT = typename std::decay_t<decltype(next)>::value_type;
      if (next.empty()) {
        // Null and empty mean the same thing; prefer null so nothing is allocated.
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
     * Inverted sticky section headers (pin to the viewport end) are an exotic
     * combination left resting, so publish no geometry for them; otherwise the native
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

    adoptIfChanged(geometry.stickyHeaderIndices, std::move(stickyHeaderIndices));
    adoptIfChanged(geometry.stickyHeaderOffsets, std::move(stickyHeaderOffsets));
    adoptIfChanged(geometry.stickyHeaderSizes, std::move(stickyHeaderSizes));

    /*
     * Resting snap offsets along the scroll axis (empty unless snapToItem is set).
     * Only the core knows element boundaries and they shift as off-screen rows are
     * measured, so they ride along on the state for the UI-thread snap.
     */
    const auto& coreSnapOffsets = this->containerManager_->getSnapOffsets();
    std::vector<Float> snapOffsets;
    snapOffsets.reserve(coreSnapOffsets.size());
    for (double snapOffset : coreSnapOffsets) {
      snapOffsets.push_back(static_cast<Float>(snapOffset));
    }
    adoptIfChanged(geometry.snapOffsets, std::move(snapOffsets));
  }

  /*
   * Pointer comparisons: both sides hold the same shared collections, and the cache only
   * takes a new pointer when the values actually moved (see adoptIfChanged), so the
   * per-layout change test stays O(1) for a snapping list.
   */
  bool stickyChanged =
    geometry.stickyHeaderIndices != nextStateData.stickyHeaderIndices_ ||
    geometry.stickyHeaderOffsets != nextStateData.stickyHeaderOffsets_ ||
    geometry.stickyHeaderSizes != nextStateData.stickyHeaderSizes_;

  bool snapChanged = geometry.snapOffsets != nextStateData.snapOffsets_;

  // The newest concealment while any row stays concealed; 0 tells the host nothing waits on it.
  double concealGeneration = geometry.concealedRows.empty() ? 0.0 : static_cast<double>(geometry.concealGeneration);
  bool concealChanged = nextStateData.concealGeneration_ != concealGeneration;

  SL_LOG("layout: elementChildren=%zu hdr=%.1f ftr=%.1f stateOffset=(%.1f,%.1f) coreOffset=(%.1f,%.1f) total=(%.1f,%.1f) applyOffset=%d changed=%d",
    getChildren().size(), headerSize, footerSize,
    nextStateData.containerOffsetX_, nextStateData.containerOffsetY_,
    stateUpdate.containerOffsetX, stateUpdate.containerOffsetY,
    stateUpdate.totalContainerWidth, stateUpdate.totalContainerHeight,
    stateUpdate.applyContainerOffset ? 1 : 0, stateUpdate.changed ? 1 : 0);

  if (stateUpdate.changed || stickyChanged || snapChanged || concealChanged) {
    if (stateUpdate.changed) {
      /*
       * What the correction was computed from; see ShadowListViewState::containerOffsetBaseX_.
       * A state carrying the operation's own token is either the core's own write, not a place
       * the host has been, or a host report echoing it, whose offset already holds the part of
       * the correction the host applied. A republish of that operation keeps the base its first
       * write started from, so the correction stays one cumulative delta per commit token and
       * the host never subtracts the part it applied twice.
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
       * Publish the correction's commit token alongside the offset so the host can echo
       * it back and the core recognises its own write (0 when no offset was applied).
       */
      nextStateData.commitToken_ = static_cast<double>(stateUpdate.commitToken);
    }
    /*
     * Copied, not moved: the cache outlives this state and is reused by the next layout.
     * The copy is a refcount bump now that these are shared pointers, and the collection
     * itself is immutable once published, so state and cache can share it safely.
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
    setStateData(std::move(nextStateData));
  }

  this->firstMeasuredTags_.clear();

  // ShadowListNative: keep the laid-out rows, and remount if they fell short of the viewport.
  if (this->nativeEngine_) {
    this->nativeEngine_->didLayout(*this, *this->containerManager_);
  }
}

void ShadowListViewShadowNode::replaceChild(
  const ShadowNode& prevElementShadowNode,
  const std::shared_ptr<const ShadowNode>& nextElementShadowNode,
  std::size_t suggestedIndex) {

  /*
   * Feed natively measured element sizes back into the virtualizer. Resolve the view to
   * its CURRENT core element by key: a child running behind a prepend/insert/reorder would
   * otherwise write its size onto whatever row now occupies its old index. findElementIndexByKey
   * gives the live position or UNDEFINED_INDEX (skip) when the key is gone; the index is a
   * fallback when no key is supplied. Take the core lock since this can run concurrently
   * with the commit phase.
   */
  if (this->suppressElementSizeFeedback_) {
    /*
     * Our own placement pass already fed this batch's measurements to the core (see
     * layout()); reporting the frame it just wrote back would be redundant at best.
     */
  } else if (const auto elementViewProps = std::dynamic_pointer_cast<const ShadowListElementViewProps>(nextElementShadowNode->getProps())) {
    if (this->containerManager_) {
      std::lock_guard<std::recursive_mutex> lock(this->containerManager_->coreMutex);

      // Resolve key -> live index under the lock; a stale child can outrun the reconcile.
      std::size_t elementIndex = elementViewProps->elementKey.empty()
        ? static_cast<std::size_t>(elementViewProps->index)
        : this->containerManager_->findElementIndexByKey(elementViewProps->elementKey);
      const auto elementViewNode = std::dynamic_pointer_cast<const YogaLayoutableShadowNode>(nextElementShadowNode);
      const auto elementViewNodeSize = elementViewNode
        ? elementViewNode->getLayoutMetrics().frame.size
        : Size{};
      /*
       * A child with no extent along the scroll axis has not been laid out: React's own node
       * for a row the layout pass last cloned, or a row mounting in this
       * commit, carries an empty frame until Yoga runs. Recording 0 would collapse the row
       * outside any anchor capture (the next update() re-anchors on the collapsed geometry,
       * so the content under the reader jumps by the row's size). The layout pass measures
       * the row from its real frame.
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

  YogaLayoutableShadowNode::replaceChild(prevElementShadowNode, nextElementShadowNode, suggestedIndex);
}

}
