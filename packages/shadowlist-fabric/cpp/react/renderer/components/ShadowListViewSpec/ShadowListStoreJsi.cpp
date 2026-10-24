#include "ShadowListStoreJsi.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iterator>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace facebook::react::shadowlist {

namespace sl = azimgd::shadowlist;

namespace {

std::mutex& registryMutex() {
  static std::mutex mutex;
  return mutex;
}

/*
 * Weak entries, swept when a new store registers. There are only ever a few lists.
 */
std::unordered_map<std::int64_t, std::weak_ptr<sl::RowKeyStore>>& registry() {
  static std::unordered_map<std::int64_t, std::weak_ptr<sl::RowKeyStore>> entries;
  return entries;
}

std::int64_t registerStore(const std::shared_ptr<sl::RowKeyStore>& store) {
  static std::atomic<std::int64_t> next{1};
  std::int64_t id = next.fetch_add(1, std::memory_order_relaxed);
  std::lock_guard<std::mutex> lock(registryMutex());
  auto& entries = registry();
  for (auto entry = entries.begin(); entry != entries.end();) {
    entry = entry->second.expired() ? entries.erase(entry) : std::next(entry);
  }
  entries.emplace(id, store);
  return id;
}

/*
 * What JS holds for its store. The store lives while JS or a node family holds it. The list's
 * selection lives here too, read and written only by JS, see the selection functions.
 */
class StoreHandle final : public jsi::HostObject {
public:
  StoreHandle() : store(std::make_shared<sl::RowKeyStore>()), id(registerStore(store)) {}

  jsi::Value get(jsi::Runtime& runtime, const jsi::PropNameID& name) override {
    return name.utf8(runtime) == "id" ? jsi::Value(static_cast<double>(id)) : jsi::Value::undefined();
  }

  const std::shared_ptr<sl::RowKeyStore> store;
  const std::int64_t id;
  sl::ListSelection selection;
};

std::shared_ptr<StoreHandle> handleOf(jsi::Runtime& runtime, const jsi::Value& value) {
  if (!value.isObject()) {
    throw jsi::JSError(runtime, "ShadowListStore: expected a store");
  }
  auto object = value.getObject(runtime);
  if (!object.isHostObject<StoreHandle>(runtime)) {
    throw jsi::JSError(runtime, "ShadowListStore: expected a store");
  }
  return object.getHostObject<StoreHandle>(runtime);
}

std::shared_ptr<sl::RowKeyStore> storeOf(jsi::Runtime& runtime, const jsi::Value& value) {
  return handleOf(runtime, value)->store;
}

std::uint64_t versionOf(const jsi::Value& value) {
  if (!value.isNumber()) {
    return 0;
  }
  double number = value.getNumber();
  return number > 0 ? static_cast<std::uint64_t>(number) : 0;
}

std::size_t sizeOf(const jsi::Value& value) {
  if (!value.isNumber()) {
    return 0;
  }
  double number = value.getNumber();
  return number > 0 ? static_cast<std::size_t>(number) : 0;
}

std::vector<std::string> stringsOf(jsi::Runtime& runtime, const jsi::Value& value) {
  std::vector<std::string> strings;
  if (!value.isObject()) {
    return strings;
  }
  auto array = value.getObject(runtime).getArray(runtime);
  std::size_t length = array.size(runtime);
  strings.reserve(length);
  for (std::size_t position = 0; position < length; ++position) {
    auto item = array.getValueAtIndex(runtime, position);
    strings.push_back(item.isString() ? item.getString(runtime).utf8(runtime) : std::string());
  }
  return strings;
}

std::vector<double> numbersOf(jsi::Runtime& runtime, const jsi::Value& value) {
  std::vector<double> numbers;
  if (!value.isObject()) {
    return numbers;
  }
  auto array = value.getObject(runtime).getArray(runtime);
  std::size_t length = array.size(runtime);
  numbers.reserve(length);
  for (std::size_t position = 0; position < length; ++position) {
    auto item = array.getValueAtIndex(runtime, position);
    numbers.push_back(item.isNumber() ? item.getNumber() : 0.0);
  }
  return numbers;
}

std::vector<sl::TreeRow> treeRowsOf(jsi::Runtime& runtime, const jsi::Value* args) {
  std::vector<std::string> keys = stringsOf(runtime, args[0]);
  std::vector<double> depths = numbersOf(runtime, args[1]);
  std::vector<double> flags = numbersOf(runtime, args[2]);
  std::vector<sl::TreeRow> rows(keys.size());
  for (std::size_t row = 0; row < keys.size(); ++row) {
    int rowFlags = row < flags.size() ? static_cast<int>(flags[row]) : 0;
    rows[row].key = std::move(keys[row]);
    rows[row].depth = row < depths.size() && depths[row] > 0 ? static_cast<std::uint32_t>(depths[row]) : 0;
    rows[row].hasChildren = (rowFlags & 1) != 0;
    rows[row].expanded = (rowFlags & 2) != 0;
  }
  return rows;
}

jsi::Array stringArrayOf(jsi::Runtime& runtime, const std::vector<std::string>& strings) {
  jsi::Array array(runtime, strings.size());
  for (std::size_t position = 0; position < strings.size(); ++position) {
    array.setValueAtIndex(runtime, position, jsi::String::createFromUtf8(runtime, strings[position]));
  }
  return array;
}

double optionalNumber(jsi::Runtime& runtime, const jsi::Object& object, const char* name, double fallback) {
  auto value = object.getProperty(runtime, name);
  return value.isNumber() ? value.getNumber() : fallback;
}

std::string optionalString(jsi::Runtime& runtime, const jsi::Object& object, const char* name) {
  auto value = object.getProperty(runtime, name);
  return value.isString() ? value.getString(runtime).utf8(runtime) : std::string();
}

/*
 * One ItemSizeSpec object as RowSizeSpec, the same rules parseRowSizeSpecs had for JSON.
 */
bool sizeSpecOf(jsi::Runtime& runtime, const jsi::Object& object, std::string key, sl::RowSizeSpec& spec) {
  spec.key = std::move(key);
  if (spec.key.empty()) {
    return false;
  }
  spec.text = optionalString(runtime, object, "text");
  spec.fontFamily = optionalString(runtime, object, "fontFamily");
  auto fontWeight = object.getProperty(runtime, "fontWeight");
  if (fontWeight.isString()) {
    spec.fontWeight = fontWeight.getString(runtime).utf8(runtime);
  } else if (fontWeight.isNumber()) {
    spec.fontWeight = std::to_string(static_cast<int>(fontWeight.getNumber()));
  }
  spec.fontStyle = optionalString(runtime, object, "fontStyle");
  spec.fontSize = optionalNumber(runtime, object, "fontSize", sl::RowSizeSpec::DEFAULT_FONT_SIZE);
  spec.lineHeight = optionalNumber(runtime, object, "lineHeight", std::numeric_limits<double>::quiet_NaN());
  spec.letterSpacing = optionalNumber(runtime, object, "letterSpacing", std::numeric_limits<double>::quiet_NaN());
  spec.numberOfLines = static_cast<int>(optionalNumber(runtime, object, "numberOfLines", 0.0));
  spec.insetWidth = optionalNumber(runtime, object, "insetWidth", 0.0);
  spec.insetHeight = optionalNumber(runtime, object, "insetHeight", 0.0);
  spec.widthFraction = optionalNumber(runtime, object, "widthFraction", 1.0);
  spec.fixedHeight = optionalNumber(runtime, object, "fixedHeight", std::numeric_limits<double>::quiet_NaN());
  return true;
}

using Body = jsi::Value (*)(jsi::Runtime&, const jsi::Value*, size_t);

struct Entry {
  const char* name;
  unsigned int argCount;
  Body body;
};

jsi::Value createStore(jsi::Runtime& runtime, const jsi::Value*, size_t) {
  return jsi::Object::createFromHostObject(runtime, std::make_shared<StoreHandle>());
}

jsi::Value setKeys(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 3) {
    return false;
  }
  auto store = storeOf(runtime, args[0]);
  return store->setKeys(versionOf(args[1]), stringsOf(runtime, args[2]));
}

jsi::Value spliceKeys(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 6) {
    return false;
  }
  auto store = storeOf(runtime, args[0]);
  return store->spliceKeys(
    versionOf(args[1]), versionOf(args[2]), sizeOf(args[3]), sizeOf(args[4]), stringsOf(runtime, args[5]));
}

jsi::Value setSections(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 8) {
    return false;
  }
  auto store = storeOf(runtime, args[0]);
  int flags = args[3].isNumber() ? static_cast<int>(args[3].getNumber()) : 0;
  sl::SectionsUpdate update;
  update.hasHeader = (flags & 1) != 0;
  update.hasFooter = (flags & 2) != 0;
  update.stickyHeaders = (flags & 4) != 0;
  update.qualifyItems = (flags & 8) != 0;
  update.sectionKeys = stringsOf(runtime, args[4]);
  for (double itemCount : numbersOf(runtime, args[5])) {
    update.itemCounts.push_back(itemCount > 0 ? static_cast<std::size_t>(itemCount) : 0);
  }
  // JS sends -1 for a section without reused rows.
  for (double reuse : numbersOf(runtime, args[6])) {
    update.reuse.push_back(reuse >= 0 ? static_cast<std::size_t>(reuse) : sl::UNDEFINED_INDEX);
  }
  update.itemKeys = stringsOf(runtime, args[7]);
  return store->setSections(versionOf(args[1]), versionOf(args[2]), std::move(update));
}

jsi::Value resetTree(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 5) {
    return false;
  }
  auto store = storeOf(runtime, args[0]);
  return store->resetTree(versionOf(args[1]), treeRowsOf(runtime, args + 2));
}

jsi::Value expandTree(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 7) {
    return -1;
  }
  auto store = storeOf(runtime, args[0]);
  auto splice = store->expandTree(versionOf(args[1]), versionOf(args[2]), sizeOf(args[3]), treeRowsOf(runtime, args + 4));
  return splice ? jsi::Value(static_cast<double>(splice->deleted)) : jsi::Value(-1);
}

jsi::Value collapseTree(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 4) {
    return -1;
  }
  auto store = storeOf(runtime, args[0]);
  auto splice = store->collapseTree(versionOf(args[1]), versionOf(args[2]), sizeOf(args[3]));
  return splice ? jsi::Value(static_cast<double>(splice->deleted)) : jsi::Value(-1);
}

jsi::Value indexOfKey(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 3 || !args[2].isString()) {
    return -1;
  }
  auto store = storeOf(runtime, args[0]);
  auto snapshot = store->snapshot(versionOf(args[1]));
  if (!snapshot) {
    return -2;
  }
  std::size_t hint = sl::UNDEFINED_INDEX;
  if (count > 3 && args[3].isNumber() && args[3].getNumber() >= 0) {
    hint = static_cast<std::size_t>(args[3].getNumber());
  }
  std::size_t row = snapshot->indexOfKey(args[2].getString(runtime).utf8(runtime), hint);
  return row == sl::UNDEFINED_INDEX ? jsi::Value(-1) : jsi::Value(static_cast<double>(row));
}

jsi::Value setSizeSpecs(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 4) {
    return jsi::Value::undefined();
  }
  auto store = storeOf(runtime, args[0]);
  std::vector<std::string> keys = stringsOf(runtime, args[2]);
  std::vector<sl::RowSizeSpec> specs;
  if (args[3].isObject()) {
    auto array = args[3].getObject(runtime).getArray(runtime);
    std::size_t length = std::min(array.size(runtime), keys.size());
    specs.reserve(length);
    for (std::size_t position = 0; position < length; ++position) {
      auto item = array.getValueAtIndex(runtime, position);
      if (!item.isObject()) {
        continue;
      }
      sl::RowSizeSpec spec;
      if (sizeSpecOf(runtime, item.getObject(runtime), std::move(keys[position]), spec)) {
        specs.push_back(std::move(spec));
      }
    }
  }
  store->setSizeSpecs(versionOf(args[1]), std::move(specs));
  return jsi::Value::undefined();
}

/*
 * The selected keys after a change, or null when nothing changed.
 */
jsi::Value selectionResult(jsi::Runtime& runtime, const sl::ListSelection& selection, bool changed) {
  return changed ? jsi::Value(runtime, stringArrayOf(runtime, selection.getKeys())) : jsi::Value::null();
}

std::string keyOf(jsi::Runtime& runtime, const jsi::Value& value) {
  return value.isString() ? value.getString(runtime).utf8(runtime) : std::string();
}

jsi::Value setSelection(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 3) {
    return jsi::Value::undefined();
  }
  sl::ListSelection& selection = handleOf(runtime, args[0])->selection;
  selection.setMultiple(args[2].isBool() && args[2].getBool());
  selection.setKeys(stringsOf(runtime, args[1]));
  return jsi::Value::undefined();
}

jsi::Value selectKey(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 2 || !args[1].isString()) {
    return jsi::Value::null();
  }
  sl::ListSelection& selection = handleOf(runtime, args[0])->selection;
  std::vector<std::string> deselected;
  return selectionResult(runtime, selection, selection.select(keyOf(runtime, args[1]), deselected));
}

jsi::Value selectOnlyKey(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 2 || !args[1].isString()) {
    return jsi::Value::null();
  }
  sl::ListSelection& selection = handleOf(runtime, args[0])->selection;
  std::vector<std::string> deselected;
  return selectionResult(runtime, selection, selection.selectOnly(keyOf(runtime, args[1]), deselected));
}

jsi::Value deselectKey(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 2 || !args[1].isString()) {
    return jsi::Value::null();
  }
  sl::ListSelection& selection = handleOf(runtime, args[0])->selection;
  return selectionResult(runtime, selection, selection.deselect(keyOf(runtime, args[1])));
}

/*
 * Drop the selected keys the version's rows no longer have. Each selected key is looked up in
 * the version's index, so this costs the selection's size once the index exists.
 */
jsi::Value retainSelection(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 2) {
    return jsi::Value::null();
  }
  auto handle = handleOf(runtime, args[0]);
  if (handle->selection.isEmpty()) {
    return jsi::Value::null();
  }
  auto snapshot = handle->store->snapshot(versionOf(args[1]));
  if (!snapshot) {
    return jsi::Value::null();
  }
  bool changed = handle->selection.retainWhere(
    [&](const std::string& key) { return snapshot->indexOfKey(key) != sl::UNDEFINED_INDEX; });
  return selectionResult(runtime, handle->selection, changed);
}

/*
 * The rows of the selected keys in the version, low to high.
 */
jsi::Value selectedIndices(jsi::Runtime& runtime, const jsi::Value* args, size_t count) {
  if (count < 2) {
    return jsi::Array(runtime, 0);
  }
  auto handle = handleOf(runtime, args[0]);
  auto snapshot = handle->selection.isEmpty() ? nullptr : handle->store->snapshot(versionOf(args[1]));
  if (!snapshot) {
    return jsi::Array(runtime, 0);
  }
  std::vector<std::size_t> rows = handle->selection.indicesWhere(
    [&](const std::string& key) { return snapshot->indexOfKey(key); }, sl::UNDEFINED_INDEX);
  jsi::Array array(runtime, rows.size());
  for (std::size_t position = 0; position < rows.size(); ++position) {
    array.setValueAtIndex(runtime, position, static_cast<double>(rows[position]));
  }
  return array;
}

const Entry ENTRIES[] = {
  {"createStore", 0, createStore},
  {"setKeys", 3, setKeys},
  {"spliceKeys", 6, spliceKeys},
  {"setSections", 8, setSections},
  {"resetTree", 5, resetTree},
  {"expandTree", 7, expandTree},
  {"collapseTree", 4, collapseTree},
  {"indexOfKey", 4, indexOfKey},
  {"setSizeSpecs", 4, setSizeSpecs},
  {"setSelection", 3, setSelection},
  {"selectKey", 2, selectKey},
  {"selectOnlyKey", 2, selectOnlyKey},
  {"deselectKey", 2, deselectKey},
  {"retainSelection", 2, retainSelection},
  {"selectedIndices", 2, selectedIndices},
};

jsi::Function functionOf(jsi::Runtime& runtime, const Entry& entry) {
  Body body = entry.body;
  return jsi::Function::createFromHostFunction(runtime, jsi::PropNameID::forAscii(runtime, entry.name), entry.argCount,
    [body](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) { return body(rt, args, count); });
}

}

std::shared_ptr<sl::RowKeyStore> findRowKeyStore(std::int64_t id) {
  std::lock_guard<std::mutex> lock(registryMutex());
  auto& entries = registry();
  auto entry = entries.find(id);
  return entry == entries.end() ? nullptr : entry->second.lock();
}

jsi::Value storeFunction(jsi::Runtime& runtime, const std::string& name) {
  for (const Entry& entry : ENTRIES) {
    if (name == entry.name) {
      return functionOf(runtime, entry);
    }
  }
  return jsi::Value::undefined();
}

jsi::Object createStoreModuleObject(jsi::Runtime& runtime) {
  jsi::Object module(runtime);
  for (const Entry& entry : ENTRIES) {
    module.setProperty(runtime, entry.name, functionOf(runtime, entry));
  }
  return module;
}

}
