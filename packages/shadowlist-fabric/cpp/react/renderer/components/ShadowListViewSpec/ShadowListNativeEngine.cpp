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
#include <climits>
#include <unordered_set>

namespace facebook::react {

namespace {

constexpr std::string_view ELEMENT_MARKER = "shadowlist:";
constexpr std::string_view TEMPLATE_MARKER = "shadowlist-template:";
constexpr const char* TEMPLATES_CONTAINER_TYPE = "native";

/*
 * Tags for synthesized nodes. React allocates even tags counting up from 2 and root tags end in 1,
 * so start far above anything React reaches and stay even (Android routes odd tags to the legacy
 * renderer).
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

folly::dynamic evaluate(const ShadowListNativeExpression& expression, const folly::dynamic& item) {
  if (expression.format) {
    std::string text;
    for (const auto& part : expression.parts) {
      text += part.path.empty() ? part.text : toText(lookupPath(item, part.path));
    }
    return text;
  }
  if (expression.parts.empty()) {
    return nullptr;
  }
  const auto& value = lookupPath(item, expression.parts.front().path);
  if (expression.negate) {
    return !truthy(value);
  }
  return value;
}

// Writes one bound value into the raw props patch for its element.
void applyBinding(folly::dynamic& patch, const std::string& prop, const folly::dynamic& value) {
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
    // Java reads colors with getInt: pass a signed 32-bit ARGB as processColor does on Android
    // (an unsigned value above INT_MAX saturates to 0x7FFFFFFF, translucent white).
    auto number = static_cast<std::int64_t>(static_cast<std::int32_t>(color.value_or(0)));
#else
    auto number = static_cast<std::int64_t>(color.value_or(0));
#endif
    patch[prop] = color ? folly::dynamic(number) : folly::dynamic(nullptr);
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
   * Android mounts a view from the props' raw props, so a patch alone would reach the platform
   * view as the whole prop set. Carry the base's raw props with it (see SHADOWLIST_NATIVE.md,
   * "Android").
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

  // Rows whose key left the list will not come back as the same nodes.
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
  const std::vector<std::string>& templates) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
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
  }
  requestCommit();
  return size();
}

std::size_t ShadowListNativeEngine::insertItems(
  std::size_t index,
  const folly::dynamic& items,
  const std::vector<std::string>& keys,
  const std::vector<std::string>& templates) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
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
  requestCommit();
  return true;
}

std::size_t ShadowListNativeEngine::removeItems(const std::vector<std::string>& keys) {
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
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
    if (found != config.items().end() && found->second.isNumber() && found->second.asDouble() >= 0) {
      target = static_cast<std::size_t>(found->second.asDouble());
    }
  };
  readCount("initialRows", initialRows_);
  readCount("padRows", padRows_);
  readCount("cacheRows", cacheRows_);
  if (initialRows_ == 0) {
    initialRows_ = 1;
  }
}

folly::dynamic ShadowListNativeEngine::getItem(const std::string& key) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = keyIndex_.find(key);
  return found == keyIndex_.end() ? folly::dynamic(nullptr) : rows_[found->second].item;
}

std::vector<std::string> ShadowListNativeEngine::getKeys() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return *keys_;
}

std::size_t ShadowListNativeEngine::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return rows_.size();
}

std::optional<ShadowListNativeEngine::ResolvedTag> ShadowListNativeEngine::resolveTag(Tag tag) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = tagKeys_.find(tag);
  if (found == tagKeys_.end()) {
    return std::nullopt;
  }
  auto index = keyIndex_.find(found->second.key);
  if (index == keyIndex_.end()) {
    return std::nullopt;
  }
  return ResolvedTag{found->second.key, index->second, found->second.repeatIndex};
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

void ShadowListNativeEngine::applyPendingScroll(azimgd::shadowlist::Container& core) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!pendingScroll_ || laidOutVersion_ < pendingScroll_->afterVersion) {
    return;
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
   * A copy of whatever state is committed when the update applies. It must not re-apply an
   * offset correction that state may still carry: the host already applied it on mount.
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
   * The instance handle is part of the shape: rows share their prototype's handle (it routes
   * their events), so when React replaces a template element's fiber, rows rebound in place
   * would keep the old, dead handle and drop their touches. A new handle rebuilds them.
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
          element.repeatMax = static_cast<std::size_t>(repeatMax->second.asDouble());
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
      // A malformed marker leaves the element static.
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
      // Same props, same structure: a layout clone of the container, nothing to redo.
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
  // Rows are new views on Android: carry the prototype's accumulated raw props, not its last diff.
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
      applyBinding(patch, prop, evaluate(expression, item));
    }
    props = cloneWithPatch(*element.prototype, element.baseProps, std::move(patch), context);
  }

  /*
   * Rebinding keeps the existing families only while the structure still lines up. A repeated
   * element's child count follows its array, so there only the component must match; its
   * children are matched by position (entry i keeps entry i's nodes).
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
   * A new family per synthesized node: every mounted view needs its own tag. The instance
   * handle is the prototype's; a clone with none crashes on its first event, and events from a
   * clone reaching the template's fiber is what lets a template element handle touches (its
   * handlers resolve the row from the touched view's tag).
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
  const PropsParserContext& context) {
  Props::Shared rootProps = rootPropsCurrent && existing
    ? existing->getProps()
    : cloneWithPatch(
        *compiled.root.prototype,
        compiled.root.baseProps,
        folly::dynamic::object("elementKey", row.key)("index", 0),
        context);
  return buildNode(compiled.root, row.item, row.key, existing, &rootProps, std::nullopt, -1, context);
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
  std::vector<std::pair<std::uint64_t, std::string>> idle;
  for (const auto& [key, rowNode] : rowNodes_) {
    if (rowNode.usedAt != clock_) {
      idle.emplace_back(rowNode.usedAt, key);
    }
  }
  if (idle.size() <= keep) {
    return;
  }
  std::sort(idle.begin(), idle.end(), [](const auto& left, const auto& right) { return left.first > right.first; });
  for (std::size_t index = keep; index < idle.size(); ++index) {
    auto found = rowNodes_.find(idle[index].second);
    if (found == rowNodes_.end()) {
      continue;
    }
    if (found->second.node) {
      forgetTagsLocked(*found->second.node, found->first);
    }
    rowNodes_.erase(found);
  }
}

std::shared_ptr<const ShadowListNativeEngine::ChildList> ShadowListNativeEngine::reconcileRows(
  const ShadowNode& listNode,
  const ChildList& children,
  const std::vector<std::string>& keys,
  azimgd::shadowlist::Container& core,
  int initialIndex,
  bool inverted) {
  const auto contextContainer = listNode.getContextContainer();
  if (!contextContainer) {
    return nullptr;
  }
  PropsParserContext context{listNode.getSurfaceId(), *contextContainer};

  std::lock_guard<std::mutex> lock(mutex_);
  reconciledVersion_ = storeVersion_;

  /*
   * Split the list's own children (templates, header, footer, empty) from the rows mounted last
   * time. Rows go back right after the templates container, where ShadowList renders its rows,
   * so the header stays below them and the footer/sticky overlay above in z-order.
   */
  ChildList others;
  others.reserve(children.size());
  std::unordered_map<std::string, std::shared_ptr<const ShadowNode>> current;
  std::size_t rowsAt = std::string::npos;
  for (const auto& child : children) {
    if (const auto elementProps = std::dynamic_pointer_cast<const ShadowListElementViewProps>(child->getProps())) {
      current.emplace(elementProps->elementKey, child);
      continue;
    }
    others.push_back(child);
    if (const auto templateProps = std::dynamic_pointer_cast<const ShadowListTemplateViewProps>(child->getProps())) {
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
   * The window: what the core measured this commit (the viewport plus its overscan), or a seed
   * from the edge the list opens at while the core has no viewport yet.
   */
  std::size_t count = keys.size();
  std::size_t targetLow = 0;
  std::size_t targetHigh = 0;
  bool haveTarget = false;
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
     * Keep the mounted rows while they still cover the window: a scroll frame inside it then
     * rebuilds nothing. Once the window runs past an end, remount around it with a pad, so the
     * next few rows of travel are free again.
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
  }

  ChildList rows;
  ++clock_;
  std::size_t builtRows = 0;
  std::size_t reboundRows = 0;
  if (haveTarget) {
    rows.reserve(targetHigh - targetLow + 1);
    for (std::size_t index = targetLow; index <= targetHigh; ++index) {
      const auto& key = keys[index];
      auto rowIndex = keyIndex_.find(key);
      if (rowIndex == keyIndex_.end()) {
        continue;
      }
      const Row& row = rows_[rowIndex->second];
      Template* compiled = templateForLocked(row.templateName);
      if (compiled == nullptr) {
        continue;
      }

      auto& entry = rowNodes_[key];
      auto mounted = current.find(key);
      std::shared_ptr<const ShadowNode> existing = mounted != current.end() ? mounted->second : entry.node;
      /*
       * A row that was unmounted cannot come back as the same nodes: unmounting released its
       * families' event targets for good, so its touches and image events would be dropped.
       * Rebuild it. A cached row that is still mounted (React re-rendered the list and handed its
       * children back without the rows) keeps its target and is reused.
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
    const auto elementProps = std::dynamic_pointer_cast<const ShadowListElementViewProps>(row->getProps());
    return elementProps ? elementProps->elementKey : std::string{};
  };
  mountedLowKey_ = rows.empty() ? std::string{} : rowKey(rows.front());
  mountedHighKey_ = rows.empty() ? std::string{} : rowKey(rows.back());
  mountedCount_ = rows.size();
  evictLocked(cacheRows_);

  ChildList next;
  next.reserve(others.size() + rows.size());
  next.insert(next.end(), others.begin(), others.begin() + static_cast<std::ptrdiff_t>(rowsAt));
  next.insert(next.end(), rows.begin(), rows.end());
  next.insert(next.end(), others.begin() + static_cast<std::ptrdiff_t>(rowsAt), others.end());
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

    // A scroll waiting on these rows can go in the next commit.
    laidOutVersion_ = reconciledVersion_;
    if (pendingScroll_ && laidOutVersion_ >= pendingScroll_->afterVersion) {
      request = true;
    }

    // The laid-out rows are what a row re-entering the window should come back as.
    for (const auto& child : listNode.getChildren()) {
      const auto elementProps = std::dynamic_pointer_cast<const ShadowListElementViewProps>(child->getProps());
      if (!elementProps) {
        continue;
      }
      auto found = rowNodes_.find(elementProps->elementKey);
      if (found != rowNodes_.end() && found->second.node && found->second.node->getTag() == child->getTag()) {
        found->second.node = child;
      }
    }

    /*
     * The window was chosen from estimates for rows that had not been measured. Once they are,
     * the mounted rows can fall short of the viewport (rows smaller than estimated). Nothing
     * else would commit until the user scrolls, so ask for the commit that remounts.
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
 * Weak only: an engine lives as long as a list node or a JS handle (`open`) holds it. Handles
 * are JSI host objects, so they die with the runtime that made them (a JS reload) and with a
 * render React discards.
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
