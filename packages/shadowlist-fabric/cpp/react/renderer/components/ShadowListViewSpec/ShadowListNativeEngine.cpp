#include "ShadowListNativeEngine.h"

#include <folly/json.h>
#include <react/renderer/components/ShadowListViewSpec/Props.h>
#include <react/renderer/core/ComponentDescriptor.h>
#include <react/renderer/core/RawProps.h>
#include <react/renderer/core/ShadowNodeFragment.h>
#ifdef RN_SERIALIZABLE_STATE
#include <react/renderer/core/DynamicPropsUtilities.h>
#endif

#include <algorithm>
#include <cmath>
#include <climits>
#include <unordered_set>

namespace facebook::react {

namespace {

constexpr std::string_view ELEMENT_MARKER = "shadowlist:";
constexpr std::string_view TEMPLATE_MARKER = "shadowlist-template:";
constexpr const char* TEMPLATES_CONTAINER_TYPE = "native";

/*
 * Tags for the nodes we build. Start far above React's tags and stay even, since Android
 * sends odd tags to the old renderer.
 */
constexpr Tag FIRST_NATIVE_TAG = 1 << 30;
std::atomic<Tag> nextNativeTag{FIRST_NATIVE_TAG};

Tag allocateNativeTag() {
  Tag tag = nextNativeTag.fetch_add(2, std::memory_order_relaxed);
  if (tag > INT_MAX - 4) {
    nextNativeTag.store(FIRST_NATIVE_TAG, std::memory_order_relaxed);
    tag = nextNativeTag.fetch_add(2, std::memory_order_relaxed);
  }
  return tag;
}

bool startsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

const folly::dynamic& lookupPath(const folly::dynamic& item, const std::vector<std::string>& path) {
  static const folly::dynamic nullValue = nullptr;
  const folly::dynamic* current = &item;
  for (const auto& segment : path) {
    if (current->isObject()) {
      auto found = current->find(segment);
      if (found == current->items().end()) {
        return nullValue;
      }
      current = &found->second;
    } else if (current->isArray()) {
      char* end = nullptr;
      long index = std::strtol(segment.c_str(), &end, 10);
      if (end == segment.c_str() || *end != '\0' || index < 0 || static_cast<std::size_t>(index) >= current->size()) {
        return nullValue;
      }
      current = &(*current)[static_cast<std::size_t>(index)];
    } else {
      return nullValue;
    }
  }
  return *current;
}

bool truthy(const folly::dynamic& value) {
  switch (value.type()) {
    case folly::dynamic::NULLT:
      return false;
    case folly::dynamic::BOOL:
      return value.getBool();
    case folly::dynamic::INT64:
      return value.getInt() != 0;
    case folly::dynamic::DOUBLE:
      return value.getDouble() != 0.0;
    case folly::dynamic::STRING:
      return !value.getString().empty();
    case folly::dynamic::ARRAY:
      return !value.empty();
    default:
      return true;
  }
}

std::string toText(const folly::dynamic& value) {
  switch (value.type()) {
    case folly::dynamic::NULLT:
    case folly::dynamic::OBJECT:
    case folly::dynamic::ARRAY:
      return {};
    case folly::dynamic::STRING:
      return value.getString();
    default:
      return value.asString();
  }
}

/*
 * Reads the value at the part's path and adds its offset when it is a number.
 * A whole number result stays an int.
 */
folly::dynamic partValue(const ShadowListNativeFormatPart& part, const folly::dynamic& item) {
  const auto& value = lookupPath(item, part.path);
  if (part.offset == 0 || !value.isNumber()) {
    return value;
  }
  double sum = value.asDouble() + part.offset;
  double whole = std::floor(sum);
  if (whole == sum && std::abs(sum) < 9e15) {
    return static_cast<std::int64_t>(whole);
  }
  return sum;
}

folly::dynamic evaluate(const ShadowListNativeExpression& expression, const folly::dynamic& item) {
  if (expression.format) {
    std::string text;
    for (const auto& part : expression.parts) {
      text += part.path.empty() ? part.text : toText(partValue(part, item));
    }
    return text;
  }
  if (expression.parts.empty()) {
    return nullptr;
  }
  auto value = partValue(expression.parts.front(), item);
  if (expression.negate) {
    return !truthy(value);
  }
  return value;
}

/*
 * Writes one bound value into the element's props patch. A missing value or a bad color is
 * skipped, so the element keeps its template value, even when a reused row loses the value.
 */
void applyBinding(
  folly::dynamic& patch,
  const std::string& prop,
  const folly::dynamic& value,
  [[maybe_unused]] const Props& base) {
  if (shadowListNativeBindingKeepsTemplate(
        prop,
        value.isNull(),
        value.isString() ? std::optional<std::string_view>(value.getString()) : std::nullopt)) {
#ifdef RN_SERIALIZABLE_STATE
    // On Android a missing key keeps the view's old value, so reset it when the template has none.
    const auto* key = prop == "uri" ? "source" : prop.c_str();
    if (!base.rawProps.isObject() || base.rawProps.find(key) == base.rawProps.items().end()) {
      patch[key] = nullptr;
    }
#endif
    return;
  }
  if (prop == "uri" || prop == "source") {
    if (value.isString()) {
      patch["source"] = folly::dynamic::array(folly::dynamic::object("uri", value));
    } else if (value.isObject()) {
      patch["source"] = folly::dynamic::array(value);
    } else if (value.isArray()) {
      patch["source"] = value;
    } else {
      patch["source"] = folly::dynamic::array();
    }
    return;
  }
  if (prop == "hidden") {
    patch["display"] = truthy(value) ? "none" : "flex";
    return;
  }
  if (prop == "visible") {
    patch["display"] = truthy(value) ? "flex" : "none";
    return;
  }
  if (isShadowListNativeColorProp(prop) && value.isString()) {
    auto color = parseShadowListNativeColor(value.getString());
#ifdef __ANDROID__
    /*
     * Java reads colors as a signed int, so pass it signed like processColor does.
     * An unsigned value that big would turn into translucent white.
     */
    auto number = static_cast<std::int64_t>(static_cast<std::int32_t>(color.value_or(0)));
#else
    auto number = static_cast<std::int64_t>(color.value_or(0));
#endif
    patch[prop] = number;
    return;
  }
  patch[prop] = value;
}

bool hasLiveEventTarget(const ShadowNode& node) {
  const auto& eventEmitter = node.getEventEmitter();
  return eventEmitter == nullptr || eventEmitter->getEventTarget() != nullptr;
}

Props::Shared cloneWithPatch(
  const ShadowNode& prototype,
  const Props::Shared& base,
  folly::dynamic patch,
  const PropsParserContext& context) {
#ifdef RN_SERIALIZABLE_STATE
  /*
   * Android builds the view from raw props, so a bare patch would be all it sees.
   * Merge the patch over the base raw props. See the Android part of SHADOWLIST_NATIVE.md.
   */
  patch = mergeDynamicProps(base->rawProps, patch, NullValueStrategy::Override);
#endif
  return prototype.getComponentDescriptor().cloneProps(context, base, RawProps(std::move(patch)));
}

}

ShadowListNativeEngine::ShadowListNativeEngine(std::string listId) :
  listId_(std::move(listId)),
  keys_(std::make_shared<const std::vector<std::string>>()) {}

#pragma mark - Data

void ShadowListNativeEngine::rebuildIndexLocked() {
  keyIndex_.clear();
  keyIndex_.reserve(rows_.size());
  for (std::size_t index = 0; index < rows_.size(); ++index) {
    keyIndex_.emplace(rows_[index].key, index);
  }
}

void ShadowListNativeEngine::structureChangedLocked() {
  rebuildIndexLocked();
  auto keys = std::make_shared<std::vector<std::string>>();
  keys->reserve(rows_.size());
  for (const auto& row : rows_) {
    keys->push_back(row.key);
  }
  keys_ = std::move(keys);
  ++keysVersion_;

  // Drop nodes for rows whose key left the list. They won't come back as the same nodes.
  for (auto iterator = rowNodes_.begin(); iterator != rowNodes_.end();) {
    if (keyIndex_.find(iterator->first) == keyIndex_.end()) {
      if (iterator->second.node) {
        forgetTagsLocked(*iterator->second.node, iterator->first);
      }
      iterator = rowNodes_.erase(iterator);
    } else {
      ++iterator;
    }
  }
}

std::size_t ShadowListNativeEngine::setData(
  const folly::dynamic& items,
  const std::vector<std::string>& keys,
  const std::vector<std::string>& templates,
  bool scrollToStart) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (indexed_) {
      indexed_ = false;
      indexedCount_ = 0;
      indexedOrder_.clear();
      indexedIds_.clear();
      idIndex_.clear();
      indexedExtras_.clear();
    }
    std::unordered_map<std::string, Row> previous;
    previous.reserve(rows_.size());
    for (auto& row : rows_) {
      previous.emplace(row.key, std::move(row));
    }

    std::vector<Row> next;
    std::size_t count = items.isArray() ? std::min(items.size(), keys.size()) : 0;
    next.reserve(count);
    std::unordered_set<std::string> seen;
    seen.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      const auto& key = keys[index];
      if (!seen.insert(key).second) {
        continue;
      }
      const std::string& templateName = index < templates.size() ? templates[index] : std::string{};
      auto found = previous.find(key);
      if (found != previous.end() && found->second.templateName == templateName && found->second.item == items[index]) {
        next.push_back(std::move(found->second));
        continue;
      }
      next.push_back(Row{key, items[index], templateName, nextRowVersion_++});
    }

    bool sameKeys = next.size() == rows_.size() &&
      std::equal(next.begin(), next.end(), keys_->begin(), [](const Row& row, const std::string& key) { return row.key == key; });
    rows_ = std::move(next);
    if (sameKeys) {
      rebuildIndexLocked();
    } else {
      structureChangedLocked();
    }
    if (scrollToStart) {
      pendingScroll_ = PendingScroll{SCROLL_TO_START, 0.0, storeVersion_, keysVersion_};
    }
  }
  requestCommit();
  return size();
}

std::size_t ShadowListNativeEngine::setIndexed(
  std::size_t count,
  std::vector<std::int32_t> order,
  const std::string& indexField,
  const std::string& valueField,
  const std::vector<std::size_t>& extraIndices,
  const folly::dynamic& extraItems,
  const std::vector<std::string>& extraTemplates,
  bool scrollToStart,
  std::vector<std::int32_t> ids) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    bool wasIndexed = indexed_;
    if (!ids.empty() && ids.size() != count) {
      ids.clear();
    }
    if (!wasIndexed) {
      rows_.clear();
      keyIndex_.clear();
      indexed_ = true;
    }
    if (!order.empty() && order.size() != count) {
      order.clear();
    }
    bool idsChanged = !wasIndexed || ids != indexedIds_;
    bool orderChanged = idsChanged || indexField != indexField_ || valueField != valueField_ ||
      order != indexedOrder_;
    indexField_ = indexField;
    valueField_ = valueField;
    indexedOrder_ = std::move(order);
    if (orderChanged) {
      indexedEpoch_ = nextRowVersion_++;
    }

    // Extra data that didn't change, with the same order, keeps its version so the row isn't rebound.
    std::unordered_map<std::size_t, IndexedExtra> extras;
    std::size_t extraCount = extraItems.isArray() ? std::min(extraItems.size(), extraIndices.size()) : 0;
    extras.reserve(extraCount);
    for (std::size_t entry = 0; entry < extraCount; ++entry) {
      std::size_t index = extraIndices[entry];
      if (index >= count) {
        continue;
      }
      IndexedExtra extra;
      extra.item = extraItems[entry].isObject() ? extraItems[entry] : folly::dynamic::object();
      extra.templateName = entry < extraTemplates.size() ? extraTemplates[entry] : std::string{};
      auto previous = indexedExtras_.find(index);
      extra.version = !orderChanged && previous != indexedExtras_.end() && previous->second.item == extra.item &&
          previous->second.templateName == extra.templateName
        ? previous->second.version
        : nextRowVersion_++;
      extras[index] = std::move(extra);
    }
    // A row that lost its extra data gets a new shared version so it doesn't match its old one.
    if (!orderChanged) {
      for (const auto& [index, previous] : indexedExtras_) {
        if (extras.find(index) == extras.end()) {
          indexedEpoch_ = std::max(indexedEpoch_, nextRowVersion_++);
          break;
        }
      }
    }
    indexedExtras_ = std::move(extras);

    bool positional = ids.empty();
    bool wasPositional = indexedIds_.empty();
    if (!wasIndexed || idsChanged || count != indexedCount_) {
      auto keys = std::make_shared<std::vector<std::string>>();
      keys->reserve(count);
      if (positional) {
        // Position keys only change at the end, so keep the ones still valid.
        std::size_t reuse = wasIndexed && wasPositional ? std::min(count, keys_->size()) : 0;
        keys->insert(keys->end(), keys_->begin(), keys_->begin() + static_cast<std::ptrdiff_t>(reuse));
        for (std::size_t index = reuse; index < count; ++index) {
          keys->push_back(std::to_string(index));
        }
        idIndex_.clear();
      } else {
        idIndex_.clear();
        idIndex_.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
          // A repeated id keeps its first row. Later ones get a key the core won't find.
          if (idIndex_.emplace(ids[index], index).second) {
            keys->push_back(std::to_string(ids[index]));
          } else {
            keys->push_back("#dup" + std::to_string(index));
          }
        }
      }
      indexedIds_ = std::move(ids);
      keys_ = std::move(keys);
      indexedCount_ = count;
      ++keysVersion_;
      for (auto iterator = rowNodes_.begin(); iterator != rowNodes_.end();) {
        auto index = indexOfKeyLocked(iterator->first);
        if (!index) {
          if (iterator->second.node) {
            forgetTagsLocked(*iterator->second.node, iterator->first);
          }
          iterator = rowNodes_.erase(iterator);
        } else {
          ++iterator;
        }
      }
    }
    if (scrollToStart) {
      pendingScroll_ = PendingScroll{SCROLL_TO_START, 0.0, storeVersion_, keysVersion_};
    }
  }
  requestCommit();
  return size();
}

std::optional<std::size_t> ShadowListNativeEngine::indexOfKeyLocked(const std::string& key) const {
  if (!indexed_) {
    auto found = keyIndex_.find(key);
    return found == keyIndex_.end() ? std::nullopt : std::optional<std::size_t>(found->second);
  }
  if (!indexedIds_.empty()) {
    char* end = nullptr;
    long id = std::strtol(key.c_str(), &end, 10);
    if (key.empty() || *end != '\0' || id < INT32_MIN || id > INT32_MAX) {
      return std::nullopt;
    }
    auto found = idIndex_.find(static_cast<std::int32_t>(id));
    return found == idIndex_.end() ? std::nullopt : std::optional<std::size_t>(found->second);
  }
  if (key.empty() || key.size() > 18) {
    return std::nullopt;
  }
  std::size_t index = 0;
  for (char character : key) {
    if (character < '0' || character > '9') {
      return std::nullopt;
    }
    index = index * 10 + static_cast<std::size_t>(character - '0');
  }
  // A key with a leading zero is not a row.
  if (key.size() > 1 && key.front() == '0') {
    return std::nullopt;
  }
  return index < indexedCount_ ? std::optional<std::size_t>(index) : std::nullopt;
}

folly::dynamic ShadowListNativeEngine::indexedItemLocked(std::size_t index) const {
  auto extra = indexedExtras_.find(index);
  folly::dynamic item = extra != indexedExtras_.end() ? extra->second.item : folly::dynamic::object();
  if (!indexField_.empty()) {
    item[indexField_] = static_cast<std::int64_t>(index);
  }
  if (!valueField_.empty()) {
    item[valueField_] = indexedOrder_.empty() ? static_cast<std::int64_t>(index)
                                              : static_cast<std::int64_t>(indexedOrder_[index]);
  }
  return item;
}

std::size_t ShadowListNativeEngine::insertItems(
  std::size_t index,
  const folly::dynamic& items,
  const std::vector<std::string>& keys,
  const std::vector<std::string>& templates) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (indexed_) {
      return indexedCount_;
    }
    std::size_t count = items.isArray() ? std::min(items.size(), keys.size()) : 0;
    std::vector<Row> inserted;
    inserted.reserve(count);
    std::unordered_set<std::string> seen;
    for (std::size_t itemIndex = 0; itemIndex < count; ++itemIndex) {
      const auto& key = keys[itemIndex];
      if (keyIndex_.find(key) != keyIndex_.end() || !seen.insert(key).second) {
        continue;
      }
      const std::string& templateName = itemIndex < templates.size() ? templates[itemIndex] : std::string{};
      inserted.push_back(Row{key, items[itemIndex], templateName, nextRowVersion_++});
    }
    if (inserted.empty()) {
      return rows_.size();
    }
    std::size_t at = std::min(index, rows_.size());
    rows_.insert(
      rows_.begin() + static_cast<std::ptrdiff_t>(at),
      std::make_move_iterator(inserted.begin()),
      std::make_move_iterator(inserted.end()));
    structureChangedLocked();
  }
  requestCommit();
  return size();
}

bool ShadowListNativeEngine::updateItem(
  const std::string& key,
  const folly::dynamic& patch,
  const std::string& templateName,
  bool replace) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (indexed_) {
      auto index = indexOfKeyLocked(key);
      if (!index) {
        return false;
      }
      auto& extra = indexedExtras_[*index];
      if (replace) {
        extra.item = patch.isObject() ? patch : folly::dynamic::object();
      } else if (patch.isObject()) {
        for (const auto& [field, value] : patch.items()) {
          if (value.isNull()) {
            extra.item.erase(field);
          } else {
            extra.item[field] = value;
          }
        }
      }
      if (!templateName.empty()) {
        extra.templateName = templateName;
      }
      extra.version = nextRowVersion_++;
      if (extra.item.empty() && extra.templateName.empty()) {
        // Back to a plain row. The shared version is older, so it still rebinds.
        indexedExtras_.erase(*index);
      }
    } else {
    auto found = keyIndex_.find(key);
    if (found == keyIndex_.end()) {
      return false;
    }
    Row& row = rows_[found->second];
    if (replace || !row.item.isObject() || !patch.isObject()) {
      row.item = patch;
    } else {
      for (const auto& [field, value] : patch.items()) {
        row.item[field] = value;
      }
    }
    if (!templateName.empty()) {
      row.templateName = templateName;
    }
    row.version = nextRowVersion_++;
    }
  }
  requestCommit();
  return true;
}

std::size_t ShadowListNativeEngine::removeItems(const std::vector<std::string>& keys) {
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (indexed_) {
      return indexedCount_;
    }
    std::unordered_set<std::string> doomed(keys.begin(), keys.end());
    auto end = std::remove_if(rows_.begin(), rows_.end(), [&](const Row& row) { return doomed.count(row.key) > 0; });
    if (end != rows_.end()) {
      rows_.erase(end, rows_.end());
      structureChangedLocked();
      changed = true;
    }
  }
  if (changed) {
    requestCommit();
  }
  return size();
}

bool ShadowListNativeEngine::moveItem(const std::string& key, std::size_t toIndex) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (indexed_) {
      return false;
    }
    auto found = keyIndex_.find(key);
    if (found == keyIndex_.end()) {
      return false;
    }
    std::size_t from = found->second;
    std::size_t to = std::min(toIndex, rows_.size() - 1);
    if (from == to) {
      return true;
    }
    Row row = std::move(rows_[from]);
    rows_.erase(rows_.begin() + static_cast<std::ptrdiff_t>(from));
    rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(to), std::move(row));
    structureChangedLocked();
  }
  requestCommit();
  return true;
}

void ShadowListNativeEngine::setTemplateStyle(
  const std::string& templateName,
  const std::string& elementId,
  const folly::dynamic& style) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& styles = templateStyles_[templateName];
    if (style.isObject() && !style.empty()) {
      styles[elementId] = style;
    } else {
      styles.erase(elementId);
    }
    auto found = templates_.find(templateName);
    if (found != templates_.end()) {
      found->second.version = nextTemplateVersion_++;
      found->second.basePropsStale = true;
    }
  }
  requestCommit();
}

void ShadowListNativeEngine::configure(const folly::dynamic& config) {
  if (!config.isObject()) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  auto readCount = [&](const char* name, std::size_t& target) {
    auto found = config.find(name);
    // Capped so a huge value can't overflow the row window math or the cast.
    if (found != config.items().end() && found->second.isNumber() && found->second.asDouble() >= 0) {
      target = static_cast<std::size_t>(std::min(found->second.asDouble(), 1e6));
    }
  };
  readCount("initialRows", initialRows_);
  readCount("padRows", padRows_);
  readCount("cacheRows", cacheRows_);
  if (initialRows_ == 0) {
    initialRows_ = 1;
  }
  ++configVersion_;
}

folly::dynamic ShadowListNativeEngine::getItem(const std::string& key) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto index = indexOfKeyLocked(key);
  if (!index) {
    return nullptr;
  }
  return indexed_ ? indexedItemLocked(*index) : rows_[*index].item;
}

std::vector<std::string> ShadowListNativeEngine::getKeys() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return *keys_;
}

std::size_t ShadowListNativeEngine::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return indexed_ ? indexedCount_ : rows_.size();
}

std::optional<ShadowListNativeEngine::ResolvedTag> ShadowListNativeEngine::resolveTag(Tag tag) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = tagKeys_.find(tag);
  if (found == tagKeys_.end()) {
    return std::nullopt;
  }
  auto index = indexOfKeyLocked(found->second.key);
  if (!index) {
    return std::nullopt;
  }
  return ResolvedTag{found->second.key, *index, found->second.repeatIndex};
}

void ShadowListNativeEngine::requestCommit() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ++storeVersion_;
  }
  nudge();
}

void ShadowListNativeEngine::requestScroll(double index, double viewPosition) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingScroll_ = PendingScroll{index, viewPosition, storeVersion_};
  }
  nudge();
}

bool ShadowListNativeEngine::applyPendingScroll(azimgd::shadowlist::Container& core, std::uint64_t keysVersion) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!pendingScroll_) {
    return false;
  }
  bool due = pendingScroll_->withKeysVersion != 0
    ? keysVersion >= pendingScroll_->withKeysVersion
    : laidOutVersion_ >= pendingScroll_->afterVersion;
  if (!due) {
    return false;
  }
  SL_LOG("native: scroll index=%.0f after=%llu", pendingScroll_->index, static_cast<unsigned long long>(pendingScroll_->afterVersion));
  if (pendingScroll_->index <= SCROLL_TO_START) {
    core.scrollToStart();
  } else if (pendingScroll_->index < 0.0) {
    core.scrollToEnd();
  } else {
    core.scrollToIndex(
      static_cast<std::size_t>(pendingScroll_->index), std::min(1.0, std::max(0.0, pendingScroll_->viewPosition)));
  }
  pendingScroll_.reset();
  return true;
}

void ShadowListNativeEngine::setMomentumYieldToken(std::uint64_t token) {
  std::lock_guard<std::mutex> lock(mutex_);
  momentumYieldToken_ = token;
}

std::uint64_t ShadowListNativeEngine::momentumYieldToken() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return momentumYieldToken_;
}

void ShadowListNativeEngine::nudge() {
  std::shared_ptr<const ListState> state;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    state = state_;
  }
  if (!state) {
    return;
  }
  /*
   * Copy the latest committed state, but don't apply its scroll correction again.
   * The host already applied it when it mounted.
   */
  state->updateState([](const ShadowListViewState& previous) -> StateData::Shared {
    auto next = std::make_shared<ShadowListViewState>(previous);
    next->containerOffsetEnabled_ = false;
    return next;
  });
}

#pragma mark - Commit side

ShadowListNativeEngine::KeysSnapshot ShadowListNativeEngine::keysSnapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return {keys_, keysVersion_};
}

void ShadowListNativeEngine::attachState(const std::shared_ptr<const ListState>& state) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_ = state;
}

void ShadowListNativeEngine::compileElement(
  Element& element,
  const std::shared_ptr<const ShadowNode>& node,
  std::vector<const void*>& signature,
  std::string& shape) {
  element.prototype = node;
  const auto& props = node->getProps();
#ifdef RN_SERIALIZABLE_STATE
  {
    auto previous = prototypeRawProps_.find(node->getTag());
    PrototypeRawProps accumulated;
    accumulated.props = props.get();
    if (previous == prototypeRawProps_.end()) {
      accumulated.raw = props->rawProps;
    } else if (previous->second.props == props.get()) {
      accumulated.raw = previous->second.raw;
    } else {
      accumulated.raw = mergeDynamicProps(previous->second.raw, props->rawProps, NullValueStrategy::Override);
    }
    nextPrototypeRawProps_.insert_or_assign(node->getTag(), std::move(accumulated));
  }
#endif
  signature.push_back(props.get());
  signature.push_back(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(node->getChildren().size())));
  shape += node->getComponentName();
  /*
   * Include the instance handle. Rows share the template's handle for events, so when React
   * replaces the template element, rows must be rebuilt or their touches get lost.
   */
  shape += '@';
  shape += std::to_string(reinterpret_cast<std::uintptr_t>(node->getFamily().getInstanceHandle().get()));
  shape += '(';

  element.isRawText = std::string_view(node->getComponentName()) == "RawText";
  const auto& nativeId = props->nativeId;
  if (startsWith(nativeId, ELEMENT_MARKER)) {
    try {
      auto spec = folly::parseJson(std::string_view(nativeId).substr(ELEMENT_MARKER.size()));
      if (auto id = spec.find("i"); id != spec.items().end() && id->second.isString()) {
        element.elementId = id->second.getString();
      }
      if (auto action = spec.find("a"); action != spec.items().end()) {
        element.keepNativeId = truthy(action->second);
      }
      if (auto repeat = spec.find("r"); repeat != spec.items().end() && repeat->second.isString()) {
        element.repeat = parseShadowListNativePath(repeat->second.getString());
        auto repeatMax = spec.find("m");
        if (repeatMax != spec.items().end() && repeatMax->second.isNumber() && repeatMax->second.asDouble() >= 0) {
          element.repeatMax = static_cast<std::size_t>(std::min(repeatMax->second.asDouble(), 1e9));
        }
      }
      if (auto bindings = spec.find("b"); bindings != spec.items().end() && bindings->second.isObject()) {
        for (const auto& [prop, source] : bindings->second.items()) {
          if (!prop.isString() || !source.isString()) {
            continue;
          }
          auto expression = parseShadowListNativeExpression(source.getString());
          if (prop.getString() == "text") {
            element.text = std::move(expression);
          } else {
            element.bindings.emplace_back(prop.getString(), std::move(expression));
          }
        }
      }
    } catch (...) {
      // A bad marker leaves the element without bindings.
    }
  }

  element.children.resize(node->getChildren().size());
  for (std::size_t index = 0; index < node->getChildren().size(); ++index) {
    compileElement(element.children[index], node->getChildren()[index], signature, shape);
  }
  shape += ')';
}

void ShadowListNativeEngine::compileTemplatesLocked(const ShadowNode& container, const PropsParserContext& context) {
  if (&container == templatesContainer_) {
    return;
  }

#ifdef RN_SERIALIZABLE_STATE
  nextPrototypeRawProps_.clear();
#endif
  std::unordered_set<std::string> seen;
  std::string firstName;
  for (const auto& templateRoot : container.getChildren()) {
    const auto& nativeId = templateRoot->getProps()->nativeId;
    if (!startsWith(nativeId, TEMPLATE_MARKER)) {
      continue;
    }
    std::string name = nativeId.substr(TEMPLATE_MARKER.size());
    if (firstName.empty()) {
      firstName = name;
    }
    seen.insert(name);

    Template compiled;
    compiled.name = name;
    compileElement(compiled.root, templateRoot, compiled.signature, compiled.shape);

    auto existing = templates_.find(name);
    if (existing != templates_.end() && existing->second.signature == compiled.signature) {
      // Nothing changed but layout, so there is nothing to recompile.
      existing->second.root.prototype = templateRoot;
      continue;
    }
    compiled.version = nextTemplateVersion_++;
    compiled.shapeVersion = existing != templates_.end() && existing->second.shape == compiled.shape
      ? existing->second.shapeVersion
      : nextTemplateVersion_++;
    compiled.basePropsStale = true;
    templates_.insert_or_assign(name, std::move(compiled));
  }

  for (auto iterator = templates_.begin(); iterator != templates_.end();) {
    iterator = seen.count(iterator->first) > 0 ? std::next(iterator) : templates_.erase(iterator);
  }
  defaultTemplate_ = templates_.count("default") > 0 ? "default" : firstName;
  templatesContainer_ = &container;
#ifdef RN_SERIALIZABLE_STATE
  prototypeRawProps_.swap(nextPrototypeRawProps_);
#endif
  (void)context;
}

void ShadowListNativeEngine::refreshElementBaseProps(
  Element& element,
  const std::string& templateName,
  const PropsParserContext& context) {
  const auto& prototypeProps = element.prototype->getProps();
  folly::dynamic patch = folly::dynamic::object;
  if (!prototypeProps->nativeId.empty() && !element.keepNativeId) {
    patch["nativeID"] = nullptr;
  }
  if (!element.elementId.empty()) {
    auto styles = templateStyles_.find(templateName);
    if (styles != templateStyles_.end()) {
      auto style = styles->second.find(element.elementId);
      if (style != styles->second.end()) {
        patch.update(style->second);
      }
    }
  }
#ifdef RN_SERIALIZABLE_STATE
  // Rows are new views on Android, so use the template's full raw props, not just the last change.
  if (auto accumulated = prototypeRawProps_.find(element.prototype->getTag());
      accumulated != prototypeRawProps_.end() && accumulated->second.props == prototypeProps.get() &&
      accumulated->second.raw != prototypeProps->rawProps) {
    patch = mergeDynamicProps(accumulated->second.raw, patch, NullValueStrategy::Override);
  }
#endif
  element.baseProps = patch.empty() ? prototypeProps : cloneWithPatch(*element.prototype, prototypeProps, std::move(patch), context);
  for (auto& child : element.children) {
    refreshElementBaseProps(child, templateName, context);
  }
}

void ShadowListNativeEngine::refreshBasePropsLocked(Template& compiled, const PropsParserContext& context) {
  refreshElementBaseProps(compiled.root, compiled.name, context);
  compiled.basePropsStale = false;
}

ShadowListNativeEngine::Template* ShadowListNativeEngine::templateForLocked(const std::string& name) {
  auto found = templates_.find(name.empty() ? defaultTemplate_ : name);
  if (found == templates_.end() && !name.empty()) {
    found = templates_.find(defaultTemplate_);
  }
  return found == templates_.end() ? nullptr : &found->second;
}

std::shared_ptr<const ShadowNode> ShadowListNativeEngine::buildNode(
  const Element& element,
  const folly::dynamic& item,
  const std::string& key,
  const std::shared_ptr<const ShadowNode>& existing,
  const Props::Shared* propsOverride,
  const std::optional<std::string>& rawText,
  int repeatIndex,
  const PropsParserContext& context) {
  Props::Shared props;
  if (propsOverride != nullptr) {
    props = *propsOverride;
  } else if (rawText) {
    props = cloneWithPatch(*element.prototype, element.baseProps, folly::dynamic::object("text", *rawText), context);
  } else if (element.bindings.empty()) {
    props = element.baseProps;
  } else {
    folly::dynamic patch = folly::dynamic::object;
    for (const auto& [prop, expression] : element.bindings) {
      applyBinding(patch, prop, evaluate(expression, item), *element.baseProps);
    }
    props = cloneWithPatch(*element.prototype, element.baseProps, std::move(patch), context);
  }

  /*
   * Reuse the existing nodes only while the structure still matches. A repeated element's
   * child count follows its array, so only the component must match and children pair up by position.
   */
  bool reuse = existing != nullptr && existing->getComponentHandle() == element.prototype->getComponentHandle() &&
    (element.repeat || existing->getChildren().size() == element.children.size());

  std::optional<std::string> text;
  if (element.text) {
    text = toText(evaluate(*element.text, item));
  }

  ChildList children;
  bool sameChildren = reuse;
  auto existingChild = [&](std::size_t position) -> std::shared_ptr<const ShadowNode> {
    return reuse && position < existing->getChildren().size() ? existing->getChildren()[position] : nullptr;
  };
  if (element.repeat) {
    const auto& entries = lookupPath(item, *element.repeat);
    std::size_t entryCount = entries.isArray() ? std::min(entries.size(), element.repeatMax) : 0;
    children.reserve(entryCount * element.children.size());
    for (std::size_t entry = 0; entry < entryCount; ++entry) {
      for (const auto& childElement : element.children) {
        auto previous = existingChild(children.size());
        auto child = buildNode(
          childElement, entries[entry], key, previous, nullptr, std::nullopt, static_cast<int>(entry), context);
        if (child != previous) {
          sameChildren = false;
        }
        children.push_back(std::move(child));
      }
    }
    if (reuse && existing->getChildren().size() != children.size()) {
      sameChildren = false;
      // Entries that went away take their tags with them.
      for (std::size_t position = children.size(); position < existing->getChildren().size(); ++position) {
        forgetTagsLocked(*existing->getChildren()[position], key);
      }
    }
  } else {
    children.reserve(element.children.size());
    for (std::size_t index = 0; index < element.children.size(); ++index) {
      const auto& childElement = element.children[index];
      std::optional<std::string> childText;
      if (text && childElement.isRawText) {
        childText = std::move(text);
        text.reset();
      }
      auto previous = existingChild(index);
      auto child = buildNode(childElement, item, key, previous, nullptr, childText, repeatIndex, context);
      if (reuse && child != previous) {
        sameChildren = false;
      }
      children.push_back(std::move(child));
    }
  }

  if (reuse) {
    if (sameChildren && props == existing->getProps()) {
      return existing;
    }
    auto childList = std::make_shared<const ChildList>(std::move(children));
    return existing->clone({.props = props, .children = childList});
  }

  /*
   * Every built node needs its own tag. It borrows the template's instance handle, since a node
   * without one crashes on its first event, and this is how template handlers get row touches.
   */
  const auto& descriptor = element.prototype->getComponentDescriptor();
  Tag tag = allocateNativeTag();
  auto family = descriptor.createFamily(
    {tag, element.prototype->getSurfaceId(), element.prototype->getFamily().getInstanceHandle()});
  auto state = descriptor.createInitialState(props, family);
  auto childList = std::make_shared<const ChildList>(std::move(children));
  auto node = descriptor.createShadowNode({.props = props, .children = childList, .state = state}, family);
  tagKeys_[tag] = TagEntry{key, repeatIndex};
  return node;
}

std::shared_ptr<const ShadowNode> ShadowListNativeEngine::buildRowLocked(
  Template& compiled,
  const Row& row,
  const std::shared_ptr<const ShadowNode>& existing,
  bool rootPropsCurrent,
  const PropsParserContext& context,
  bool inFlow) {
  folly::dynamic rootPatch = folly::dynamic::object("elementKey", row.key)("index", 0);
  if (inFlow) {
    // In the overlay the row sizes its container instead of being placed by the list.
    rootPatch["position"] = "relative";
  }
  Props::Shared rootProps = rootPropsCurrent && existing
    ? existing->getProps()
    : cloneWithPatch(*compiled.root.prototype, compiled.root.baseProps, std::move(rootPatch), context);
  return buildNode(compiled.root, row.item, row.key, existing, &rootProps, std::nullopt, -1, context);
}

std::size_t ShadowListNativeEngine::activeStickyIndexLocked(
  const std::vector<std::string>& keys,
  const azimgd::shadowlist::Container& core,
  bool inverted) const {
  // Inverted lists have no sticky headers.
  if (inverted || core.stickyIndices.empty()) {
    return SIZE_MAX;
  }
  double offset = std::max(0.0, core.getContainerOffset());
  std::size_t elements = std::min(core.getElementsSize(), keys.size());
  std::size_t active = SIZE_MAX;
  double activeOffset = -1.0;
  for (std::size_t index : core.stickyIndices) {
    if (index >= elements) {
      continue;
    }
    double elementOffset = core.getElementOffset(index);
    if (elementOffset <= offset && elementOffset >= activeOffset) {
      active = index;
      activeOffset = elementOffset;
    }
  }
  return active;
}

void ShadowListNativeEngine::dropStickyLocked() {
  if (stickyNode_.node) {
    forgetTagsLocked(*stickyNode_.node, stickyKey_);
  }
  stickyNode_ = RowNode{};
  stickyKey_.clear();
}

std::shared_ptr<const ShadowNode> ShadowListNativeEngine::stickyRowLocked(
  const std::vector<std::string>& keys,
  azimgd::shadowlist::Container& core,
  bool inverted,
  const PropsParserContext& context) {
  // A kept copy that was unmounted has lost its event targets for good, so rebuild it.
  if (stickyNode_.node && !hasLiveEventTarget(*stickyNode_.node)) {
    dropStickyLocked();
  }
  std::size_t active = activeStickyIndexLocked(keys, core, inverted);
  if (active == SIZE_MAX) {
    dropStickyLocked();
    return nullptr;
  }

  const std::string& key = keys[active];
  Row row;
  if (indexed_) {
    auto position = indexOfKeyLocked(key);
    if (!position) {
      dropStickyLocked();
      return nullptr;
    }
    auto extra = indexedExtras_.find(*position);
    row.key = key;
    row.templateName = extra != indexedExtras_.end() ? extra->second.templateName : std::string{};
    row.version = extra != indexedExtras_.end() ? extra->second.version : indexedEpoch_;
    row.item = indexedItemLocked(*position);
  } else {
    auto found = keyIndex_.find(key);
    if (found == keyIndex_.end()) {
      dropStickyLocked();
      return nullptr;
    }
    row = rows_[found->second];
  }
  Template* compiled = templateForLocked(row.templateName);
  if (compiled == nullptr) {
    dropStickyLocked();
    return nullptr;
  }

  // A different pinned row gets new nodes, so a press on it reaches the row it shows.
  bool sameShape = stickyNode_.node != nullptr && stickyKey_ == key && stickyNode_.templateName == compiled->name &&
    stickyNode_.shapeVersion == compiled->shapeVersion;
  if (sameShape && stickyNode_.rowVersion == row.version && stickyNode_.templateVersion == compiled->version) {
    return stickyNode_.node;
  }
  if (!sameShape) {
    dropStickyLocked();
  }
  auto node = buildRowLocked(
    *compiled,
    row,
    sameShape ? stickyNode_.node : nullptr,
    sameShape && stickyNode_.templateVersion == compiled->version,
    context,
    true);
  stickyNode_.node = node;
  stickyNode_.rowVersion = row.version;
  stickyNode_.templateVersion = compiled->version;
  stickyNode_.shapeVersion = compiled->shapeVersion;
  stickyNode_.templateName = compiled->name;
  stickyKey_ = key;
  return node;
}

void ShadowListNativeEngine::forgetTagsLocked(const ShadowNode& node, const std::string& key) {
  auto found = tagKeys_.find(node.getTag());
  if (found != tagKeys_.end() && found->second.key == key) {
    tagKeys_.erase(found);
  }
  for (const auto& child : node.getChildren()) {
    forgetTagsLocked(*child, key);
  }
}

void ShadowListNativeEngine::evictLocked(std::size_t keep) {
  // Idle rows are a part of all rows, so a cache under the cap has nothing to drop.
  if (rowNodes_.size() <= keep) {
    return;
  }
  std::vector<std::pair<std::uint64_t, const std::string*>> idle;
  for (const auto& [key, rowNode] : rowNodes_) {
    if (rowNode.usedAt != clock_) {
      idle.emplace_back(rowNode.usedAt, &key);
    }
  }
  if (idle.size() <= keep) {
    return;
  }
  // Only which rows are the newest matters, not their order, so partition instead of sorting.
  std::nth_element(
    idle.begin(),
    idle.begin() + static_cast<std::ptrdiff_t>(keep),
    idle.end(),
    [](const auto& left, const auto& right) { return left.first > right.first; });
  // Copy the keys out first, since erasing an entry frees the key a pointer refers to.
  std::vector<std::string> doomed;
  doomed.reserve(idle.size() - keep);
  for (std::size_t index = keep; index < idle.size(); ++index) {
    doomed.push_back(*idle[index].second);
  }
  for (const auto& doomedKey : doomed) {
    auto found = rowNodes_.find(doomedKey);
    if (found == rowNodes_.end()) {
      continue;
    }
    if (found->second.node) {
      forgetTagsLocked(*found->second.node, found->first);
    }
    rowNodes_.erase(found);
  }
}

bool ShadowListNativeEngine::reconcileUnchangedLocked(
  const ChildList& children,
  const std::shared_ptr<const std::vector<std::string>>& keys,
  const azimgd::shadowlist::Container& core,
  int initialIndex,
  bool inverted) const {
  const auto& cache = reconcileCache_;
  if (!cache.valid || cache.keys != keys || cache.storeVersion != storeVersion_ ||
      cache.configVersion != configVersion_ || cache.initialIndex != initialIndex || cache.inverted != inverted ||
      children.size() != cache.childTags.size()) {
    return false;
  }
  for (std::size_t index = 0; index < children.size(); ++index) {
    if (children[index]->getTag() != cache.childTags[index]) {
      return false;
    }
  }
  // Templates recompile when their container node changes, and restyled ones rebuild.
  bool templatesSame = false;
  for (const auto& child : children) {
    if (child.get() == cache.templatesContainer) {
      templatesSame = true;
      break;
    }
  }
  if (!templatesSame || templatesContainer_ != cache.templatesContainer) {
    return false;
  }
  for (const auto& [name, compiled] : templates_) {
    if (compiled.basePropsStale) {
      return false;
    }
  }

  /*
   * The full pass keeps the mounted rows while the core's window sits inside them, and
   * the rows mounted then are these same rows. Check the core indexes the keys the same way.
   */
  const auto& keyList = *keys;
  if (core.getWindowContainerSize() <= 0.0 || core.getElementsSize() != keyList.size() ||
      cache.targetHigh >= keyList.size() ||
      core.findElementIndexByKey(keyList[cache.targetLow]) != cache.targetLow ||
      core.findElementIndexByKey(keyList[cache.targetHigh]) != cache.targetHigh) {
    return false;
  }
  auto [start, end] = core.getVisibleIndices();
  if (start == azimgd::shadowlist::UNDEFINED_INDEX || end == azimgd::shadowlist::UNDEFINED_INDEX) {
    return false;
  }
  std::size_t count = keyList.size();
  std::size_t low = std::min(std::min(start, end), count - 1);
  std::size_t high = std::min(std::max(start, end), count - 1);
  if (cache.targetLow > low || cache.targetHigh < high) {
    return false;
  }

  // The section header overlay must still show the row the full pass would pin.
  if (cache.overlayChildIndex != SIZE_MAX) {
    if (activeStickyIndexLocked(keyList, core, inverted) != cache.stickyActive) {
      return false;
    }
    const auto& overlayChildren = children[cache.overlayChildIndex]->getChildren();
    if (stickyNode_.node) {
      if (overlayChildren.size() != 1 || overlayChildren.front() != stickyNode_.node ||
          !hasLiveEventTarget(*stickyNode_.node)) {
        return false;
      }
    } else if (!overlayChildren.empty()) {
      return false;
    }
  }
  return true;
}

std::shared_ptr<const ShadowListNativeEngine::ChildList> ShadowListNativeEngine::reconcileRows(
  const ShadowNode& listNode,
  const ChildList& children,
  const std::shared_ptr<const std::vector<std::string>>& keysSnapshot,
  azimgd::shadowlist::Container& core,
  int initialIndex,
  bool inverted) {
  const auto contextContainer = listNode.getContextContainer();
  if (!contextContainer || !keysSnapshot) {
    return nullptr;
  }
  PropsParserContext context{listNode.getSurfaceId(), *contextContainer};
  const std::vector<std::string>& keys = *keysSnapshot;

  std::lock_guard<std::mutex> lock(mutex_);
  reconciledVersion_ = storeVersion_;

  // Most commits, like every scroll frame, change nothing here. See ReconcileCache.
  if (reconcileUnchangedLocked(children, keysSnapshot, core, initialIndex, inverted)) {
    return nullptr;
  }
  reconcileCache_.valid = false;

  /*
   * Separate the list's own children, like the header and footer, from last time's rows.
   * Rows go back right after the templates so the header draws below them and the footer above.
   */
  ChildList others;
  others.reserve(children.size());
  std::unordered_map<std::string, std::shared_ptr<const ShadowNode>> current;
  std::size_t rowsAt = std::string::npos;
  for (const auto& child : children) {
    if (const auto elementProps = dynamic_cast<const ShadowListElementViewProps*>(child->getProps().get())) {
      current.emplace(elementProps->elementKey, child);
      continue;
    }
    others.push_back(child);
    if (const auto templateProps = dynamic_cast<const ShadowListTemplateViewProps*>(child->getProps().get())) {
      if (templateProps->templateType == TEMPLATES_CONTAINER_TYPE) {
        compileTemplatesLocked(*child, context);
        templatesContainerHold_ = child;
        rowsAt = others.size();
      }
    }
  }
  if (rowsAt == std::string::npos) {
    rowsAt = others.size();
  }
  for (auto& [name, compiled] : templates_) {
    if (compiled.basePropsStale) {
      refreshBasePropsLocked(compiled, context);
    }
  }

  /*
   * Pick which rows to mount. Use what the core measured, or start from the list's opening
   * edge while it has no size yet.
   */
  std::size_t count = keys.size();
  std::size_t targetLow = 0;
  std::size_t targetHigh = 0;
  bool haveTarget = false;
  bool measuredTarget = false;
  if (count > 0 && !templates_.empty()) {
    std::size_t low = 0;
    std::size_t high = 0;
    bool measured = false;
    if (core.getWindowContainerSize() > 0.0) {
      auto [start, end] = core.getVisibleIndices();
      if (start != azimgd::shadowlist::UNDEFINED_INDEX && end != azimgd::shadowlist::UNDEFINED_INDEX) {
        low = std::min(std::min(start, end), count - 1);
        high = std::min(std::max(start, end), count - 1);
        measured = true;
      }
    }
    if (!measured) {
      std::size_t seed = std::min(initialRows_, count);
      if (initialIndex >= 0 && static_cast<std::size_t>(initialIndex) < count) {
        low = static_cast<std::size_t>(initialIndex);
        high = std::min(count - 1, low + seed - 1);
      } else if (inverted) {
        high = count - 1;
        low = count - seed;
      } else {
        low = 0;
        high = seed - 1;
      }
    }

    /*
     * Keep the mounted rows while they still cover what's needed, so small scrolls build nothing.
     * Past either end, remount with some padding so the next few rows are free again.
     */
    std::size_t mountedLow = azimgd::shadowlist::UNDEFINED_INDEX;
    std::size_t mountedHigh = 0;
    std::size_t mountedCount = 0;
    for (const auto& [key, node] : current) {
      std::size_t index = core.findElementIndexByKey(key);
      if (index >= count) {
        continue;
      }
      mountedLow = std::min(mountedLow, index);
      mountedHigh = std::max(mountedHigh, index);
      ++mountedCount;
    }
    bool contiguous = mountedCount > 0 && mountedHigh - mountedLow + 1 == mountedCount;
    if (measured && contiguous && mountedLow <= low && mountedHigh >= high) {
      targetLow = mountedLow;
      targetHigh = mountedHigh;
    } else {
      targetLow = low > padRows_ ? low - padRows_ : 0;
      targetHigh = std::min(count - 1, high + padRows_);
    }
    haveTarget = true;
    measuredTarget = measured;
  }

  ChildList rows;
  ++clock_;
  std::size_t builtRows = 0;
  std::size_t reboundRows = 0;
  if (haveTarget) {
    rows.reserve(targetHigh - targetLow + 1);
    for (std::size_t index = targetLow; index <= targetHigh; ++index) {
      const auto& key = keys[index];
      /*
       * Indexed rows get their version and template from their extra data, and their item is
       * only built below when the row is built or rebound.
       */
      const Row* stored = nullptr;
      Row indexedRow;
      std::size_t rowPosition = 0;
      if (indexed_) {
        auto position = indexOfKeyLocked(key);
        if (!position) {
          continue;
        }
        rowPosition = *position;
        auto extra = indexedExtras_.find(rowPosition);
        indexedRow.key = key;
        indexedRow.templateName = extra != indexedExtras_.end() ? extra->second.templateName : std::string{};
        indexedRow.version = extra != indexedExtras_.end() ? extra->second.version : indexedEpoch_;
      } else {
        auto rowIndex = keyIndex_.find(key);
        if (rowIndex == keyIndex_.end()) {
          continue;
        }
        stored = &rows_[rowIndex->second];
      }
      const Row& row = stored != nullptr ? *stored : indexedRow;
      Template* compiled = templateForLocked(row.templateName);
      if (compiled == nullptr) {
        continue;
      }

      auto& entry = rowNodes_[key];
      auto mounted = current.find(key);
      std::shared_ptr<const ShadowNode> existing = mounted != current.end() ? mounted->second : entry.node;
      /*
       * Rebuild a row that was unmounted. Unmounting drops its event targets for good, so its
       * touches and image events would be lost. A cached row that is still mounted is reused.
       */
      if (mounted == current.end() && existing && !hasLiveEventTarget(*existing)) {
        forgetTagsLocked(*existing, key);
        existing = nullptr;
        entry.node = nullptr;
      }
      bool sameShape = existing != nullptr && entry.node != nullptr && entry.templateName == compiled->name &&
        entry.shapeVersion == compiled->shapeVersion;

      std::shared_ptr<const ShadowNode> node;
      if (sameShape && entry.rowVersion == row.version && entry.templateVersion == compiled->version) {
        node = existing;
      } else {
        if (existing && !sameShape) {
          forgetTagsLocked(*existing, key);
        }
        ++(sameShape ? reboundRows : builtRows);
        if (stored == nullptr) {
          indexedRow.item = indexedItemLocked(rowPosition);
        }
        node = buildRowLocked(
          *compiled,
          row,
          sameShape ? existing : nullptr,
          sameShape && entry.templateVersion == compiled->version,
          context);
        entry.rowVersion = row.version;
        entry.templateVersion = compiled->version;
        entry.shapeVersion = compiled->shapeVersion;
        entry.templateName = compiled->name;
      }
      entry.node = node;
      entry.usedAt = clock_;
      rows.push_back(std::move(node));
    }
  }

  auto rowKey = [](const std::shared_ptr<const ShadowNode>& row) -> std::string {
    const auto elementProps = dynamic_cast<const ShadowListElementViewProps*>(row->getProps().get());
    return elementProps ? elementProps->elementKey : std::string{};
  };
  mountedLowKey_ = rows.empty() ? std::string{} : rowKey(rows.front());
  mountedHighKey_ = rows.empty() ? std::string{} : rowKey(rows.back());
  mountedCount_ = rows.size();
  evictLocked(cacheRows_);

  // Put a copy of the current sticky row in the section header overlay.
  bool hasStickyOverlay = false;
  std::size_t overlayOthersIndex = SIZE_MAX;
  for (auto& child : others) {
    const auto templateProps = dynamic_cast<const ShadowListTemplateViewProps*>(child->getProps().get());
    if (!templateProps || templateProps->templateType != "sectionHeader") {
      continue;
    }
    hasStickyOverlay = true;
    overlayOthersIndex = static_cast<std::size_t>(&child - others.data());
    auto sticky = stickyRowLocked(keys, core, inverted, context);
    ChildList overlay;
    if (sticky) {
      overlay.push_back(std::move(sticky));
    }
    if (child->getChildren() != overlay) {
      child = child->clone(ShadowNodeFragment{.children = std::make_shared<const ChildList>(std::move(overlay))});
    }
    break;
  }
  if (!hasStickyOverlay) {
    // The overlay went away with the sticky indices. Let go of its row and tags.
    dropStickyLocked();
  }

  ChildList next;
  next.reserve(others.size() + rows.size());
  next.insert(next.end(), others.begin(), others.begin() + static_cast<std::ptrdiff_t>(rowsAt));
  next.insert(next.end(), rows.begin(), rows.end());
  next.insert(next.end(), others.begin() + static_cast<std::ptrdiff_t>(rowsAt), others.end());

  /*
   * Remember this pass for the fast path, but only when it kept a full measured window. A
   * gap in the rows or a window picked before the list had a size makes the next pass differ.
   */
  if (haveTarget && measuredTarget && rows.size() == targetHigh - targetLow + 1) {
    reconcileCache_.valid = true;
    reconcileCache_.childTags.clear();
    reconcileCache_.childTags.reserve(next.size());
    for (const auto& child : next) {
      reconcileCache_.childTags.push_back(child->getTag());
    }
    reconcileCache_.keys = keysSnapshot;
    reconcileCache_.storeVersion = storeVersion_;
    reconcileCache_.configVersion = configVersion_;
    reconcileCache_.templatesContainer = templatesContainer_;
    reconcileCache_.initialIndex = initialIndex;
    reconcileCache_.inverted = inverted;
    reconcileCache_.targetLow = targetLow;
    reconcileCache_.targetHigh = targetHigh;
    reconcileCache_.overlayChildIndex = overlayOthersIndex == SIZE_MAX
      ? SIZE_MAX
      : overlayOthersIndex + (overlayOthersIndex >= rowsAt ? rows.size() : 0);
    reconcileCache_.stickyActive = hasStickyOverlay ? activeStickyIndexLocked(keys, core, inverted) : SIZE_MAX;
  }

  if (next == children) {
    return nullptr;
  }
  SL_LOG("native: rows=[%zu..%zu] mounted=%zu built=%zu rebound=%zu",
    targetLow, targetHigh, rows.size(), builtRows, reboundRows);
  return std::make_shared<const ChildList>(std::move(next));
}

void ShadowListNativeEngine::didLayout(const ShadowNode& listNode, azimgd::shadowlist::Container& core) {
  bool request = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);

    // A scroll waiting for these rows can run in the next commit.
    laidOutVersion_ = reconciledVersion_;
    if (pendingScroll_ && laidOutVersion_ >= pendingScroll_->afterVersion) {
      request = true;
    }

    // Keep the laid out rows so a row coming back into view reuses them.
    for (const auto& child : listNode.getChildren()) {
      const auto elementProps = dynamic_cast<const ShadowListElementViewProps*>(child->getProps().get());
      if (!elementProps) {
        /*
         * Same for the pinned copy in the section header overlay. Keeping the laid out copy
         * lets the next pass see the overlay as unchanged instead of swapping it back in.
         */
        const auto templateProps = dynamic_cast<const ShadowListTemplateViewProps*>(child->getProps().get());
        if (templateProps && templateProps->templateType == "sectionHeader" && stickyNode_.node &&
            child->getChildren().size() == 1 && child->getChildren().front()->getTag() == stickyNode_.node->getTag()) {
          stickyNode_.node = child->getChildren().front();
        }
        continue;
      }
      auto found = rowNodes_.find(elementProps->elementKey);
      if (found != rowNodes_.end() && found->second.node && found->second.node->getTag() == child->getTag()) {
        found->second.node = child;
      }
    }

    /*
     * Rows were picked using estimates. If they turn out smaller, they may not fill the screen,
     * and nothing would commit until the user scrolls, so ask for a commit now.
     */
    auto coverageShort = [&]() {
      double windowSize = core.getWindowContainerSize();
      std::size_t count = core.getElementsSize();
      if (mountedLowKey_.empty() || windowSize <= 0.0 || count == 0) {
        return false;
      }
      std::size_t low = core.findElementIndexByKey(mountedLowKey_);
      std::size_t high = core.findElementIndexByKey(mountedHighKey_);
      if (low >= count || high >= count) {
        return false;
      }
      double offset = core.getContainerOffset();
      double margin = windowSize * 0.5;
      bool coveredLow = low == 0 || core.getElementOffset(low) <= offset - margin;
      bool coveredHigh = high + 1 >= count || core.getElementOffset(high) + core.getElementSize(high) >= offset + windowSize + margin;
      if (coveredLow && coveredHigh) {
        return false;
      }
      std::tuple<std::size_t, std::size_t, std::uint64_t, std::size_t> signature{low, high, core.geometryVersion, count};
      if (signature == lastCoverageRequest_) {
        return false;
      }
      lastCoverageRequest_ = signature;
      return true;
    };
    if (coverageShort()) {
      request = true;
    }
  }
  if (request) {
    nudge();
  }
}

#pragma mark - Registry

namespace {

std::mutex& registryMutex() {
  static std::mutex mutex;
  return mutex;
}

/*
 * Weak on purpose. An engine lives while a list node or a JS handle holds it, and handles
 * go away with a JS reload or a discarded render.
 */
std::unordered_map<std::string, std::weak_ptr<ShadowListNativeEngine>>& registryEntries() {
  static std::unordered_map<std::string, std::weak_ptr<ShadowListNativeEngine>> entries;
  return entries;
}

void sweepLocked() {
  auto& entries = registryEntries();
  for (auto iterator = entries.begin(); iterator != entries.end();) {
    iterator = iterator->second.expired() ? entries.erase(iterator) : std::next(iterator);
  }
}

}

std::shared_ptr<ShadowListNativeEngine> ShadowListNativeRegistry::obtain(const std::string& listId) {
  std::lock_guard<std::mutex> lock(registryMutex());
  auto& entry = registryEntries()[listId];
  auto engine = entry.lock();
  if (!engine) {
    engine = std::make_shared<ShadowListNativeEngine>(listId);
    entry = engine;
  }
  return engine;
}

std::shared_ptr<ShadowListNativeEngine> ShadowListNativeRegistry::find(const std::string& listId) {
  std::lock_guard<std::mutex> lock(registryMutex());
  auto& entries = registryEntries();
  auto found = entries.find(listId);
  return found == entries.end() ? nullptr : found->second.lock();
}

std::shared_ptr<ShadowListNativeEngine> ShadowListNativeRegistry::open(const std::string& listId) {
  {
    std::lock_guard<std::mutex> lock(registryMutex());
    sweepLocked();
  }
  return obtain(listId);
}

}
