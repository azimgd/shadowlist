/*
 * JNI side of com.shadowlist.kit.SLKCore. Every call is a thin hop into the core's ListDriver
 * on the UI thread. Arrays the Kotlin side reuses carry values in and out. A layout allocates
 * nothing.
 */

#include <jni.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListSections.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>
#include <shadowlist-core/host/SwipeReveal.hpp>

namespace {

namespace sl = azimgd::shadowlist;

/*
 * Slots of SLKCore.PASS_*. The first block is what the view reports, the second what it applies.
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
 * The driver and the Java object it measures through. The env and target are only valid
 * during runPasses, the one call that measures.
 */
struct Peer {
  sl::ListDriver driver;
  sl::ListSections sections;
  JNIEnv* env = nullptr;
  jobject target = nullptr;
  jmethodID measureItem = nullptr;
};

Peer* peerOf(jlong handle) {
  return reinterpret_cast<Peer*>(handle);
}

std::vector<jint> readInts(JNIEnv* env, jintArray array) {
  if (array == nullptr) {
    return {};
  }
  jsize length = env->GetArrayLength(array);
  std::vector<jint> values(static_cast<std::size_t>(length));
  env->GetIntArrayRegion(array, 0, length, values.data());
  return values;
}

/*
 * The indices without negative values, which no row has.
 */
std::vector<std::size_t> readIndices(JNIEnv* env, jintArray array) {
  std::vector<std::size_t> indices;
  for (jint value : readInts(env, array)) {
    if (value >= 0) {
      indices.push_back(static_cast<std::size_t>(value));
    }
  }
  return indices;
}

std::string readString(JNIEnv* env, jstring value) {
  if (value == nullptr) {
    return {};
  }
  const char* chars = env->GetStringUTFChars(value, nullptr);
  std::string result(chars);
  env->ReleaseStringUTFChars(value, chars);
  return result;
}

/*
 * Append one UTF-16 unit the way GetStringUTFChars encodes it, modified UTF-8: NUL as two
 * bytes and each half of a surrogate pair on its own. Keys read either way then match.
 */
void appendModifiedUtf8(std::string& out, jchar unit) {
  if (unit != 0 && unit < 0x80) {
    out.push_back(static_cast<char>(unit));
  } else if (unit < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (unit >> 6)));
    out.push_back(static_cast<char>(0x80 | (unit & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xE0 | (unit >> 12)));
    out.push_back(static_cast<char>(0x80 | ((unit >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (unit & 0x3F)));
  }
}

/*
 * Keys joined into one char array, with where each one ends. See SLKCore.pack.
 */
std::vector<std::string> readPackedKeys(JNIEnv* env, jcharArray packed, jintArray ends) {
  std::vector<jint> bounds = readInts(env, ends);
  std::vector<std::string> keys;
  if (packed == nullptr || bounds.empty()) {
    return keys;
  }
  jsize length = env->GetArrayLength(packed);
  keys.reserve(bounds.size());
  auto* chars = static_cast<const jchar*>(env->GetPrimitiveArrayCritical(packed, nullptr));
  jint start = 0;
  for (jint end : bounds) {
    end = std::min(std::max(end, start), static_cast<jint>(length));
    std::string& key = keys.emplace_back();
    key.reserve(static_cast<std::size_t>(end - start));
    for (jint at = start; at < end; ++at) {
      appendModifiedUtf8(key, chars[at]);
    }
    start = end;
  }
  env->ReleasePrimitiveArrayCritical(packed, const_cast<jchar*>(chars), JNI_ABORT);
  return keys;
}

sl::PassInput readPassInput(const jdouble* slots) {
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

void writePassResult(const sl::ListDriver& driver, const sl::PassResult& result, jdouble* slots) {
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

void writeRect(JNIEnv* env, const sl::RowRect& rect, jdoubleArray out) {
  jdouble values[4] = {rect.x, rect.y, rect.width, rect.height};
  env->SetDoubleArrayRegion(out, 0, 4, values);
}

void writeDragOffset(JNIEnv* env, const sl::DragOffset& offset, jdoubleArray out) {
  jdouble values[2] = {offset.leading, offset.cross};
  env->SetDoubleArrayRegion(out, 0, 2, values);
}

bool validRow(const sl::ListDriver& driver, jint index) {
  return index >= 0 && static_cast<std::size_t>(index) < driver.getCount();
}

/*
 * Slots of SLKCore.SWIPE_*: the spec of a swiped row, then where a released swipe rests.
 */
enum SwipeSlot {
  SWIPE_LEADING_WIDTH = 0,
  SWIPE_TRAILING_WIDTH,
  SWIPE_LEADING_FULL,
  SWIPE_TRAILING_FULL,
  SWIPE_ROW_SIZE,
  SWIPE_OUT_SIDE,
  SWIPE_OUT_FULL,
  SWIPE_OUT_OFFSET,
  SWIPE_SLOTS,
};

sl::SwipeSpec readSwipeSpec(const jdouble* slots) {
  sl::SwipeSpec spec;
  spec.leadingWidth = slots[SWIPE_LEADING_WIDTH];
  spec.trailingWidth = slots[SWIPE_TRAILING_WIDTH];
  spec.leadingFullSwipe = slots[SWIPE_LEADING_FULL] != 0.0;
  spec.trailingFullSwipe = slots[SWIPE_TRAILING_FULL] != 0.0;
  spec.rowSize = slots[SWIPE_ROW_SIZE];
  return spec;
}

jintArray makeIntArray(JNIEnv* env, const std::vector<jint>& values) {
  jintArray array = env->NewIntArray(static_cast<jsize>(values.size()));
  if (array != nullptr && !values.empty()) {
    env->SetIntArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
  }
  return array;
}

/*
 * Kotlin carries a missing index as -1, the core as UNDEFINED_INDEX.
 */
jint jintFromIndex(std::size_t index) {
  return index == sl::UNDEFINED_INDEX ? -1 : static_cast<jint>(index);
}

std::size_t indexFromJint(jint index) {
  return index < 0 ? sl::UNDEFINED_INDEX : static_cast<std::size_t>(index);
}

}

#define SLK_JNI(name) Java_com_shadowlist_kit_SLKCore_##name

extern "C" {

JNIEXPORT jlong JNICALL SLK_JNI(nativeCreate)(JNIEnv* env, jobject thiz) {
  auto* peer = new Peer();
  jclass clazz = env->GetObjectClass(thiz);
  peer->measureItem = env->GetMethodID(clazz, "measureItem", "(ID)D");
  peer->driver.setMeasureItem([peer](std::size_t index, const std::string&, double cross) {
    if (peer->env == nullptr || peer->target == nullptr) {
      return 0.0;
    }
    return static_cast<double>(
      peer->env->CallDoubleMethod(peer->target, peer->measureItem, static_cast<jint>(index), cross));
  });
  return reinterpret_cast<jlong>(peer);
}

JNIEXPORT void JNICALL SLK_JNI(nativeDestroy)(JNIEnv*, jclass, jlong handle) {
  delete peerOf(handle);
}

JNIEXPORT void JNICALL SLK_JNI(nativeSetSettings)(JNIEnv*, jclass, jlong handle, jdouble estimatedItemSize,
  jdouble overscan, jdouble startReachedThreshold, jdouble endReachedThreshold, jint columns, jboolean inverted,
  jboolean followAppends, jboolean horizontal, jboolean snapToItem, jint snapAlignment) {
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

JNIEXPORT void JNICALL SLK_JNI(nativeSetStickyIndices)(JNIEnv* env, jclass, jlong handle, jintArray indices) {
  peerOf(handle)->driver.setStickyIndices(readIndices(env, indices));
}

JNIEXPORT void JNICALL SLK_JNI(nativeReplaceKeys)(
  JNIEnv* env, jclass, jlong handle, jint start, jint count, jcharArray packed, jintArray ends) {
  peerOf(handle)->driver.replaceKeys(static_cast<std::size_t>(std::max(start, 0)),
    static_cast<std::size_t>(std::max(count, 0)), readPackedKeys(env, packed, ends));
}

/*
 * A negative index drops its key with it, which keeps the two lists paired.
 */
JNIEXPORT void JNICALL SLK_JNI(nativeInsertKeys)(
  JNIEnv* env, jclass, jlong handle, jintArray indices, jcharArray packed, jintArray ends) {
  std::vector<jint> raw = readInts(env, indices);
  std::vector<std::string> strings = readPackedKeys(env, packed, ends);
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

JNIEXPORT void JNICALL SLK_JNI(nativeDeleteKeys)(JNIEnv* env, jclass, jlong handle, jintArray indices) {
  peerOf(handle)->driver.deleteKeys(readIndices(env, indices));
}

JNIEXPORT void JNICALL SLK_JNI(nativeMarkRemeasure)(JNIEnv* env, jclass, jlong handle, jintArray indices) {
  peerOf(handle)->driver.markRemeasure(readIndices(env, indices));
}

JNIEXPORT void JNICALL SLK_JNI(nativeRunPasses)(JNIEnv* env, jobject thiz, jlong handle, jdoubleArray io) {
  Peer* peer = peerOf(handle);
  jdouble slots[PASS_SLOTS];
  env->GetDoubleArrayRegion(io, 0, PASS_SLOTS, slots);
  peer->env = env;
  peer->target = thiz;
  sl::PassResult result = peer->driver.runPasses(readPassInput(slots));
  peer->env = nullptr;
  peer->target = nullptr;
  writePassResult(peer->driver, result, slots);
  env->SetDoubleArrayRegion(io, PASS_OUT_OFFSET, PASS_SLOTS - PASS_OUT_OFFSET, slots + PASS_OUT_OFFSET);
}

JNIEXPORT void JNICALL SLK_JNI(nativeResetKeepingPosition)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->driver.resetKeepingPosition();
}

JNIEXPORT jint JNICALL SLK_JNI(nativeCount)(JNIEnv*, jclass, jlong handle) {
  return static_cast<jint>(peerOf(handle)->driver.getCount());
}

/*
 * Frames of rows low to high, four values each, x y width height.
 */
JNIEXPORT void JNICALL SLK_JNI(nativeCopyRowRects)(
  JNIEnv* env, jclass, jlong handle, jint low, jint high, jdoubleArray out) {
  const sl::ListDriver& driver = peerOf(handle)->driver;
  if (!validRow(driver, low) || !validRow(driver, high) || high < low) {
    return;
  }
  std::vector<jdouble> values(static_cast<std::size_t>(high - low + 1) * 4);
  for (jint index = low; index <= high; ++index) {
    sl::RowRect rect = driver.getRowRect(static_cast<std::size_t>(index));
    std::size_t at = static_cast<std::size_t>(index - low) * 4;
    values[at] = rect.x;
    values[at + 1] = rect.y;
    values[at + 2] = rect.width;
    values[at + 3] = rect.height;
  }
  env->SetDoubleArrayRegion(out, 0, static_cast<jsize>(values.size()), values.data());
}

JNIEXPORT jboolean JNICALL SLK_JNI(nativeRowRect)(JNIEnv* env, jclass, jlong handle, jint index, jdoubleArray out) {
  const sl::ListDriver& driver = peerOf(handle)->driver;
  if (!validRow(driver, index)) {
    return JNI_FALSE;
  }
  writeRect(env, driver.getRowRect(static_cast<std::size_t>(index)), out);
  return JNI_TRUE;
}

JNIEXPORT jdouble JNICALL SLK_JNI(nativeFooterStart)(JNIEnv*, jclass, jlong handle, jdouble footerSize) {
  return peerOf(handle)->driver.getFooterStart(footerSize);
}

/*
 * The visible rows packed as low in the high 32 bits and high in the low ones, or -1.
 */
JNIEXPORT jlong JNICALL SLK_JNI(nativeVisibleRange)(JNIEnv*, jclass, jlong handle) {
  std::optional<sl::MountedRange> visible = peerOf(handle)->driver.getVisibleRange();
  if (!visible) {
    return -1;
  }
  return (static_cast<jlong>(visible->low) << 32) | static_cast<jlong>(visible->high);
}

JNIEXPORT jint JNICALL SLK_JNI(nativeIndexOfKey)(JNIEnv* env, jclass, jlong handle, jstring key) {
  return jintFromIndex(peerOf(handle)->driver.indexOfKey(readString(env, key)));
}

/*
 * Leading edge and extent of every sticky row, two values each. Returns false when out is too short.
 */
JNIEXPORT jboolean JNICALL SLK_JNI(nativeCopyStickyFrames)(JNIEnv* env, jclass, jlong handle, jdoubleArray out) {
  std::vector<double> frames;
  peerOf(handle)->driver.stickyFrames(frames);
  if (env->GetArrayLength(out) < static_cast<jsize>(frames.size())) {
    return JNI_FALSE;
  }
  env->SetDoubleArrayRegion(out, 0, static_cast<jsize>(frames.size()), frames.data());
  return JNI_TRUE;
}

JNIEXPORT void JNICALL SLK_JNI(nativeScrollToIndex)(
  JNIEnv*, jclass, jlong handle, jint index, jdouble viewPosition) {
  if (index >= 0) {
    peerOf(handle)->driver.scrollToIndex(static_cast<std::size_t>(index), viewPosition);
  }
}

JNIEXPORT void JNICALL SLK_JNI(nativeScrollToStart)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->driver.scrollToStart();
}

JNIEXPORT void JNICALL SLK_JNI(nativeScrollToEnd)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->driver.scrollToEnd();
}

JNIEXPORT jdouble JNICALL SLK_JNI(nativeNearestSnapOffset)(JNIEnv*, jclass, jlong handle, jdouble target) {
  return peerOf(handle)->driver.nearestSnapOffset(target);
}

/*
 * Where an animated scroll to a row aims, or NaN for a row the core has not placed yet.
 */
JNIEXPORT jdouble JNICALL SLK_JNI(nativeAnimatedTargetOffset)(
  JNIEnv*, jclass, jlong handle, jint index, jdouble viewPosition, jdouble windowAlong, jdouble maxOffset) {
  const sl::ListDriver& driver = peerOf(handle)->driver;
  if (!validRow(driver, index)) {
    return std::nan("");
  }
  return driver.animatedTargetOffset(static_cast<std::size_t>(index), viewPosition, windowAlong, maxOffset);
}

/*
 * Target is SLKCore.LANDING_*, the same order as ScrollLanding::Target.
 */
JNIEXPORT void JNICALL SLK_JNI(nativeSetLanding)(
  JNIEnv*, jclass, jlong handle, jint target, jint index, jdouble viewPosition) {
  sl::ScrollLanding landing;
  landing.target = static_cast<sl::ScrollLanding::Target>(target);
  landing.index = static_cast<std::size_t>(index < 0 ? 0 : index);
  landing.viewPosition = viewPosition;
  peerOf(handle)->driver.setLanding(landing);
}

JNIEXPORT void JNICALL SLK_JNI(nativeCancelLanding)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->driver.cancelLanding();
}

JNIEXPORT jboolean JNICALL SLK_JNI(nativeLand)(JNIEnv*, jclass, jlong handle) {
  return peerOf(handle)->driver.land() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL SLK_JNI(nativeDragBegin)(
  JNIEnv*, jclass, jlong handle, jint index, jdouble touchAlong, jdouble touchCross) {
  if (index >= 0) {
    peerOf(handle)->driver.dragBegin(static_cast<std::size_t>(index), touchAlong, touchCross);
  }
}

JNIEXPORT void JNICALL SLK_JNI(nativeDragEnd)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->driver.dragEnd();
}

JNIEXPORT jint JNICALL SLK_JNI(nativeHeldIndex)(JNIEnv*, jclass, jlong handle) {
  return jintFromIndex(peerOf(handle)->driver.getHeldIndex());
}

JNIEXPORT void JNICALL SLK_JNI(nativePlaceHeld)(
  JNIEnv* env, jclass, jlong handle, jint held, jdouble touchAlong, jdouble touchCross, jdoubleArray out) {
  sl::ListDriver& driver = peerOf(handle)->driver;
  sl::DragOffset offset;
  if (validRow(driver, held)) {
    offset = driver.placeHeld(static_cast<std::size_t>(held), touchAlong, touchCross);
  }
  writeDragOffset(env, offset, out);
}

JNIEXPORT void JNICALL SLK_JNI(nativeDragUpdateInsertion)(JNIEnv* env, jclass, jlong handle, jintArray mounted) {
  peerOf(handle)->driver.dragUpdateInsertion(readIndices(env, mounted));
}

JNIEXPORT void JNICALL SLK_JNI(nativeDragShiftFor)(
  JNIEnv* env, jclass, jlong handle, jint index, jdoubleArray out) {
  writeDragOffset(env, peerOf(handle)->driver.dragShiftFor(indexFromJint(index)), out);
}

JNIEXPORT jint JNICALL SLK_JNI(nativeDragOriginIndex)(JNIEnv*, jclass, jlong handle) {
  return jintFromIndex(peerOf(handle)->driver.getDragOriginIndex());
}

JNIEXPORT jint JNICALL SLK_JNI(nativeDragInsertionIndex)(JNIEnv*, jclass, jlong handle) {
  return jintFromIndex(peerOf(handle)->driver.getDragInsertionIndex());
}

JNIEXPORT jdouble JNICALL SLK_JNI(nativeDragAutoScrollOffset)(
  JNIEnv*, jclass, jdouble touch, jdouble windowSize, jdouble offset, jdouble maxOffset, jdouble density) {
  sl::DragAutoScrollConfig config{sl::DRAG_AUTO_SCROLL_ANDROID.edge * density,
    sl::DRAG_AUTO_SCROLL_ANDROID.maxSpeed * density};
  return sl::dragAutoScrollOffset(config, touch, windowSize, offset, maxOffset);
}


/*
 * Sections over the rows. counts has each section's item count, flags its SECTION_* bits.
 * Without counts the list has no sections and count items.
 */
JNIEXPORT void JNICALL SLK_JNI(nativeSetSections)(
  JNIEnv* env, jclass, jlong handle, jint count, jintArray counts, jintArray flags) {
  Peer* peer = peerOf(handle);
  if (counts == nullptr) {
    peer->sections.setPlain(static_cast<std::size_t>(std::max(count, 0)));
    return;
  }
  std::vector<jint> itemCounts = readInts(env, counts);
  std::vector<jint> bits = readInts(env, flags);
  std::vector<sl::SectionSpec> specs(itemCounts.size());
  for (std::size_t section = 0; section < specs.size(); ++section) {
    specs[section].itemCount = static_cast<std::size_t>(std::max(itemCounts[section], 0));
    jint bit = section < bits.size() ? bits[section] : 0;
    specs[section].hasHeader = (bit & 1) != 0;
    specs[section].hasFooter = (bit & 2) != 0;
  }
  peer->sections.setSections(std::move(specs));
}

/*
 * The item of every row, -1 for headers and footers, into items, and whether a separator
 * follows the row into separators.
 */
JNIEXPORT void JNICALL SLK_JNI(nativeCopyRows)(
  JNIEnv* env, jclass, jlong handle, jintArray items, jbooleanArray separators) {
  const sl::ListSections& sections = peerOf(handle)->sections;
  std::size_t count = sections.getRowCount();
  std::vector<jint> rowItems(count);
  std::vector<jboolean> rowSeparators(count);
  for (std::size_t row = 0; row < count; ++row) {
    rowItems[row] = jintFromIndex(sections.itemForRow(row));
    rowSeparators[row] = sections.isItemBeforeItem(row) ? JNI_TRUE : JNI_FALSE;
  }
  env->SetIntArrayRegion(items, 0, std::min(env->GetArrayLength(items), static_cast<jsize>(count)), rowItems.data());
  env->SetBooleanArrayRegion(separators, 0, std::min(env->GetArrayLength(separators), static_cast<jsize>(count)),
    rowSeparators.data());
}

JNIEXPORT jint JNICALL SLK_JNI(nativeItemForDrop)(JNIEnv*, jclass, jlong handle, jint fromRow, jint toRow) {
  if (fromRow < 0 || toRow < 0) {
    return -1;
  }
  return jintFromIndex(peerOf(handle)->sections.itemForDrop(static_cast<std::size_t>(fromRow), static_cast<std::size_t>(toRow)));
}

JNIEXPORT jint JNICALL SLK_JNI(nativeRowForItem)(JNIEnv*, jclass, jlong handle, jint item) {
  return item < 0 ? -1 : jintFromIndex(peerOf(handle)->sections.rowForItem(static_cast<std::size_t>(item)));
}

JNIEXPORT jint JNICALL SLK_JNI(nativeSectionForItem)(JNIEnv*, jclass, jlong handle, jint item) {
  return item < 0 ? -1 : jintFromIndex(peerOf(handle)->sections.sectionForItem(static_cast<std::size_t>(item)));
}

JNIEXPORT jint JNICALL SLK_JNI(nativeFirstItemInSection)(JNIEnv*, jclass, jlong handle, jint section) {
  return section < 0 ? -1 : jintFromIndex(peerOf(handle)->sections.firstItemInSection(static_cast<std::size_t>(section)));
}

JNIEXPORT jint JNICALL SLK_JNI(nativeHeaderRow)(JNIEnv*, jclass, jlong handle, jint section) {
  return section < 0 ? -1 : jintFromIndex(peerOf(handle)->sections.headerRow(static_cast<std::size_t>(section)));
}

JNIEXPORT jint JNICALL SLK_JNI(nativeFirstRowInSection)(JNIEnv*, jclass, jlong handle, jint section) {
  return section < 0 ? -1 : jintFromIndex(peerOf(handle)->sections.firstRowInSection(static_cast<std::size_t>(section)));
}

/*
 * Section and kind of a row: section shl 2 or kind, kind 0 item, 1 header, 2 footer, or -1.
 */
JNIEXPORT jint JNICALL SLK_JNI(nativePlaceOfRow)(JNIEnv*, jclass, jlong handle, jint row) {
  const sl::ListSections& sections = peerOf(handle)->sections;
  if (row < 0 || static_cast<std::size_t>(row) >= sections.getRowCount()) {
    return -1;
  }
  sl::RowPlace place = sections.placeOfRow(static_cast<std::size_t>(row));
  jint kind = place.kind == sl::RowKind::Header ? 1 : place.kind == sl::RowKind::Footer ? 2 : 0;
  return (static_cast<jint>(place.section) << 2) | kind;
}

JNIEXPORT jintArray JNICALL SLK_JNI(nativeHeaderRows)(JNIEnv* env, jclass, jlong handle) {
  std::vector<jint> rows;
  for (std::size_t row : peerOf(handle)->sections.headerRows()) {
    rows.push_back(static_cast<jint>(row));
  }
  return makeIntArray(env, rows);
}

/*
 * Prefetch changes for the mounted rows, packed: the prefetch count, those rows, the cancel
 * count, those rows.
 */
JNIEXPORT jintArray JNICALL SLK_JNI(nativeUpdatePrefetch)(JNIEnv* env, jclass, jlong handle, jint low, jint high) {
  std::vector<std::size_t> prefetch;
  std::vector<std::size_t> cancel;
  peerOf(handle)->driver.updatePrefetch(indexFromJint(low), indexFromJint(high), prefetch, cancel);
  std::vector<jint> packed;
  packed.reserve(prefetch.size() + cancel.size() + 2);
  packed.push_back(static_cast<jint>(prefetch.size()));
  for (std::size_t row : prefetch) {
    packed.push_back(static_cast<jint>(row));
  }
  packed.push_back(static_cast<jint>(cancel.size()));
  for (std::size_t row : cancel) {
    packed.push_back(static_cast<jint>(row));
  }
  return makeIntArray(env, packed);
}

/*
 * The row at the viewport start: its key, with the distance into it in out[0], or null.
 */
JNIEXPORT jstring JNICALL SLK_JNI(nativeAnchor)(JNIEnv* env, jclass, jlong handle, jdouble offset, jdoubleArray out) {
  std::optional<sl::ListAnchor> anchor = peerOf(handle)->driver.getAnchor(offset);
  if (!anchor) {
    return nullptr;
  }
  jdouble distance = anchor->offset;
  env->SetDoubleArrayRegion(out, 0, 1, &distance);
  return env->NewStringUTF(anchor->key.c_str());
}

JNIEXPORT jboolean JNICALL SLK_JNI(nativeRestoreAnchor)(JNIEnv* env, jclass, jlong handle, jstring key, jdouble offset) {
  return peerOf(handle)->driver.restoreAnchor({readString(env, key), offset}) ? JNI_TRUE : JNI_FALSE;
}

/*
 * The diff of two key lists, packed: the delete count and previous indices, the insert count
 * and next indices, the move count and from, to pairs.
 */
JNIEXPORT jintArray JNICALL SLK_JNI(nativeDiffKeys)(JNIEnv* env, jclass, jcharArray previousPacked,
  jintArray previousEnds, jcharArray nextPacked, jintArray nextEnds) {
  sl::KeyDiff diff = sl::diffKeys(readPackedKeys(env, previousPacked, previousEnds), readPackedKeys(env, nextPacked, nextEnds));
  std::vector<jint> packed;
  packed.push_back(static_cast<jint>(diff.deleted.size()));
  for (std::size_t index : diff.deleted) {
    packed.push_back(static_cast<jint>(index));
  }
  packed.push_back(static_cast<jint>(diff.inserted.size()));
  for (std::size_t index : diff.inserted) {
    packed.push_back(static_cast<jint>(index));
  }
  packed.push_back(static_cast<jint>(diff.moved.size()));
  for (const sl::KeyMove& move : diff.moved) {
    packed.push_back(static_cast<jint>(move.from));
    packed.push_back(static_cast<jint>(move.to));
  }
  return makeIntArray(env, packed);
}

/*
 * The source of every next row, a previous index or -1 for an insert, or null for a batch that
 * does not add up.
 */
JNIEXPORT jintArray JNICALL SLK_JNI(nativePlanBatch)(JNIEnv* env, jclass, jint previousCount, jint nextCount,
  jintArray deleted, jintArray inserted, jintArray movedFrom, jintArray movedTo) {
  sl::BatchUpdate batch;
  batch.deleted = readIndices(env, deleted);
  batch.inserted = readIndices(env, inserted);
  std::vector<jint> from = readInts(env, movedFrom);
  std::vector<jint> to = readInts(env, movedTo);
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
  std::vector<jint> sources;
  sources.reserve(plan->sources.size());
  for (std::size_t source : plan->sources) {
    sources.push_back(jintFromIndex(source));
  }
  return makeIntArray(env, sources);
}

JNIEXPORT jintArray JNICALL SLK_JNI(nativeInsertionPositions)(
  JNIEnv* env, jclass, jintArray indices, jint previousCount) {
  std::size_t count = static_cast<std::size_t>(std::max(previousCount, 0));
  std::vector<jint> positions;
  for (std::size_t position : sl::insertionPositions(readIndices(env, indices), count)) {
    positions.push_back(static_cast<jint>(position));
  }
  return makeIntArray(env, positions);
}

JNIEXPORT jintArray JNICALL SLK_JNI(nativeDeletionPositions)(
  JNIEnv* env, jclass, jintArray indices, jint previousCount) {
  std::size_t count = static_cast<std::size_t>(std::max(previousCount, 0));
  std::vector<jint> positions;
  for (std::size_t position : sl::deletionPositions(readIndices(env, indices), count)) {
    positions.push_back(static_cast<jint>(position));
  }
  return makeIntArray(env, positions);
}

/*
 * The offset of a swiped row for a finger that moved translation since it began at start.
 */
JNIEXPORT jdouble JNICALL SLK_JNI(nativeSwipeDrag)(
  JNIEnv* env, jclass, jdoubleArray io, jdouble start, jdouble translation) {
  jdouble slots[SWIPE_SLOTS];
  env->GetDoubleArrayRegion(io, 0, SWIPE_SLOTS, slots);
  sl::SwipeReveal swipe;
  swipe.begin(readSwipeSpec(slots), start);
  return swipe.drag(translation);
}

JNIEXPORT jboolean JNICALL SLK_JNI(nativeSwipePastFull)(JNIEnv* env, jclass, jdoubleArray io, jdouble offset) {
  jdouble slots[SWIPE_SLOTS];
  env->GetDoubleArrayRegion(io, 0, SWIPE_SLOTS, slots);
  sl::SwipeReveal swipe;
  swipe.begin(readSwipeSpec(slots), 0.0);
  return swipe.isPastFullSwipe(offset) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Where a swipe let go at offset rests, into the SWIPE_OUT_* slots. Side is 0 none, 1 leading,
 * 2 trailing.
 */
JNIEXPORT void JNICALL SLK_JNI(nativeSwipeSettle)(
  JNIEnv* env, jclass, jdoubleArray io, jdouble offset, jdouble velocity, jdouble flingVelocity) {
  jdouble slots[SWIPE_SLOTS];
  env->GetDoubleArrayRegion(io, 0, SWIPE_SLOTS, slots);
  sl::SwipeReveal swipe;
  swipe.begin(readSwipeSpec(slots), 0.0);
  sl::SwipeRest rest = swipe.settle(offset, velocity, flingVelocity);
  slots[SWIPE_OUT_SIDE] = rest.side == sl::SwipeSide::Leading ? 1.0 : rest.side == sl::SwipeSide::Trailing ? 2.0 : 0.0;
  slots[SWIPE_OUT_FULL] = rest.full ? 1.0 : 0.0;
  slots[SWIPE_OUT_OFFSET] = rest.offset;
  env->SetDoubleArrayRegion(io, SWIPE_OUT_SIDE, SWIPE_SLOTS - SWIPE_OUT_SIDE, slots + SWIPE_OUT_SIDE);
}

}
