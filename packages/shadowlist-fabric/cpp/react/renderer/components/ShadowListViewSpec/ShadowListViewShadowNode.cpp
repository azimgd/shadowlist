#include "ShadowListViewShadowNode.h"

#include "ShadowListOffsetBand.h"
#include "ShadowListMountDriver.h"

#include <folly/dynamic.h>
#include <react/renderer/core/ComponentDescriptor.h>
#include <react/renderer/core/LayoutConstraints.h>
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/RawProps.h>

#include <algorithm>
#include <cmath>
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

std::vector<std::string> ShadowListViewShadowNode::getMountedRowKeys() const {
  std::vector<std::string> keys;
  keys.reserve(getChildren().size());
  for (const auto& child : getChildren()) {
    const auto* props = dynamic_cast<const ShadowListCellViewProps*>(child->getProps().get());
    if (props && !props->rowKey.empty()) keys.push_back(props->rowKey);
  }
  return keys;
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
  suppressRowSizeFeedback_ = true;
  replaceChild(*child, nextChild, childIndex);
  suppressRowSizeFeedback_ = false;
  if (layoutContext.affectedNodes != nullptr) {
    layoutContext.affectedNodes->push_back(nextChild.get());
  }
}

void ShadowListViewShadowNode::layout(LayoutContext layoutContext) {
  ConcreteViewShadowNode::layout(layoutContext);

  if (!containerManager_ || !geometryCache_) {
    return;
  }
  std::unique_lock<std::recursive_mutex> coreLock(containerManager_->coreMutex);
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
  if (!measureChildren(core, horizontal, slots, layoutContext, coreLock)) {
    core.state.measuredRangeStale = true;
    dirtyLayout();
    return;
  }
  placeRows(core, slots.mountedRows, horizontal, layoutContext);
  placeTemplates(core, slots, horizontal, layoutContext);
  publishLayoutState(core, slots.headerSize, slots.footerSize);
  if (getConcreteProps().rowColumnsEventEnabled && geometryCache_->keysProps == getProps()) {
    const auto& mountDriver = getStateData().mountDriver_;
    if (mountDriver) mountDriver->reportRowColumns(core, getMountedRowKeys(), geometryCache_->rowSpansVersion);
  }

  firstMeasuredTags_.clear();
}

bool ShadowListViewShadowNode::measureChildren(
  azimgd::shadowlist::Container& core, bool horizontal, LayoutSlots& slots,
  LayoutContext& layoutContext, std::unique_lock<std::recursive_mutex>& coreLock) {
  slots.mountedRows.reserve(getChildren().size());
  std::vector<azimgd::shadowlist::MeasuredRow> measuredRows;
  measuredRows.reserve(getChildren().size());

  for (std::size_t childIndex = 0; childIndex < getChildren().size(); ++childIndex) {
    const auto& child = getChildren()[childIndex];
    const facebook::react::Props* childProps = child->getProps().get();
    if (const auto cellViewProps = dynamic_cast<const ShadowListCellViewProps*>(childProps)) {
      /*
       * Look the row up by key, since a child committed before a prepend, insert or reorder
       * sits at another index now. Skip it if the key is gone.
       */
      std::size_t rowIndex = cellViewProps->rowKey.empty() ? azimgd::shadowlist::UNDEFINED_INDEX
                                                           : core.indexOfKey(cellViewProps->rowKey);
      const auto cellViewNode = dynamic_cast<const YogaLayoutableShadowNode*>(child.get());
      if (cellViewNode != nullptr && rowIndex < core.getRowCount()) {
        slots.mountedRows.push_back({childIndex, rowIndex, cellViewNode});

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
  struct Measurement {
    std::size_t mountedIndex;
    std::shared_ptr<const ShadowNode> previous;
    std::shared_ptr<YogaLayoutableShadowNode> next;
    std::vector<const LayoutableShadowNode*> affectedNodes;
  };
  std::vector<Measurement> measurements;
  std::vector<azimgd::shadowlist::Row> rows;
  for (std::size_t mountedIndex = 0; mountedIndex < slots.mountedRows.size(); ++mountedIndex) {
    const auto& mounted = slots.mountedRows[mountedIndex];
    const auto& row = core.getRowAtIndex(mounted.rowIndex);
    const auto cross = horizontal ? row.height : row.width;
    const auto measured = mounted.node->getLayoutMetrics().frame.size;
    const auto measuredCross = horizontal ? measured.height : measured.width;
    const auto scale = layoutContext.pointScaleFactor;
    const auto roundedCross = scale > 0 ? std::round(cross * scale) / scale : cross;
    const auto previous = getChildren()[mounted.childIndex];
    const auto previousProps = previous->getProps();
    const auto* props = dynamic_cast<const ShadowListCellViewProps*>(previousProps.get());
    const auto& paddings = geometryCache_->columnPaddings;
    const auto leadingEdge = horizontal ? yoga::Edge::Top : yoga::Edge::Left;
    const auto trailingEdge = horizontal ? yoga::Edge::Bottom : yoga::Edge::Right;
    const bool hasPadding = row.column < core.numberOfColumns && row.columnSpan <= core.numberOfColumns - row.column &&
      paddings.size() == core.numberOfColumns * 2;
    const double leading = hasPadding ? paddings[row.column * 2] : 0;
    const double trailing = hasPadding ? paddings[(row.column + row.columnSpan - 1) * 2 + 1] : 0;
    const bool paddingChanged = hasPadding && props &&
      (props->yogaStyle.padding(leadingEdge) != yoga::Style::Length::points(static_cast<float>(leading)) ||
       props->yogaStyle.padding(trailingEdge) != yoga::Style::Length::points(static_cast<float>(trailing)));
    // placeRows writes canonical sizes; Yoga may instead publish their pixel-rounded values.
    const bool crossChanged = measuredCross != static_cast<Float>(cross) &&
      measuredCross != static_cast<Float>(roundedCross);
    const auto contextContainer = paddingChanged ? previous->getContextContainer() : nullptr;
    if (core.numberOfColumns > 1 && cross > 0 && (crossChanged || (paddingChanged && contextContainer))) {
      if (rows.empty()) {
        rows.reserve(slots.mountedRows.size());
        for (const auto& current : slots.mountedRows) rows.push_back(core.getRowAtIndex(current.rowIndex));
      }
      const auto& snapshot = rows[mountedIndex];
      const auto sourceProps = geometryCache_->keysProps;
      const auto numberOfColumns = core.numberOfColumns;
      const auto windowCross = horizontal ? core.revision.windowHeight : core.revision.windowWidth;
      const auto geometryVersion = core.state.geometryVersion;
      const auto rowSpansVersion = geometryCache_->rowSpansVersion;
      const auto columnPaddings = geometryCache_->columnPaddings;
      // Yoga can call platform or custom-component measurement. Never hold the core lock there.
      coreLock.unlock();
      auto nextProps = previousProps;
      if (paddingChanged && contextContainer) {
        PropsParserContext context{previous->getSurfaceId(), *contextContainer};
        auto padding = folly::dynamic::object(horizontal ? "paddingTop" : "paddingLeft", leading)
          (horizontal ? "paddingBottom" : "paddingRight", trailing);
        nextProps = previous->getComponentDescriptor().cloneProps(context, previousProps, RawProps(std::move(padding)));
      }
      auto next = std::static_pointer_cast<YogaLayoutableShadowNode>(previous->clone({
        .props = nextProps, .runtimeShadowNodeReference = false}));
      LayoutConstraints constraints;
      constraints.layoutDirection = getLayoutMetrics().layoutDirection;
      if (horizontal) {
        constraints.minimumSize.height = constraints.maximumSize.height = static_cast<Float>(cross);
      } else {
        constraints.minimumSize.width = constraints.maximumSize.width = static_cast<Float>(cross);
      }
      auto measurementContext = layoutContext;
      std::vector<const LayoutableShadowNode*> affectedNodes;
      measurementContext.affectedNodes = layoutContext.affectedNodes ? &affectedNodes : nullptr;
      next->layoutTree(measurementContext, constraints);
      coreLock.lock();
      // Another clone may have applied new source or constraints while Yoga measured this one.
      if (sourceProps != geometryCache_->keysProps || numberOfColumns != core.numberOfColumns ||
          geometryVersion != core.state.geometryVersion ||
          rowSpansVersion != geometryCache_->rowSpansVersion ||
          columnPaddings != geometryCache_->columnPaddings || core.horizontal != horizontal ||
          core.indexOfKey(snapshot.key) != mounted.rowIndex) {
        return false;
      }
      const auto& current = core.getRowAtIndex(mounted.rowIndex);
      if (current.width != snapshot.width || current.height != snapshot.height ||
          current.column != snapshot.column || current.columnSpan != snapshot.columnSpan ||
          windowCross != (horizontal ? core.revision.windowHeight : core.revision.windowWidth)) {
        return false;
      }
      measurements.push_back({mountedIndex, previous, std::move(next), std::move(affectedNodes)});
    }
  }
  // Unlocked work can also allow an unchanged child to receive a newer measured size.
  for (std::size_t position = 0; position < rows.size(); ++position) {
    const auto& row = rows[position];
    const auto index = core.indexOfKey(row.key);
    if (index != slots.mountedRows[position].rowIndex) return false;
    const auto& current = core.getRowAtIndex(index);
    if (current.width != row.width || current.height != row.height ||
        current.column != row.column || current.columnSpan != row.columnSpan) return false;
  }
  // Keep the tree and its raw layout-event pointers untouched until every measurement is accepted.
  for (auto& measurement : measurements) {
    if (layoutContext.affectedNodes) {
      layoutContext.affectedNodes->insert(layoutContext.affectedNodes->end(),
        measurement.affectedNodes.begin(), measurement.affectedNodes.end());
    }
    auto& mounted = slots.mountedRows[measurement.mountedIndex];
    replacedChildren_.push_back(measurement.previous);
    suppressRowSizeFeedback_ = true;
    replaceChild(*measurement.previous, measurement.next, mounted.childIndex);
    suppressRowSizeFeedback_ = false;
    mounted.node = measurement.next.get();
  }
  for (const auto& mounted : slots.mountedRows) {
    const auto& size = mounted.node->getLayoutMetrics().frame.size;
    measuredRows.push_back({mounted.rowIndex, size.width, size.height,
      static_cast<std::uint64_t>(mounted.node->getTag())});
  }
  std::vector<std::uint64_t> firstMeasured;
  azimgd::shadowlist::applyMeasuredRows(core, measuredRows, horizontal, firstMeasured);
  for (auto tag : firstMeasured) {
    firstMeasuredTags_.push_back(static_cast<Tag>(tag));
  }
  return true;
}

void ShadowListViewShadowNode::placeRows(
  azimgd::shadowlist::Container& core,
  const std::vector<MountedRow>& mountedRows,
  bool horizontal,
  LayoutContext& layoutContext) {
  auto& geometry = *geometryCache_;
  auto& concealed = geometry.concealedRows;

  /*
   * Row hiding, see azimgd::shadowlist::ConcealTracker. The correction this pass publishes is
   * final here, resolveStateUpdate below only reads it.
   */
  const auto& inputStateData = getStateData();
  bool correcting = core.state.offsetCorrected;
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
  for (const auto& mounted : mountedRows) {
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
        SHADOWLIST_LOG("  reveal: tag=%d gen=%llu ack=%llu passes=%zu correcting=%d",
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
    } else if (mounted.rowIndex < concealBeforeIndex &&
               std::binary_search(firstMeasuredTags_.begin(), firstMeasuredTags_.end(), tag)) {
      auto concealedProps = concealedPropsForRow(*previousChild);
      if (concealedProps != nullptr) {
        [[maybe_unused]] auto generation = concealed.hide(static_cast<std::uint64_t>(tag), {previousChild->getProps(), concealedProps});
        SHADOWLIST_LOG("  conceal: tag=%d index=%zu anchorIndex=%zu gen=%llu",
          tag, mounted.rowIndex, concealBeforeIndex, static_cast<unsigned long long>(generation));
        stillConcealedTags.push_back(static_cast<std::uint64_t>(tag));
        nextProps = std::move(concealedProps);
      }
    }

    LayoutMetrics layoutMetrics = previousLayoutableChild.getLayoutMetrics();
    const LayoutMetrics previousLayoutMetrics = layoutMetrics;
    auto frame = azimgd::shadowlist::rowFrame(core, mounted.rowIndex, horizontal);
    layoutMetrics.frame.origin.x = frame.x;
    layoutMetrics.frame.origin.y = frame.y;
    if (frame.setsWidth) {
      layoutMetrics.frame.size.width = frame.width;
    }
    if (frame.setsHeight) {
      layoutMetrics.frame.size.height = frame.height;
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

bool ShadowListViewShadowNode::hasPendingCommand() const {
  const auto& core = *containerManager_;
  if (core.hasPendingCommand() || core.state.measuredRangeStale) return true;
  const auto& state = getStateData();
  if (state.mountDriver_ && state.mountDriver_->isPending()) return true;
  const auto& props = getConcreteProps();
  if (props.mountedLow >= 0 && props.mountedHigh >= props.mountedLow) {
    const auto high = static_cast<std::size_t>(props.mountedHigh);
    for (auto index = static_cast<std::size_t>(props.mountedLow); index <= high && index < core.getRowCount(); ++index) {
      if (!core.getRowAtIndex(index).measured) return true;
    }
  }
  return false;
}

void ShadowListViewShadowNode::publishLayoutState(azimgd::shadowlist::Container& core, double headerSize, double footerSize) {
  auto& geometry = *geometryCache_;
  auto& concealed = geometry.concealedRows;

  /*
   * Work out what to publish. The core decides if the content size changed and if it wants
   * to move the scroll view. The offset is only written then. We never fight the user. The
   * state is only copied when something is published.
   */
  const auto& currentStateData = getStateData();
  auto stateUpdate = core.resolveStateUpdate(
    currentStateData.offsetX_,
    currentStateData.offsetY_,
    currentStateData.contentWidth_,
    currentStateData.contentHeight_);

  // Sticky header and snap positions, rebuilt only when the row geometry moved.
  auto& published = geometry.published;
  published.refresh(core);

  // Compare pointers. This stays cheap because the cache only takes a new one when the values changed.
  bool stickyChanged =
    published.stickyIndices != currentStateData.stickyIndices_ ||
    published.stickyOffsets != currentStateData.stickyOffsets_ ||
    published.stickySizes != currentStateData.stickySizes_;
  bool snapChanged = published.snapOffsets != currentStateData.snapOffsets_;

  double concealGeneration = concealed.getPublishedGeneration();
  bool concealChanged = currentStateData.concealGeneration_ != concealGeneration;

  // The offsets the host can scroll through without a state update, see ShadowListOffsetBand.h.
  auto offsetBand = shadowListOffsetBand(*this);
  bool bandChanged = !shadowListOffsetBandPublished(currentStateData, offsetBand);

  SHADOWLIST_LOG("layout: cellChildren=%zu hdr=%.1f ftr=%.1f stateOffset=(%.1f,%.1f) coreOffset=(%.1f,%.1f) total=(%.1f,%.1f) applyOffset=%d changed=%d",
    getChildren().size(), headerSize, footerSize,
    currentStateData.offsetX_, currentStateData.offsetY_,
    stateUpdate.offsetX, stateUpdate.offsetY,
    stateUpdate.contentWidth, stateUpdate.contentHeight,
    stateUpdate.applyOffset ? 1 : 0, stateUpdate.changed ? 1 : 0);

  // A new animated command's estimate, see ShadowListViewGeometryCache::animationSequence.
  bool animationChanged = currentStateData.animationTargetSequence_ != geometry.animationSequence;
  bool commandPending = hasPendingCommand();
  bool commandPendingChanged = currentStateData.commandPending_ != commandPending;

  if (stateUpdate.changed || stickyChanged || snapChanged || concealChanged || bandChanged || animationChanged || commandPendingChanged) {
    auto nextStateData = currentStateData;
    auto scrollState = nextStateData.scrollState();
    if (azimgd::shadowlist::publishStateUpdate(scrollState, stateUpdate)) {
      nextStateData.setScrollState(scrollState);
    }
    /*
     * Copy, don't move, since the next layout reuses the cache. It's just a shared pointer
     * and the list never changes once published. Sharing it is safe.
     */
    if (stickyChanged) {
      nextStateData.stickyIndices_ = published.stickyIndices;
      nextStateData.stickyOffsets_ = published.stickyOffsets;
      nextStateData.stickySizes_ = published.stickySizes;
    }
    if (snapChanged) {
      nextStateData.snapOffsets_ = published.snapOffsets;
    }
    nextStateData.concealGeneration_ = concealGeneration;
    nextStateData.offsetBandLow_ = offsetBand.low;
    nextStateData.offsetBandHigh_ = offsetBand.high;
    nextStateData.commandPending_ = commandPending;
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
  const ShadowNode& previousChildShadowNode,
  const std::shared_ptr<const ShadowNode>& nextChildShadowNode,
  std::size_t suggestedIndex) {
  /*
   * Send measured row sizes to the core. Look rows up by key, or a child behind a prepend
   * would write its size onto whatever row now has its previous index. Take the core lock,
   * since this can run at the same time as the commit phase.
   */
  if (suppressRowSizeFeedback_) {
    // layout() already gave the core these sizes. Skip the frame it just wrote.
  } else if (const auto cellViewProps = dynamic_cast<const ShadowListCellViewProps*>(nextChildShadowNode->getProps().get())) {
    if (containerManager_) {
      std::lock_guard<std::recursive_mutex> lock(containerManager_->coreMutex);

      // Look up the index under the lock, since a stale child can arrive before the data catches up.
      std::size_t rowIndex = cellViewProps->rowKey.empty() ? azimgd::shadowlist::UNDEFINED_INDEX
                                                           : containerManager_->indexOfKey(cellViewProps->rowKey);
      const auto cellViewNode = dynamic_cast<const YogaLayoutableShadowNode*>(nextChildShadowNode.get());
      const auto cellViewNodeSize = cellViewNode
        ? cellViewNode->getLayoutMetrics().frame.size
        : Size{};
      /*
       * A zero size means Yoga hasn't laid the row out yet. Recording 0 would collapse the
       * row and the content under the reader would jump by its size. Let the layout pass
       * measure it from the real frame instead.
       */
      bool laidOut = (containerManager_->horizontal ? cellViewNodeSize.width : cellViewNodeSize.height) > 0.0;
      if (rowIndex < containerManager_->getRowCount() && cellViewNode && laidOut) {
        bool firstMeasurement = !containerManager_->getRowAtIndex(rowIndex).measured;

        azimgd::shadowlist::Virtualizer::updateRowAtIndex(
          *containerManager_,
          rowIndex,
          azimgd::shadowlist::measuredRowSize(*containerManager_,
            {rowIndex, cellViewNodeSize.width, cellViewNodeSize.height, static_cast<std::uint64_t>(nextChildShadowNode->getTag())},
            containerManager_->horizontal));

        if (firstMeasurement) {
          firstMeasuredTags_.push_back(nextChildShadowNode->getTag());
        }
      }
    }
  }

  YogaLayoutableShadowNode::replaceChild(previousChildShadowNode, nextChildShadowNode, suggestedIndex);
}

}
