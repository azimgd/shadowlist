/*
 * Node-API side of ShadowListKitCore.ts. Every call is a thin hop into the core's ListDriver on the
 * main thread, or into a list's RowKeyStore and ListSelection for the ShadowListStore module, the
 * functions ShadowListStoreJsi.h lists under the same names. A Float64Array the TypeScript side
 * reuses carries a layout's values in and out.
 */

#include <node_api.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListSelection.hpp>
#include <shadowlist-core/host/RowKeyStore.hpp>
#include <shadowlist-core/host/ScrollTarget.hpp>
#include <shadowlist-core/host/StickyLayout.hpp>
#include <shadowlist-core/host/Viewability.hpp>

namespace {

namespace sl = azimgd::shadowlist;

/*
 * Slots of ShadowListKitCore.PASS_*. The first block is what the view reports, the second what it applies.
 */
enum PassSlot {
  PASS_OFFSET = 0,
  PASS_WINDOW_ALONG,
  PASS_WINDOW_CROSS,
  PASS_HEADER_SIZE,
  PASS_FOOTER_SIZE,
  PASS_PHASE,
  PASS_USER_SCROLLED,
  PASS_OUT_OFFSET,
  PASS_OUT_CONTENT,
  PASS_OUT_BAND_LOW,
  PASS_OUT_BAND_HIGH,
  PASS_OUT_SETTLING,
  PASS_OUT_REACHED_START,
  PASS_OUT_REACHED_END,
  PASS_OUT_FOOTER_START,
  PASS_SLOTS,
};

constexpr std::size_t MAX_ARGS = 16;

/*
 * A store and the selection of its list, what JS holds a handle to.
 */
struct StoreHandle {
  std::shared_ptr<sl::RowKeyStore> store = std::make_shared<sl::RowKeyStore>();
  sl::ListSelection selection;
  std::int64_t id = 0;
};

std::mutex registryMutex;
std::unordered_map<std::int64_t, std::weak_ptr<sl::RowKeyStore>> registry;
std::atomic<std::int64_t> nextStoreId{1};

/*
 * A store by the id the list's keysStore prop names, or null once JS dropped its handle.
 */
std::shared_ptr<sl::RowKeyStore> findRowKeyStore(std::int64_t id) {
  std::lock_guard<std::mutex> lock(registryMutex);
  auto entry = registry.find(id);
  return entry == registry.end() ? nullptr : entry->second.lock();
}

/*
 * The driver behind one list. env and measure are only valid during runPasses, the one call that
 * measures, and failed is set once the measure callback threw, which then stays pending in JS.
 */
struct Peer {
  sl::ListDriver driver;
  sl::StickyState sticky;
  std::vector<sl::ViewabilityTracker> viewability;
  std::vector<std::size_t> prefetchFound;
  std::vector<std::size_t> cancelFound;
  napi_env env = nullptr;
  napi_value measure = nullptr;
  bool failed = false;
};

/*
 * One call's arguments. A getter reads what the TypeScript side sends, with no checks past the
 * type it expects: nothing but ShadowListKitCore.ts and the store module's callers reach this.
 */
struct Args {
  napi_env env;
  std::size_t count = MAX_ARGS;
  napi_value values[MAX_ARGS] = {};

  bool has(std::size_t at) const {
    return at < count;
  }

  double number(std::size_t at, double fallback = 0.0) const {
    double value = fallback;
    if (has(at)) {
      napi_get_value_double(env, values[at], &value);
    }
    return value;
  }

  bool boolean(std::size_t at) const {
    bool value = false;
    if (has(at)) {
      napi_get_value_bool(env, values[at], &value);
    }
    return value;
  }

  std::size_t size(std::size_t at) const {
    double value = number(at);
    return value > 0.0 ? static_cast<std::size_t>(value) : 0;
  }

  std::uint64_t version(std::size_t at) const {
    double value = number(at);
    return value > 0.0 ? static_cast<std::uint64_t>(value) : 0;
  }

  std::size_t index(std::size_t at) const {
    double value = number(at, -1.0);
    return value >= 0.0 ? static_cast<std::size_t>(value) : sl::UNDEFINED_INDEX;
  }

  std::string string(std::size_t at) const {
    return has(at) ? stringOf(env, values[at]) : std::string();
  }

  std::vector<std::string> strings(std::size_t at) const {
    std::vector<std::string> result;
    std::uint32_t length = 0;
    if (!has(at) || napi_get_array_length(env, values[at], &length) != napi_ok) {
      return result;
    }
    result.reserve(length);
    for (std::uint32_t position = 0; position < length; ++position) {
      napi_handle_scope scope;
      napi_open_handle_scope(env, &scope);
      napi_value item;
      napi_get_element(env, values[at], position, &item);
      result.push_back(stringOf(env, item));
      napi_close_handle_scope(env, scope);
    }
    return result;
  }

  std::vector<double> numbers(std::size_t at) const {
    std::vector<double> result;
    std::uint32_t length = 0;
    if (!has(at) || napi_get_array_length(env, values[at], &length) != napi_ok) {
      return result;
    }
    result.reserve(length);
    for (std::uint32_t position = 0; position < length; ++position) {
      napi_value item;
      napi_get_element(env, values[at], position, &item);
      double value = 0.0;
      napi_get_value_double(env, item, &value);
      result.push_back(value);
    }
    return result;
  }

  double* doubles(std::size_t at) const {
    void* data = nullptr;
    napi_get_typedarray_info(env, values[at], nullptr, nullptr, &data, nullptr, nullptr);
    return static_cast<double*>(data);
  }

  Peer& peer() const {
    void* data = nullptr;
    napi_get_value_external(env, values[0], &data);
    return *static_cast<Peer*>(data);
  }

  StoreHandle& store() const {
    void* data = nullptr;
    napi_unwrap(env, values[0], &data);
    return **static_cast<std::shared_ptr<StoreHandle>*>(data);
  }

  static std::string stringOf(napi_env env, napi_value value) {
    std::size_t length = 0;
    if (napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok) {
      return {};
    }
    std::string result(length, '\0');
    napi_get_value_string_utf8(env, value, result.data(), length + 1, &length);
    return result;
  }
};

napi_value undefinedValue(napi_env env) {
  napi_value value;
  napi_get_undefined(env, &value);
  return value;
}

napi_value nullValue(napi_env env) {
  napi_value value;
  napi_get_null(env, &value);
  return value;
}

napi_value numberValue(napi_env env, double number) {
  napi_value value;
  napi_create_double(env, number, &value);
  return value;
}

napi_value boolValue(napi_env env, bool boolean) {
  napi_value value;
  napi_get_boolean(env, boolean, &value);
  return value;
}

napi_value stringValue(napi_env env, const std::string& string) {
  napi_value value;
  napi_create_string_utf8(env, string.data(), string.size(), &value);
  return value;
}

napi_value stringsValue(napi_env env, const std::vector<std::string>& strings) {
  napi_value array;
  napi_create_array_with_length(env, strings.size(), &array);
  for (std::size_t position = 0; position < strings.size(); ++position) {
    napi_set_element(env, array, static_cast<std::uint32_t>(position), stringValue(env, strings[position]));
  }
  return array;
}

template <typename Number>
napi_value numbersValue(napi_env env, const std::vector<Number>& numbers) {
  napi_value array;
  napi_create_array_with_length(env, numbers.size(), &array);
  for (std::size_t position = 0; position < numbers.size(); ++position) {
    napi_set_element(env, array, static_cast<std::uint32_t>(position), numberValue(env, static_cast<double>(numbers[position])));
  }
  return array;
}

/*
 * A row index for JS, -1 for none.
 */
napi_value indexValue(napi_env env, std::size_t index) {
  return numberValue(env, index == sl::UNDEFINED_INDEX ? -1.0 : static_cast<double>(index));
}

using Body = napi_value (*)(const Args&);

/*
 * Every exported function runs through here: C++ exceptions become JS errors, since none may cross
 * Node-API.
 */
napi_value trampoline(napi_env env, napi_callback_info info) {
  Args args;
  args.env = env;
  void* data = nullptr;
  napi_get_cb_info(env, info, &args.count, args.values, nullptr, &data);
  try {
    return reinterpret_cast<Body>(data)(args);
  } catch (const std::exception& error) {
    napi_throw_error(env, nullptr, error.what());
    return nullptr;
  }
}

void finalizeStore(napi_env, void* data, void*) {
  delete static_cast<std::shared_ptr<StoreHandle>*>(data);
}

napi_value createStore(const Args& args) {
  auto handle = std::make_shared<StoreHandle>();
  handle->id = nextStoreId.fetch_add(1, std::memory_order_relaxed);
  {
    std::lock_guard<std::mutex> lock(registryMutex);
    for (auto entry = registry.begin(); entry != registry.end();) {
      entry = entry->second.expired() ? registry.erase(entry) : std::next(entry);
    }
    registry.emplace(handle->id, handle->store);
  }
  napi_value object;
  napi_create_object(args.env, &object);
  napi_set_named_property(args.env, object, "id", numberValue(args.env, static_cast<double>(handle->id)));
  napi_wrap(args.env, object, new std::shared_ptr<StoreHandle>(handle), finalizeStore, nullptr, nullptr);
  return object;
}

napi_value setKeys(const Args& args) {
  return boolValue(args.env, args.store().store->setKeys(args.version(1), args.strings(2)));
}

napi_value spliceKeys(const Args& args) {
  return boolValue(
    args.env,
    args.store().store->spliceKeys(
      args.version(1), args.version(2), args.size(3), args.size(4), args.strings(5)));
}

napi_value setSections(const Args& args) {
  int flags = static_cast<int>(args.number(3));
  sl::SectionsUpdate update;
  update.hasHeader = (flags & 1) != 0;
  update.hasFooter = (flags & 2) != 0;
  update.stickyHeaders = (flags & 4) != 0;
  update.qualifyItems = (flags & 8) != 0;
  update.sectionKeys = args.strings(4);
  for (double itemCount : args.numbers(5)) {
    update.itemCounts.push_back(itemCount > 0.0 ? static_cast<std::size_t>(itemCount) : 0);
  }
  // JS sends -1 for a section without reused rows.
  for (double reuse : args.numbers(6)) {
    update.reuse.push_back(reuse >= 0.0 ? static_cast<std::size_t>(reuse) : sl::UNDEFINED_INDEX);
  }
  update.itemKeys = args.strings(7);
  return boolValue(args.env, args.store().store->setSections(args.version(1), args.version(2), std::move(update)));
}

std::vector<sl::TreeRow> treeRowsOf(const Args& args, std::size_t at) {
  std::vector<std::string> keys = args.strings(at);
  std::vector<double> depths = args.numbers(at + 1);
  std::vector<double> flags = args.numbers(at + 2);
  std::vector<sl::TreeRow> rows(keys.size());
  for (std::size_t row = 0; row < keys.size(); ++row) {
    int rowFlags = row < flags.size() ? static_cast<int>(flags[row]) : 0;
    rows[row].key = std::move(keys[row]);
    rows[row].depth = row < depths.size() && depths[row] > 0.0 ? static_cast<std::uint32_t>(depths[row]) : 0;
    rows[row].hasChildren = (rowFlags & 1) != 0;
    rows[row].expanded = (rowFlags & 2) != 0;
  }
  return rows;
}

napi_value resetTree(const Args& args) {
  return boolValue(args.env, args.store().store->resetTree(args.version(1), treeRowsOf(args, 2)));
}

napi_value expandTree(const Args& args) {
  auto splice = args.store().store->expandTree(args.version(1), args.version(2), args.size(3), treeRowsOf(args, 4));
  return numberValue(args.env, splice ? static_cast<double>(splice->deleted) : -1.0);
}

napi_value collapseTree(const Args& args) {
  auto splice = args.store().store->collapseTree(args.version(1), args.version(2), args.size(3));
  return numberValue(args.env, splice ? static_cast<double>(splice->deleted) : -1.0);
}

napi_value indexOfStoreKey(const Args& args) {
  auto snapshot = args.store().store->snapshot(args.version(1));
  if (!snapshot) {
    return numberValue(args.env, -2.0);
  }
  return indexValue(args.env, snapshot->indexOfKey(args.string(2), args.index(3)));
}

/*
 * A terminal measures every row it mounts, and a row's height is whole cells, so no text is laid
 * out ahead of time and the specs have no reader here.
 */
napi_value setSizeSpecs(const Args& args) {
  return undefinedValue(args.env);
}

napi_value selectionResult(const Args& args, const sl::ListSelection& selection, bool changed) {
  return changed ? stringsValue(args.env, selection.getKeys()) : nullValue(args.env);
}

napi_value setSelection(const Args& args) {
  sl::ListSelection& selection = args.store().selection;
  selection.setMultiple(args.boolean(2));
  selection.setKeys(args.strings(1));
  return undefinedValue(args.env);
}

napi_value selectKey(const Args& args) {
  sl::ListSelection& selection = args.store().selection;
  std::vector<std::string> deselected;
  return selectionResult(args, selection, selection.select(args.string(1), deselected));
}

napi_value selectOnlyKey(const Args& args) {
  sl::ListSelection& selection = args.store().selection;
  std::vector<std::string> deselected;
  return selectionResult(args, selection, selection.selectOnly(args.string(1), deselected));
}

napi_value deselectKey(const Args& args) {
  sl::ListSelection& selection = args.store().selection;
  return selectionResult(args, selection, selection.deselect(args.string(1)));
}

napi_value retainSelection(const Args& args) {
  StoreHandle& handle = args.store();
  auto snapshot = handle.selection.isEmpty() ? nullptr : handle.store->snapshot(args.version(1));
  if (!snapshot) {
    return nullValue(args.env);
  }
  bool changed = handle.selection.retainWhere(
    [&](const std::string& key) { return snapshot->indexOfKey(key) != sl::UNDEFINED_INDEX; });
  return selectionResult(args, handle.selection, changed);
}

napi_value selectedIndices(const Args& args) {
  StoreHandle& handle = args.store();
  auto snapshot = handle.selection.isEmpty() ? nullptr : handle.store->snapshot(args.version(1));
  if (!snapshot) {
    return numbersValue(args.env, std::vector<std::size_t>{});
  }
  return numbersValue(
    args.env,
    handle.selection.indicesWhere([&](const std::string& key) { return snapshot->indexOfKey(key); }, sl::UNDEFINED_INDEX));
}

void finalizePeer(napi_env, void* data, void*) {
  delete static_cast<Peer*>(data);
}

/*
 * The row's size along the scroll axis from the view's measure callback.
 */
double measureThroughJs(Peer& peer, std::size_t index, const std::string& key) {
  napi_env env = peer.env;
  if (peer.failed) {
    return 1.0;
  }
  napi_handle_scope scope;
  napi_open_handle_scope(env, &scope);
  napi_value argv[2] = {numberValue(env, static_cast<double>(index)), stringValue(env, key)};
  napi_value result = nullptr;
  double size = 1.0;
  if (napi_call_function(env, undefinedValue(env), peer.measure, 2, argv, &result) == napi_ok) {
    napi_get_value_double(env, result, &size);
  } else {
    peer.failed = true;
  }
  napi_close_handle_scope(env, scope);
  return size;
}

napi_value createDriver(const Args& args) {
  auto* peer = new Peer();
  peer->driver.setMeasureRow(
    [peer](std::size_t index, const std::string& key, double) { return measureThroughJs(*peer, index, key); });
  napi_value external;
  napi_create_external(args.env, peer, finalizePeer, nullptr, &external);
  return external;
}

napi_value setSettings(const Args& args) {
  sl::ListSettings settings;
  settings.estimatedRowSize = args.number(1);
  settings.overscan = args.number(2);
  settings.startReachedThreshold = args.number(3);
  settings.endReachedThreshold = args.number(4);
  settings.numberOfColumns = args.size(5);
  settings.inverted = args.boolean(6);
  settings.followAppends = args.boolean(7);
  settings.invertedFollowBand = args.number(8);
  settings.horizontal = args.boolean(9);
  settings.snapToItem = args.boolean(10);
  settings.snapAlignment = static_cast<sl::SnapAlignment>(static_cast<int>(args.number(11)));
  args.peer().driver.setSettings(settings);
  return undefinedValue(args.env);
}

/*
 * Takes the keys of a store's version, the rows that stick with them. Returns false when the store
 * or the version is gone.
 */
napi_value adoptKeys(const Args& args) {
  auto store = findRowKeyStore(static_cast<std::int64_t>(args.number(1)));
  std::uint64_t version = args.version(2);
  auto snapshot = store ? store->snapshot(version) : nullptr;
  if (!snapshot) {
    return boolValue(args.env, false);
  }
  sl::KeyList list = snapshot->keys();
  std::vector<std::string> keys;
  keys.reserve(list.size());
  for (std::size_t index = 0; index < list.size(); ++index) {
    keys.push_back(list[index]);
  }
  sl::ListDriver& driver = args.peer().driver;
  driver.reloadKeys(std::move(keys));
  driver.setStickyIndices(snapshot->getStickyRows());
  store->adopted(version);
  return boolValue(args.env, true);
}

napi_value setStickyIndices(const Args& args) {
  std::vector<std::size_t> sticky;
  for (double index : args.numbers(1)) {
    if (index >= 0.0) {
      sticky.push_back(static_cast<std::size_t>(index));
    }
  }
  args.peer().driver.setStickyIndices(std::move(sticky));
  return undefinedValue(args.env);
}

napi_value runPasses(const Args& args) {
  Peer& peer = args.peer();
  double* pass = args.doubles(1);
  peer.env = args.env;
  peer.measure = args.values[2];
  peer.failed = false;
  sl::PassInput input;
  input.offset = pass[PASS_OFFSET];
  input.windowAlong = pass[PASS_WINDOW_ALONG];
  input.windowCross = pass[PASS_WINDOW_CROSS];
  input.headerSize = pass[PASS_HEADER_SIZE];
  input.footerSize = pass[PASS_FOOTER_SIZE];
  input.phase = static_cast<sl::ScrollPhase>(static_cast<int>(pass[PASS_PHASE]));
  input.userScrolled = pass[PASS_USER_SCROLLED] != 0.0;
  sl::PassResult result = peer.driver.runPasses(input);
  peer.env = nullptr;
  peer.measure = nullptr;
  pass[PASS_OUT_OFFSET] = result.offset;
  pass[PASS_OUT_CONTENT] = result.contentAlong;
  pass[PASS_OUT_BAND_LOW] = result.band.low;
  pass[PASS_OUT_BAND_HIGH] = result.band.high;
  pass[PASS_OUT_SETTLING] = result.settling ? 1.0 : 0.0;
  pass[PASS_OUT_REACHED_START] = result.reachedStart ? 1.0 : 0.0;
  pass[PASS_OUT_REACHED_END] = result.reachedEnd ? 1.0 : 0.0;
  pass[PASS_OUT_FOOTER_START] = result.footerStart;
  return undefinedValue(args.env);
}

napi_value markRemeasure(const Args& args) {
  args.peer().driver.markRemeasure({args.size(1)});
  return undefinedValue(args.env);
}

napi_value resetKeepingPosition(const Args& args) {
  args.peer().driver.resetKeepingPosition();
  return undefinedValue(args.env);
}

napi_value getRowRect(const Args& args) {
  sl::RowRect rect = args.peer().driver.getRowRect(args.size(1));
  double* out = args.doubles(2);
  out[0] = rect.x;
  out[1] = rect.y;
  out[2] = rect.width;
  out[3] = rect.height;
  return undefinedValue(args.env);
}

napi_value getLeadingAt(const Args& args) {
  return numberValue(args.env, args.peer().driver.getLeadingAt(args.size(1)));
}

napi_value getExtentAt(const Args& args) {
  return numberValue(args.env, args.peer().driver.getExtentAt(args.size(1)));
}

napi_value getRowCount(const Args& args) {
  return numberValue(args.env, static_cast<double>(args.peer().driver.getRowCount()));
}

napi_value getKeyAt(const Args& args) {
  return stringValue(args.env, args.peer().driver.getKeyAt(args.size(1)));
}

napi_value indexOfKey(const Args& args) {
  return indexValue(args.env, args.peer().driver.indexOfKey(args.string(1)));
}

/*
 * The rows overlapping the viewport into out as low and high. Returns false before the first layout.
 */
napi_value getVisibleRange(const Args& args) {
  std::optional<sl::MountedRange> range = args.peer().driver.getVisibleRange();
  if (range) {
    double* out = args.doubles(1);
    out[0] = static_cast<double>(range->low);
    out[1] = static_cast<double>(range->high);
  }
  return boolValue(args.env, range.has_value());
}

napi_value activeStickyIndex(const Args& args) {
  return indexValue(args.env, args.peer().driver.activeStickyIndex(args.number(1)));
}

napi_value stickyLeading(const Args& args) {
  return numberValue(args.env, args.peer().driver.stickyLeading(args.size(1), args.number(2)));
}

/*
 * The pinned header's and footer's translations into out, from the numbers ShadowListKitCore sends.
 */
napi_value stickyTranslations(const Args& args) {
  Peer& peer = args.peer();
  const double* in = args.doubles(1);
  sl::StickyInput input;
  input.offset = in[0];
  input.windowSize = in[1];
  input.contentSize = in[2];
  input.hasHeader = in[3] != 0.0;
  input.headerSize = in[4];
  input.stickyHeader = in[5] != 0.0;
  input.autoHideHeader = in[6] != 0.0;
  input.hasFooter = in[7] != 0.0;
  input.footerSize = in[8];
  input.footerStart = in[9];
  input.stickyFooter = in[10] != 0.0;
  input.autoHideFooter = in[11] != 0.0;
  input.accumulate = in[12] != 0.0;
  sl::StickyTranslations translations = sl::stickyTranslations(input, peer.sticky);
  double* out = args.doubles(2);
  out[0] = translations.header;
  out[1] = translations.footer;
  return undefinedValue(args.env);
}

napi_value scrollToRow(const Args& args) {
  args.peer().driver.scrollToRow(args.size(1), args.number(2), args.number(3));
  return undefinedValue(args.env);
}

napi_value scrollToEnd(const Args& args) {
  args.peer().driver.scrollToEnd();
  return undefinedValue(args.env);
}

napi_value nearestSnapOffset(const Args& args) {
  return numberValue(args.env, args.peer().driver.nearestSnapOffset(args.number(1)));
}

napi_value getAnchorState(const Args& args) {
  std::optional<sl::AnchorState> anchor = args.peer().driver.getAnchorState(args.number(1));
  if (!anchor) {
    return nullValue(args.env);
  }
  napi_value object;
  napi_create_object(args.env, &object);
  napi_set_named_property(args.env, object, "key", stringValue(args.env, anchor->key));
  napi_set_named_property(args.env, object, "offset", numberValue(args.env, anchor->offset));
  return object;
}

/*
 * The rows to prefetch and to cancel now that low to high are mounted, or null when none changed.
 */
napi_value updatePrefetch(const Args& args) {
  Peer& peer = args.peer();
  peer.driver.updatePrefetch(args.size(1), args.size(2), peer.prefetchFound, peer.cancelFound);
  if (peer.prefetchFound.empty() && peer.cancelFound.empty()) {
    return nullValue(args.env);
  }
  napi_value object;
  napi_create_object(args.env, &object);
  napi_set_named_property(args.env, object, "prefetch", numbersValue(args.env, peer.prefetchFound));
  napi_set_named_property(args.env, object, "cancel", numbersValue(args.env, peer.cancelFound));
  return object;
}

/*
 * One config's viewable rows now: what it has to report at this time, as added and removed keys,
 * or null. waiting says a report is held back for minimumViewTime.
 */
napi_value updateViewability(const Args& args) {
  Peer& peer = args.peer();
  std::size_t config = args.size(1);
  if (config >= peer.viewability.size()) {
    peer.viewability.resize(config + 1);
  }
  sl::ViewabilityTracker& tracker = peer.viewability[config];
  sl::ViewabilityTiming timing;
  timing.minimumViewTime = args.number(2);
  timing.waitForInteraction = args.boolean(3);
  tracker.setTiming(timing);
  double now = args.number(6);
  if (args.boolean(4)) {
    tracker.recordInteraction(now);
  }
  tracker.update(args.strings(5), now);
  sl::ViewabilityReport report;
  bool due = tracker.take(now, report);
  napi_value object;
  napi_create_object(args.env, &object);
  napi_set_named_property(args.env, object, "waiting", boolValue(args.env, tracker.isWaiting()));
  napi_set_named_property(args.env, object, "added", stringsValue(args.env, due ? report.changes.added : std::vector<std::string>{}));
  napi_set_named_property(args.env, object, "removed", stringsValue(args.env, due ? report.changes.removed : std::vector<std::string>{}));
  return object;
}

struct Entry {
  const char* name;
  Body body;
};

const Entry STORE_ENTRIES[] = {
  {"createStore", createStore},
  {"setKeys", setKeys},
  {"spliceKeys", spliceKeys},
  {"setSections", setSections},
  {"resetTree", resetTree},
  {"expandTree", expandTree},
  {"collapseTree", collapseTree},
  {"indexOfKey", indexOfStoreKey},
  {"setSizeSpecs", setSizeSpecs},
  {"setSelection", setSelection},
  {"selectKey", selectKey},
  {"selectOnlyKey", selectOnlyKey},
  {"deselectKey", deselectKey},
  {"retainSelection", retainSelection},
  {"selectedIndices", selectedIndices},
};

const Entry DRIVER_ENTRIES[] = {
  {"createDriver", createDriver},
  {"setSettings", setSettings},
  {"adoptKeys", adoptKeys},
  {"setStickyIndices", setStickyIndices},
  {"runPasses", runPasses},
  {"markRemeasure", markRemeasure},
  {"resetKeepingPosition", resetKeepingPosition},
  {"getRowRect", getRowRect},
  {"getLeadingAt", getLeadingAt},
  {"getExtentAt", getExtentAt},
  {"getRowCount", getRowCount},
  {"getKeyAt", getKeyAt},
  {"indexOfKey", indexOfKey},
  {"getVisibleRange", getVisibleRange},
  {"activeStickyIndex", activeStickyIndex},
  {"stickyLeading", stickyLeading},
  {"stickyTranslations", stickyTranslations},
  {"scrollToRow", scrollToRow},
  {"scrollToEnd", scrollToEnd},
  {"nearestSnapOffset", nearestSnapOffset},
  {"getAnchorState", getAnchorState},
  {"updatePrefetch", updatePrefetch},
  {"updateViewability", updateViewability},
};

template <std::size_t Size>
napi_value objectOf(napi_env env, const Entry (&entries)[Size]) {
  napi_value object;
  napi_create_object(env, &object);
  for (const Entry& entry : entries) {
    napi_value function;
    napi_create_function(env, entry.name, NAPI_AUTO_LENGTH, trampoline, reinterpret_cast<void*>(entry.body), &function);
    napi_set_named_property(env, object, entry.name, function);
  }
  return object;
}

napi_value init(napi_env env, napi_value exports) {
  napi_set_named_property(env, exports, "store", objectOf(env, STORE_ENTRIES));
  napi_set_named_property(env, exports, "driver", objectOf(env, DRIVER_ENTRIES));
  return exports;
}

}

NAPI_MODULE(shadowlist_tty, init)
