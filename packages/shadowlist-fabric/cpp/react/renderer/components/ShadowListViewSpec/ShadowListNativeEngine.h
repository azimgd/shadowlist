#pragma once

#include <folly/dynamic.h>
#include <react/renderer/core/ConcreteState.h>
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/ShadowNode.h>

#include "ShadowListNativeBinding.h"
#include "ShadowListViewState.h"

#include <shadowlist-core/Container.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace facebook::react {

/*
 * The native side of one <ShadowListNative>: its data, its compiled templates, and the rows it
 * synthesized from them. No React component renders a row. A row is a clone of a template's
 * shadow subtree with the row's data bound into its props, built synchronously on the commit
 * that needs it (see ShadowListViewComponentDescriptor::cloneShadowNode).
 *
 * Shared by every committed clone of the list node (like the Container), and registered under
 * the list's `nativeListId` so the JSI binding can mutate the data from JS
 * (see ShadowListNativeJSI.h). Guarded by one mutex: JS mutates on the JS thread, commits read
 * on whichever thread commits. Lock order is core (Container::coreMutex) then engine.
 */
class ShadowListNativeEngine final {
public:
  using ListState = ConcreteState<ShadowListViewState>;
  using ChildList = std::vector<std::shared_ptr<const ShadowNode>>;

  explicit ShadowListNativeEngine(std::string listId);

  const std::string& listId() const { return listId_; }

#pragma mark - Data (JS thread)

  /*
   * Replace the rows. `keys` and `templates` run parallel to `items` (templates may be empty,
   * then every row uses the default template). A row whose key existed keeps its synthesized
   * nodes when its item is unchanged, so a refetch that returns the same data rebinds nothing.
   * Returns the new row count.
   */
  std::size_t setData(const folly::dynamic& items, const std::vector<std::string>& keys, const std::vector<std::string>& templates);

  // Insert before `index` (clamped; past the end appends). Keys already present are skipped.
  std::size_t insertItems(
    std::size_t index,
    const folly::dynamic& items,
    const std::vector<std::string>& keys,
    const std::vector<std::string>& templates);

  // Shallow-merge `patch` into the row's item (or replace it). False when the key is unknown.
  bool updateItem(const std::string& key, const folly::dynamic& patch, const std::string& templateName, bool replace);

  std::size_t removeItems(const std::vector<std::string>& keys);

  bool moveItem(const std::string& key, std::size_t toIndex);

  /*
   * Style props merged over one element of a template (by its `id`) for every row, current and
   * future. A null style clears the override.
   */
  void setTemplateStyle(const std::string& templateName, const std::string& elementId, const folly::dynamic& style);

  // { initialRows, padRows, cacheRows }
  void configure(const folly::dynamic& config);

  folly::dynamic getItem(const std::string& key) const;

  std::vector<std::string> getKeys() const;

  std::size_t size() const;

  /*
   * The row a native view tag belongs to: any node of a synthesized row resolves to that row's
   * key and current index. Touch events carry the hit view's tag, which is how a press on a
   * cloned element is routed to its row.
   */
  std::optional<std::pair<std::string, std::size_t>> resolveTag(Tag tag) const;

  /*
   * Ask for a commit of the list, so the next adopt/reconcile picks up the store. Coalesced by
   * the event queue: many mutations in one JS task make one commit.
   */
  void requestCommit();

#pragma mark - Commit side

  struct KeysSnapshot {
    std::shared_ptr<const std::vector<std::string>> keys;
    std::uint64_t version = 0;
  };

  // The row keys the core reconciles against this commit.
  KeysSnapshot keysSnapshot() const;

  // The newest state of the list node, used to request commits.
  void attachState(const std::shared_ptr<const ListState>& state);

  /*
   * The children the list node should carry: its own non-row children (templates, header,
   * footer) in order, with the synthesized rows for the window the core wants mounted placed
   * right after the templates container. Null when `children` already are exactly that.
   *
   * `keys` is the snapshot the core reconciled this commit, so every mounted row is one the core
   * knows. Before the core has a viewport the window is seeded from `initialIndex` (or the end
   * of an inverted list).
   */
  std::shared_ptr<const ChildList> reconcileRows(
    const ShadowNode& listNode,
    const ChildList& children,
    const std::vector<std::string>& keys,
    azimgd::shadowlist::Container& core,
    int initialIndex,
    bool inverted);

  /*
   * After the list laid out: remember the laid-out rows (reused as-is when they re-enter), and
   * request another commit when the measured rows no longer cover the viewport.
   */
  void didLayout(const ShadowNode& listNode, azimgd::shadowlist::Container& core);

private:
  struct Row {
    std::string key;
    folly::dynamic item;
    std::string templateName;
    std::uint64_t version = 0;
  };

  struct Element {
    std::shared_ptr<const ShadowNode> prototype;
    std::string elementId;
    std::vector<std::pair<std::string, ShadowListNativeExpression>> bindings;
    std::optional<ShadowListNativeExpression> text;
    bool keepNativeId = false;
    bool isRawText = false;
    // Prototype props with the element marker stripped and the style override applied.
    Props::Shared baseProps;
    std::vector<Element> children;
  };

  struct Template {
    std::string name;
    Element root;
    // Props pointers and child counts of the subtree, in walk order; a change recompiles.
    std::vector<const void*> signature;
    // Component names and child counts; a change means new rows cannot reuse old nodes.
    std::string shape;
    std::uint64_t version = 0;
    std::uint64_t shapeVersion = 0;
    bool basePropsStale = true;
  };

  struct RowNode {
    std::shared_ptr<const ShadowNode> node;
    std::uint64_t rowVersion = 0;
    std::string templateName;
    std::uint64_t templateVersion = 0;
    std::uint64_t shapeVersion = 0;
    std::uint64_t usedAt = 0;
  };

  void rebuildIndexLocked();
  void structureChangedLocked();

  void compileTemplatesLocked(const ShadowNode& container, const PropsParserContext& context);
  void compileElement(Element& element, const std::shared_ptr<const ShadowNode>& node, std::vector<const void*>& signature, std::string& shape);
  void refreshBasePropsLocked(Template& compiled, const PropsParserContext& context);
  void refreshElementBaseProps(Element& element, const std::string& templateName, const PropsParserContext& context);
  Template* templateForLocked(const std::string& name);

  std::shared_ptr<const ShadowNode> buildRowLocked(
    Template& compiled,
    const Row& row,
    const std::shared_ptr<const ShadowNode>& existing,
    bool rootPropsCurrent,
    const PropsParserContext& context);
  std::shared_ptr<const ShadowNode> buildNode(
    const Element& element,
    const folly::dynamic& item,
    const std::string& key,
    const std::shared_ptr<const ShadowNode>& existing,
    const Props::Shared* propsOverride,
    const std::optional<std::string>& rawText,
    const PropsParserContext& context);

  void forgetTagsLocked(const ShadowNode& node, const std::string& key);
  void evictLocked(std::size_t keep);

  const std::string listId_;
  mutable std::mutex mutex_;

  std::vector<Row> rows_;
  std::unordered_map<std::string, std::size_t> keyIndex_;
  std::shared_ptr<const std::vector<std::string>> keys_;
  std::uint64_t keysVersion_ = 1;
  std::uint64_t nextRowVersion_ = 1;

  std::unordered_map<std::string, Template> templates_;
  std::string defaultTemplate_;
  const ShadowNode* templatesContainer_ = nullptr;
  std::shared_ptr<const ShadowNode> templatesContainerHold_;
  std::unordered_map<std::string, std::unordered_map<std::string, folly::dynamic>> templateStyles_;
  std::uint64_t nextTemplateVersion_ = 1;

  std::unordered_map<std::string, RowNode> rowNodes_;
  std::unordered_map<Tag, std::string> tagKeys_;
  std::uint64_t clock_ = 0;

  // Keys at the ends of the rows the last reconcile mounted.
  std::string mountedLowKey_;
  std::string mountedHighKey_;
  std::size_t mountedCount_ = 0;
  // What the last coverage request was made for, so an unfixable gap does not loop.
  std::tuple<std::size_t, std::size_t, std::uint64_t, std::size_t> lastCoverageRequest_{0, 0, 0, 0};

  std::size_t initialRows_ = 10;
  std::size_t padRows_ = 2;
  std::size_t cacheRows_ = 64;

  /*
   * Held strongly: only its family matters for updateState, and the layout pass replaces the
   * node's state object (setStateData) after adopt attached it, so a weak reference to the adopted
   * one expires whenever a commit publishes geometry, and every later nudge would be dropped.
   */
  std::shared_ptr<const ListState> state_;
};

/*
 * listId -> engine. A list node holds its engine strongly; the registry holds it strongly only
 * while JS has it pinned (from the first data call until the component unmounts), and weakly
 * otherwise, so an engine dies with the last list node that used it.
 */
class ShadowListNativeRegistry final {
public:
  static std::shared_ptr<ShadowListNativeEngine> obtain(const std::string& listId);
  static std::shared_ptr<ShadowListNativeEngine> find(const std::string& listId);
  static void pin(const std::string& listId);
  static void release(const std::string& listId);
};

}
