#pragma once

#include <jsi/jsi.h>
#include <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#include <react/renderer/components/ShadowListViewSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>

#include "ShadowListElementSizeSpec.h"
#include "ShadowListNativeEngine.h"
#include "ShadowListViewState.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace facebook::react {

JSI_EXPORT extern const char ShadowListViewComponentName[];

/*
 * Sticky header and snap positions the list sends to the platform view through state,
 * so it can pin headers and snap on the UI thread. Only the core knows them, and they
 * move as off screen rows get measured.
 *
 * Building them walks every row, but they depend only on row geometry, never on the
 * scroll offset. So they are built once per Container::geometryVersion and reused.
 * Every committed clone of one list shares this cache, just like the Container.
 */
struct ShadowListViewGeometryCache {
  std::uint64_t geometryVersion = 0;  // 0 means nothing cached yet
  bool snapToItem = false;
  int snapAlignment = -1;
  bool inverted = false;
  bool horizontal = false;
  double windowSize = -1.0;
  double totalSize = -1.0;
  std::vector<std::size_t> sourceStickyIndices;

  /*
   * The published lists, shared with every state that carries them. Holding the same
   * pointers makes the change check a pointer compare, and adoptIfChanged only swaps a
   * pointer when the values really changed, so an identical rebuild publishes nothing.
   * Null means empty.
   */
  std::shared_ptr<const std::vector<int>> stickyHeaderIndices;
  std::shared_ptr<const std::vector<Float>> stickyHeaderOffsets;
  std::shared_ptr<const std::vector<Float>> stickyHeaderSizes;
  std::shared_ptr<const std::vector<Float>> snapOffsets;

  /*
   * The props whose keys the core last took in. Holding a strong reference keeps the
   * address from being freed and reused, so a pointer match really means same keys.
   */
  std::shared_ptr<const Props> keysProps;

  // For ShadowListNative, the store keys version the core last took in, or 0 for none.
  std::uint64_t nativeKeysVersion = 0;

  /*
   * The props whose elementsSizeSpecs were last parsed and measured. Same pointer trick:
   * measuring costs a text layout per row, and the specs stay the same on almost every
   * commit. The strong reference keeps the pointer check safe from address reuse.
   */
  std::shared_ptr<const Props> sizeSpecsProps;

  /*
   * How far the measuring pass got through the current specs.
   * Measuring is capped per commit, see applyElementSizeSpecs. Cached rows are cheap but
   * each new row is a real text layout of about a tenth of a millisecond, and doing them
   * all at once spikes the commit thread. So the pass stops at the cap and the next commit
   * picks up here. A row not measured yet just uses the normal estimate.
   */
  std::size_t sizeSpecsCursor = 0;

  // Set once every spec is measured, so a finished commit skips the JSON parse.
  bool sizeSpecsDone = false;

  /*
   * The parsed specs. Measuring is spread over several commits, and parsing the whole
   * JSON again on each one would cost more than the measuring, so the parse is cached too.
   */
  std::vector<ShadowListElementSizeSpec> sizeSpecs;

  /*
   * Rows the layout pass hid with opacity 0, by row tag. Guarded by Container::coreMutex.
   *
   * A row above the anchor measured for the first time can move the anchor, and the layout
   * pass publishes an offset correction. Until the host mounts it, the content looks like it
   * shifts for a few frames. So the row stays hidden from the commit that measures it until
   * a host report echoes its generation and no correction is pending.
   *
   * Both props are kept so a React commit that hands back the original props gets hidden
   * again from the cache, while truly new props get parsed again.
   */
  struct ConcealedRow {
    std::shared_ptr<const Props> sourceProps;
    std::shared_ptr<const Props> concealedProps;
    std::uint64_t generation = 0;
    // How many layout passes the row stayed hidden, so it can't stay hidden forever.
    std::size_t layoutPasses = 0;
  };
  std::unordered_map<Tag, ConcealedRow> concealedRows;

  // The last generation given to a hide, or 0 if nothing was ever hidden.
  std::uint64_t concealGeneration = 0;
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
   * Inherited constructors leave derived members empty, so the clone copies them from
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

  /*
   * For ShadowListNative, the engine that builds this list's rows, null for a ShadowList,
   * and the row keys the core matched up on the commit that made this node.
   */
  void setNativeEngine(std::shared_ptr<ShadowListNativeEngine> nativeEngine) { nativeEngine_ = std::move(nativeEngine); }
  const std::shared_ptr<ShadowListNativeEngine>& getNativeEngine() const { return nativeEngine_; }
  void setNativeKeys(std::shared_ptr<const std::vector<std::string>> nativeKeys) { nativeKeys_ = std::move(nativeKeys); }
  const std::shared_ptr<const std::vector<std::string>>& getNativeKeys() const { return nativeKeys_; }

private:
  /*
   * Whether this node's Yoga node owns the child. Only a child cloned or adopted for this
   * very node is owned, so it belongs to this commit alone and no other tree shares it.
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

  std::shared_ptr<ShadowListNativeEngine> nativeEngine_;
  std::shared_ptr<const std::vector<std::string>> nativeKeys_;

  // Geometry from the core to publish, shared across this list's clones.
  std::shared_ptr<ShadowListViewGeometryCache> geometryCache_;

  /*
   * Set while layout() writes its own frames into the tree, so replaceChild doesn't
   * report them as new measurements. Only used inside one single threaded layout pass.
   */
  bool suppressElementSizeFeedback_ = false;

  /*
   * Rows measured for the first time in this layout cycle, which may get hidden.
   * Yoga reports a new row through replaceChild before layout() runs, so we record it there.
   * Cleared at the end of layout().
   */
  std::vector<Tag> firstMeasuredTags_;

  /*
   * Children this layout pass swapped out, kept alive until the next pass.
   * Warning: Yoga stores raw pointers to relaid children in LayoutContext::affectedNodes,
   * and ShadowTree::tryCommit reads them after layout in emitLayoutEvents. We replace those
   * children with moved clones, so without this a child could be freed while still listed.
   * That crashes inside emitLayoutEvents, more often the more rows mount per commit.
   */
  std::vector<std::shared_ptr<const ShadowNode>> replacedChildren_;
};

}
