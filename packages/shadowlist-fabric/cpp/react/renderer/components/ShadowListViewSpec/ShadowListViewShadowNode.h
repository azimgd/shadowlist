#pragma once

#include <jsi/jsi.h>
#include <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#include <react/renderer/components/ShadowListViewSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>

#include "ShadowListElementSizeSpec.h"
#include "ShadowListViewState.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace facebook::react {

JSI_EXPORT extern const char ShadowListViewComponentName[];

/*
 * Geometry the list publishes to the platform view so it can pin sticky section headers
 * and snap on the UI thread: only the core knows these offsets, and they move as
 * off-screen rows are measured, so they ride along on the state.
 *
 * Building them is O(rows) (one snap target per row) but they depend solely on element
 * geometry and a few scalars -- never on the scroll offset, which is what actually
 * changes on the frames that publish. So they are built once per geometry change and
 * reused, keyed by Container::geometryVersion. Shared by every committed clone of one
 * list, exactly like the Container itself.
 */
struct ShadowListViewGeometryCache {
  std::uint64_t geometryVersion = 0;  // 0 = nothing cached yet
  bool snapToItem = false;
  int snapAlignment = -1;
  bool inverted = false;
  bool horizontal = false;
  double windowSize = -1.0;
  double totalSize = -1.0;
  std::vector<std::size_t> sourceStickyIndices;

  /*
   * The published collections, shared with every state that carries them (see
   * ShadowListViewState). Holding the same pointers the state holds is what makes the
   * per-frame "did this change?" test a pointer comparison rather than an O(rows)
   * element-wise one, and what keeps a rebuild that produces identical values from
   * publishing a new state at all: adoptIfChanged below only swaps the pointer when the
   * contents really moved. Null means empty.
   */
  std::shared_ptr<const std::vector<int>> stickyHeaderIndices;
  std::shared_ptr<const std::vector<Float>> stickyHeaderOffsets;
  std::shared_ptr<const std::vector<Float>> stickyHeaderSizes;
  std::shared_ptr<const std::vector<Float>> snapOffsets;

  /*
   * The props whose key collection the core last consumed successfully. Holding a strong
   * reference is what makes pointer identity a sound "the keys did not change" proof:
   * the previous props cannot be freed and its address reused while we still own it.
   */
  std::shared_ptr<const Props> keysProps;

  /*
   * The props whose `elementsSizeSpecs` were last parsed and measured. Same pointer
   * trick, same reason: measuring a window of rows is real work (a text layout each), and
   * the specs prop is unchanged on every scroll frame, every state publish and every
   * unrelated prop change -- which is almost every commit. Holding a strong reference is
   * what makes the identity test sound rather than an address-reuse hazard.
   */
  std::shared_ptr<const Props> sizeSpecsProps;

  /*
   * How far through the current specs prop the measurement pass has got.
   *
   * Measuring is capped per commit (see applyElementSizeSpecs). A republish brings in a
   * handful of genuinely new rows plus a long tail of rows already in the text measure
   * cache; the cached ones cost a hash and a lookup, but the new ones are real text layouts
   * at roughly a tenth of a millisecond each. Doing them all in one commit puts a
   * multi-millisecond spike on the commit thread every time the window advances.
   *
   * So the pass stops at the cap and records where it stopped; the next commit resumes.
   * Partial application is already safe -- a row whose prediction has not landed yet simply
   * uses the ordinary estimate until it does.
   */
  std::size_t sizeSpecsCursor = 0;

  /*
   * Set once the cursor has walked the whole of sizeSpecsProps, so the common
   * already-finished commit costs a bool test rather than a JSON parse.
   */
  bool sizeSpecsDone = false;

  /*
   * The parsed specs for sizeSpecsProps.
   *
   * Parsing has to be cached, not just the measuring. Because measurement is spread across
   * several commits, a prop that is still being worked through is revisited on each of
   * them, and re-parsing the whole JSON payload every time costs far more than the
   * measurements it protects.
   */
  std::vector<ShadowListElementSizeSpec> sizeSpecs;

  /*
   * Set once this list is registered with the commit hook's per-surface registry (see
   * ShadowListCommitHook::trackList), so adopt() registers each list family exactly once.
   */
  bool commitHookTracked = false;

  /*
   * The props a row carried before the commit hook hid it, and the display:none props it
   * was given, keyed by row tag. Guarded by Container::coreMutex.
   *
   * Holding both is what keeps pruning cheap and reversible. React's host instances keep
   * the rows it last rendered, so a React commit that re-appends the list's children hands
   * every pruned row back with its original props: a cache hit reuses the hidden props
   * instead of parsing them again. Promotion restores the original props pointer rather
   * than forcing `display: flex`, so a row's own `display` style survives. A row whose props
   * match neither entry was not hidden by the hook and is left alone.
   */
  struct RowProps {
    std::shared_ptr<const Props> sourceProps;
    std::shared_ptr<const Props> hiddenProps;
  };
  std::unordered_map<Tag, RowProps> rowProps;
};

/*
 * `ShadowNode` for <ShadowListView> component.
 */
class ShadowListViewShadowNode final : public ConcreteViewShadowNode<
  ShadowListViewComponentName,
  ShadowListViewProps,
  ShadowListViewEventEmitter,
  ShadowListViewState> {
public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

  /*
   * Clone constructor. The core instances (container/header/footer/geometry cache)
   * live on the ShadowNode so they are freed when the node family is destroyed,
   * but inherited constructors default-initialize derived members, so the clone
   * must carry the shared instances forward from its source. This keeps a single
   * core per list instance shared across all its committed clones.
   */
  ShadowListViewShadowNode(
    const ShadowNode& sourceShadowNode,
    const ShadowNodeFragment& fragment);

#pragma mark - LayoutableShadowNode

  void layout(LayoutContext layoutContext) override;
  void appendChild(const std::shared_ptr<const ShadowNode>& nextElementShadowNode) override;
  void replaceChild(
    const ShadowNode& prevElementShadowNode,
    const std::shared_ptr<const ShadowNode>& nextElementShadowNode,
    std::size_t suggestedIndex = SIZE_MAX) override;

  void setContainerManager(std::shared_ptr<azimgd::shadowlist::Container> containerManager);
  void setHeaderSize(std::shared_ptr<double> headerSize);
  void setFooterSize(std::shared_ptr<double> footerSize);
  void setGeometryCache(std::shared_ptr<ShadowListViewGeometryCache> geometryCache);

  const std::shared_ptr<azimgd::shadowlist::Container>& getContainerManager() const { return containerManager_; }
  const std::shared_ptr<double>& getHeaderSize() const { return headerSize_; }
  const std::shared_ptr<double>& getFooterSize() const { return footerSize_; }
  const std::shared_ptr<ShadowListViewGeometryCache>& getGeometryCache() const { return geometryCache_; }

private:
  std::shared_ptr<azimgd::shadowlist::Container> containerManager_;

  /*
   * Header/footer sizes are measured during layout and fed back into the next
   * frame's Virtualizer::update so the core can position elements after the header
   */
  std::shared_ptr<double> headerSize_;
  std::shared_ptr<double> footerSize_;

  /*
   * Publishable geometry derived from the core, shared across this list's clones.
   */
  std::shared_ptr<ShadowListViewGeometryCache> geometryCache_;

  /*
   * Set while layout() writes its own placement frames back into the tree, so the
   * replaceChild override does not re-report those frames as fresh measurements. Purely
   * local to one node's layout pass, which is single-threaded.
   */
  bool suppressElementSizeFeedback_ = false;

  /*
   * Children this layout pass swapped out of the tree, held alive until the next one.
   *
   * Yoga's layout records every child it re-laid out in LayoutContext::affectedNodes as a
   * RAW pointer, and ShadowTree::tryCommit dereferences that list AFTER layout finishes
   * (emitLayoutEvents dynamic_casts each entry to look for an onLayout handler). This node
   * then replaces those very children with repositioned clones. A child whose only owner
   * was the slot we just overwrote is destroyed immediately, leaving a dangling pointer in
   * a list the framework is still going to read: a use-after-free that shows up as a
   * segfault inside emitLayoutEvents, and one that gets dramatically more likely the more
   * rows are mounted per commit.
   *
   * Holding a strong reference until the next layout of this node keeps every such pointer
   * valid for the rest of the commit that recorded it, which is all the framework needs.
   */
  std::vector<std::shared_ptr<const ShadowNode>> replacedChildren_;
};

}
