#pragma once

#include <folly/dynamic.h>
#include <react/renderer/components/root/RootShadowNode.h>
#include <react/renderer/components/view/YogaLayoutableShadowNode.h>
#include <react/renderer/uimanager/UIManager.h>
#include <react/renderer/uimanager/UIManagerCommitHook.h>
#include <react/utils/ContextContainer.h>

#include "ShadowListViewShadowNode.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace facebook::react {

/*
 * Dematerializes rows that are retained but off screen, in the commit itself.
 *
 * WHY A COMMIT HOOK AND NOT layout()
 *
 * `overscan` currently decides two unrelated things at once: how far out rows stay
 * reconciled into the shadow tree, and how far out they exist as native views. The first is
 * what keeps a fast scroll off the JS round trip (native scroll -> event -> setState ->
 * render -> commit) that produces blank rows; the second is what costs the UI thread a
 * layoutSubviews / hit-test / accessibility walk on every frame for as long as the row is
 * mounted. Raising overscan to kill blanks therefore buys latency with permanent UI-thread
 * cost. Container::materializationOverscan splits the two, and this hook is what enforces
 * the narrower of the bands.
 *
 * It has to run here rather than in ShadowListViewShadowNode::layout() because of ordering:
 * ShadowTree::commit calls the commit hooks (ShadowTree.cpp, `delegate_.shadowTreeWillCommit`)
 * BEFORE `newRootShadowNode->layoutIfNeeded(...)`. Demoting a row from layout() would land
 * after Yoga had already laid its subtree out -- saving diff and mount but not layout -- and
 * would mutate children after Yoga calculation, next to
 * `react_native_assert(!YGNodeIsDirty(&yogaNode_))`.
 *
 * WHY display:none AND NOT CHILD REMOVAL
 *
 * Removing a row's children would make the demotion irreversible without React. The tree
 * handed to a scroll-driven commit is derived from the last COMMITTED tree, not from React's
 * fiber tree, so children this hook dropped are simply gone until React renders that row
 * again -- which is exactly the round trip the band exists to avoid. Setting
 * `yogaStyle.display = none` instead keeps the subtree intact while removing all of its cost:
 * ConcreteViewShadowNode::initialize() (re-run by the clone constructor) sets
 * ShadowNodeTraits::Trait::Hidden, which makes sliceChildShadowNodeViewPairs skip the node
 * and every descendant, so no native views are produced; and Yoga's CalculateLayout
 * zeroOutLayoutRecursively skips the subtree entirely. Promotion is then just flipping the
 * style back, which this hook can do on any commit, including one React never saw.
 *
 * That combination is the "Shadow-only" state: shadow nodes retained (so promotion costs a
 * mount rather than a render), native views gone, layout skipped.
 */
namespace shadowlistCommitHookDetail {

inline bool isHidden(const ShadowNode& shadowNode) {
  return shadowNode.getTraits().check(ShadowNodeTraits::Trait::Hidden);
}

/*
 * Whether this row's contents may be dropped from the view tree this frame.
 *
 * Two guards beyond the band itself:
 *
 *   * a row with no key falls back to its committed index, which a prepend or reorder can
 *     make name different content -- the same staleness the core avoids everywhere by
 *     resolving keys to live indices, so an unkeyed row is never pruned;
 *   * a row whose size the core cannot TRUST is never pruned. A hidden row is laid out to
 *     zero by Yoga and can therefore never be measured, so pruning one before its real size
 *     is known would freeze it at its estimate permanently. It also keeps the size feedback
 *     in ShadowListViewShadowNode::replaceChild honest: every pruned row already has a
 *     recorded size, so suppressing its zeroed feedback loses nothing.
 *
 *     Trust used to mean "natively measured", which made the band structurally unable to
 *     prune a row before it had been mounted at least once -- so a cold scroll paid the full
 *     mount cost for every row it passed, and only got the saving on the way back. An
 *     ahead-of-time PREDICTION (Container::predictedSizes) is the same knowledge arriving
 *     earlier, so Container::hasTrustedSize accepts either, and a predicted row can go
 *     straight to shadow-only without ever having existed as native views.
 */
inline bool shouldPruneRow(
  const azimgd::shadowlist::Container& container,
  const ShadowListElementViewProps& elementViewProps,
  std::size_t bandLow,
  std::size_t bandHigh) {
  if (elementViewProps.elementKey.empty()) {
    return false;
  }

  std::size_t elementIndex = container.findElementIndexByKey(elementViewProps.elementKey);
  if (elementIndex >= container.getElementsSize()) {
    return false;
  }

  /*
   * The band is passed in rather than re-derived per row. Container::shouldMaterialize
   * recomputes getMaterializedIndices() on every call, and that is an O(window) scan --
   * so asking it once per mounted row made this loop O(window^2) per commit, and twice
   * that per frame because the hook runs a detection pass before the rewrite.
   *
   * Hoisting is sound because the band cannot move while we hold it: nextChildrenForList
   * takes coreMutex for the whole loop, and the band derives only from scroll offset,
   * window size and geometry, none of which another thread can change under that lock.
   *
   * UNDEFINED_INDEX means the band could not be trusted, which fails OPEN -- materialize
   * everything -- exactly as Container::shouldMaterialize does.
   */
  if (bandLow == azimgd::shadowlist::UNDEFINED_INDEX) {
    return false;
  }
  if (elementIndex >= bandLow && elementIndex <= bandHigh) {
    return false;
  }

  return container.hasTrustedSize(elementIndex);
}

/*
 * Re-clone a row with `display` flipped. The clone constructor re-runs
 * ConcreteViewShadowNode::initialize(), which sets or clears Trait::Hidden from the new
 * style, and YogaLayoutableShadowNode's clone constructor calls updateYogaProps() because
 * the fragment carries props, which pushes the style into the Yoga node.
 */
inline std::shared_ptr<const ShadowNode> cloneRowWithDisplay(
  const ShadowNode& rowShadowNode,
  const ContextContainer& contextContainer,
  bool hidden) {
  /*
   * Props are not copy-constructible, so the style change goes through the same
   * parse-from-raw path a real prop update would take, rather than being poked into a
   * duplicated props object. `display` is a ViewProps/YogaStylableProps field, so the
   * codegen'd clone constructor picks it up for free.
   */
  PropsParserContext propsParserContext{rowShadowNode.getSurfaceId(), contextContainer};

  auto rawProps = RawProps(folly::dynamic::object("display", hidden ? "none" : "flex"));
  auto nextProps = rowShadowNode.getComponentDescriptor().cloneProps(
    propsParserContext, rowShadowNode.getProps(), std::move(rawProps));

  return rowShadowNode.clone({.props = std::move(nextProps)});
}

/*
 * Produce the list's next children, or nullptr when every row already carries the state the
 * band asks for. Returning nullptr is the important case: it is what keeps an unaffected
 * commit free of clones, so the hook costs a scan rather than a tree rewrite.
 */
inline std::shared_ptr<const std::vector<std::shared_ptr<const ShadowNode>>> nextChildrenForList(
  const ShadowListViewShadowNode& listShadowNode,
  const ContextContainer* contextContainer) {
  const auto& containerManager = listShadowNode.getContainerManager();
  if (!containerManager) {
    return nullptr;
  }

  std::lock_guard<std::recursive_mutex> lock(containerManager->coreMutex);

  if (containerManager->materializationOverscan < 0.0) {
    return nullptr;
  }

  const auto& children = listShadowNode.getChildren();
  std::vector<std::shared_ptr<const ShadowNode>> nextChildren;
  bool changed = false;

  /*
   * Resolve the materialization band ONCE for this commit (see shouldPruneRow).
   */
  std::size_t bandLow = azimgd::shadowlist::UNDEFINED_INDEX;
  std::size_t bandHigh = azimgd::shadowlist::UNDEFINED_INDEX;
  {
    auto band = containerManager->getMaterializedIndices();
    if (band.first != azimgd::shadowlist::UNDEFINED_INDEX &&
        band.second != azimgd::shadowlist::UNDEFINED_INDEX) {
      bandLow = containerManager->inverted ? band.second : band.first;
      bandHigh = containerManager->inverted ? band.first : band.second;
    }
  }

  for (std::size_t childIndex = 0; childIndex < children.size(); ++childIndex) {
    const auto& child = children[childIndex];
    const auto elementViewProps =
        std::dynamic_pointer_cast<const ShadowListElementViewProps>(child->getProps());

    if (!elementViewProps) {
      continue;
    }

    bool wantsHidden = shouldPruneRow(*containerManager, *elementViewProps, bandLow, bandHigh);
    if (wantsHidden == isHidden(*child)) {
      continue;
    }

    if (contextContainer == nullptr) {
      // Detection pass only: report that a rewrite is needed without building one.
      return std::make_shared<const std::vector<std::shared_ptr<const ShadowNode>>>();
    }

    if (!changed) {
      // First divergence: take the copy we are going to mutate.
      nextChildren = children;
      changed = true;
    }
    nextChildren[childIndex] = cloneRowWithDisplay(*child, *contextContainer, wantsHidden);
  }

  if (!changed) {
    return nullptr;
  }

  return std::make_shared<const std::vector<std::shared_ptr<const ShadowNode>>>(
    std::move(nextChildren));
}

inline void collectListsNeedingUpdate(
  const ShadowNode& shadowNode,
  std::unordered_set<std::shared_ptr<const ShadowNodeFamily>>& families) {
  /*
   * A component-handle compare rather than dynamic_cast. This walk visits every node in
   * the surface on every commit -- with a mounted window of rows that is well over a
   * thousand nodes -- and dynamic_cast on a deep hierarchy is a __dynamic_cast call, not
   * a pointer compare. The handle IS Fabric's type tag, so the static_cast that follows
   * is guaranteed by construction of the descriptor.
   */
  if (shadowNode.getComponentHandle() == ShadowListViewShadowNode::Handle()) {
    const auto listShadowNode = static_cast<const ShadowListViewShadowNode*>(&shadowNode);
    if (nextChildrenForList(*listShadowNode, nullptr) != nullptr) {
      families.insert(listShadowNode->getFamilyShared());
    }
    /*
     * Keep descending: a list nested inside another list's row is a real arrangement, and
     * the outer list's own rows are never ShadowListViewShadowNodes themselves.
     */
  }

  for (const auto& child : shadowNode.getChildren()) {
    collectListsNeedingUpdate(*child, families);
  }
}

}

class ShadowListCommitHook final : public UIManagerCommitHook {
public:
  /*
   * Registers a process-wide hook on `uiManager`, once. Safe to call from every list's
   * first adopt(); subsequent calls for the same UIManager are no-ops.
   *
   * The hook is intentionally never unregistered: during a reload the binding that owns the
   * UIManager can already be torn down when a destructor would run, and the UIManager frees
   * its hooks when it is destroyed anyway. This mirrors RNSScreenShadowNodeCommitHook.
   */
  static void registerOnce(
    UIManager& uiManager,
    std::shared_ptr<const ContextContainer> contextContainer);

  /*
   * Latches once any list asks for pruning, so an app that never opts in pays nothing but an
   * atomic load per commit instead of a tree walk. Deliberately monotonic: a list that later
   * disables pruning only costs a walk that finds nothing, whereas refcounting would have to
   * survive unmount ordering to avoid wrongly skipping live lists.
   */
  static void noteMaterializationEnabled() noexcept;

  void commitHookWasRegistered(const UIManager& uiManager) noexcept override {}
  void commitHookWasUnregistered(const UIManager& uiManager) noexcept override {}

  RootShadowNode::Unshared shadowTreeWillCommit(
    const ShadowTree& shadowTree,
    const RootShadowNode::Shared& oldRootShadowNode,
    const RootShadowNode::Unshared& newRootShadowNode,
    const ShadowTreeCommitOptions& commitOptions) noexcept override;

private:
  /*
   * Needed to build a PropsParserContext: props are not copy-constructible, so flipping
   * `display` goes through the normal parse-from-raw clone path (see cloneRowWithDisplay).
   */
  std::shared_ptr<const ContextContainer> contextContainer_;

  inline static std::atomic<bool> materializationEverEnabled_{false};
};

inline void ShadowListCommitHook::noteMaterializationEnabled() noexcept {
  materializationEverEnabled_.store(true, std::memory_order_relaxed);
}

inline void ShadowListCommitHook::registerOnce(
  UIManager& uiManager,
  std::shared_ptr<const ContextContainer> contextContainer) {
  /*
   * One hook per process, leaked deliberately: it must outlive every list, and unregistering
   * during a reload can touch an already-destroyed binding. The UIManager drops its hooks
   * when it is destroyed. Guarded so repeat calls from each list's first adopt() are no-ops,
   * and keyed on the UIManager so a reload that builds a new one registers again.
   */
  static std::mutex registrationMutex;
  static const UIManager* registeredUIManager = nullptr;

  std::lock_guard<std::mutex> lock(registrationMutex);
  if (registeredUIManager == &uiManager) {
    return;
  }

  static ShadowListCommitHook* hook = new ShadowListCommitHook();
  hook->contextContainer_ = std::move(contextContainer);
  uiManager.registerCommitHook(*hook);
  registeredUIManager = &uiManager;
}

inline RootShadowNode::Unshared ShadowListCommitHook::shadowTreeWillCommit(
  const ShadowTree& shadowTree,
  const RootShadowNode::Shared& oldRootShadowNode,
  const RootShadowNode::Unshared& newRootShadowNode,
  const ShadowTreeCommitOptions& commitOptions) noexcept {
  // Apps that never opt in pay one relaxed atomic load per commit, not a tree walk.
  if (!materializationEverEnabled_.load(std::memory_order_relaxed)) {
    return newRootShadowNode;
  }

  if (!newRootShadowNode || !contextContainer_) {
    return newRootShadowNode;
  }

  std::unordered_set<std::shared_ptr<const ShadowNodeFamily>> families;
  shadowlistCommitHookDetail::collectListsNeedingUpdate(*newRootShadowNode, families);

  if (families.empty()) {
    return newRootShadowNode;
  }

  /*
   * cloneMultiple rewrites every affected family in one pass, so a screen with several lists
   * clones the shared root-to-list spine once rather than once per list.
   *
   * nextChildrenForList runs a second time here, rather than being cached from the collect
   * pass, because the callback receives the node as it exists inside the tree being built,
   * which may already carry another family's rewrite.
   */
  auto nextRootShadowNode = newRootShadowNode->cloneMultiple(
    families,
    [contextContainer = contextContainer_.get()](
      const ShadowNode& oldShadowNode,
      const ShadowNodeFragment& fragment) -> std::shared_ptr<ShadowNode> {
      const auto& listShadowNode = static_cast<const ShadowListViewShadowNode&>(oldShadowNode);

      auto nextChildren =
        shadowlistCommitHookDetail::nextChildrenForList(listShadowNode, contextContainer);
      if (!nextChildren) {
        return oldShadowNode.clone(fragment);
      }

      return oldShadowNode.clone({
        .props = fragment.props,
        .children = nextChildren,
        .state = fragment.state,
      });
    });

  if (!nextRootShadowNode) {
    // cloneMultiple could not place the rewrite; commit the tree unchanged.
    return newRootShadowNode;
  }

  return std::static_pointer_cast<RootShadowNode>(nextRootShadowNode);
}

}
