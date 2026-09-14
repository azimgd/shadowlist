#pragma once

#include <folly/dynamic.h>
#include <react/renderer/components/root/RootShadowNode.h>
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/RawProps.h>
#include <react/renderer/runtimescheduler/RuntimeScheduler.h>
#include <react/renderer/uimanager/UIManager.h>
#include <react/renderer/uimanager/UIManagerBinding.h>
#include <react/renderer/uimanager/UIManagerCommitHook.h>
#include <react/utils/ContextContainer.h>

#ifdef ANDROID
#include <react/featureflags/ReactNativeFeatureFlags.h>
#endif

#include "ShadowListElementViewShadowNode.h"
#include "ShadowListViewShadowNode.h"
#include "ShadowListViewState.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace facebook::react {

/*
 * Dematerializes rows that are retained but off screen, in the commit itself.
 *
 * WHY A COMMIT HOOK AND NOT layout()
 *
 * `overscan` decides how far out rows stay reconciled into the shadow tree, which keeps a
 * fast scroll off the JS round trip (native scroll -> event -> setState -> render -> commit)
 * that produces blank rows. Every one of those rows would also exist as a native view, and a
 * mounted view costs the UI thread a layoutSubviews / hit-test / accessibility walk on every
 * frame. `nativeViewOverscan` (Container::materializationOverscan) splits the two, and this
 * hook enforces the narrower band.
 *
 * It runs here rather than in ShadowListViewShadowNode::layout() because of ordering:
 * ShadowTree::commit calls the commit hooks BEFORE `newRootShadowNode->layoutIfNeeded(...)`.
 * Hiding a row from layout() would land after Yoga had already laid its subtree out, and
 * would mutate children after the Yoga calculation, next to
 * `react_native_assert(!YGNodeIsDirty(&yogaNode_))`.
 *
 * WHY display:none AND NOT CHILD REMOVAL
 *
 * The tree handed to a scroll-driven commit is derived from the last COMMITTED tree, not from
 * React's fiber tree, so children removed here would stay gone until React rendered that row
 * again. `display: none` keeps the subtree intact while removing its cost:
 * ConcreteViewShadowNode::initialize() (re-run by the clone constructor) sets
 * ShadowNodeTraits::Trait::Hidden, which makes sliceChildShadowNodeViewPairs skip the node
 * and every descendant, so no native views are produced; Yoga skips the subtree as well.
 * Promotion restores the row's original props, which this hook can do on any commit,
 * including one React never saw.
 *
 * REGISTRATION
 *
 * One hook per component descriptor, registered lazily by the first list that opts in (see
 * ensureRegistered). The UIManager is not reachable from a descriptor directly, so the hook
 * asks the ContextContainer for the RuntimeScheduler and, on the JS thread, reads the
 * UIManager from the runtime's UIManagerBinding. All of these belong to the same React
 * instance as the descriptor. Hooks run on Apple platforms; Android skips registration unless
 * the app enables `useTraitHiddenOnAndroid`, without which RN still mounts hidden rows.
 */
namespace shadowlist::detail {

/*
 * Extra viewports a materialized row must leave the band by before it is hidden. Without it
 * a row at the band edge flips on every small offset change, and each flip is a mount.
 */
constexpr double NATIVE_VIEW_HIDE_HYSTERESIS = 0.5;

/*
 * Minimum band, in viewports, while a finger or momentum moves the list. A hidden row is
 * shown by a commit, and commits driven by scroll reports run on the JS thread, so a fast
 * fling can outrun a narrow band. Widening it while moving buys that commit time; the band
 * narrows again once the list settles.
 */
constexpr double NATIVE_VIEW_MOVING_OVERSCAN = 1.5;
constexpr double NATIVE_VIEW_MOVING_HIDE_HYSTERESIS = 1.0;

/*
 * Tracks whether the hook is cloning on this thread. The descriptor's adopt() reads it to
 * skip a second Virtualizer::update for a list clone that only changes row visibility.
 */
inline bool& commitHookCloneInProgress() {
  thread_local bool inProgress = false;
  return inProgress;
}

inline bool isCommitHookCloneInProgress() {
  return commitHookCloneInProgress();
}

class CommitHookCloneScope final {
public:
  CommitHookCloneScope() {
    commitHookCloneInProgress() = true;
  }
  ~CommitHookCloneScope() {
    commitHookCloneInProgress() = false;
  }
  CommitHookCloneScope(const CommitHookCloneScope&) = delete;
  CommitHookCloneScope& operator=(const CommitHookCloneScope&) = delete;
};

// A row whose props change in this commit, by its position among the list's children.
struct RowChange {
  std::size_t childIndex;
  std::shared_ptr<const Props> props;
};

/*
 * The scroll-axis ranges, in content coordinates, that rows must intersect to stay
 * materialized. `valid` false means the band cannot be trusted, which fails open: nothing is
 * hidden, and rows the hook hid are shown again.
 *
 * There are two ranges because the hook runs before the host has moved. The core offset is
 * where this commit sends the list (a scrollToIndex, an MVCP correction), while the offset
 * in state is the one the host reported and is still showing until the commit mounts. A row
 * stays while it is near either, so a jump never hides the rows on screen before the host
 * has scrolled away from them; the next scroll report hides them.
 */
struct Band {
  bool valid = false;
  double targetStart = 0.0;
  double targetEnd = 0.0;
  double shownStart = 0.0;
  double shownEnd = 0.0;
};

inline Band bandForContainer(
  const azimgd::shadowlist::Container& container,
  double shownOffset,
  double overscanViewports) {
  Band band;
  if (container.materializationOverscan < 0.0) {
    return band;
  }

  std::size_t measuredStartIndex = container.revision.measurementElementStartIndex;
  std::size_t measuredEndIndex = container.revision.measurementElementEndIndex;
  if (measuredStartIndex == azimgd::shadowlist::UNDEFINED_INDEX ||
      measuredEndIndex == azimgd::shadowlist::UNDEFINED_INDEX) {
    return band;
  }

  double windowSize = container.getWindowContainerSize();
  if (windowSize <= 0.0) {
    return band;
  }

  double targetOffset = container.getContainerOffset();
  band.valid = true;
  band.targetStart = targetOffset - windowSize * overscanViewports;
  band.targetEnd = targetOffset + windowSize * (1.0 + overscanViewports);
  band.shownStart = shownOffset - windowSize * overscanViewports;
  band.shownEnd = shownOffset + windowSize * (1.0 + overscanViewports);
  return band;
}

/*
 * Whether a row belongs outside `band`, and may therefore be hidden.
 *
 * Evaluated per row from its geometry: a row intersecting the band stays, including a
 * zero-sized one that still needs measuring. Two guards beyond the band:
 *
 *   * a row with no key could name different content after a prepend or reorder, so an
 *     unkeyed row is never hidden;
 *   * a row that has not been measured natively is never hidden. Yoga lays a hidden row out
 *     to zero, so hiding it first would freeze it at its estimate. An ahead-of-time
 *     prediction is not enough either: the band is evaluated before layout, so a row
 *     arriving with a prediction (after a jump, or in a list's first commits) can be judged
 *     against geometry that the same commit's layout is about to move, and nothing commits
 *     again to show it. A native measurement means the row has been on screen once and its
 *     geometry has settled.
 */
inline bool isOutsideBand(
  const azimgd::shadowlist::Container& container,
  const ShadowListElementViewProps& elementViewProps,
  const Band& band) {
  if (!band.valid || elementViewProps.elementKey.empty()) {
    return false;
  }

  std::size_t elementIndex = container.findElementIndexByKey(elementViewProps.elementKey);
  if (elementIndex >= container.getElementsSize()) {
    return false;
  }

  const auto& element = container.getElementAtIndex(elementIndex);
  if (!element.measured) {
    return false;
  }

  double elementStart = container.horizontal ? element.offsetX : element.offsetY;
  double elementEnd = elementStart + (container.horizontal ? element.width : element.height);
  bool outsideTarget = elementEnd < band.targetStart || elementStart > band.targetEnd;
  bool outsideShown = elementEnd < band.shownStart || elementStart > band.shownEnd;
  return outsideTarget && outsideShown;
}

/*
 * Parse `display: none` onto a row's props. Props are not copy-constructible, so the style
 * change goes through the same parse-from-raw path a real prop update takes. `display` is a
 * ViewProps field, so the codegen'd props constructor picks it up.
 */
inline std::shared_ptr<const Props> hiddenPropsForRow(
  const ShadowNode& rowShadowNode,
  const ContextContainer& contextContainer) {
  PropsParserContext propsParserContext{rowShadowNode.getSurfaceId(), contextContainer};
  return rowShadowNode.getComponentDescriptor().cloneProps(
    propsParserContext,
    rowShadowNode.getProps(),
    RawProps(folly::dynamic::object("display", "none")));
}

/*
 * Decide which of the list's rows change visibility in this commit. Returns false when every
 * row already has the visibility the band asks for, which keeps an unaffected commit free of
 * clones.
 */
inline bool collectRowChanges(
  const ShadowListViewShadowNode& listShadowNode,
  const ContextContainer& contextContainer,
  std::vector<RowChange>& changes) {
  const auto& containerManager = listShadowNode.getContainerManager();
  const auto& geometryCache = listShadowNode.getGeometryCache();
  if (!containerManager || !geometryCache) {
    return false;
  }

  std::lock_guard<std::recursive_mutex> lock(containerManager->coreMutex);

  auto& rowProps = geometryCache->rowProps;
  double materializationOverscan = containerManager->materializationOverscan;
  if (materializationOverscan < 0.0 && rowProps.empty()) {
    return false;
  }

  const auto& stateData = listShadowNode.getStateData();
  bool moving = stateData.scrollPhase_ != SCROLL_PHASE_IDLE;
  double showOverscan = moving ? std::max(materializationOverscan, NATIVE_VIEW_MOVING_OVERSCAN) : materializationOverscan;
  double hideOverscan = showOverscan + (moving ? NATIVE_VIEW_MOVING_HIDE_HYSTERESIS : NATIVE_VIEW_HIDE_HYSTERESIS);

  double shownOffset = containerManager->horizontal ? stateData.containerOffsetX_ : stateData.containerOffsetY_;
  Band showBand = bandForContainer(*containerManager, shownOffset, showOverscan);
  Band hideBand = bandForContainer(*containerManager, shownOffset, hideOverscan);

  /*
   * A row picked up for drag-to-reorder is carried by auto-scroll; hiding it would unmount
   * its view and abort the gesture. Rows are only shown while a drag is active (type 1 is
   * pickup, 3 is drop).
   */
  bool hidingAllowed = static_cast<int>(stateData.dragEventType_) != 1;

  const auto& children = listShadowNode.getChildren();
  for (std::size_t childIndex = 0; childIndex < children.size(); ++childIndex) {
    const auto& child = children[childIndex];
    if (child->getComponentHandle() != ShadowListElementViewShadowNode::Handle()) {
      continue;
    }

    const auto& elementViewProps = static_cast<const ShadowListElementViewProps&>(*child->getProps());
    auto rowPropsIterator = rowProps.find(child->getTag());

    if (child->getTraits().check(ShadowNodeTraits::Trait::Hidden)) {
      // Only rows this hook hid are shown again; a row hidden by its own style is not ours.
      if (rowPropsIterator == rowProps.end() || rowPropsIterator->second.hiddenProps != child->getProps()) {
        continue;
      }
      if (!isOutsideBand(*containerManager, elementViewProps, showBand)) {
        changes.push_back({childIndex, rowPropsIterator->second.sourceProps});
      }
      continue;
    }

    if (!hidingAllowed || !isOutsideBand(*containerManager, elementViewProps, hideBand)) {
      continue;
    }

    if (rowPropsIterator == rowProps.end() || rowPropsIterator->second.sourceProps != child->getProps()) {
      rowPropsIterator = rowProps.insert_or_assign(
        child->getTag(),
        ShadowListViewGeometryCache::RowProps{child->getProps(), hiddenPropsForRow(*child, contextContainer)}).first;
    }
    changes.push_back({childIndex, rowPropsIterator->second.hiddenProps});
  }

  // Forget rows that left the list, once the cache clearly outgrows the children.
  if (rowProps.size() > children.size() * 2 + 32) {
    std::unordered_set<Tag> childTags;
    childTags.reserve(children.size());
    for (const auto& child : children) {
      childTags.insert(child->getTag());
    }
    for (auto iterator = rowProps.begin(); iterator != rowProps.end();) {
      iterator = childTags.contains(iterator->first) ? std::next(iterator) : rowProps.erase(iterator);
    }
  }

  return !changes.empty();
}

}

class ShadowListCommitHook final : public UIManagerCommitHook,
                                   public std::enable_shared_from_this<ShadowListCommitHook> {
public:
  explicit ShadowListCommitHook(std::shared_ptr<const ContextContainer> contextContainer) :
    contextContainer_(std::move(contextContainer)) {}

  /*
   * Registers this hook with the UIManager of its React instance, once. Called from adopt()
   * of every list that opts in; after the first call it costs an atomic load. Returns false
   * on platforms where hiding rows does not remove their native views, in which case lists
   * are not tracked either.
   *
   * The hook is never unregistered. The descriptor owns it, the component descriptor registry
   * owns the descriptor and the UIManager owns the registry, so it outlives every commit of
   * the UIManager it is registered with; unregistering from a destructor would reach into a
   * UIManager that is already being torn down.
   */
  bool ensureRegistered();

  // Adds a list to the registry of its surface, so commits of other surfaces exit early.
  void trackList(std::shared_ptr<const ShadowNodeFamily> family);

  void commitHookWasRegistered(const UIManager& uiManager) noexcept override {}
  void commitHookWasUnregistered(const UIManager& uiManager) noexcept override {}

  RootShadowNode::Unshared shadowTreeWillCommit(
    const ShadowTree& shadowTree,
    const RootShadowNode::Shared& oldRootShadowNode,
    const RootShadowNode::Unshared& newRootShadowNode,
    const ShadowTreeCommitOptions& commitOptions) noexcept override;

private:
  static constexpr std::uint8_t REGISTRATION_IDLE = 0;
  static constexpr std::uint8_t REGISTRATION_SCHEDULED = 1;
  static constexpr std::uint8_t REGISTRATION_DONE = 2;

  std::atomic<std::uint8_t> registration_{REGISTRATION_IDLE};

  /*
   * Needed to build a PropsParserContext: props are not copy-constructible, so hiding a row
   * goes through the normal parse-from-raw clone path (see hiddenPropsForRow).
   */
  const std::shared_ptr<const ContextContainer> contextContainer_;

  std::atomic<std::size_t> trackedListCount_{0};
  std::mutex listsMutex_;
  std::unordered_map<SurfaceId, std::vector<std::weak_ptr<const ShadowNodeFamily>>> lists_;
};

inline bool ShadowListCommitHook::ensureRegistered() {
#if defined(ANDROID)
  if (!ReactNativeFeatureFlags::useTraitHiddenOnAndroid()) {
    return false;
  }
#elif !defined(__APPLE__)
  return false;
#endif

  if (registration_.load(std::memory_order_relaxed) != REGISTRATION_IDLE) {
    return true;
  }
  std::uint8_t expected = REGISTRATION_IDLE;
  if (!registration_.compare_exchange_strong(expected, REGISTRATION_SCHEDULED)) {
    return true;
  }

  auto weakRuntimeScheduler = contextContainer_
    ? contextContainer_->find<std::weak_ptr<RuntimeScheduler>>(RuntimeSchedulerKey)
    : std::nullopt;
  auto runtimeScheduler = weakRuntimeScheduler ? weakRuntimeScheduler->lock() : nullptr;
  if (!runtimeScheduler) {
    // Rows stay materialized until a later adopt() manages to register.
    registration_.store(REGISTRATION_IDLE);
    return true;
  }

  std::weak_ptr<ShadowListCommitHook> weakHook = shared_from_this();
  runtimeScheduler->scheduleWork([weakHook](jsi::Runtime& runtime) {
    auto hook = weakHook.lock();
    if (!hook) {
      return;
    }
    auto binding = UIManagerBinding::getBinding(runtime);
    if (!binding) {
      hook->registration_.store(REGISTRATION_IDLE);
      return;
    }
    binding->getUIManager().registerCommitHook(*hook);
    hook->registration_.store(REGISTRATION_DONE);
  });
  return true;
}

inline void ShadowListCommitHook::trackList(std::shared_ptr<const ShadowNodeFamily> family) {
  if (!family) {
    return;
  }
  std::lock_guard<std::mutex> lock(listsMutex_);
  lists_[family->getSurfaceId()].push_back(std::weak_ptr<const ShadowNodeFamily>(family));
  trackedListCount_.fetch_add(1, std::memory_order_relaxed);
}

inline RootShadowNode::Unshared ShadowListCommitHook::shadowTreeWillCommit(
  const ShadowTree& shadowTree,
  const RootShadowNode::Shared& oldRootShadowNode,
  const RootShadowNode::Unshared& newRootShadowNode,
  const ShadowTreeCommitOptions& commitOptions) noexcept {
  if (trackedListCount_.load(std::memory_order_relaxed) == 0 || !newRootShadowNode || !contextContainer_) {
    return newRootShadowNode;
  }

  std::vector<std::shared_ptr<const ShadowNodeFamily>> families;
  {
    std::lock_guard<std::mutex> lock(listsMutex_);
    auto surfaceLists = lists_.find(shadowTree.getSurfaceId());
    if (surfaceLists == lists_.end()) {
      return newRootShadowNode;
    }

    auto& entries = surfaceLists->second;
    for (std::size_t entryIndex = 0; entryIndex < entries.size();) {
      if (auto family = entries[entryIndex].lock()) {
        families.push_back(std::move(family));
        ++entryIndex;
      } else {
        entries[entryIndex] = std::move(entries.back());
        entries.pop_back();
        trackedListCount_.fetch_sub(1, std::memory_order_relaxed);
      }
    }
    if (entries.empty()) {
      lists_.erase(surfaceLists);
    }
  }

  /*
   * Locate each tracked list through its family's ancestor chain, O(depth), instead of
   * walking the whole tree, and decide its row changes against the node as it stands in the
   * tree being committed.
   */
  std::unordered_set<std::shared_ptr<const ShadowNodeFamily>> familiesToUpdate;
  std::unordered_map<Tag, std::vector<shadowlist::detail::RowChange>> changesByListTag;
  for (const auto& family : families) {
    auto ancestors = family->getAncestors(*newRootShadowNode);
    if (ancestors.empty()) {
      continue;
    }

    const auto& [parentShadowNode, childIndex] = ancestors.back();
    const auto& listShadowNode = parentShadowNode.get().getChildren()[childIndex];
    if (listShadowNode->getComponentHandle() != ShadowListViewShadowNode::Handle()) {
      continue;
    }

    std::vector<shadowlist::detail::RowChange> changes;
    if (shadowlist::detail::collectRowChanges(
          static_cast<const ShadowListViewShadowNode&>(*listShadowNode), *contextContainer_, changes)) {
      changesByListTag.emplace(family->getTag(), std::move(changes));
      familiesToUpdate.insert(family);
    }
  }

  if (familiesToUpdate.empty()) {
    return newRootShadowNode;
  }

  /*
   * cloneMultiple clones the root-to-list spine once for every affected list. The callback
   * runs for each node on that spine and receives the ORIGINAL node, with any rewritten
   * descendants in `fragment.children`; row changes are therefore applied on top of those
   * children, so a nested list's rewrite survives its outer list's.
   */
  shadowlist::detail::CommitHookCloneScope cloneScope;
  auto nextRootShadowNode = newRootShadowNode->cloneMultiple(
    familiesToUpdate,
    [&changesByListTag](const ShadowNode& oldShadowNode, const ShadowNodeFragment& fragment) -> std::shared_ptr<ShadowNode> {
      auto listChanges = changesByListTag.find(oldShadowNode.getTag());
      if (listChanges == changesByListTag.end()) {
        return oldShadowNode.clone(fragment);
      }

      auto nextChildren = std::make_shared<std::vector<std::shared_ptr<const ShadowNode>>>(
        fragment.children ? *fragment.children : oldShadowNode.getChildren());
      for (const auto& change : listChanges->second) {
        if (change.childIndex >= nextChildren->size()) {
          continue;
        }
        auto& child = (*nextChildren)[change.childIndex];
        child = child->clone({.props = change.props});
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
