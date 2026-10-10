/*
 * Wasm side of ShadowListKitCore.ts. Every call is a thin hop into the core's ListDriver on the
 * main thread. Values cross through a scratch area of the heap the TypeScript side reuses.
 * Results longer than one value go out through one buffer, valid until the next call.
 */

#include <emscripten/emscripten.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>
#include <shadowlist-core/host/ScrollTarget.hpp>

/*
 * Measures a row through the TypeScript core that owns the handle. Only runPasses calls it.
 */
EM_JS(double, shadowListKitMeasureItem, (std::uintptr_t handle, int index, double crossSize), {
  return Module.shadowListKitMeasureItem(handle, index, crossSize);
});

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
  PASS_TRACKING,
  PASS_OUT_OFFSET,
  PASS_OUT_CONTENT,
  PASS_OUT_BAND_LOW,
  PASS_OUT_BAND_HIGH,
  PASS_OUT_SETTLING,
  PASS_OUT_REACHED_START,
  PASS_OUT_REACHED_END,
  PASS_OUT_GEOMETRY,
  PASS_OUT_WINDOW_LOW,
  PASS_OUT_WINDOW_HIGH,
  PASS_SLOTS,
};

/*
 * Slots of the anchor out values, ShadowListKitCore.ANCHOR_* in TypeScript.
 */
enum AnchorSlot {
  ANCHOR_OFFSET = 0,
  ANCHOR_KEY_LENGTH,
  ANCHOR_SLOTS,
};

/*
 * The driver behind one list and its pass slots. A measure that calls into the core during
 * runPasses never touches them.
 */
struct Peer {
  sl::ListDriver driver;
  double pass[PASS_SLOTS] = {};
};

Peer* peerOf(std::uintptr_t handle) {
  return reinterpret_cast<Peer*>(handle);
}

std::vector<std::int32_t> intResult;
std::string stringResult;

/*
 * Write values into the int result, its length first. Returns where it starts.
 */
const std::int32_t* writeInts(const std::vector<std::int32_t>& values) {
  intResult.clear();
  intResult.reserve(values.size() + 1);
  intResult.push_back(static_cast<std::int32_t>(values.size()));
  intResult.insert(intResult.end(), values.begin(), values.end());
  return intResult.data();
}

std::vector<std::int32_t> readInts(const std::int32_t* values, std::int32_t count) {
  if (values == nullptr || count <= 0) {
    return {};
  }
  return std::vector<std::int32_t>(values, values + count);
}

/*
 * The indices without negative values, which no row has.
 */
std::vector<std::size_t> readIndices(const std::int32_t* values, std::int32_t count) {
  std::vector<std::size_t> indices;
  for (std::int32_t value : readInts(values, count)) {
    if (value >= 0) {
      indices.push_back(static_cast<std::size_t>(value));
    }
  }
  return indices;
}

std::string readString(const char* chars, std::int32_t length) {
  if (chars == nullptr || length <= 0) {
    return {};
  }
  return std::string(chars, static_cast<std::size_t>(length));
}

/*
 * Keys joined into one UTF-8 buffer, with where each one ends. See ShadowListKitCore.pack.
 */
std::vector<std::string> readPackedKeys(const char* packed, const std::int32_t* ends, std::int32_t count) {
  std::vector<std::string> keys;
  if (packed == nullptr || ends == nullptr || count <= 0) {
    return keys;
  }
  keys.reserve(static_cast<std::size_t>(count));
  std::int32_t start = 0;
  for (std::int32_t at = 0; at < count; ++at) {
    std::int32_t end = std::max(ends[at], start);
    keys.emplace_back(packed + start, static_cast<std::size_t>(end - start));
    start = end;
  }
  return keys;
}

sl::PassInput readPassInput(const double* slots) {
  sl::PassInput input;
  input.offset = slots[PASS_OFFSET];
  input.windowAlong = slots[PASS_WINDOW_ALONG];
  input.windowCross = slots[PASS_WINDOW_CROSS];
  input.headerSize = slots[PASS_HEADER_SIZE];
  input.footerSize = slots[PASS_FOOTER_SIZE];
  input.phase = static_cast<sl::ScrollPhase>(static_cast<int>(slots[PASS_PHASE]));
  input.userScrolled = slots[PASS_USER_SCROLLED] != 0.0;
  input.tracking = slots[PASS_TRACKING] != 0.0;
  return input;
}

void writePassResult(const sl::ListDriver& driver, const sl::PassResult& result, double* slots) {
  slots[PASS_OUT_OFFSET] = result.offset;
  slots[PASS_OUT_CONTENT] = result.contentAlong;
  slots[PASS_OUT_BAND_LOW] = result.band.low;
  slots[PASS_OUT_BAND_HIGH] = result.band.high;
  slots[PASS_OUT_SETTLING] = result.settling ? 1.0 : 0.0;
  slots[PASS_OUT_REACHED_START] = result.reachedStart ? 1.0 : 0.0;
  slots[PASS_OUT_REACHED_END] = result.reachedEnd ? 1.0 : 0.0;
  slots[PASS_OUT_GEOMETRY] = static_cast<double>(driver.getGeometryVersion());
  std::optional<sl::MountedRange> window = driver.getMeasuredWindow();
  slots[PASS_OUT_WINDOW_LOW] = window ? static_cast<double>(window->low) : -1.0;
  slots[PASS_OUT_WINDOW_HIGH] = window ? static_cast<double>(window->high) : -1.0;
}

void writeRect(const sl::RowRect& rect, double* out) {
  out[0] = rect.x;
  out[1] = rect.y;
  out[2] = rect.width;
  out[3] = rect.height;
}

bool validRow(const sl::ListDriver& driver, std::int32_t index) {
  return index >= 0 && static_cast<std::size_t>(index) < driver.getCount();
}

/*
 * TypeScript carries a missing index as -1, the core as UNDEFINED_INDEX.
 */
std::int32_t intFromIndex(std::size_t index) {
  return index == sl::UNDEFINED_INDEX ? -1 : static_cast<std::int32_t>(index);
}

std::size_t indexFromInt(std::int32_t index) {
  return index < 0 ? sl::UNDEFINED_INDEX : static_cast<std::size_t>(index);
}

std::vector<std::int32_t> intsFromIndices(const std::vector<std::size_t>& indices) {
  std::vector<std::int32_t> values;
  values.reserve(indices.size());
  for (std::size_t index : indices) {
    values.push_back(intFromIndex(index));
  }
  return values;
}

}

#define SHADOWLIST_KIT_WASM(name) EMSCRIPTEN_KEEPALIVE ShadowListKitCore_##name

extern "C" {

std::uintptr_t SHADOWLIST_KIT_WASM(nativeCreate)() {
  auto* peer = new Peer();
  auto handle = reinterpret_cast<std::uintptr_t>(peer);
  peer->driver.setMeasureItem([handle](std::size_t index, const std::string&, double cross) {
    return shadowListKitMeasureItem(handle, static_cast<int>(index), cross);
  });
  return handle;
}

void SHADOWLIST_KIT_WASM(nativeDestroy)(std::uintptr_t handle) {
  delete peerOf(handle);
}

void SHADOWLIST_KIT_WASM(nativeSetSettings)(std::uintptr_t handle, double estimatedItemSize, double overscan,
  double startReachedThreshold, double endReachedThreshold, std::int32_t columns, bool inverted, bool followAppends,
  bool horizontal, bool snapToItem, std::int32_t snapAlignment) {
  sl::ListSettings settings;
  settings.estimatedItemSize = estimatedItemSize;
  settings.overscan = overscan;
  settings.startReachedThreshold = startReachedThreshold;
  settings.endReachedThreshold = endReachedThreshold;
  settings.columns = static_cast<std::size_t>(columns < 1 ? 1 : columns);
  settings.inverted = inverted;
  settings.followAppends = followAppends;
  settings.horizontal = horizontal;
  settings.snapToItem = snapToItem;
  settings.snapAlignment = snapAlignment;
  peerOf(handle)->driver.setSettings(settings);
}

void SHADOWLIST_KIT_WASM(nativeSetStickyIndices)(std::uintptr_t handle, const std::int32_t* indices, std::int32_t count) {
  peerOf(handle)->driver.setStickyIndices(readIndices(indices, count));
}

void SHADOWLIST_KIT_WASM(nativeReplaceKeys)(std::uintptr_t handle, std::int32_t start, std::int32_t count,
  const char* packed, const std::int32_t* ends, std::int32_t keyCount) {
  peerOf(handle)->driver.replaceKeys(static_cast<std::size_t>(std::max(start, 0)),
    static_cast<std::size_t>(std::max(count, 0)), readPackedKeys(packed, ends, keyCount));
}

/*
 * A negative index drops its key with it, which keeps the two lists paired.
 */
void SHADOWLIST_KIT_WASM(nativeInsertKeys)(std::uintptr_t handle, const std::int32_t* indices, const char* packed,
  const std::int32_t* ends, std::int32_t keyCount) {
  std::vector<std::int32_t> raw = readInts(indices, keyCount);
  std::vector<std::string> strings = readPackedKeys(packed, ends, keyCount);
  std::vector<std::size_t> kept;
  std::vector<std::string> keptKeys;
  for (std::size_t position = 0; position < raw.size() && position < strings.size(); ++position) {
    if (raw[position] >= 0) {
      kept.push_back(static_cast<std::size_t>(raw[position]));
      keptKeys.push_back(std::move(strings[position]));
    }
  }
  peerOf(handle)->driver.insertKeys(std::move(kept), std::move(keptKeys));
}

void SHADOWLIST_KIT_WASM(nativeDeleteKeys)(std::uintptr_t handle, const std::int32_t* indices, std::int32_t count) {
  peerOf(handle)->driver.deleteKeys(readIndices(indices, count));
}

void SHADOWLIST_KIT_WASM(nativeMarkRemeasure)(std::uintptr_t handle, const std::int32_t* indices, std::int32_t count) {
  peerOf(handle)->driver.markRemeasure(readIndices(indices, count));
}

/*
 * Where the peer's pass slots start. TypeScript fills the inputs there before runPasses and reads
 * the outputs after.
 */
double* SHADOWLIST_KIT_WASM(nativePassSlots)(std::uintptr_t handle) {
  return peerOf(handle)->pass;
}

void SHADOWLIST_KIT_WASM(nativeRunPasses)(std::uintptr_t handle) {
  Peer* peer = peerOf(handle);
  sl::PassResult result = peer->driver.runPasses(readPassInput(peer->pass));
  writePassResult(peer->driver, result, peer->pass);
}

void SHADOWLIST_KIT_WASM(nativeResetKeepingPosition)(std::uintptr_t handle) {
  peerOf(handle)->driver.resetKeepingPosition();
}

std::int32_t SHADOWLIST_KIT_WASM(nativeCount)(std::uintptr_t handle) {
  return static_cast<std::int32_t>(peerOf(handle)->driver.getCount());
}

/*
 * Frames of rows low to high, four values each, x y width height.
 */
void SHADOWLIST_KIT_WASM(nativeCopyRowRects)(std::uintptr_t handle, std::int32_t low, std::int32_t high, double* out) {
  const sl::ListDriver& driver = peerOf(handle)->driver;
  if (!validRow(driver, low) || !validRow(driver, high) || high < low) {
    return;
  }
  for (std::int32_t index = low; index <= high; ++index) {
    writeRect(driver.getRowRect(static_cast<std::size_t>(index)), out + static_cast<std::size_t>(index - low) * 4);
  }
}

bool SHADOWLIST_KIT_WASM(nativeRowRect)(std::uintptr_t handle, std::int32_t index, double* out) {
  const sl::ListDriver& driver = peerOf(handle)->driver;
  if (!validRow(driver, index)) {
    return false;
  }
  writeRect(driver.getRowRect(static_cast<std::size_t>(index)), out);
  return true;
}

double SHADOWLIST_KIT_WASM(nativeFooterStart)(std::uintptr_t handle, double footerSize) {
  return peerOf(handle)->driver.getFooterStart(footerSize);
}

/*
 * The rows overlapping the viewport, low and high into out. Returns false before the first layout.
 */
bool SHADOWLIST_KIT_WASM(nativeVisibleRange)(std::uintptr_t handle, std::int32_t* out) {
  std::optional<sl::MountedRange> visible = peerOf(handle)->driver.getVisibleRange();
  if (!visible) {
    return false;
  }
  out[0] = intFromIndex(visible->low);
  out[1] = intFromIndex(visible->high);
  return true;
}

std::int32_t SHADOWLIST_KIT_WASM(nativeIndexOfKey)(std::uintptr_t handle, const char* key, std::int32_t length) {
  return intFromIndex(peerOf(handle)->driver.indexOfKey(readString(key, length)));
}

/*
 * Leading edge and extent of every sticky row, two values each. Returns false when out is too short.
 */
bool SHADOWLIST_KIT_WASM(nativeCopyStickyFrames)(std::uintptr_t handle, double* out, std::int32_t capacity) {
  std::vector<double> frames;
  peerOf(handle)->driver.stickyFrames(frames);
  if (capacity < static_cast<std::int32_t>(frames.size())) {
    return false;
  }
  std::copy(frames.begin(), frames.end(), out);
  return true;
}

void SHADOWLIST_KIT_WASM(nativeScrollToIndex)(std::uintptr_t handle, std::int32_t index, double viewPosition) {
  if (index >= 0) {
    peerOf(handle)->driver.scrollToIndex(static_cast<std::size_t>(index), viewPosition);
  }
}

void SHADOWLIST_KIT_WASM(nativeScrollToStart)(std::uintptr_t handle) {
  peerOf(handle)->driver.scrollToStart();
}

void SHADOWLIST_KIT_WASM(nativeScrollToEnd)(std::uintptr_t handle) {
  peerOf(handle)->driver.scrollToEnd();
}

double SHADOWLIST_KIT_WASM(nativeNearestSnapOffset)(std::uintptr_t handle, double target) {
  return peerOf(handle)->driver.nearestSnapOffset(target);
}

/*
 * Where an animated scroll to a row aims, or NaN for a row the core has not placed yet.
 */
double SHADOWLIST_KIT_WASM(nativeAnimatedTargetOffset)(
  std::uintptr_t handle, std::int32_t index, double viewPosition, double windowAlong, double maxOffset) {
  const sl::ListDriver& driver = peerOf(handle)->driver;
  if (!validRow(driver, index)) {
    return std::nan("");
  }
  return driver.animatedTargetOffset(static_cast<std::size_t>(index), viewPosition, windowAlong, maxOffset);
}

/*
 * Target is ShadowListKitCore.LANDING_*, the same order as ScrollLanding::Target.
 */
void SHADOWLIST_KIT_WASM(nativeSetLanding)(
  std::uintptr_t handle, std::int32_t target, std::int32_t index, double viewPosition) {
  sl::ScrollLanding landing;
  landing.target = static_cast<sl::ScrollLanding::Target>(target);
  landing.index = static_cast<std::size_t>(index < 0 ? 0 : index);
  landing.viewPosition = viewPosition;
  peerOf(handle)->driver.setLanding(landing);
}

void SHADOWLIST_KIT_WASM(nativeCancelLanding)(std::uintptr_t handle) {
  peerOf(handle)->driver.cancelLanding();
}

bool SHADOWLIST_KIT_WASM(nativeLand)(std::uintptr_t handle) {
  return peerOf(handle)->driver.land();
}

/*
 * Prefetch changes for the mounted rows, packed: the prefetch count, those rows, the cancel
 * count, those rows.
 */
const std::int32_t* SHADOWLIST_KIT_WASM(nativeUpdatePrefetch)(std::uintptr_t handle, std::int32_t low, std::int32_t high) {
  std::vector<std::size_t> prefetch;
  std::vector<std::size_t> cancel;
  peerOf(handle)->driver.updatePrefetch(indexFromInt(low), indexFromInt(high), prefetch, cancel);
  std::vector<std::int32_t> packed;
  packed.reserve(prefetch.size() + cancel.size() + 2);
  packed.push_back(static_cast<std::int32_t>(prefetch.size()));
  for (std::size_t row : prefetch) {
    packed.push_back(static_cast<std::int32_t>(row));
  }
  packed.push_back(static_cast<std::int32_t>(cancel.size()));
  for (std::size_t row : cancel) {
    packed.push_back(static_cast<std::int32_t>(row));
  }
  return writeInts(packed);
}

/*
 * The row at the viewport start: its key, with the distance into it and the key's length in the
 * ANCHOR_* slots of out, or null.
 */
const char* SHADOWLIST_KIT_WASM(nativeAnchor)(std::uintptr_t handle, double offset, double* out) {
  std::optional<sl::ListAnchor> anchor = peerOf(handle)->driver.getAnchor(offset);
  if (!anchor) {
    return nullptr;
  }
  stringResult = std::move(anchor->key);
  out[ANCHOR_OFFSET] = anchor->offset;
  out[ANCHOR_KEY_LENGTH] = static_cast<double>(stringResult.size());
  return stringResult.data();
}

bool SHADOWLIST_KIT_WASM(nativeRestoreAnchor)(std::uintptr_t handle, const char* key, std::int32_t length, double offset) {
  return peerOf(handle)->driver.restoreAnchor({readString(key, length), offset});
}

/*
 * The diff of two key lists, packed: the delete count and previous indices, the insert count
 * and next indices, the move count and from, to pairs.
 */
const std::int32_t* SHADOWLIST_KIT_WASM(nativeDiffKeys)(const char* previousPacked, const std::int32_t* previousEnds,
  std::int32_t previousCount, const char* nextPacked, const std::int32_t* nextEnds, std::int32_t nextCount) {
  sl::KeyDiff diff = sl::diffKeys(
    readPackedKeys(previousPacked, previousEnds, previousCount), readPackedKeys(nextPacked, nextEnds, nextCount));
  std::vector<std::int32_t> packed;
  packed.push_back(static_cast<std::int32_t>(diff.deleted.size()));
  for (std::size_t index : diff.deleted) {
    packed.push_back(static_cast<std::int32_t>(index));
  }
  packed.push_back(static_cast<std::int32_t>(diff.inserted.size()));
  for (std::size_t index : diff.inserted) {
    packed.push_back(static_cast<std::int32_t>(index));
  }
  packed.push_back(static_cast<std::int32_t>(diff.moved.size()));
  for (const sl::KeyMove& move : diff.moved) {
    packed.push_back(static_cast<std::int32_t>(move.from));
    packed.push_back(static_cast<std::int32_t>(move.to));
  }
  return writeInts(packed);
}

/*
 * The source of every next row, a previous index or -1 for an insert, or null for a batch that
 * does not add up.
 */
const std::int32_t* SHADOWLIST_KIT_WASM(nativePlanBatch)(std::int32_t previousCount, std::int32_t nextCount,
  const std::int32_t* deleted, std::int32_t deletedCount, const std::int32_t* inserted, std::int32_t insertedCount,
  const std::int32_t* movedFrom, const std::int32_t* movedTo, std::int32_t movedCount) {
  sl::BatchUpdate batch;
  batch.deleted = readIndices(deleted, deletedCount);
  batch.inserted = readIndices(inserted, insertedCount);
  std::vector<std::int32_t> from = readInts(movedFrom, movedCount);
  std::vector<std::int32_t> to = readInts(movedTo, movedCount);
  for (std::size_t at = 0; at < from.size() && at < to.size(); ++at) {
    if (from[at] < 0 || to[at] < 0) {
      return nullptr;
    }
    batch.moved.push_back({static_cast<std::size_t>(from[at]), static_cast<std::size_t>(to[at])});
  }
  std::optional<sl::BatchPlan> plan = sl::planBatch(static_cast<std::size_t>(std::max(previousCount, 0)),
    static_cast<std::size_t>(std::max(nextCount, 0)), batch);
  if (!plan) {
    return nullptr;
  }
  return writeInts(intsFromIndices(plan->sources));
}

const std::int32_t* SHADOWLIST_KIT_WASM(nativeInsertionPositions)(
  const std::int32_t* indices, std::int32_t count, std::int32_t previousCount) {
  std::size_t previous = static_cast<std::size_t>(std::max(previousCount, 0));
  return writeInts(intsFromIndices(sl::insertionPositions(readIndices(indices, count), previous)));
}

const std::int32_t* SHADOWLIST_KIT_WASM(nativeDeletionPositions)(
  const std::int32_t* indices, std::int32_t count, std::int32_t previousCount) {
  std::size_t previous = static_cast<std::size_t>(std::max(previousCount, 0));
  return writeInts(intsFromIndices(sl::deletionPositions(readIndices(indices, count), previous)));
}

}
