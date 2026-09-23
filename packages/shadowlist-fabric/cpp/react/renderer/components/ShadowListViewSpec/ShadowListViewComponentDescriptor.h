#pragma once

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>

#include "ShadowListNativeJSI.h"
#include "ShadowListOffsetBand.h"
#include "ShadowListTextMeasurer.h"
#include "ShadowListTrace.h"
#include "ShadowListViewShadowNode.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <atomic>
#include <mutex>

namespace facebook::react {

class ShadowListViewComponentDescriptor final : public ConcreteComponentDescriptor<ShadowListViewShadowNode> {
public:
  ShadowListViewComponentDescriptor(const ComponentDescriptorParameters& parameters) :
    ConcreteComponentDescriptor<ShadowListViewShadowNode>(parameters),
    /*
     * Done in the constructor on purpose. It puts the instance in the ContextContainer so
     * React Native's Paragraph descriptor, if built after us, shares our measure cache.
     * Doing it later at measure time would be too late. See getSharedTextLayoutManager.
     */
    textLayoutManager_(getSharedTextLayoutManager(this->contextContainer_)) {
    // Install the ShadowListNative data API early so JS rarely waits for it.
    installShadowListNativeJSI(this->contextContainer_);
  };

  /*
   * ShadowListNative rows are children that no React component renders. Any clone with new
   * props, children or state matches them to the window the core just picked in adopt(),
   * and commits the result as this node's children.
   *
   * Build a second node instead of editing children in place. A node built with new children
   * gets its Yoga subtree set up by the layout pass like any React child. Editing a node
   * already marked set up would lay the new rows out with Yoga defaults.
   * Layout only clones are left alone.
   */
  std::shared_ptr<ShadowNode> cloneShadowNode(const ShadowNode& sourceShadowNode, const ShadowNodeFragment& fragment)
    const override {
    auto shadowNode = ConcreteComponentDescriptor::cloneShadowNode(sourceShadowNode, fragment);
    if (!fragment.props && !fragment.children && !fragment.state) {
      return shadowNode;
    }
    auto& listShadowNode = static_cast<ShadowListViewShadowNode&>(*shadowNode);
    auto children = reconcileNativeRows(listShadowNode);
    if (!children) {
      return shadowNode;
    }
    auto nextShadowNode = std::make_shared<ShadowListViewShadowNode>(*shadowNode, ShadowNodeFragment{.children = children});
    shadowNode->transferRuntimeShadowNodeReference(nextShadowNode, fragment);
    return nextShadowNode;
  }

  /*
   * React appends a list's children one by one. Put the rows right after the
   * ShadowListNative templates container, same as cloneShadowNode does.
   */
  void appendChild(
    const std::shared_ptr<const ShadowNode>& parentShadowNode,
    const std::shared_ptr<const ShadowNode>& childShadowNode) const override {
    ConcreteComponentDescriptor::appendChild(parentShadowNode, childShadowNode);
    const auto templateProps = std::dynamic_pointer_cast<const ShadowListTemplateViewProps>(childShadowNode->getProps());
    if (!templateProps || templateProps->templateType != "native") {
      return;
    }
    auto& listShadowNode = const_cast<ShadowListViewShadowNode&>(static_cast<const ShadowListViewShadowNode&>(*parentShadowNode));
    auto children = reconcileNativeRows(listShadowNode);
    if (!children) {
      return;
    }
    const auto& current = listShadowNode.getChildren();
    // The container was just appended last, so only rows can follow it.
    if (children->size() < current.size() || !std::equal(current.begin(), current.end(), children->begin())) {
      return;
    }
    for (std::size_t index = current.size(); index < children->size(); ++index) {
      listShadowNode.appendChild((*children)[index]);
    }
  }

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);

    // Debug trace, does nothing unless the app was launched with SHADOWLIST_FRAME_TRACE=1.
    if (!jsTraceInstalled_.exchange(true, std::memory_order_relaxed)) {
      shadowlist::detail::installJsTrace(this->contextContainer_);
    }

    if (!nativeJsiInstalled_.exchange(true, std::memory_order_relaxed)) {
      installShadowListNativeJSI(this->contextContainer_);
    }

    auto& shadowlistViewShadowNode = static_cast<ShadowListViewShadowNode&>(shadowNode);

    /*
     * Create the core for a list's first node. Clones carry it along, so one core is shared
     * by all clones and freed with the node family. No registry here that could leak.
     */
    if (!shadowlistViewShadowNode.getContainerManager()) {
      shadowlistViewShadowNode.setContainerManager(std::make_shared<azimgd::shadowlist::Container>());
      shadowlistViewShadowNode.setGeometryCache(std::make_shared<ShadowListViewGeometryCache>());
    }

    auto& shadowlistViewProps = static_cast<const ShadowListViewShadowNode::ConcreteProps&>(*shadowNode.getProps());
    /*
     * A React commit clones the list with the state React last saw, which can be several
     * reports old, for example still holding the offset from before a correction. Fabric
     * catches up later in the commit, but update() would already have anchored on the old
     * offset and moved the content by the difference. So read the newest state instead.
     */
    const auto& adoptedState = shadowNode.getState();
    const auto newerState = adoptedState->getMostRecentStateIfObsolete();
    auto& shadowlistViewState = static_cast<const ShadowListViewShadowNode::ConcreteState&>(
      newerState ? *newerState : *adoptedState);
    auto& shadowlistViewEventEmitter = static_cast<const ShadowListViewShadowNode::ConcreteEventEmitter&>(*shadowNode.getEventEmitter());

    /*
     * The host only sends a state update when its offset leaves the published band, but it
     * writes every frame into the live report. If that report is newer than this state, this
     * commit runs on it, so the core sees where the screen really is, see ShadowListLiveScroll.
     * The report goes into this node's state too, so the layout pass starts a correction from
     * the same offset the core used. A state with an offset of our own to apply, like a scroll
     * command, is left alone.
     */
    bool tookLiveReport = adoptLiveScrollReport(shadowlistViewShadowNode, shadowlistViewState.getData());

    /*
     * Take a reference. A copy of the state costs allocations and refcount bumps on every
     * commit, including every scroll frame, and we only read it.
     */
    const auto& shadowlistViewStateData =
      tookLiveReport ? shadowlistViewShadowNode.getStateData() : shadowlistViewState.getData();
    if (newerState) {
      SL_LOG("adopt: obsolete state rev=%zu -> newest rev=%zu off=(%.1f,%.1f)",
        adoptedState->getRevision(), newerState->getRevision(),
        shadowlistViewStateData.containerOffsetX_, shadowlistViewStateData.containerOffsetY_);
    }
    auto shadowlistViewLayoutMetrics = static_cast<YogaLayoutableShadowNode&>(shadowNode).getLayoutMetrics();

    auto containerManager = shadowlistViewShadowNode.getContainerManager().get();

    /*
     * For ShadowListNative, attach the native store and snapshot its keys. The core and the
     * mounted rows both use this snapshot, so a row never mounts for a key the core doesn't know.
     */
    ShadowListNativeEngine::KeysSnapshot nativeKeys;
    if (!shadowlistViewProps.nativeListId.empty()) {
      if (!shadowlistViewShadowNode.getNativeEngine() ||
          shadowlistViewShadowNode.getNativeEngine()->listId() != shadowlistViewProps.nativeListId) {
        shadowlistViewShadowNode.setNativeEngine(ShadowListNativeRegistry::obtain(shadowlistViewProps.nativeListId));
      }
      const auto& nativeEngine = shadowlistViewShadowNode.getNativeEngine();
      nativeEngine->attachState(std::static_pointer_cast<const ShadowListViewShadowNode::ConcreteState>(shadowNode.getState()));
      nativeKeys = nativeEngine->keysSnapshot();
      shadowlistViewShadowNode.setNativeKeys(nativeKeys.keys);
    }

    /*
     * Lock the shared core for this commit. adopt() can run at the same time as layout,
     * replaceChild or update on another clone of the same list. The mutex is recursive
     * because update() takes it again.
     */
    std::lock_guard<std::recursive_mutex> coreLock(containerManager->coreMutex);

    /*
     * Pass core events to the event emitter. The core already drops repeats.
     *
     * The three per frame events below skip the generated emitter methods and use
     * dispatchUniqueEvent, which marks them unique and continuous, like React Native's own
     * ScrollView scroll event. The generated methods cause two problems:
     * Events are not merged, so when JS falls behind, like while dragging the scroll
     * indicator, the queue grows for the whole gesture. That backlog is the long freeze.
     * Events are sent as discrete, so React renders each one synchronously. Dragging content
     * is fine, but the scroll indicator, a macOS scroller and iOS momentum frames all turned
     * into one blocking render per frame.
     * Event names stay the same either way.
     *
     * Merging only looks at the last event queued for this view, so sending several kinds of
     * event per frame defeats it. That's another reason scroll and viewable only fire when
     * someone listens.
     */
    containerManager->onStartReachedCallback = [shadowlistViewEventEmitter]() -> void {
      shadowlistViewEventEmitter.onStartReached({});
    };
    containerManager->onEndReachedCallback = [shadowlistViewEventEmitter]() -> void {
      shadowlistViewEventEmitter.onEndReached({});
    };
    /*
     * setStartReachedEnabled and setEndReachedEnabled write these flags into state, and the
     * core checks them before firing the reached callbacks.
     */
    containerManager->setStartReachedEnabled(shadowlistViewStateData.startReachedEnabled_);
    containerManager->setEndReachedEnabled(shadowlistViewStateData.endReachedEnabled_);
    containerManager->onVisibleIndicesChangeCallback = [shadowlistViewEventEmitter](std::size_t startIndex, std::size_t endIndex) -> void {
      int visibleStartIndex = static_cast<int>(startIndex);
      int visibleEndIndex = static_cast<int>(endIndex);
      shadowlistViewEventEmitter.dispatchUniqueEvent("visibleIndicesChange",
        [visibleStartIndex, visibleEndIndex](jsi::Runtime& runtime) {
          auto payload = jsi::Object(runtime);
          payload.setProperty(runtime, "visibleStartIndex", visibleStartIndex);
          payload.setProperty(runtime, "visibleEndIndex", visibleEndIndex);
          return payload;
        });
    };

    /*
     * Only track what JS listens to. Otherwise every frame pays for a viewable scan and
     * an event that nobody handles.
     */
    if (shadowlistViewProps.viewableEventEnabled) {
      containerManager->onViewableIndicesChangeCallback = [shadowlistViewEventEmitter](std::size_t startIndex, std::size_t endIndex) -> void {
        int viewableStartIndex = static_cast<int>(startIndex);
        int viewableEndIndex = static_cast<int>(endIndex);
        shadowlistViewEventEmitter.dispatchUniqueEvent("viewableIndicesChange",
          [viewableStartIndex, viewableEndIndex](jsi::Runtime& runtime) {
            auto payload = jsi::Object(runtime);
            payload.setProperty(runtime, "viewableStartIndex", viewableStartIndex);
            payload.setProperty(runtime, "viewableEndIndex", viewableEndIndex);
            return payload;
          });
      };
    } else {
      containerManager->onViewableIndicesChangeCallback = nullptr;
    }

    if (shadowlistViewProps.scrollEventEnabled) {
      containerManager->onScrollCallback = [shadowlistViewEventEmitter](double containerOffsetX, double containerOffsetY) -> void {
        shadowlistViewEventEmitter.dispatchUniqueEvent("scroll",
          [containerOffsetX, containerOffsetY](jsi::Runtime& runtime) {
            auto payload = jsi::Object(runtime);
            payload.setProperty(runtime, "contentOffsetX", containerOffsetX);
            payload.setProperty(runtime, "contentOffsetY", containerOffsetY);
            return payload;
          });
      };
    } else {
      containerManager->onScrollCallback = nullptr;
    }

    /*
     * Tell JS when a drag starts or ends. The platform view bumps the sequence only on pick
     * up and drop, since finger tracking stays native. Fire once per new sequence.
     * Type 1 is pick up and 3 is drop.
     */
    if (shadowlistViewStateData.dragEventSequence_ != containerManager->lastDragEventSequence) {
      containerManager->lastDragEventSequence = shadowlistViewStateData.dragEventSequence_;
      const std::string& dragFromKey = shadowlistViewStateData.dragFromKey_;
      const std::string& dragToKey = shadowlistViewStateData.dragToKey_;
      switch (static_cast<int>(shadowlistViewStateData.dragEventType_)) {
        case 1:
          shadowlistViewEventEmitter.onDragStart({ .key = dragFromKey });
          break;
        case 3:
          shadowlistViewEventEmitter.onDragEnd({ .fromKey = dragFromKey, .toKey = dragToKey });
          break;
        default:
          break;
      }
    }

    /*
     * Handle a pending scrollToIndex. The command writes its target into state and the
     * prop gives the initial index. The core picks which wins and scrolls once per target.
     */
    containerManager->requestScrollToIndex(
      shadowlistViewStateData.containerOffsetIndex_,
      shadowlistViewStateData.containerOffsetIndexSequence_,
      shadowlistViewProps.containerOffsetIndex,
      shadowlistViewStateData.containerOffsetIndexViewPosition_);

    // Run ShadowListNative scroll commands once the rows they wait for are laid out.
    bool nativeScrollCommand = shadowlistViewShadowNode.getNativeEngine() &&
      shadowlistViewShadowNode.getNativeEngine()->applyPendingScroll(*containerManager, nativeKeys.version);

    azimgd::shadowlist::FrameInput input;
    /*
     * Point the core at the props' keys instead of copying them. Props never change and
     * outlive this call. Copying cost milliseconds per commit on a large chat list.
     */
    input.keysRef = nativeKeys.keys ? nativeKeys.keys.get() : &shadowlistViewProps.elementsAllKeys;
    /*
     * Keys of decoration rows like date pills and dividers that the core must never use as
     * the anchor for keeping content in place. An empty list means any row can be the anchor.
     */
    input.nonAnchorableKeysRef = &shadowlistViewProps.elementsAnchorIgnoreKeys;

    /*
     * A scroll keeps the same props, so the same props pointer means the same keys.
     * The cache holds the old props so the address can't be reused. This lets the core
     * skip comparing every key on scroll frames.
     */
    auto geometryCache = shadowlistViewShadowNode.getGeometryCache();
    const auto& currentProps = shadowNode.getProps();
    input.keysUnchanged = nativeKeys.keys
      ? geometryCache && geometryCache->nativeKeysVersion == nativeKeys.version
      : geometryCache && geometryCache->keysProps == currentProps;
    input.containerOffsetX = shadowlistViewStateData.containerOffsetX_;
    input.containerOffsetY = shadowlistViewStateData.containerOffsetY_;
    input.containerOffsetEnabled = shadowlistViewStateData.containerOffsetEnabled_;
    input.windowContainerWidth = shadowlistViewLayoutMetrics.frame.size.width;
    input.windowContainerHeight = shadowlistViewLayoutMetrics.frame.size.height;
    // The layout pass writes the header and footer sizes into the core, so these are current.
    input.headerSize = containerManager->headerSize;
    input.footerSize = containerManager->footerSize;
    // SectionList header indices. Skip negatives, the core wants valid ascending indices.
    input.stickyIndices.reserve(shadowlistViewProps.stickyHeaderIndices.size());
    for (auto stickyHeaderIndex : shadowlistViewProps.stickyHeaderIndices) {
      if (stickyHeaderIndex >= 0) {
        input.stickyIndices.push_back(static_cast<std::size_t>(stickyHeaderIndex));
      }
    }
    input.inverted = shadowlistViewProps.inverted;
    input.followAppends = shadowlistViewProps.followAppends;
    input.horizontal = shadowlistViewProps.horizontal;
    input.columns = shadowlistViewProps.columns > 0 ? static_cast<std::size_t>(shadowlistViewProps.columns) : 1;
    input.overscan = shadowlistViewProps.overscan;
    input.startReachedThreshold = shadowlistViewProps.startReachedThreshold;
    input.endReachedThreshold = shadowlistViewProps.endReachedThreshold;
    input.viewablePercentThreshold = shadowlistViewProps.viewablePercentThreshold;
    input.snapToItem = shadowlistViewProps.snapToItem;
    input.snapAlignment = shadowlistViewProps.snapToAlignment;

    /*
     * A real user scroll drops any pending correction so the user isn't snapped back.
     * Without it a correction can get stuck and freeze the window, leaving a blank list.
     */
    input.userScrolled = shadowlistViewStateData.userScrolled_;

    /*
     * The phase lasts across reports, so while a finger rests on the list the inverted
     * bottom pin doesn't pull the content under it. See Container::gestureActive.
     */
    input.scrollPhase = shadowlistViewStateData.scrollPhase_ == SCROLL_PHASE_DRAGGING
      ? azimgd::shadowlist::ScrollPhase::Dragging
      : shadowlistViewStateData.scrollPhase_ == SCROLL_PHASE_SETTLING
        ? azimgd::shadowlist::ScrollPhase::Settling
        : azimgd::shadowlist::ScrollPhase::Idle;

    /*
     * A ShadowListNative scroll command stops momentum when the host mounts it, so treat its
     * frame as idle. Otherwise the core would think the fling drives the correction.
     * A finger on the list is different. The drag stays, and it cancels the command.
     */
    bool nativeScrollYieldsMomentum = nativeScrollCommand &&
      input.scrollPhase != azimgd::shadowlist::ScrollPhase::Dragging;
    if (nativeScrollYieldsMomentum) {
      input.userScrolled = false;
      input.scrollPhase = azimgd::shadowlist::ScrollPhase::Idle;
    }

    // The token the host echoed back, so the core can spot its own write. 0 if none.
    input.commitToken = static_cast<std::uint64_t>(shadowlistViewStateData.commitToken_);

    /*
     * Give the core predicted sizes before update(), so this frame picks its window from
     * real sizes instead of estimates.
     */
    applyElementSizeSpecs(
      shadowlistViewShadowNode,
      shadowlistViewProps,
      containerManager,
      shadowlistViewLayoutMetrics.frame.size.width,
      shadowlistViewLayoutMetrics.pointScaleFactor,
      shadowNode.getSurfaceId());

    // If the core throws, skip the frame instead of failing the commit. The next frame recovers.
    try {
      azimgd::shadowlist::Virtualizer::update(containerManager, input);
      if (nativeScrollYieldsMomentum) {
        shadowlistViewShadowNode.getNativeEngine()->setMomentumYieldToken(
          containerManager->operation ? containerManager->operation->id : 0);
      }
      /*
       * Only the layout pass publishes to the host, and a state only commit won't run it.
       * So mark layout dirty when there's a correction or the state has old geometry.
       * Hosts build scroll reports on the state they mounted, which can carry an old content
       * size. Left alone, a fling can coast past the real end into blank space.
       */
      bool geometryStale =
        shadowlistViewStateData.totalContainerWidth_ != containerManager->revision.totalContainerWidth ||
        shadowlistViewStateData.totalContainerHeight_ != containerManager->revision.totalContainerHeight ||
        (geometryCache &&
         (geometryCache->snapOffsets != shadowlistViewStateData.snapOffsets_ ||
          geometryCache->stickyHeaderIndices != shadowlistViewStateData.stickyHeaderIndices_ ||
          geometryCache->stickyHeaderOffsets != shadowlistViewStateData.stickyHeaderOffsets_ ||
          geometryCache->stickyHeaderSizes != shadowlistViewStateData.stickyHeaderSizes_));
      /*
       * The layout pass shows hidden rows again, and the host's echo usually comes in a plain
       * scroll report, so keep layout dirty while any row is hidden.
       */
      bool rowsConcealed = geometryCache && !geometryCache->concealedRows.empty();
      /*
       * The band is also only published by the layout pass. If this frame moved it, like a
       * new window, an edge crossed or rows reconciled, lay out again so the host gets the
       * new one. Otherwise the host would keep sending every frame, or skip frames it needs.
       */
      bool bandStale = !shadowListOffsetBandPublished(shadowlistViewStateData, shadowListOffsetBand(shadowlistViewShadowNode));
      if (containerManager->containerOffsetCorrected || geometryStale || rowsConcealed || bandStale) {
        shadowlistViewShadowNode.dirtyLayout();
      }
      /*
       * Remember these props only after the core took their keys. If a frame throws, the
       * next one has to check the keys again.
       */
      if (geometryCache) {
        geometryCache->keysProps = currentProps;
        geometryCache->nativeKeysVersion = nativeKeys.version;
      }
    } catch (...) {
      if (geometryCache) {
        geometryCache->keysProps = nullptr;
        geometryCache->nativeKeysVersion = 0;
      }
    }
  };

private:
  /*
   * Write the host's live scroll report into the node's state when it is newer than
   * stateData, the state this commit would otherwise run on. Returns whether it did.
   * Only the host owned fields change: offset, echoed token, conceal ack, user scroll flag
   * and gesture phase. The rest of stateData is kept, including the newer state Fabric found.
   */
  static bool adoptLiveScrollReport(ShadowListViewShadowNode& listShadowNode, const ShadowListViewState& stateData) {
    const auto& liveScroll = stateData.liveScroll_;
    if (!liveScroll || stateData.containerOffsetEnabled_) {
      return false;
    }
    auto report = liveScroll->read();
    if (report.sequence == 0 || static_cast<double>(report.sequence) <= stateData.hostSequence_) {
      return false;
    }
    SL_LOG("adopt: live report seq=%llu over state seq=%.0f off=(%.1f,%.1f)->(%.1f,%.1f) token=%.0f phase=%.0f",
      static_cast<unsigned long long>(report.sequence), stateData.hostSequence_,
      stateData.containerOffsetX_, stateData.containerOffsetY_, report.offsetX, report.offsetY,
      report.commitToken, report.scrollPhase);
    ShadowListViewState nextStateData = stateData;
    nextStateData.containerOffsetX_ = report.offsetX;
    nextStateData.containerOffsetY_ = report.offsetY;
    nextStateData.containerOffsetEnabled_ = false;
    nextStateData.commitToken_ = report.commitToken;
    nextStateData.concealGenerationAck_ = report.concealGenerationAck;
    nextStateData.userScrolled_ = report.userScrolled;
    nextStateData.scrollPhase_ = report.scrollPhase;
    nextStateData.hostSequence_ = static_cast<double>(report.sequence);
    listShadowNode.setStateData(std::move(nextStateData));
    return true;
  }

  /*
   * The children a ShadowListNative node should commit with. Null when it already has them,
   * isn't a ShadowListNative, or its templates aren't mounted yet.
   */
  std::shared_ptr<const ShadowListNativeEngine::ChildList> reconcileNativeRows(ShadowListViewShadowNode& listShadowNode) const {
    const auto& nativeEngine = listShadowNode.getNativeEngine();
    const auto& nativeKeys = listShadowNode.getNativeKeys();
    const auto& containerManager = listShadowNode.getContainerManager();
    if (!nativeEngine || !nativeKeys || !containerManager) {
      return nullptr;
    }
    bool hasTemplates = std::any_of(
      listShadowNode.getChildren().begin(), listShadowNode.getChildren().end(), [](const auto& child) {
        const auto templateProps = std::dynamic_pointer_cast<const ShadowListTemplateViewProps>(child->getProps());
        return templateProps && templateProps->templateType == "native";
      });
    if (!hasTemplates) {
      return nullptr;
    }
    const auto& props = listShadowNode.getConcreteProps();
    std::lock_guard<std::recursive_mutex> coreLock(containerManager->coreMutex);
    try {
      return nativeEngine->reconcileRows(
        listShadowNode,
        listShadowNode.getChildren(),
        *nativeKeys,
        *containerManager,
        props.containerOffsetIndex,
        props.inverted);
    } catch (...) {
      // If a row fails to build, keep the mounted rows for this commit.
      return nullptr;
    }
  }

  /*
   * Turn elementsSizeSpecs into predicted sizes for the core. Skipped unless the specs
   * changed, which is almost never.
   * Text wraps to the list width, so do nothing at zero width, and measure again when
   * the width changes.
   */
  void applyElementSizeSpecs(
    ShadowListViewShadowNode& shadowlistViewShadowNode,
    const ShadowListViewShadowNode::ConcreteProps& shadowlistViewProps,
    azimgd::shadowlist::Container* containerManager,
    Float availableWidth,
    Float pointScaleFactor,
    SurfaceId surfaceId) const {
    if (!textLayoutManager_ || shadowlistViewProps.elementsSizeSpecs.empty()) {
      return;
    }

    if (!(availableWidth > 0.0f)) {
      return;
    }

    auto geometryCache = shadowlistViewShadowNode.getGeometryCache();
    const auto& currentProps = shadowlistViewShadowNode.getProps();

    /*
     * A new width makes every predicted height wrong, so drop them all. Rows in these specs
     * are measured again below, and the rest use the estimate until measured natively.
     * The first layout counts as a change too, but there is nothing to clear yet.
     */
    bool widthChanged = containerManager->revision.windowContainerWidth != availableWidth;
    if (widthChanged) {
      azimgd::shadowlist::Virtualizer::invalidatePredictions(containerManager);
    }

    // Stop only when these specs are fully measured. A partly measured set must continue.
    if (geometryCache && geometryCache->sizeSpecsProps == currentProps && !widthChanged &&
        geometryCache->sizeSpecsDone) {
      return;
    }

    /*
     * Pick up where the last commit stopped for the same specs, or start over.
     * See ShadowListViewGeometryCache::sizeSpecsCursor and sizeSpecs.
     */
    bool sameSpecs = geometryCache && geometryCache->sizeSpecsProps == currentProps && !widthChanged;

    std::vector<ShadowListElementSizeSpec> parsedSpecs;
    if (!sameSpecs) {
      parsedSpecs = parseElementSizeSpecs(shadowlistViewProps.elementsSizeSpecs);
    }
    const std::vector<ShadowListElementSizeSpec>& specs =
      (sameSpecs && geometryCache) ? geometryCache->sizeSpecs : parsedSpecs;

    std::size_t cursor = sameSpecs ? geometryCache->sizeSpecsCursor : 0;
    std::size_t measured = 0;

    while (cursor < specs.size() && measured < MEASURE_BUDGET_PER_COMMIT) {
      const auto& spec = specs[cursor];
      containerManager->setPredictedSize(
        spec.key,
        measureElementSizeSpec(*textLayoutManager_, spec, availableWidth, pointScaleFactor, surfaceId));
      ++cursor;
      ++measured;
    }

    if (geometryCache) {
      std::size_t specCount = specs.size();
      if (!sameSpecs) {
        geometryCache->sizeSpecs = std::move(parsedSpecs);
      }
      geometryCache->sizeSpecsProps = currentProps;
      geometryCache->sizeSpecsCursor = cursor;
      geometryCache->sizeSpecsDone = cursor >= specCount;
    }
  }

  /*
   * Text layouts per commit. Small enough to stay inside a frame when nothing is cached,
   * big enough to finish a window in a couple of commits.
   */
  static constexpr std::size_t MEASURE_BUDGET_PER_COMMIT = 24;

  /*
   * Shared with React Native's text rendering if we published it first, or a private one
   * that just misses the shared cache. Null without a ContextContainer, which turns off
   * prediction.
   */
  const std::shared_ptr<const TextLayoutManager> textLayoutManager_;

  mutable std::atomic<bool> jsTraceInstalled_{false};
  mutable std::atomic<bool> nativeJsiInstalled_{false};
};

void ShadowListViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
