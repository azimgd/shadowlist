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
 * Native side of one ShadowListNative: its data, its templates and the rows built from them.
 * React never renders a row. Each row is a copy of a template with the item's data filled in,
 * built during the commit that needs it.
 * Every clone of the list node shares one engine, found by nativeListId so JS can change the data.
 * One mutex guards it, since JS writes on the JS thread and commits read on any thread.
 * Lock order is the core's mutex first, then the engine's.
 */
class ShadowListNativeEngine final {
public:
  using ListState = ConcreteState<ShadowListViewState>;
  using ChildList = std::vector<std::shared_ptr<const ShadowNode>>;

  explicit ShadowListNativeEngine(std::string listId);

  const std::string& listId() const { return listId_; }

#pragma mark - Data (JS thread)

  /*
   * Replaces all rows and returns the new count. Keys and templates line up with items, and
   * empty templates means every row uses the default one. Rows with an unchanged item keep
   * their nodes, so refetching the same data rebuilds nothing.
   * scrollToStart jumps to the top in the same commit as the new rows. Waiting a commit would
   * let the list briefly hold the old first row in place, which shows while it is moving.
   */
  std::size_t setData(
    const folly::dynamic& items,
    const std::vector<std::string>& keys,
    const std::vector<std::string>& templates,
    bool scrollToStart = false);

  /*
   * Indexed rows are keyed by position and never stored. Each row's item is built on demand
   * from its index and order value, plus any extra data given for that row.
   * A million rows only cost the order array. Keys are made once per count, so a new order
   * with the same count only rebinds the mounted rows that changed.
   * updateItem, getItem and resolveTag take the position as the key, and updateItem merges into
   * the row's extra data, dropping null fields. Insert, remove and move don't work here, and
   * setData leaves indexed mode.
   * With ids, each row is keyed by its id instead, so inserts and moves keep the visible rows
   * in place. Keys and the id lookup are rebuilt when the ids change.
   */
  std::size_t setIndexed(
    std::size_t count,
    std::vector<std::int32_t> order,
    const std::string& indexField,
    const std::string& valueField,
    const std::vector<std::size_t>& extraIndices,
    const folly::dynamic& extraItems,
    const std::vector<std::string>& extraTemplates,
    bool scrollToStart = false,
    std::vector<std::int32_t> ids = {});

  /*
   * Inserts before index, or appends past the end. Keys that already exist are skipped.
   */
  std::size_t insertItems(
    std::size_t index,
    const folly::dynamic& items,
    const std::vector<std::string>& keys,
    const std::vector<std::string>& templates);

  /*
   * Merges patch into the row's item, or replaces it. Returns false for an unknown key.
   */
  bool updateItem(const std::string& key, const folly::dynamic& patch, const std::string& templateName, bool replace);

  std::size_t removeItems(const std::vector<std::string>& keys);

  bool moveItem(const std::string& key, std::size_t toIndex);

  /*
   * Overrides the style of one template element, found by id, in every row now and later.
   * A null style clears it.
   */
  void setTemplateStyle(const std::string& templateName, const std::string& elementId, const folly::dynamic& style);

  void configure(const folly::dynamic& config);

  folly::dynamic getItem(const std::string& key) const;

  std::vector<std::string> getKeys() const;

  std::size_t size() const;

  struct ResolvedTag {
    std::string key;
    std::size_t index = 0;
    // Entry of the innermost repeated element holding the tag, or -1 when there is none.
    int repeatIndex = -1;
  };

  /*
   * Finds the row a view tag belongs to. Touches carry the tag, so this is how a press on a
   * row's element reaches that row.
   */
  std::optional<ResolvedTag> resolveTag(Tag tag) const;

  /*
   * Asks for a commit so the list picks up the new data. Many changes in one JS task make one commit.
   */
  void requestCommit();

  /*
   * Scrolls to a row, to the end with -1, or to the start, once earlier changes are laid out.
   * A view command could land before the rows change or before they are measured, and it can
   * merge with a data nudge. So the core is told directly once the rows it needs are measured.
   */
  static constexpr double SCROLL_TO_START = -2.0;
  void requestScroll(double index, double viewPosition);

#pragma mark - Commit side

  struct KeysSnapshot {
    std::shared_ptr<const std::vector<std::string>> keys;
    std::uint64_t version = 0;
  };

  /*
   * The row keys the core reconciles against this commit.
   */
  KeysSnapshot keysSnapshot() const;

  /*
   * The list node's newest state, used to ask for commits.
   */
  void attachState(const std::shared_ptr<const ListState>& state);

  /*
   * Builds the list node's children: its own children like the header, with the mounted rows
   * right after the templates. Returns null when the children are already right.
   * Keys are the ones the core saw this commit, so the core knows every mounted row.
   * Before the list has a size, the rows start at initialIndex or at the end when inverted.
   */
  std::shared_ptr<const ChildList> reconcileRows(
    const ShadowNode& listNode,
    const ChildList& children,
    const std::vector<std::string>& keys,
    azimgd::shadowlist::Container& core,
    int initialIndex,
    bool inverted);

  /*
   * Keeps the laid out rows for reuse and asks for another commit if they no longer fill the screen.
   */
  void didLayout(const ShadowNode& listNode, azimgd::shadowlist::Container& core);

  /*
   * Passes a pending scroll to the core before it updates. A scrollToStart from setData waits
   * for the commit that has its rows. Returns true when a scroll was passed on.
   */
  bool applyPendingScroll(azimgd::shadowlist::Container& core, std::uint64_t keysVersion);

  /*
   * A scroll command stops a fling, like the host's own commands do. The host only sees an engine
   * scroll when the commit mounts, so the commit carries this token and the host then stops the
   * fling and sets the offset. Zero means none.
   */
  void setMomentumYieldToken(std::uint64_t token);
  std::uint64_t momentumYieldToken() const;

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
    /*
     * Repeats the children once per entry of the array at this path, up to repeatMax.
     * Bindings inside read from that entry.
     */
    std::optional<std::vector<std::string>> repeat;
    std::size_t repeatMax = SIZE_MAX;
    bool keepNativeId = false;
    bool isRawText = false;
    // Template props without the element marker and with the style override applied.
    Props::Shared baseProps;
    std::vector<Element> children;
  };

  struct Template {
    std::string name;
    Element root;
    // Props and child counts of the subtree. A change recompiles the template.
    std::vector<const void*> signature;
    // Component names and child counts. A change means rows can't reuse their old nodes.
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

  struct IndexedExtra {
    folly::dynamic item = folly::dynamic::object();
    std::string templateName;
    std::uint64_t version = 0;
  };

  std::optional<std::size_t> indexOfKeyLocked(const std::string& key) const;
  folly::dynamic indexedItemLocked(std::size_t index) const;

  void nudge();
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
    const PropsParserContext& context,
    bool inFlow = false);

  /*
   * A pinned copy of the current sticky row for the section header overlay, or null.
   * It is kept while the same row stays pinned, and presses on it reach that row.
   */
  std::shared_ptr<const ShadowNode> stickyRowLocked(
    const std::vector<std::string>& keys,
    azimgd::shadowlist::Container& core,
    bool inverted,
    const PropsParserContext& context);
  void dropStickyLocked();
  std::shared_ptr<const ShadowNode> buildNode(
    const Element& element,
    const folly::dynamic& item,
    const std::string& key,
    const std::shared_ptr<const ShadowNode>& existing,
    const Props::Shared* propsOverride,
    const std::optional<std::string>& rawText,
    int repeatIndex,
    const PropsParserContext& context);

  void forgetTagsLocked(const ShadowNode& node, const std::string& key);
  void evictLocked(std::size_t keep);

  const std::string listId_;
  mutable std::mutex mutex_;

  std::vector<Row> rows_;
  std::unordered_map<std::string, std::size_t> keyIndex_;

  // In indexed mode rows_ and keyIndex_ stay empty.
  bool indexed_ = false;
  std::size_t indexedCount_ = 0;
  std::vector<std::int32_t> indexedOrder_;
  // Row ids used as keys, empty when rows are keyed by position.
  std::vector<std::int32_t> indexedIds_;
  std::unordered_map<std::int32_t, std::size_t> idIndex_;
  std::string indexField_;
  std::string valueField_;
  std::unordered_map<std::size_t, IndexedExtra> indexedExtras_;
  // Version of every row without extra data. A new order bumps it.
  std::uint64_t indexedEpoch_ = 0;
  std::shared_ptr<const std::vector<std::string>> keys_;
  std::uint64_t keysVersion_ = 1;
  std::uint64_t nextRowVersion_ = 1;

  std::unordered_map<std::string, Template> templates_;
  std::string defaultTemplate_;
  const ShadowNode* templatesContainer_ = nullptr;
  std::shared_ptr<const ShadowNode> templatesContainerHold_;
  std::unordered_map<std::string, std::unordered_map<std::string, folly::dynamic>> templateStyles_;
  std::uint64_t nextTemplateVersion_ = 1;
#ifdef RN_SERIALIZABLE_STATE
  /*
   * Android builds views from raw props, which after a template update only hold what changed.
   * Rows are new views and need every prop, so keep the full raw props for each template element.
   */
  struct PrototypeRawProps {
    const Props* props = nullptr;
    folly::dynamic raw;
  };
  std::unordered_map<Tag, PrototypeRawProps> prototypeRawProps_;
  std::unordered_map<Tag, PrototypeRawProps> nextPrototypeRawProps_;
#endif

  std::unordered_map<std::string, RowNode> rowNodes_;
  RowNode stickyNode_;
  std::string stickyKey_;
  struct TagEntry {
    std::string key;
    int repeatIndex = -1;
  };
  std::unordered_map<Tag, TagEntry> tagKeys_;
  std::uint64_t clock_ = 0;

  // First and last keys the last pass mounted.
  std::string mountedLowKey_;
  std::string mountedHighKey_;
  std::size_t mountedCount_ = 0;
  // Remembers the last request to fill the screen, so a gap that can't be fixed doesn't loop.
  std::tuple<std::size_t, std::size_t, std::uint64_t, std::size_t> lastCoverageRequest_{0, 0, 0, 0};

  // Bumped on every data change, then what the last pass read and what has been laid out.
  std::uint64_t storeVersion_ = 0;
  std::uint64_t reconciledVersion_ = 0;
  std::uint64_t laidOutVersion_ = 0;
  struct PendingScroll {
    double index = -1.0;
    double viewPosition = 0.0;
    std::uint64_t afterVersion = 0;
    // When set, the scroll runs with these keys instead of after layout. Used by setData's scrollToStart.
    std::uint64_t withKeysVersion = 0;
  };
  std::optional<PendingScroll> pendingScroll_;
  std::uint64_t momentumYieldToken_ = 0;

  std::size_t initialRows_ = 10;
  std::size_t padRows_ = 2;
  std::size_t cacheRows_ = 64;

  /*
   * Held strongly on purpose. The layout pass swaps the node's state object, so a weak
   * reference would expire and every later commit request would be dropped.
   */
  std::shared_ptr<const ListState> state_;
};

/*
 * Maps list ids to engines without owning them. An engine lives while a list node or a JS
 * handle holds it, so none outlives the runtime or the render that made it.
 */
class ShadowListNativeRegistry final {
public:
  /*
   * Returns the engine for the list, making one if none is alive. Used by the list node.
   */
  static std::shared_ptr<ShadowListNativeEngine> obtain(const std::string& listId);
  static std::shared_ptr<ShadowListNativeEngine> find(const std::string& listId);
  /*
   * Same as obtain after clearing dead entries. Used by the JS handle.
   */
  static std::shared_ptr<ShadowListNativeEngine> open(const std::string& listId);
};

}
