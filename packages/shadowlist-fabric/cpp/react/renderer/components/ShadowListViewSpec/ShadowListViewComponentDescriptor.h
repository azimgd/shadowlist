#pragma once

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>

#include "ShadowListOffsetBand.h"
#include "ShadowListTextMeasurer.h"
#include "ShadowListTrace.h"
#include "ShadowListViewShadowNode.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/DragReorder.hpp>
#include <shadowlist-core/host/ListCommit.hpp>

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
    textLayoutManager_(getSharedTextLayoutManager(this->contextContainer_)) {};

  /*
   * Counts commits for the perf suite. Layout only clones are skipped.
   */
  std::shared_ptr<ShadowNode> cloneShadowNode(const ShadowNode& sourceShadowNode, const ShadowNodeFragment& fragment)
    const override {
    auto shadowNode = ConcreteComponentDescriptor::cloneShadowNode(sourceShadowNode, fragment);
    if (fragment.props || fragment.children || fragment.state) {
      SL_TRACE_COMMIT(shadowNode->getTag());
    }
    return shadowNode;
  }

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);

    // Debug trace, does nothing unless the app was launched with SHADOWLIST_FRAME_TRACE=1.
    if (!jsTraceInstalled_.exchange(true, std::memory_order_relaxed)) {
      shadowlist::detail::installJsTrace(this->contextContainer_);
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
    /*
     * The callbacks hold the family's event emitter, so they only need building again when
     * the emitter or the listened events change. Rebuilding five std::function objects on
     * every commit, scroll frames included, was pure allocation.
     */
    const auto& callbacksCache = shadowlistViewShadowNode.getGeometryCache();
    const auto& eventEmitter = shadowNode.getEventEmitter();
    bool callbacksCurrent = callbacksCache && callbacksCache->callbacksEmitter == eventEmitter &&
      callbacksCache->callbacksViewable == shadowlistViewProps.viewableEventEnabled &&
      callbacksCache->callbacksScroll == shadowlistViewProps.scrollEventEnabled;
    if (!callbacksCurrent) {
      auto emitter = std::static_pointer_cast<const ShadowListViewShadowNode::ConcreteEventEmitter>(eventEmitter);
      containerManager->onStartReachedCallback = [emitter]() -> void {
        emitter->onStartReached({});
      };
      containerManager->onEndReachedCallback = [emitter]() -> void {
        emitter->onEndReached({});
      };
      containerManager->onVisibleIndicesChangeCallback = [emitter](std::size_t startIndex, std::size_t endIndex) -> void {
        int visibleStartIndex = static_cast<int>(startIndex);
        int visibleEndIndex = static_cast<int>(endIndex);
        emitter->dispatchUniqueEvent("visibleIndicesChange",
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
        containerManager->onViewableIndicesChangeCallback = [emitter](std::size_t startIndex, std::size_t endIndex) -> void {
          int viewableStartIndex = static_cast<int>(startIndex);
          int viewableEndIndex = static_cast<int>(endIndex);
          emitter->dispatchUniqueEvent("viewableIndicesChange",
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
        containerManager->onScrollCallback = [emitter](double containerOffsetX, double containerOffsetY) -> void {
          emitter->dispatchUniqueEvent("scroll",
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

      if (callbacksCache) {
        callbacksCache->callbacksEmitter = eventEmitter;
        callbacksCache->callbacksViewable = shadowlistViewProps.viewableEventEnabled;
        callbacksCache->callbacksScroll = shadowlistViewProps.scrollEventEnabled;
      }
    }
    /*
     * setStartReachedEnabled and setEndReachedEnabled write these flags into state, and the
     * core checks them before firing the reached callbacks.
     */
    containerManager->setStartReachedEnabled(shadowlistViewStateData.startReachedEnabled_);
    containerManager->setEndReachedEnabled(shadowlistViewStateData.endReachedEnabled_);

    /*
     * Tell JS when a drag starts or ends. The platform view bumps the sequence only on pick
     * up and drop, since finger tracking stays native. Fire once per new sequence.
     */
    if (shadowlistViewStateData.dragEventSequence_ != containerManager->lastDragEventSequence) {
      containerManager->lastDragEventSequence = shadowlistViewStateData.dragEventSequence_;
      const std::string& dragFromKey = shadowlistViewStateData.dragFromKey_;
      const std::string& dragToKey = shadowlistViewStateData.dragToKey_;
      switch (static_cast<int>(shadowlistViewStateData.dragEventType_)) {
        case azimgd::shadowlist::DRAG_EVENT_START:
          shadowlistViewEventEmitter.onDragStart({ .key = dragFromKey });
          break;
        case azimgd::shadowlist::DRAG_EVENT_END:
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

    azimgd::shadowlist::FrameInput input;
    /*
     * Point the core at the props' keys instead of copying them. Props never change and
     * outlive this call. Copying cost milliseconds per commit on a large chat list.
     */
    input.keysRef = &shadowlistViewProps.elementsAllKeys;
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
    input.keysUnchanged = geometryCache && geometryCache->keysProps == currentProps;
    // The same props also mean the same anchor ignore keys.
    input.nonAnchorableKeysUnchanged = geometryCache && geometryCache->keysProps == currentProps;
    input.windowContainerWidth = shadowlistViewLayoutMetrics.frame.size.width;
    input.windowContainerHeight = shadowlistViewLayoutMetrics.frame.size.height;
    // The layout pass writes the header and footer sizes into the core, so these are current.
    input.headerSize = containerManager->headerSize;
    input.footerSize = containerManager->footerSize;
    /*
     * SectionList header indices, converted once per props and lent to the core, since they
     * only change with the props.
     */
    if (geometryCache) {
      if (geometryCache->stickyIndicesProps != currentProps) {
        azimgd::shadowlist::stickyIndicesFromProps(shadowlistViewProps.stickyHeaderIndices, geometryCache->stickyIndices);
        geometryCache->stickyIndicesProps = currentProps;
      }
      input.stickyIndicesRef = &geometryCache->stickyIndices;
    } else {
      azimgd::shadowlist::stickyIndicesFromProps(shadowlistViewProps.stickyHeaderIndices, input.stickyIndices);
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

    // Offset, echoed token, user scroll flag and gesture phase, see applyHostScroll.
    azimgd::shadowlist::applyHostScroll(input, shadowlistViewStateData.scrollState());

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
         (geometryCache->published.snapOffsets != shadowlistViewStateData.snapOffsets_ ||
          geometryCache->published.stickyHeaderIndices != shadowlistViewStateData.stickyHeaderIndices_ ||
          geometryCache->published.stickyHeaderOffsets != shadowlistViewStateData.stickyHeaderOffsets_ ||
          geometryCache->published.stickyHeaderSizes != shadowlistViewStateData.stickyHeaderSizes_));
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
      }
    } catch (...) {
      if (geometryCache) {
        geometryCache->keysProps = nullptr;
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
    ShadowListLiveScroll::Report report;
    if (!liveScroll || !liveScroll->newerReport(stateData.hostSequence_, stateData.containerOffsetEnabled_, report)) {
      return false;
    }
    SL_LOG("adopt: live report seq=%llu over state seq=%.0f off=(%.1f,%.1f)->(%.1f,%.1f) token=%.0f phase=%.0f",
      static_cast<unsigned long long>(report.sequence), stateData.hostSequence_,
      stateData.containerOffsetX_, stateData.containerOffsetY_, report.offsetX, report.offsetY,
      report.commitToken, report.scrollPhase);
    ShadowListViewState nextStateData = stateData;
    nextStateData.applyLiveReport(report);
    listShadowNode.setStateData(std::move(nextStateData));
    return true;
  }

  /*
   * Turn elementsSizeSpecs into predicted sizes for the core, a few rows per commit.
   * See azimgd::shadowlist::SizeSpecQueue.
   */
  void applyElementSizeSpecs(
    ShadowListViewShadowNode& shadowlistViewShadowNode,
    const ShadowListViewShadowNode::ConcreteProps& shadowlistViewProps,
    azimgd::shadowlist::Container* containerManager,
    Float availableWidth,
    Float pointScaleFactor,
    SurfaceId surfaceId) const {
    const auto& geometryCache = shadowlistViewShadowNode.getGeometryCache();
    if (!textLayoutManager_ || !geometryCache || shadowlistViewProps.elementsSizeSpecs.empty()) {
      return;
    }
    const auto& textLayoutManager = *textLayoutManager_;
    geometryCache->sizeSpecs.run(
      *containerManager,
      shadowlistViewShadowNode.getProps(),
      availableWidth,
      [&]() { return parseElementSizeSpecs(shadowlistViewProps.elementsSizeSpecs); },
      [&](const azimgd::shadowlist::ElementSizeSpec& spec, double width) {
        return measureElementSizeSpec(textLayoutManager, spec, width, pointScaleFactor, surfaceId);
      });
  }

  /*
   * Shared with React Native's text rendering if we published it first, or a private one
   * that just misses the shared cache. Null without a ContextContainer, which turns off
   * prediction.
   */
  const std::shared_ptr<const TextLayoutManager> textLayoutManager_;

  mutable std::atomic<bool> jsTraceInstalled_{false};
};

void ShadowListViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
