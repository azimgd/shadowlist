#pragma once

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>

#include "ShadowListCommitHook.h"
#include "ShadowListTextMeasurer.h"
#include "ShadowListViewShadowNode.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

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
  };

  void adopt(ShadowNode& shadowNode) const override {
    ConcreteComponentDescriptor::adopt(shadowNode);

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
      shadowlistViewShadowNode.setVirtualizerManager(std::make_shared<azimgd::shadowlist::Virtualizer>());
      shadowlistViewShadowNode.setHeaderSize(std::make_shared<double>(0.0));
      shadowlistViewShadowNode.setFooterSize(std::make_shared<double>(0.0));
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
      shadowlistViewProps.containerOffsetIndex);

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
    input.keysRef = &shadowlistViewProps.elementsAllKeys;
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
    input.keysUnchanged = geometryCache && geometryCache->keysProps == currentProps;
    input.containerOffsetX = shadowlistViewStateData.containerOffsetX_;
    input.containerOffsetY = shadowlistViewStateData.containerOffsetY_;
    input.containerOffsetEnabled = shadowlistViewStateData.containerOffsetEnabled_;
    input.windowContainerWidth = shadowlistViewLayoutMetrics.frame.size.width;
    input.windowContainerHeight = shadowlistViewLayoutMetrics.frame.size.height;
    input.headerSize = *shadowlistViewShadowNode.getHeaderSize();
    input.footerSize = *shadowlistViewShadowNode.getFooterSize();
    input.stickyHeader = shadowlistViewProps.stickyHeader;
    input.stickyFooter = shadowlistViewProps.stickyFooter;
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
    input.horizontal = shadowlistViewProps.horizontal;
    input.columns = shadowlistViewProps.columns > 0 ? static_cast<std::size_t>(shadowlistViewProps.columns) : 1;
    input.overscan = shadowlistViewProps.overscan;
    /*
     * Splits retention from materialization: rows stay reconciled out to `overscan` so a
     * fast scroll never waits on the JS round trip, while only this narrower band exists
     * as native views. Negative (the default) keeps the two identical, i.e. exactly the
     * behaviour before the band existed. Latching the flag here is what lets the commit
     * hook skip its tree walk entirely in apps that never opt in.
     */
    input.materializationOverscan = shadowlistViewProps.materializationOverscan;
    if (shadowlistViewProps.materializationOverscan >= 0.0) {
      ShadowListCommitHook::noteMaterializationEnabled();
    }
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
     * The commit token the host echoed back (the id of the correction whose offset
     * write produced this report, or 0). Lets the core match its own echo exactly.
     */
    input.commitToken = static_cast<std::uint64_t>(shadowlistViewStateData.commitToken_);

    /*
     * Contain core exceptions: skip the frame rather than abort the Fabric commit. The
     * core resets its own revision status on throw, so the next frame recovers.
     */
    /*
     * Stage ahead-of-time sizes BEFORE update(): the core consumes them between its
     * reconcile and its measure pass, so this frame's visible window is chosen from the
     * real geometry rather than from estimates it is about to replace.
     */
    applyMeasureSpecs(
      shadowlistViewShadowNode,
      shadowlistViewProps,
      containerManager,
      shadowlistViewLayoutMetrics.frame.size.width,
      shadowlistViewLayoutMetrics.pointScaleFactor,
      shadowNode.getSurfaceId());

    try {
      azimgd::shadowlist::Virtualizer::update(containerManager, input);
      /*
       * Only remember these props once the core has actually consumed their keys. A frame
       * that threw part-way through left the element list in an unknown state, so the next
       * one must re-validate the keys rather than trust this shortcut.
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
   * Turn this commit's `elementsMeasureSpecs` into predictions the core can lay out with.
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
  void applyMeasureSpecs(
    ShadowListViewShadowNode& shadowlistViewShadowNode,
    const ShadowListViewShadowNode::ConcreteProps& shadowlistViewProps,
    azimgd::shadowlist::Container* containerManager,
    Float availableWidth,
    Float pointScaleFactor,
    SurfaceId surfaceId) const {
    if (!textLayoutManager_ || shadowlistViewProps.elementsMeasureSpecs.empty()) {
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
    if (geometryCache && geometryCache->measureSpecsProps == currentProps && !widthChanged &&
        geometryCache->measureSpecsDone) {
      return;
    }

    /*
     * Resume where the last commit stopped when this is the same specs prop, otherwise
     * start over. See ShadowListViewGeometryCache::measureSpecsCursor for why measuring is
     * capped rather than done in one pass, and measureSpecs for why the PARSE is kept too.
     */
    bool sameSpecs = geometryCache && geometryCache->measureSpecsProps == currentProps && !widthChanged;

    std::vector<ShadowListMeasureSpec> parsedSpecs;
    if (!sameSpecs) {
      parsedSpecs = parseMeasureSpecs(shadowlistViewProps.elementsMeasureSpecs);
    }
    const std::vector<ShadowListMeasureSpec>& specs =
      (sameSpecs && geometryCache) ? geometryCache->measureSpecs : parsedSpecs;

    std::size_t cursor = sameSpecs ? geometryCache->measureSpecsCursor : 0;
    std::size_t measured = 0;

    while (cursor < specs.size() && measured < MEASURE_BUDGET_PER_COMMIT) {
      const auto& spec = specs[cursor];
      containerManager->setPredictedSize(
        spec.key,
        measureSpec(*textLayoutManager_, spec, availableWidth, pointScaleFactor, surfaceId));
      ++cursor;
      ++measured;
    }

    if (geometryCache) {
      std::size_t specCount = specs.size();
      if (!sameSpecs) {
        geometryCache->measureSpecs = std::move(parsedSpecs);
      }
      geometryCache->measureSpecsProps = currentProps;
      geometryCache->measureSpecsCursor = cursor;
      geometryCache->measureSpecsDone = cursor >= specCount;
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
};

void ShadowListViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
