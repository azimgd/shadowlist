#include "ShadowListNativeEngine.h"

#include <folly/dynamic.h>
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

using azimgd::shadowlist::JsonValue;

/*
 * A host JSON value as folly::dynamic, for props. Bound values are mostly scalars, so this is cheap.
 */
folly::dynamic toDynamic(const JsonValue& value) {
  switch (value.type()) {
    case JsonValue::Type::Null:
      return nullptr;
    case JsonValue::Type::Bool:
      return value.getBool();
    case JsonValue::Type::Int:
      return value.getInt();
    case JsonValue::Type::Double:
      return value.getDouble();
    case JsonValue::Type::String:
      return value.getString();
    case JsonValue::Type::Array: {
      folly::dynamic array = folly::dynamic::array();
      for (const auto& entry : value.getArray()) {
        array.push_back(toDynamic(entry));
      }
      return array;
    }
    case JsonValue::Type::Object: {
      folly::dynamic object = folly::dynamic::object();
      for (const auto& [field, entry] : value.items()) {
        object[field] = toDynamic(entry);
      }
      return object;
    }
  }
  return nullptr;
}

/*
 * Writes one bound value into the element's props patch. A missing value or a bad color is
 * skipped, so the element keeps its template value, even when a reused row loses the value.
 */
void applyBinding(
  folly::dynamic& patch,
  const std::string& prop,
  const JsonValue& value,
  [[maybe_unused]] const Props& base) {
  if (azimgd::shadowlist::nativeBindingKeepsTemplate(
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
      patch["source"] = folly::dynamic::array(folly::dynamic::object("uri", value.getString()));
    } else if (value.isObject()) {
      patch["source"] = folly::dynamic::array(toDynamic(value));
    } else if (value.isArray()) {
      patch["source"] = toDynamic(value);
    } else {
      patch["source"] = folly::dynamic::array();
    }
    return;
  }
  if (prop == "hidden") {
    patch["display"] = azimgd::shadowlist::nativeTruthy(value) ? "none" : "flex";
    return;
  }
  if (prop == "visible") {
    patch["display"] = azimgd::shadowlist::nativeTruthy(value) ? "flex" : "none";
    return;
  }
  if (azimgd::shadowlist::isNativeColorProp(prop) && value.isString()) {
    auto color = azimgd::shadowlist::parseNativeColor(value.getString());
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
  patch[prop] = toDynamic(value);
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

ShadowListNativeEngine::ShadowListNativeEngine(std::string listId) : listId_(std::move(listId)) {}

#pragma mark - Data

void ShadowListNativeEngine::dropRemovedRowNodesLocked() {
  for (auto iterator = rowNodes_.begin(); iterator != rowNodes_.end();) {
    if (!store_.indexOf(iterator->first)) {
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
  const JsonValue& items,
  const std::vector<std::string>& keys,
  const std::vector<std::string>& templates,
  bool scrollToStart) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (store_.setData(items, keys, templates)) {
      dropRemovedRowNodesLocked();
    }
    if (scrollToStart) {
      pendingScroll_ = PendingScroll{SCROLL_TO_START, 0.0, storeVersion_, store_.keysVersion()};
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
  const JsonValue& extraItems,
  const std::vector<std::string>& extraTemplates,
  bool scrollToStart,
  std::vector<std::int32_t> ids) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    bool keysChanged = store_.setIndexed(
      count, std::move(order), indexField, valueField, extraIndices, extraItems, extraTemplates, std::move(ids));
    if (keysChanged) {
      dropRemovedRowNodesLocked();
    }
    if (scrollToStart) {
      pendingScroll_ = PendingScroll{SCROLL_TO_START, 0.0, storeVersion_, store_.keysVersion()};
    }
  }
  requestCommit();
  return size();
}

std::size_t ShadowListNativeEngine::insertItems(
  std::size_t index,
  const JsonValue& items,
  const std::vector<std::string>& keys,
  const std::vector<std::string>& templates) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (store_.indexed()) {
      return store_.size();
    }
    if (!store_.insertItems(index, items, keys, templates)) {
      return store_.size();
    }
    dropRemovedRowNodesLocked();
  }
  requestCommit();
  return size();
}

bool ShadowListNativeEngine::updateItem(
  const std::string& key,
  const JsonValue& patch,
  const std::string& templateName,
  bool replace) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!store_.updateItem(key, patch, templateName, replace)) {
      return false;
    }
  }
  requestCommit();
  return true;
}

std::size_t ShadowListNativeEngine::removeItems(const std::vector<std::string>& keys) {
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (store_.indexed()) {
      return store_.size();
    }
    changed = store_.removeItems(keys);
    if (changed) {
      dropRemovedRowNodesLocked();
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
    auto result = store_.moveItem(key, toIndex);
    if (result == azimgd::shadowlist::NativeStore::MoveResult::Missing) {
      return false;
    }
    if (result == azimgd::shadowlist::NativeStore::MoveResult::Unchanged) {
      return true;
    }
    dropRemovedRowNodesLocked();
  }
  requestCommit();
  return true;
}

void ShadowListNativeEngine::setTemplateStyle(
  const std::string& templateName,
  const std::string& elementId,
  const JsonValue& style) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& styles = templateStyles_[templateName];
    if (style.isObject() && !style.empty()) {
      styles[elementId] = toDynamic(style);
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

void ShadowListNativeEngine::configure(const JsonValue& config) {
  if (!config.isObject()) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  auto readCount = [&](const char* name, std::size_t& target) {
    const JsonValue* found = config.find(name);
    // Capped so a huge value can't overflow the row window math or the cast.
    if (found != nullptr && found->isNumber() && found->asDouble() >= 0) {
      target = static_cast<std::size_t>(std::min(found->asDouble(), 1e6));
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

JsonValue ShadowListNativeEngine::getItem(const std::string& key) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return store_.item(key);
}

std::vector<std::string> ShadowListNativeEngine::getKeys() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return *store_.keys();
}

std::size_t ShadowListNativeEngine::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return store_.size();
}

std::optional<ShadowListNativeEngine::ResolvedTag> ShadowListNativeEngine::resolveTag(Tag tag) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = tagKeys_.find(tag);
  if (found == tagKeys_.end()) {
    return std::nullopt;
  }
  auto index = store_.indexOf(found->second.key);
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
  return {store_.keys(), store_.keysVersion()};
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
  // A bad marker leaves the element without bindings.
  auto spec = startsWith(nativeId, ELEMENT_MARKER)
    ? JsonValue::parse(std::string_view(nativeId).substr(ELEMENT_MARKER.size()))
    : std::nullopt;
  if (spec && spec->isObject()) {
    if (const JsonValue* id = spec->find("i"); id != nullptr && id->isString()) {
      element.elementId = id->getString();
    }
    if (const JsonValue* action = spec->find("a"); action != nullptr) {
      element.keepNativeId = azimgd::shadowlist::nativeTruthy(*action);
    }
    if (const JsonValue* repeat = spec->find("r"); repeat != nullptr && repeat->isString()) {
      element.repeat = azimgd::shadowlist::parseNativePath(repeat->getString());
      const JsonValue* repeatMax = spec->find("m");
      if (repeatMax != nullptr && repeatMax->isNumber() && repeatMax->asDouble() >= 0) {
        element.repeatMax = static_cast<std::size_t>(std::min(repeatMax->asDouble(), 1e9));
      }
    }
    if (const JsonValue* bindings = spec->find("b"); bindings != nullptr && bindings->isObject()) {
      for (const auto& [prop, source] : bindings->items()) {
        if (!source.isString()) {
          continue;
        }
        auto expression = azimgd::shadowlist::parseNativeExpression(source.getString());
        if (prop == "text") {
          element.text = std::move(expression);
        } else {
          element.bindings.emplace_back(prop, std::move(expression));
        }
      }
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
  const JsonValue& item,
  const std::string& key,
  const std::shared_ptr<const ShadowNode>& existing,
  const Props::Shared* propsOverride,
  const std::optional<std::string>& rawText,
  int repeatIndex,
  const PropsParserContext& context) {
  /*
   * Reuse the existing nodes only while the structure still matches. A repeated element's
   * child count follows its array, so only the component must match and children pair up by position.
   */
  bool reuse = existing != nullptr && existing->getComponentHandle() == element.prototype->getComponentHandle() &&
    (element.repeat || existing->getChildren().size() == element.children.size());

  Props::Shared props;
  // The patch these props are made from, when they come from bound values or text.
  std::optional<folly::dynamic> boundPatch;
  bool boundPropsReused = false;
  if (propsOverride != nullptr) {
    props = *propsOverride;
  } else if (rawText) {
    boundPatch = folly::dynamic::object("text", *rawText);
  } else if (element.bindings.empty()) {
    props = element.baseProps;
  } else {
    folly::dynamic patch = folly::dynamic::object;
    for (const auto& [prop, expression] : element.bindings) {
      applyBinding(patch, prop, azimgd::shadowlist::evaluateNative(expression, item), *element.baseProps);
    }
    boundPatch = std::move(patch);
  }
  if (boundPatch) {
    /*
     * A rebind usually leaves most elements' values alone. Parsing props is the costly part,
     * and new props also make the node look changed, so keep the existing props when they
     * came from the same base and the same patch. See boundProps_.
     */
    if (reuse) {
      auto previous = boundProps_.find(existing->getTag());
      if (previous != boundProps_.end() && previous->second.base == element.baseProps &&
          previous->second.patch == *boundPatch) {
        props = existing->getProps();
        boundPropsReused = true;
      }
    }
    if (!boundPropsReused) {
      props = cloneWithPatch(*element.prototype, element.baseProps, *boundPatch, context);
    }
  }
  // Records the patch for the node that ends up with these props.
  auto rememberBoundProps = [&](Tag tag) {
    if (boundPatch && !boundPropsReused) {
      boundProps_.insert_or_assign(tag, BoundProps{element.baseProps, std::move(*boundPatch)});
    }
  };

  std::optional<std::string> text;
  if (element.text) {
    text = azimgd::shadowlist::nativeText(azimgd::shadowlist::evaluateNative(*element.text, item));
  }

  ChildList children;
  bool sameChildren = reuse;
  auto existingChild = [&](std::size_t position) -> std::shared_ptr<const ShadowNode> {
    return reuse && position < existing->getChildren().size() ? existing->getChildren()[position] : nullptr;
  };
  if (element.repeat) {
    const auto& entries = azimgd::shadowlist::lookupNativePath(item, *element.repeat);
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
    rememberBoundProps(existing->getTag());
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
  rememberBoundProps(tag);
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
  Row scratch;
  std::size_t position = 0;
  const Row* found = store_.find(key, scratch, position);
  if (found == nullptr) {
    dropStickyLocked();
    return nullptr;
  }
  Row row = *found;
  if (found == &scratch) {
    row.item = store_.indexedItem(position);
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
  boundProps_.erase(node.getTag());
  auto found = tagKeys_.find(node.getTag());
  if (found != tagKeys_.end() && found->second.key == key) {
    tagKeys_.erase(found);
  }
  for (const auto& child : node.getChildren()) {
    forgetTagsLocked(*child, key);
  }
}

void ShadowListNativeEngine::evictLocked(std::size_t keep) {
  auto doomed = azimgd::shadowlist::staleNativeRows(
    rowNodes_, clock_, keep, [](const RowNode& rowNode) { return rowNode.usedAt; });
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
      Row indexedRow;
      std::size_t rowPosition = 0;
      const Row* found = store_.find(key, indexedRow, rowPosition);
      if (found == nullptr) {
        continue;
      }
      bool stored = found != &indexedRow;
      const Row& row = *found;
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
        if (!stored) {
          indexedRow.item = store_.indexedItem(rowPosition);
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
