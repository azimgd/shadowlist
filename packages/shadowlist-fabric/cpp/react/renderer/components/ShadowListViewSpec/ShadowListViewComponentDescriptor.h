#pragma once

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>

#include "ShadowListNativeJSI.h"
#include "ShadowListTextMeasurer.h"
#include "ShadowListTrace.h"
#include "ShadowListViewShadowNode.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <atomic>
#include <mutex>

namespace facebook::react {

/*
 * Descriptor for <ShadowListView> component.
 */
class ShadowListViewComponentDescriptor final : public ConcreteComponentDescriptor<ShadowListViewShadowNode> {
public:
  ShadowListViewComponentDescriptor(const ComponentDescriptorParameters& parameters) :
    ConcreteComponentDescriptor<ShadowListViewShadowNode>(parameters),
    /*
     * Resolved once per descriptor, and deliberately in the CONSTRUCTOR: it publishes the
     * instance into the ContextContainer so that RN's own Paragraph descriptor, if it is
     * built after us, shares our measure cache instead of creating a private one. Doing it
     * lazily at measure time would always be too late. See getSharedTextLayoutManager.
     */
    textLayoutManager_(getSharedTextLayoutManager(this->contextContainer_)) {
    // ShadowListNative's data API; installed early so JS rarely has to wait for it.
    installShadowListNativeJSI(this->contextContainer_);
  };

  /*
   * ShadowListNative rows are children no React component renders. Every clone that brings new
   * props, children or state (a React update, a scroll report, a data nudge) reconciles them
   * against the window the core just chose in adopt(), and the result is committed as this
   * node's children.
   *
   * A second construction, rather than editing the adopted node's children in place: a node
   * built with a children fragment has not configured its Yoga subtree, so the layout pass
   * configures the new rows (point scale factor, errata) like any React child. Rewriting the
   * children of a node that inherited "configured" would leave fresh rows laid out with Yoga's
   * defaults. Layout-only clones (empty fragment) are left alone.
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
   * React builds a list by appending its children one by one (a new node, or a clone with new
   * children). The rows go in right after the ShadowListNative templates container, the way
   * cloneShadowNode places them, so the header stays below them and the footer above.
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
    // Only rows can follow: the container was just appended last.
    if (children->size() < current.size() || !std::equal(current.begin(), current.end(), children->begin())) {
      return;
    }
    for (std::size_t index = current.size(); index < children->size(); ++index) {
      listShadowNode.appendChild((*children)[index]);
    }
  }

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);

    // Debug device trace only; a no-op unless the app was launched with SHADOWLIST_FRAME_TRACE=1.
    if (!jsTraceInstalled_.exchange(true, std::memory_order_relaxed)) {
      shadowlist::detail::installJsTrace(this->contextContainer_);
    }

    if (!nativeJsiInstalled_.exchange(true, std::memory_order_relaxed)) {
      installShadowListNativeJSI(this->contextContainer_);
    }

    auto& shadowlistViewShadowNode = static_cast<ShadowListViewShadowNode&>(shadowNode);

    /*
     * Lazily create the core for the initial node of a list. Clones carry these
     * instances forward (see the ShadowNode clone constructor), so a single core
     * is shared across a list's committed clones and freed when the node family
     * is destroyed; no descriptor-level registry that would leak one entry per
     * mounted list.
     */
    if (!shadowlistViewShadowNode.getContainerManager()) {
      shadowlistViewShadowNode.setContainerManager(std::make_shared<azimgd::shadowlist::Container>());
      shadowlistViewShadowNode.setGeometryCache(std::make_shared<ShadowListViewGeometryCache>());
    }

    auto& shadowlistViewProps = static_cast<const ShadowListViewShadowNode::ConcreteProps&>(*shadowNode.getProps());
    auto& shadowlistViewState = static_cast<const ShadowListViewShadowNode::ConcreteState&>(*shadowNode.getState());
    auto& shadowlistViewEventEmitter = static_cast<const ShadowListViewShadowNode::ConcreteEventEmitter&>(*shadowNode.getEventEmitter());

    /*
     * A reference, not a copy. getData() returns `const Data&`, and ShadowListViewState
     * holds two std::strings and four shared_ptrs -- so `auto` cost two heap allocations
     * and four atomic refcount pairs on EVERY commit, including every scroll frame, to
     * produce something this function only ever reads.
     */
    const auto& shadowlistViewStateData = shadowlistViewState.getData();
    auto shadowlistViewLayoutMetrics = static_cast<YogaLayoutableShadowNode&>(shadowNode).getLayoutMetrics();

    auto containerManager = shadowlistViewShadowNode.getContainerManager().get();

    /*
     * ShadowListNative: attach the list's native store and take this commit's key snapshot. The
     * core reconciles against the snapshot, and the rows mounted for this commit come from the
     * same snapshot, so a row is never mounted for a key the core does not know yet.
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
     * Serialize all access to the shared core for this commit. adopt() can run
     * concurrently with layout()/replaceChild()/update() on another committed clone of
     * the same list, so the recursive coreMutex (re-entered by update()) guards the
     * callback assignments, the drag sequence, requestScrollToIndex and the header/footer
     * reads below against those passes.
     */
    std::lock_guard<std::recursive_mutex> coreLock(containerManager->coreMutex);

    /*
     * Forward core events to the event emitter. The core deduplicates so we
     * only need to translate the payloads here.
     *
     * The three HIGH-FREQUENCY observers below deliberately bypass the codegen'd emitter
     * methods and dispatch through EventEmitter::dispatchUniqueEvent instead. The generated
     * methods call dispatchEvent(..., Category::Unspecified), which has two consequences on
     * a per-scroll-frame event that this list cannot live with:
     *
     *   * NOT COALESCED. EventQueue::enqueueEvent only collapses a repeat onto an existing
     *     entry for events flagged unique, so every frame appends. When the JS thread cannot
     *     keep up -- which is exactly what happens while the scroll indicator is dragged,
     *     because each report lands in a completely different part of the list and forces a
     *     full remount of the mounted window -- the queue grows for the length of the
     *     gesture and then drains afterwards. That backlog is the multi-second freeze.
     *
     *   * DISPATCHED AS DISCRETE. EventQueueProcessor maps Unspecified to Discrete unless a
     *     continuous gesture is in flight, and Discrete makes React flush the render
     *     synchronously and uninterruptibly. Content dragging happens to be fine (the touch
     *     sequence sets the continuous flag), but a scroll-indicator drag cancels that touch
     *     -- and on macOS a scroller drag is not an RN touch at all -- so every frame of the
     *     gesture became one blocking render. iOS momentum frames after the finger lifts
     *     have the same problem.
     *
     * dispatchUniqueEvent fixes both at once: it marks the event unique AND tags it
     * Category::Continuous. This is the same treatment RN gives its own ScrollView `scroll`
     * event (ScrollViewEventEmitter::onScroll). Wire names are unchanged: both paths run the
     * type through EventEmitter::normalizeEventType.
     *
     * Coalescing only reaches back to the last event queued for this target, so emitting
     * three different high-frequency types per frame would defeat it. That is the other half
     * of why scroll/viewable are gated on someone actually listening below.
     */
    containerManager->onStartReachedCallback = [shadowlistViewEventEmitter]() -> void {
      shadowlistViewEventEmitter.onStartReached({});
    };
    containerManager->onEndReachedCallback = [shadowlistViewEventEmitter]() -> void {
      shadowlistViewEventEmitter.onEndReached({});
    };
    /*
     * The imperative setStartReachedEnabled / setEndReachedEnabled commands write these flags
     * into state; the core gates the reached callbacks on them when update() ends the frame.
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
     * Only observe what JS is listening to. An unobserved viewable range still costs
     * getViewableIndices' O(window) overlap scan on every frame, plus a queued event and a
     * JS round trip that finds no handler.
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
     * Relay a drag-to-reorder boundary to JS. The platform view bumps dragEventSequence_
     * in state only at pickup and drop (the per-frame finger tracking and make-room
     * shuffle stay native and never reach here); fire the matching event once per
     * fresh sequence. type 1 = pickup, 3 = drop.
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
     * Resolve a pending scrollToIndex. The imperative command writes the target
     * into state (containerOffsetIndex_); the declarative prop provides an initial
     * index. The core resolves precedence and fires once per distinct target.
     */
    containerManager->requestScrollToIndex(
      shadowlistViewStateData.containerOffsetIndex_,
      shadowlistViewStateData.containerOffsetIndexSequence_,
      shadowlistViewProps.containerOffsetIndex,
      shadowlistViewStateData.containerOffsetIndexViewPosition_);

    // ShadowListNative's scroll commands, once the rows they follow are laid out.
    bool nativeScrollCommand = shadowlistViewShadowNode.getNativeEngine() &&
      shadowlistViewShadowNode.getNativeEngine()->applyPendingScroll(*containerManager);

    /*
     * Reconcile, measure and resolve scrolling in a single core call
     */
    azimgd::shadowlist::FrameInput input;
    /*
     * Lend the core the props' own key collection instead of copying it. Props are
     * immutable and outlive this synchronous call, and the core only reads keys during
     * it. Copying meant one allocation per row for any key longer than the small-string
     * buffer -- on the order of milliseconds per commit for a large chat list, on every
     * commit including pure scrolls.
     */
    input.keysRef = nativeKeys.keys ? nativeKeys.keys.get() : &shadowlistViewProps.elementsAllKeys;
    /*
     * Decoration row keys the core must never auto-capture as the MVCP anchor (date pills,
     * unread dividers, reaction strips, padding). Keeps a key change on decoration from
     * perturbing the maintained scroll position, so JS can stop encoding "ignore me" into
     * the row's key. An empty list means every row is anchorable.
     */
    input.nonAnchorableKeysRef = &shadowlistViewProps.elementsAnchorIgnoreKeys;

    /*
     * A scroll clones the shadow node with fresh state and the SAME immutable props, so
     * the props pointer is a sound proof that the key collection did not change -- and the
     * geometry cache holds a strong reference to the previous props, so its address cannot
     * have been recycled underneath us. Let the core skip its per-commit O(rows) key
     * comparison on those frames.
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
    /*
     * The layout pass measures the header/footer templates and applies them to the core
     * directly (see ShadowListViewShadowNode::layout), so the core's own values are the
     * latest measurements.
     */
    input.headerSize = containerManager->headerSize;
    input.footerSize = containerManager->footerSize;
    /*
     * Section-header element indices (SectionList). Skip negatives defensively; the
     * core expects an ascending list of valid indices.
     */
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
     * A genuine user scroll abandons any in-flight scroll correction so the user
     * takes over instead of being snapped back. Without this the core's "yield to
     * the user" path never fires and a transient correction can latch and freeze
     * the visible window (blank list on deep scroll).
     */
    input.userScrolled = shadowlistViewStateData.userScrolled_;

    /*
     * The gesture phase outlives a single report: a finger resting on the list makes
     * every commit in between a gesture frame too, so the inverted bottom pin does not
     * re-assert itself under the finger (see Container::gestureActive).
     */
    input.scrollPhase = shadowlistViewStateData.scrollPhase_ == SCROLL_PHASE_DRAGGING
      ? azimgd::shadowlist::ScrollPhase::Dragging
      : shadowlistViewStateData.scrollPhase_ == SCROLL_PHASE_SETTLING
        ? azimgd::shadowlist::ScrollPhase::Settling
        : azimgd::shadowlist::ScrollPhase::Idle;

    /*
     * A ShadowListNative scroll command stops momentum (the host does it when it mounts the
     * correction, see ShadowListNativeEngine::setMomentumYieldToken), so its frame runs idle as
     * a host command's does. Otherwise the core would read the fling's settling phase as a
     * gesture driving the correction, and the host would shift it onto the coasting offset.
     */
    if (nativeScrollCommand) {
      input.userScrolled = false;
      input.scrollPhase = azimgd::shadowlist::ScrollPhase::Idle;
    }

    /*
     * The commit token the host echoed back (the id of the correction whose offset
     * write produced this report, or 0). Lets the core match its own echo exactly.
     */
    input.commitToken = static_cast<std::uint64_t>(shadowlistViewStateData.commitToken_);

    /*
     * Stage ahead-of-time sizes BEFORE update(): the core consumes them between its
     * reconcile and its measure pass, so this frame's visible window is chosen from the
     * real geometry rather than from estimates it is about to replace.
     */
    applyElementSizeSpecs(
      shadowlistViewShadowNode,
      shadowlistViewProps,
      containerManager,
      shadowlistViewLayoutMetrics.frame.size.width,
      shadowlistViewLayoutMetrics.pointScaleFactor,
      shadowNode.getSurfaceId());

    /*
     * Contain core exceptions: skip the frame rather than abort the Fabric commit. The
     * core resets its own revision status on throw, so the next frame recovers.
     */
    try {
      azimgd::shadowlist::Virtualizer::update(containerManager, input);
      if (nativeScrollCommand) {
        shadowlistViewShadowNode.getNativeEngine()->setMomentumYieldToken(
          containerManager->operation ? containerManager->operation->id : 0);
      }
      /*
       * Only the layout pass publishes to the host, and a commit that carries nothing but new
       * state (a scroll report, a scroll command) leaves this node's layout clean, so layout()
       * would not run. Dirty it when there is something to publish: a correction, or core-owned
       * geometry this state no longer carries. The Android host builds each scroll report on
       * the state it last mounted, which can predate the latest published geometry, so the
       * report writes the old content size (and snap/sticky tables) back; iOS copies the
       * mounted state the same way. Left alone, the host keeps scrolling an outdated content
       * size, and a fling can coast past the real end into blank space.
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
       * Concealed rows are revealed by the layout pass, and the commit that carries the host's
       * ack is usually a bare scroll report, so dirty it while any row is concealed.
       */
      bool rowsConcealed = geometryCache && !geometryCache->concealedRows.empty();
      if (containerManager->containerOffsetCorrected || geometryStale || rowsConcealed) {
        shadowlistViewShadowNode.dirtyLayout();
      }
      /*
       * Only remember these props once the core has actually consumed their keys. A frame
       * that threw part-way through left the element list in an unknown state, so the next
       * one must re-validate the keys rather than trust this shortcut.
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
   * The children a ShadowListNative node should commit with, or null when it already has them
   * (or is not a ShadowListNative, or its templates are not mounted yet).
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
      // A row that failed to build leaves the mounted rows in place for this commit.
      return nullptr;
    }
  }

  /*
   * Turn this commit's `elementsSizeSpecs` into predictions the core can lay out with.
   *
   * Skipped entirely unless the specs prop actually changed -- which it does not on a
   * scroll frame, a state publish, or any unrelated prop change, i.e. on almost every
   * commit. A list that never sets the prop pays one empty-string test per commit.
   *
   * Width matters: text wraps to the list's width, so a list that has not been laid out
   * yet (or is mid-resize to zero) cannot produce a meaningful measurement, and a width
   * change invalidates every measurement taken at the old one. Both cases re-measure
   * rather than publish a wrong height.
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
     * A width change invalidates every prediction taken at the old width -- text wraps to
     * the row width, so those heights are now wrong. Drop them all rather than let the list
     * sit on confidently wrong geometry; the rows covered by this commit's specs are
     * re-measured immediately below, and the rest fall back to the estimate and are
     * measured natively, exactly as a list that never predicted anything would.
     *
     * The very first layout counts as a change (the core starts at width 0) but clears
     * nothing, because nothing has been predicted yet.
     */
    bool widthChanged = containerManager->revision.windowContainerWidth != availableWidth;
    if (widthChanged) {
      azimgd::shadowlist::Virtualizer::invalidatePredictions(containerManager);
    }

    /*
     * Nothing to do only when this exact specs prop has already been measured THROUGH --
     * a partially measured one must be resumed, or the tail of the window would never get
     * predictions at all.
     */
    if (geometryCache && geometryCache->sizeSpecsProps == currentProps && !widthChanged &&
        geometryCache->sizeSpecsDone) {
      return;
    }

    /*
     * Resume where the last commit stopped when this is the same specs prop, otherwise
     * start over. See ShadowListViewGeometryCache::sizeSpecsCursor for why measuring is
     * capped rather than done in one pass, and sizeSpecs for why the PARSE is kept too.
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
   * Text layouts to perform in one commit. Sized so the worst case -- every spec a cache
   * miss -- stays well inside a frame, while a window advance still completes within a
   * couple of commits.
   */
  static constexpr std::size_t MEASURE_BUDGET_PER_COMMIT = 24;

  /*
   * Shared with RN's text rendering where we won the race to publish it (see
   * getSharedTextLayoutManager); a private instance otherwise, which measures the same
   * sizes and merely misses the cache. Null only when there is no ContextContainer, which
   * disables prediction rather than failing.
   */
  const std::shared_ptr<const TextLayoutManager> textLayoutManager_;

  mutable std::atomic<bool> jsTraceInstalled_{false};
  mutable std::atomic<bool> nativeJsiInstalled_{false};
};

void ShadowListViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
