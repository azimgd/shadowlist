#pragma once

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>

#include "ShadowListOffsetBand.h"
#include "ShadowListScrollEvent.h"
#include "ShadowListTextMeasurer.h"
#include "ShadowListTrace.h"
#include "ShadowListViewShadowNode.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/DragReorder.hpp>
#include <shadowlist-core/host/ListCommit.hpp>
#include <shadowlist-core/host/ScrollTarget.hpp>

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
    textLayoutManager_(getSharedTextLayoutManager(contextContainer_)) {}

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

  void adopt(ShadowNode& shadowNode) const override;

private:
  /*
   * Points the core's callbacks at this node family's event emitter.
   */
  static void applyEventCallbacks(
    ShadowListViewShadowNode& shadowlistViewShadowNode,
    const ShadowListViewShadowNode::ConcreteProps& shadowlistViewProps,
    azimgd::shadowlist::Container* containerManager);

  static void viewableRulesFromProps(
    const std::vector<double>& values,
    std::vector<azimgd::shadowlist::ViewableRule>& rules);

  /*
   * Write the host's live scroll report into the node's state when it is newer than
   * stateData, the state this commit would otherwise run on. Returns whether it did.
   * Only the host owned fields change: offset, echoed token, conceal ack, user scroll flag
   * and gesture phase. The rest of stateData is kept, including the newer state Fabric found.
   */
  static bool adoptLiveScrollReport(ShadowListViewShadowNode& listShadowNode, const ShadowListViewState& stateData);

  /*
   * Turn rowSizeSpecs into predicted sizes for the core, a few rows per commit.
   * See azimgd::shadowlist::SizeSpecQueue.
   */
  void applyRowSizeSpecs(
    ShadowListViewShadowNode& shadowlistViewShadowNode,
    const ShadowListViewShadowNode::ConcreteProps& shadowlistViewProps,
    azimgd::shadowlist::Container* containerManager,
    Float availableWidth,
    Float pointScaleFactor,
    SurfaceId surfaceId) const;

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
