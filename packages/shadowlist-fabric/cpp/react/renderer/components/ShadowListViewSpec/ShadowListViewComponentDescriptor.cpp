#include "ShadowListViewComponentDescriptor.h"

namespace facebook::react {

void ShadowListViewComponentDescriptor::adopt(ShadowNode& shadowNode) const {
  ConcreteComponentDescriptor::adopt(shadowNode);

  // Debug trace, does nothing unless the app was launched with SHADOWLIST_FRAME_TRACE=1.
  if (!jsTraceInstalled_.exchange(true, std::memory_order_relaxed)) {
    shadowlist::detail::installJsTrace(contextContainer_);
  }

  auto& shadowlistViewShadowNode = static_cast<ShadowListViewShadowNode&>(shadowNode);

  /*
   * Create the core for a list's first node. Clones carry it along. One core is shared
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
   * commit runs on it. The core then sees where the screen really is, see ShadowListLiveScroll.
   * The report goes into this node's state too. The layout pass starts a correction from
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

  applyEventCallbacks(shadowlistViewShadowNode, shadowlistViewProps, containerManager);

  const auto& callbacksCache = shadowlistViewShadowNode.getGeometryCache();
  const auto& eventEmitter = shadowNode.getEventEmitter();

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
  if (shadowlistViewStateData.dragEventSequence_ != containerManager->previousDragEventSequence) {
    containerManager->previousDragEventSequence = shadowlistViewStateData.dragEventSequence_;
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
   * Handle a pending scrollToItem. The command writes its target into state and the
   * prop gives the initial index. The core picks which wins and scrolls once per target.
   * An animated command only gets an estimate here, which the layout pass publishes. The
   * host animates there and sends the same command again without the animation.
   */
  bool animatedCommand = shadowlistViewStateData.containerOffsetIndexAnimated_;
  if (animatedCommand && callbacksCache &&
      shadowlistViewStateData.containerOffsetIndexSequence_ > callbacksCache->animationSequence) {
    auto target = azimgd::shadowlist::commandTargetOffset(
      *containerManager,
      shadowlistViewStateData.containerOffsetIndex_,
      shadowlistViewStateData.containerOffsetIndexViewPosition_,
      shadowlistViewStateData.containerOffsetIndexRowOffset_);
    if (target) {
      callbacksCache->animationSequence = shadowlistViewStateData.containerOffsetIndexSequence_;
      callbacksCache->animationOffset = *target;
    }
  }
  containerManager->requestScrollToRow(
    shadowlistViewStateData.containerOffsetIndex_,
    animatedCommand ? 0.0 : shadowlistViewStateData.containerOffsetIndexSequence_,
    shadowlistViewProps.containerOffsetIndex,
    shadowlistViewStateData.containerOffsetIndexViewPosition_,
    shadowlistViewStateData.containerOffsetIndexRowOffset_);

  /*
   * Answer an anchor request once, with the row at the viewport start of the offset this
   * commit runs on.
   */
  if (callbacksCache && shadowlistViewStateData.anchorRequestSequence_ > callbacksCache->anchorRequestSequence) {
    callbacksCache->anchorRequestSequence = shadowlistViewStateData.anchorRequestSequence_;
    double offset = shadowlistViewProps.horizontal ? shadowlistViewStateData.containerOffsetX_
                                                   : shadowlistViewStateData.containerOffsetY_;
    auto anchor = azimgd::shadowlist::anchorStateAt(*containerManager, offset);
    ShadowListViewEventEmitter::OnAnchorState event;
    event.found = anchor.has_value();
    event.key = anchor ? anchor->key : std::string();
    event.offset = anchor ? anchor->offset : 0.0;
    shadowlistViewEventEmitter.onAnchorState(event);
  }

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
  input.nonAnchorKeysRef = &shadowlistViewProps.elementsAnchorIgnoreKeys;

  /*
   * A scroll keeps the same props. The same props pointer means the same keys.
   * The cache holds the previous props so the address can't be reused. This lets the core
   * skip comparing every key on scroll frames.
   */
  auto geometryCache = shadowlistViewShadowNode.getGeometryCache();
  const auto& currentProps = shadowNode.getProps();
  input.keysUnchanged = geometryCache && geometryCache->keysProps == currentProps;
  // The same props also mean the same anchor ignore keys.
  input.nonAnchorKeysUnchanged = geometryCache && geometryCache->keysProps == currentProps;
  input.windowWidth = shadowlistViewLayoutMetrics.frame.size.width;
  input.windowHeight = shadowlistViewLayoutMetrics.frame.size.height;
  // These are current because the layout pass writes the header and footer sizes into the core.
  input.headerSize = containerManager->headerSize;
  input.footerSize = containerManager->footerSize;
  /*
   * SectionList header indices, converted once per props and lent to the core, since they
   * only change with the props.
   */
  if (geometryCache) {
    if (geometryCache->stickyIndicesProps != currentProps) {
      azimgd::shadowlist::stickyIndicesFromProps(shadowlistViewProps.stickyIndices, geometryCache->stickyIndices);
      geometryCache->stickyIndicesProps = currentProps;
    }
    input.stickyIndicesRef = &geometryCache->stickyIndices;
  } else {
    azimgd::shadowlist::stickyIndicesFromProps(shadowlistViewProps.stickyIndices, input.stickyIndices);
  }
  input.inverted = shadowlistViewProps.inverted;
  input.followAppends = shadowlistViewProps.followAppends;
  input.horizontal = shadowlistViewProps.horizontal;
  input.numberOfColumns =
    shadowlistViewProps.numberOfColumns > 0 ? static_cast<std::size_t>(shadowlistViewProps.numberOfColumns) : 1;
  input.overscan = shadowlistViewProps.overscan;
  input.startReachedThreshold = shadowlistViewProps.startReachedThreshold;
  input.endReachedThreshold = shadowlistViewProps.endReachedThreshold;
  /*
   * Viewability rules, two values each: the threshold from 0 to 1, then 1 for viewport
   * coverage or 0 for the row's own share. Read once per props and lent to the core.
   */
  if (geometryCache) {
    if (geometryCache->viewableRulesProps != currentProps) {
      viewableRulesFromProps(shadowlistViewProps.viewableRules, geometryCache->viewableRules);
      geometryCache->viewableRulesProps = currentProps;
    }
    input.viewableRulesRef = &geometryCache->viewableRules;
  } else {
    viewableRulesFromProps(shadowlistViewProps.viewableRules, input.viewableRules);
  }
  input.snapToItem = shadowlistViewProps.snapToItem;
  input.snapAlignment = static_cast<azimgd::shadowlist::SnapAlignment>(shadowlistViewProps.snapAlignment);

  // Offset, echoed token, user scroll flag and gesture phase, see applyHostScroll.
  azimgd::shadowlist::applyHostScroll(input, shadowlistViewStateData.scrollState());

  /*
   * Give the core predicted sizes before update(). This frame then picks its window from
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
    azimgd::shadowlist::Virtualizer::update(*containerManager, input);
    /*
     * The list came to rest with a scroll event still held back by scrollEventThrottle.
     * Send it now. JS would otherwise keep an offset from before the scroll stopped.
     */
    if (callbacksCache && callbacksCache->scrollTracker &&
        shadowlistViewStateData.scrollPhase_ == SCROLL_PHASE_IDLE && !shadowlistViewStateData.userScrolled_) {
      ShadowListScrollMetrics trailing;
      if (callbacksCache->scrollTracker->takeTrailing(trailing)) {
        std::static_pointer_cast<const ShadowListViewShadowNode::ConcreteEventEmitter>(eventEmitter)
          ->dispatchUniqueEvent("scroll",
            [trailing](jsi::Runtime& runtime) { return shadowListScrollPayload(runtime, trailing); });
      }
    }
    /*
     * Only the layout pass publishes to the host, and a state only commit won't run it.
     * So mark layout dirty when there's a correction or the state has old geometry.
     * Hosts build scroll reports on the state they mounted, which can carry an old content
     * size. Left alone, a fling can coast past the real end into blank space.
     */
    bool geometryStale =
      shadowlistViewStateData.totalContainerWidth_ != containerManager->revision.contentWidth ||
      shadowlistViewStateData.totalContainerHeight_ != containerManager->revision.contentHeight ||
      (geometryCache &&
       (geometryCache->published.snapOffsets != shadowlistViewStateData.snapOffsets_ ||
        geometryCache->published.stickyIndices != shadowlistViewStateData.stickyHeaderIndices_ ||
        geometryCache->published.stickyOffsets != shadowlistViewStateData.stickyHeaderOffsets_ ||
        geometryCache->published.stickySizes != shadowlistViewStateData.stickyHeaderSizes_ ||
        geometryCache->animationSequence != shadowlistViewStateData.animationTargetSequence_));
    /*
     * The layout pass shows hidden rows again, and the host's echo usually comes in a plain
     * scroll report. Keep layout dirty while any row is hidden.
     */
    bool rowsConcealed = geometryCache && !geometryCache->concealedRows.isEmpty();
    /*
     * The band is also only published by the layout pass. If this frame moved it, like a
     * new window, an edge crossed or rows reconciled, lay out again so the host gets the
     * new one. Otherwise the host would keep sending every frame, or skip frames it needs.
     */
    bool bandStale = !shadowListOffsetBandPublished(shadowlistViewStateData, shadowListOffsetBand(shadowlistViewShadowNode));
    if (containerManager->offsetCorrected || geometryStale || rowsConcealed || bandStale) {
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
}

/*
 * Pass core events to the event emitter. The core already drops repeats.
 *
 * The three per frame events below skip the generated emitter methods and use
 * dispatchUniqueEvent, which marks them unique and continuous, like React Native's own
 * ScrollView scroll event. The generated methods cause two problems:
 * Events are not merged. When JS falls behind, like while dragging the scroll
 * indicator, the queue grows for the whole gesture. That backlog is the long freeze.
 * Events are sent as discrete. React renders each one synchronously. Dragging content
 * is fine, but the scroll indicator, a macOS scroller and iOS momentum frames would each
 * cost one blocking render per frame.
 * Event names stay the same either way.
 *
 * Merging only looks at the last event queued for this view. Sending several kinds of
 * event per frame defeats it. That's another reason scroll and viewable only fire when
 * someone listens.
 */
void ShadowListViewComponentDescriptor::applyEventCallbacks(
  ShadowListViewShadowNode& shadowlistViewShadowNode,
  const ShadowListViewShadowNode::ConcreteProps& shadowlistViewProps,
  azimgd::shadowlist::Container* containerManager) {
  /*
   * The callbacks hold the family's event emitter. They only need building again when
   * the emitter or the listened events change. Rebuilding five std::function objects on
   * every commit, scroll frames included, is pure allocation.
   */
  const auto& callbacksCache = shadowlistViewShadowNode.getGeometryCache();
  const auto& eventEmitter = shadowlistViewShadowNode.getEventEmitter();
  bool callbacksCurrent = callbacksCache && callbacksCache->callbacksEmitter == eventEmitter &&
    callbacksCache->callbacksViewable == shadowlistViewProps.viewableEventEnabled &&
    callbacksCache->callbacksScroll == shadowlistViewProps.scrollEventEnabled &&
    callbacksCache->callbacksScrollThrottle == shadowlistViewProps.scrollEventThrottle;
  if (!callbacksCurrent) {
    auto emitter = std::static_pointer_cast<const ShadowListViewShadowNode::ConcreteEventEmitter>(eventEmitter);
    containerManager->onStartReachedCallback = [emitter]() -> void {
      emitter->onStartReached({});
    };
    containerManager->onEndReachedCallback = [emitter]() -> void {
      emitter->onEndReached({});
    };
    containerManager->onMeasuredRangeChangeCallback = [emitter](std::size_t low, std::size_t high) -> void {
      int visibleStartIndex = static_cast<int>(low);
      int visibleEndIndex = static_cast<int>(high);
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
      /*
       * One range per viewability rule, two indices each and -1 for none. The first rule's
       * range also goes out as viewableStartIndex and viewableEndIndex.
       */
      containerManager->onViewableIndicesChangeCallback = [emitter](const std::vector<std::size_t>& ranges) -> void {
        std::vector<int> indices;
        indices.reserve(ranges.size());
        for (auto index : ranges) {
          indices.push_back(index == azimgd::shadowlist::UNDEFINED_INDEX ? -1 : static_cast<int>(index));
        }
        emitter->dispatchUniqueEvent("viewableIndicesChange",
          [indices = std::move(indices)](jsi::Runtime& runtime) {
            auto payload = jsi::Object(runtime);
            payload.setProperty(runtime, "viewableStartIndex", indices.size() >= 2 ? indices[0] : -1);
            payload.setProperty(runtime, "viewableEndIndex", indices.size() >= 2 ? indices[1] : -1);
            auto ranges = jsi::Array(runtime, indices.size());
            for (std::size_t position = 0; position < indices.size(); ++position) {
              ranges.setValueAtIndex(runtime, position, indices[position]);
            }
            payload.setProperty(runtime, "ranges", ranges);
            return payload;
          });
      };
    } else {
      containerManager->onViewableIndicesChangeCallback = nullptr;
    }

    if (shadowlistViewProps.scrollEventEnabled) {
      /*
       * The callback lives in the core and only runs while the core does. Reading its sizes
       * through the raw pointer is safe. The tracker adds velocity and drops events inside
       * scrollEventThrottle.
       */
      auto tracker = std::make_shared<ShadowListScrollTracker>(shadowlistViewProps.scrollEventThrottle);
      containerManager->onScrollCallback = [emitter, tracker, containerManager](
                                             double containerOffsetX, double containerOffsetY) -> void {
        ShadowListScrollMetrics metrics;
        metrics.offsetX = containerOffsetX;
        metrics.offsetY = containerOffsetY;
        metrics.contentWidth = containerManager->revision.contentWidth;
        metrics.contentHeight = containerManager->revision.contentHeight;
        metrics.viewportWidth = containerManager->revision.windowWidth;
        metrics.viewportHeight = containerManager->revision.windowHeight;
        if (!tracker->track(metrics)) {
          return;
        }
        emitter->dispatchUniqueEvent("scroll",
          [metrics](jsi::Runtime& runtime) { return shadowListScrollPayload(runtime, metrics); });
      };
      if (callbacksCache) {
        callbacksCache->scrollTracker = tracker;
      }
    } else {
      containerManager->onScrollCallback = nullptr;
      if (callbacksCache) {
        callbacksCache->scrollTracker = nullptr;
      }
    }

    if (callbacksCache) {
      callbacksCache->callbacksEmitter = eventEmitter;
      callbacksCache->callbacksViewable = shadowlistViewProps.viewableEventEnabled;
      callbacksCache->callbacksScroll = shadowlistViewProps.scrollEventEnabled;
      callbacksCache->callbacksScrollThrottle = shadowlistViewProps.scrollEventThrottle;
    }
  }
}

void ShadowListViewComponentDescriptor::viewableRulesFromProps(
  const std::vector<double>& values,
  std::vector<azimgd::shadowlist::ViewableRule>& rules) {
  rules.clear();
  for (std::size_t position = 0; position + 1 < values.size(); position += 2) {
    rules.push_back({values[position], values[position + 1] != 0.0});
  }
  if (rules.empty()) {
    rules.push_back({});
  }
}

bool ShadowListViewComponentDescriptor::adoptLiveScrollReport(
  ShadowListViewShadowNode& listShadowNode,
  const ShadowListViewState& stateData) {
  const auto& liveScroll = stateData.liveScroll_;
  ShadowListLiveScroll::Report report;
  if (!liveScroll || !liveScroll->newerReport(stateData.hostSequence_, stateData.containerOffsetEnabled_, report)) {
    return false;
  }
  SL_LOG("adopt: live report seq=%llu over state seq=%.0f off=(%.1f,%.1f)->(%.1f,%.1f) token=%llu phase=%.0f",
    static_cast<unsigned long long>(report.sequence), stateData.hostSequence_,
    stateData.containerOffsetX_, stateData.containerOffsetY_, report.offsetX, report.offsetY,
    static_cast<unsigned long long>(report.commitToken), report.scrollPhase);
  ShadowListViewState nextStateData = stateData;
  nextStateData.applyLiveReport(report);
  listShadowNode.setStateData(std::move(nextStateData));
  return true;
}

void ShadowListViewComponentDescriptor::applyElementSizeSpecs(
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
    [&](const azimgd::shadowlist::RowSizeSpec& spec, double width) {
      return measureElementSizeSpec(textLayoutManager, spec, width, pointScaleFactor, surfaceId);
    });
}

}
