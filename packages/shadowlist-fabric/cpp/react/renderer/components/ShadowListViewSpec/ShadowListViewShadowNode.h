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

  // ShadowListNative: the store keys version the core last consumed (0 = none).
  std::uint64_t nativeKeysVersion = 0;

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
   * Rows the layout pass concealed (opacity 0), keyed by row tag. Guarded by
   * Container::coreMutex.
   *
   * A row above the anchor that is measured natively for the first time can move the anchor,
   * and the layout pass publishes an offset correction for it. Until the host has mounted that
   * correction the row sits where the estimate put it relative to the view, which shows as the
   * content shifting for a few frames. The row is concealed in the commit that mounts and
   * measures it, and revealed once a host report acks its generation (see
   * ShadowListViewState::concealGenerationAck_) and no correction is in flight.
   *
   * sourceProps/concealedProps are both kept so a React commit that
   * hands the original props back is concealed again from the cache, and a genuinely new props
   * object is re-parsed.
   */
  struct ConcealedRow {
    std::shared_ptr<const Props> sourceProps;
    std::shared_ptr<const Props> concealedProps;
    std::uint64_t generation = 0;
    // Layout passes the row has stayed concealed through; bounds the concealment.
    std::size_t layoutPasses = 0;
  };
  std::unordered_map<Tag, ConcealedRow> concealedRows;

  // Last generation handed out to a concealment; 0 = never concealed.
  std::uint64_t concealGeneration = 0;
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
   * Clone constructor. The core instances (container/geometry cache)
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
  void replaceChild(
    const ShadowNode& prevElementShadowNode,
    const std::shared_ptr<const ShadowNode>& nextElementShadowNode,
    std::size_t suggestedIndex = SIZE_MAX) override;

  void setContainerManager(std::shared_ptr<azimgd::shadowlist::Container> containerManager);
  void setGeometryCache(std::shared_ptr<ShadowListViewGeometryCache> geometryCache);

  const std::shared_ptr<azimgd::shadowlist::Container>& getContainerManager() const { return containerManager_; }
  const std::shared_ptr<ShadowListViewGeometryCache>& getGeometryCache() const { return geometryCache_; }

  /*
   * ShadowListNative: the engine that synthesizes this list's rows (null for a ShadowList), and
   * the row keys the core reconciled on the commit that made this node.
   */
  void setNativeEngine(std::shared_ptr<ShadowListNativeEngine> nativeEngine) { nativeEngine_ = std::move(nativeEngine); }
  const std::shared_ptr<ShadowListNativeEngine>& getNativeEngine() const { return nativeEngine_; }
  void setNativeKeys(std::shared_ptr<const std::vector<std::string>> nativeKeys) { nativeKeys_ = std::move(nativeKeys); }
  const std::shared_ptr<const std::vector<std::string>>& getNativeKeys() const { return nativeKeys_; }

private:
  std::shared_ptr<azimgd::shadowlist::Container> containerManager_;

  std::shared_ptr<ShadowListNativeEngine> nativeEngine_;
  std::shared_ptr<const std::vector<std::string>> nativeKeys_;

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
   * Rows natively measured for the first time during this node's layout cycle. Yoga's
   * clone-in-place reports a new row through replaceChild before layout() runs, so the
   * first measurement is recorded where it happens rather than inferred in layout(). Candidates
   * for concealment; cleared at the end of layout().
   */
  std::vector<Tag> firstMeasuredTags_;

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
