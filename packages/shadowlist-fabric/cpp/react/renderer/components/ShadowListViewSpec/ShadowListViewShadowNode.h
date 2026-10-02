#pragma once

#include <jsi/jsi.h>
#include <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#include <react/renderer/components/ShadowListViewSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>

#include "ShadowListViewState.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/ElementSizeSpec.hpp>
#include <shadowlist-core/host/ListLayout.hpp>

#include <memory>
#include <vector>

namespace facebook::react {

JSI_EXPORT extern const char ShadowListViewComponentName[];

/*
 * What one list caches across commits, shared by every committed clone like the Container.
 */
struct ShadowListViewGeometryCache {
  /*
   * Sticky header and snap positions sent to the platform view through state. It pins
   * headers and snaps on the UI thread. See azimgd::shadowlist::PublishedGeometry.
   */
  azimgd::shadowlist::PublishedGeometry published;

  /*
   * The props whose keys the core last took in. Holding a strong reference keeps the
   * address from being freed and reused. A pointer match really means same keys.
   */
  std::shared_ptr<const Props> keysProps;

  /*
   * The event emitter the core's callbacks dispatch through, and which optional ones were
   * set. The emitter belongs to the node family. The callbacks are only built again
   * when it or the listened events change, not on every commit.
   */
  std::shared_ptr<const EventEmitter> callbacksEmitter;
  bool callbacksViewable = false;
  bool callbacksScroll = false;

  // stickyHeaderIndices from these props, cleaned up for the core. Same pointer trick.
  std::shared_ptr<const Props> stickyIndicesProps;
  std::vector<std::size_t> stickyIndices;

  /*
   * elementsSizeSpecs measured a few rows per commit, keyed by the props they came from.
   * See azimgd::shadowlist::SizeSpecQueue.
   */
  azimgd::shadowlist::SizeSpecQueue sizeSpecs;

  /*
   * Rows the layout pass hid with opacity 0, by row tag, see azimgd::shadowlist::ConcealTracker.
   * Guarded by Container::coreMutex. Both props are kept so a React commit that hands back the
   * original props gets hidden again, while truly new props get parsed again.
   */
  struct ConcealedProps {
    std::shared_ptr<const Props> sourceProps;
    std::shared_ptr<const Props> concealedProps;
  };
  azimgd::shadowlist::ConcealTracker<ConcealedProps> concealedRows;
};

class ShadowListViewShadowNode final : public ConcreteViewShadowNode<
  ShadowListViewComponentName,
  ShadowListViewProps,
  ShadowListViewEventEmitter,
  ShadowListViewState> {
public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

  /*
   * The core and geometry cache live on the node so they die with the node family.
   * Inherited constructors leave derived members empty. The clone copies them from
   * its source. That keeps one core per list across all its clones.
   */
  ShadowListViewShadowNode(
    const ShadowNode& sourceShadowNode,
    const ShadowNodeFragment& fragment);

#pragma mark - LayoutableShadowNode

  void layout(LayoutContext layoutContext) override;
  void replaceChild(
    const ShadowNode& previousElementShadowNode,
    const std::shared_ptr<const ShadowNode>& nextElementShadowNode,
    std::size_t suggestedIndex = SIZE_MAX) override;

  void setContainerManager(std::shared_ptr<azimgd::shadowlist::Container> containerManager);
  void setGeometryCache(std::shared_ptr<ShadowListViewGeometryCache> geometryCache);

  const std::shared_ptr<azimgd::shadowlist::Container>& getContainerManager() const { return containerManager_; }
  const std::shared_ptr<ShadowListViewGeometryCache>& getGeometryCache() const { return geometryCache_; }

private:
  /*
   * Whether this node's Yoga node owns the child. Only a child cloned or adopted for this
   * very node is owned. It belongs to this commit alone and no other tree shares it.
   * Yoga writes layout metrics into owned children in place, and so can we.
   */
  bool ownsLayoutableChild(const YogaLayoutableShadowNode& child) const;

  /*
   * Moves a child to layoutMetrics, and gives it nextProps when set. An owned child with
   * no new props is updated in place. Anything else is cloned and swapped in.
   */
  void placeChild(
    const std::shared_ptr<const ShadowNode>& child,
    const YogaLayoutableShadowNode& layoutableChild,
    const LayoutMetrics& layoutMetrics,
    const std::shared_ptr<const facebook::react::Props>& nextProps,
    std::size_t childIndex,
    LayoutContext& layoutContext);

  std::shared_ptr<azimgd::shadowlist::Container> containerManager_;

  // Geometry from the core to publish, shared across this list's clones.
  std::shared_ptr<ShadowListViewGeometryCache> geometryCache_;

  /*
   * Set while layout() writes its own frames into the tree. replaceChild then doesn't
   * report them as new measurements. Only used inside one single threaded layout pass.
   */
  bool suppressElementSizeFeedback_ = false;

  /*
   * Rows measured for the first time in this layout cycle, which may get hidden.
   * Yoga reports a new row through replaceChild before layout() runs. We record it there.
   * Cleared at the end of layout().
   */
  std::vector<Tag> firstMeasuredTags_;

  /*
   * Children this layout pass swapped out, kept alive until the next pass.
   * Warning: Yoga stores raw pointers to relaid children in LayoutContext::affectedNodes,
   * and ShadowTree::tryCommit reads them after layout in emitLayoutEvents. We replace those
   * children with moved clones. Without this a child could be freed while still listed.
   * That crashes inside emitLayoutEvents, more often the more rows mount per commit.
   */
  std::vector<std::shared_ptr<const ShadowNode>> replacedChildren_;
};

}
