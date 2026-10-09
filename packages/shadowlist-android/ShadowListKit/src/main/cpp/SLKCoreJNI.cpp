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

#include <shadowlist-core/host/ChangeAnimation.hpp>
#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListSections.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>
#include <shadowlist-core/host/ScrollTarget.hpp>
#include <shadowlist-core/host/SectionIndex.hpp>
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
  sl::ChangeAnimation changes;
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
  if (chars == nullptr) {
    return {};
  }
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
  if (chars == nullptr) {
    return {};
  }
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

/*
 * Values of SWIPE_OUT_SIDE, SLKCore.SWIPE_SIDE_* in Kotlin.
 */
enum SwipeSideValue {
  SWIPE_SIDE_NONE = 0,
  SWIPE_SIDE_LEADING,
  SWIPE_SIDE_TRAILING,
};

/*
 * Bits of a section's flags in nativeSetSections, SLKCore.SECTION_* in Kotlin.
 */
enum SectionFlag {
  SECTION_HEADER = 1,
  SECTION_FOOTER = 2,
};

/*
 * Kinds of a row in nativePlaceOfRow and the low bits they take, SLKCore.ROW_* in Kotlin.
 */
enum RowKindValue {
  ROW_ITEM = 0,
  ROW_HEADER,
  ROW_FOOTER,
  ROW_KIND_BITS = 2,
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
 * Slots of a change animation step in SLKCore.runChange, CHANGE_STEP_* in Kotlin.
 */
enum ChangeStepSlot {
  CHANGE_STEP_KIND = 0,
  CHANGE_STEP_FROM_X,
  CHANGE_STEP_FROM_Y,
  CHANGE_STEP_SLOTS,
};

/*
 * Screen points as x, y pairs.
 */
std::vector<sl::ScreenPoint> readPoints(JNIEnv* env, jdoubleArray array) {
  std::vector<sl::ScreenPoint> points;
  if (array == nullptr) {
    return points;
  }
  jsize length = env->GetArrayLength(array);
  std::vector<jdouble> values(static_cast<std::size_t>(length));
  env->GetDoubleArrayRegion(array, 0, length, values.data());
  points.reserve(values.size() / 2);
  for (std::size_t at = 0; at + 1 < values.size(); at += 2) {
    points.push_back({values[at], values[at + 1]});
  }
  return points;
}

/*
 * Slots of SLKCore.constants, CONSTANT_* in Kotlin: kit constants of the core in dp at scale 1
 * and durations in milliseconds.
 */
enum ConstantSlot {
  CONSTANT_SWIPE_FLING_VELOCITY = 0,
  CONSTANT_SWIPE_SETTLE_DURATION_MS,
  CONSTANT_DRAG_LIFT_SCALE,
  CONSTANT_DRAG_LIFT_DURATION_MS,
  CONSTANT_DRAG_SHIFT_DURATION_MS,
  CONSTANT_DRAG_DROP_DURATION_MS,
  CONSTANT_SECTION_INDEX_TITLE_HEIGHT,
  CONSTANT_SECTION_INDEX_WIDTH,
  CONSTANT_SLOTS,
};

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
  env->DeleteLocalRef(clazz);
  peer->driver.setMeasureItem([peer](std::size_t index, const std::string&, double cross) {
    // A measure that threw leaves its exception pending. No JNI call may follow until runPasses returns.
    if (peer->env == nullptr || peer->target == nullptr || peer->env->ExceptionCheck()) {
      return 0.0;
    }
    double size = peer->env->CallDoubleMethod(peer->target, peer->measureItem, static_cast<jint>(index), cross);
    return peer->env->ExceptionCheck() ? 0.0 : size;
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
  // The exception thrown by a measure reaches Kotlin with the outputs left as they were.
  if (env->ExceptionCheck()) {
    return;
  }
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
 * The items among the visible rows packed as low in the high 32 bits and high in the low ones,
 * or -1.
 */
JNIEXPORT jlong JNICALL SLK_JNI(nativeVisibleItemRange)(JNIEnv*, jclass, jlong handle) {
  Peer* peer = peerOf(handle);
  std::optional<sl::MountedRange> visible = peer->driver.getVisibleRange();
  std::optional<sl::MountedRange> items =
    visible ? peer->sections.itemRangeOfRows(visible->low, visible->high) : std::nullopt;
  if (!items) {
    return -1;
  }
  return (static_cast<jlong>(items->low) << 32) | static_cast<jlong>(items->high);
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
    specs[section].hasHeader = (bit & SECTION_HEADER) != 0;
    specs[section].hasFooter = (bit & SECTION_FOOTER) != 0;
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
  return jintFromIndex(
    peerOf(handle)->sections.itemForDrop(static_cast<std::size_t>(fromRow), static_cast<std::size_t>(toRow)));
}

JNIEXPORT jint JNICALL SLK_JNI(nativeRowForItem)(JNIEnv*, jclass, jlong handle, jint item) {
  return item < 0 ? -1 : jintFromIndex(peerOf(handle)->sections.rowForItem(static_cast<std::size_t>(item)));
}

JNIEXPORT jint JNICALL SLK_JNI(nativeSectionForItem)(JNIEnv*, jclass, jlong handle, jint item) {
  return item < 0 ? -1 : jintFromIndex(peerOf(handle)->sections.sectionForItem(static_cast<std::size_t>(item)));
}

JNIEXPORT jint JNICALL SLK_JNI(nativeFirstItemInSection)(JNIEnv*, jclass, jlong handle, jint section) {
  return section < 0 ? -1
                     : jintFromIndex(peerOf(handle)->sections.firstItemInSection(static_cast<std::size_t>(section)));
}

JNIEXPORT jint JNICALL SLK_JNI(nativeHeaderRow)(JNIEnv*, jclass, jlong handle, jint section) {
  return section < 0 ? -1 : jintFromIndex(peerOf(handle)->sections.headerRow(static_cast<std::size_t>(section)));
}

JNIEXPORT jint JNICALL SLK_JNI(nativeFirstRowInSection)(JNIEnv*, jclass, jlong handle, jint section) {
  return section < 0 ? -1
                     : jintFromIndex(peerOf(handle)->sections.firstRowInSection(static_cast<std::size_t>(section)));
}

/*
 * Section and kind of a row: section shl ROW_KIND_BITS or a ROW_* kind, or -1.
 */
JNIEXPORT jint JNICALL SLK_JNI(nativePlaceOfRow)(JNIEnv*, jclass, jlong handle, jint row) {
  const sl::ListSections& sections = peerOf(handle)->sections;
  if (row < 0 || static_cast<std::size_t>(row) >= sections.getRowCount()) {
    return -1;
  }
  sl::RowPlace place = sections.placeOfRow(static_cast<std::size_t>(row));
  RowKindValue kind = place.kind == sl::RowKind::Header ? ROW_HEADER
    : place.kind == sl::RowKind::Footer                 ? ROW_FOOTER
                                                        : ROW_ITEM;
  return (static_cast<jint>(place.section) << ROW_KIND_BITS) | kind;
}

JNIEXPORT jintArray JNICALL SLK_JNI(nativeStickyRows)(
  JNIEnv* env, jclass, jlong handle, jintArray items, jboolean sectionHeaders) {
  std::vector<jint> rows;
  for (std::size_t row : peerOf(handle)->sections.stickyRows(readIndices(env, items), sectionHeaders)) {
    rows.push_back(static_cast<jint>(row));
  }
  return makeIntArray(env, rows);
}

/*
 * The keys of every header and footer row, in row order. A null section key takes the core's
 * default.
 */
JNIEXPORT jobjectArray JNICALL SLK_JNI(nativeEdgeRowKeys)(
  JNIEnv* env, jclass, jlong handle, jobjectArray sectionKeys, jobjectArray firstItemKeys) {
  jsize sectionCount = env->GetArrayLength(sectionKeys);
  std::vector<std::optional<std::string>> given(static_cast<std::size_t>(sectionCount));
  std::vector<std::string> firsts(static_cast<std::size_t>(sectionCount));
  for (jsize section = 0; section < sectionCount; ++section) {
    auto key = static_cast<jstring>(env->GetObjectArrayElement(sectionKeys, section));
    if (key != nullptr) {
      given[static_cast<std::size_t>(section)] = readString(env, key);
      env->DeleteLocalRef(key);
    }
    if (section < env->GetArrayLength(firstItemKeys)) {
      auto first = static_cast<jstring>(env->GetObjectArrayElement(firstItemKeys, section));
      firsts[static_cast<std::size_t>(section)] = readString(env, first);
      env->DeleteLocalRef(first);
    }
  }
  std::vector<std::string> edges = peerOf(handle)->sections.edgeRowKeys(given, firsts);
  jclass stringClass = env->FindClass("java/lang/String");
  jobjectArray array = env->NewObjectArray(static_cast<jsize>(edges.size()), stringClass, nullptr);
  for (std::size_t at = 0; array != nullptr && at < edges.size(); ++at) {
    jstring key = env->NewStringUTF(edges[at].c_str());
    if (key == nullptr) {
      break;
    }
    env->SetObjectArrayElement(array, static_cast<jsize>(at), key);
    env->DeleteLocalRef(key);
  }
  env->DeleteLocalRef(stringClass);
  return array;
}

/*
 * Prefetch changes for the mounted rows, as items, packed: the prefetch count, those items, the
 * cancel count, those items.
 */
JNIEXPORT jintArray JNICALL SLK_JNI(nativeUpdatePrefetch)(JNIEnv* env, jclass, jlong handle, jint low, jint high) {
  Peer* peer = peerOf(handle);
  std::vector<std::size_t> prefetchRows;
  std::vector<std::size_t> cancelRows;
  peer->driver.updatePrefetch(indexFromJint(low), indexFromJint(high), prefetchRows, cancelRows);
  std::vector<std::size_t> prefetch = peer->sections.itemsOfRows(prefetchRows);
  std::vector<std::size_t> cancel = peer->sections.itemsOfRows(cancelRows);
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

JNIEXPORT jboolean JNICALL SLK_JNI(nativeRestoreAnchor)(
  JNIEnv* env, jclass, jlong handle, jstring key, jdouble offset) {
  return peerOf(handle)->driver.restoreAnchor({readString(env, key), offset}) ? JNI_TRUE : JNI_FALSE;
}

/*
 * The diff of two key lists, packed: the delete count and previous indices, the insert count
 * and next indices, the move count and from, to pairs.
 */
JNIEXPORT jintArray JNICALL SLK_JNI(nativeDiffKeys)(JNIEnv* env, jclass, jcharArray previousPacked,
  jintArray previousEnds, jcharArray nextPacked, jintArray nextEnds) {
  sl::KeyDiff diff =
    sl::diffKeys(readPackedKeys(env, previousPacked, previousEnds), readPackedKeys(env, nextPacked, nextEnds));
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

/*
 * A change is about to reach the core. The mounted rows and where they show, x and y pairs,
 * are recorded when the change starts an animation.
 */
JNIEXPORT void JNICALL SLK_JNI(nativeCaptureChange)(JNIEnv* env, jclass, jlong handle, jcharArray removedPacked,
  jintArray removedEnds, jcharArray insertedPacked, jintArray insertedEnds, jcharArray mountedPacked,
  jintArray mountedEnds, jdoubleArray mountedPositions) {
  sl::ChangeAnimation& changes = peerOf(handle)->changes;
  std::vector<std::string> removed = readPackedKeys(env, removedPacked, removedEnds);
  std::vector<std::string> inserted = readPackedKeys(env, insertedPacked, insertedEnds);
  if (!changes.capture(removed, inserted)) {
    return;
  }
  std::vector<std::string> mounted = readPackedKeys(env, mountedPacked, mountedEnds);
  std::vector<sl::ScreenPoint> positions = readPoints(env, mountedPositions);
  for (std::size_t at = 0; at < mounted.size() && at < positions.size(); ++at) {
    changes.recordPosition(mounted[at], positions[at]);
  }
}

/*
 * Where a removed row showed before the change, x and y into out, or false when it does not
 * fade out.
 */
JNIEXPORT jboolean JNICALL SLK_JNI(nativeRemovedPosition)(
  JNIEnv* env, jclass, jlong handle, jstring key, jdoubleArray out) {
  std::optional<sl::ScreenPoint> position = peerOf(handle)->changes.removedPosition(readString(env, key));
  if (!position) {
    return JNI_FALSE;
  }
  jdouble values[2] = {position->x, position->y};
  env->SetDoubleArrayRegion(out, 0, 2, values);
  return JNI_TRUE;
}

/*
 * The animation of the rows mounted after the layout pass, CHANGE_STEP_SLOTS values per row,
 * or null without a pending change.
 */
JNIEXPORT jdoubleArray JNICALL SLK_JNI(nativeRunChange)(
  JNIEnv* env, jclass, jlong handle, jcharArray keysPacked, jintArray keysEnds, jdoubleArray positions) {
  sl::ChangeAnimation& changes = peerOf(handle)->changes;
  if (!changes.isPending()) {
    return nullptr;
  }
  std::vector<sl::ChangeStep> steps =
    changes.run(readPackedKeys(env, keysPacked, keysEnds), readPoints(env, positions));
  std::vector<jdouble> values(steps.size() * CHANGE_STEP_SLOTS);
  for (std::size_t at = 0; at < steps.size(); ++at) {
    jdouble* step = values.data() + at * CHANGE_STEP_SLOTS;
    step[CHANGE_STEP_KIND] = static_cast<jdouble>(static_cast<int>(steps[at].kind));
    step[CHANGE_STEP_FROM_X] = steps[at].fromX;
    step[CHANGE_STEP_FROM_Y] = steps[at].fromY;
  }
  jdoubleArray array = env->NewDoubleArray(static_cast<jsize>(values.size()));
  if (array != nullptr && !values.empty()) {
    env->SetDoubleArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
  }
  return array;
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

JNIEXPORT jdoubleArray JNICALL SLK_JNI(nativeConstants)(JNIEnv* env, jclass) {
  jdouble values[CONSTANT_SLOTS];
  values[CONSTANT_SWIPE_FLING_VELOCITY] = sl::SWIPE_FLING_VELOCITY;
  values[CONSTANT_SWIPE_SETTLE_DURATION_MS] = sl::SWIPE_SETTLE_DURATION_MS;
  values[CONSTANT_DRAG_LIFT_SCALE] = sl::DRAG_LIFT_SCALE;
  values[CONSTANT_DRAG_LIFT_DURATION_MS] = sl::DRAG_LIFT_DURATION_MS;
  values[CONSTANT_DRAG_SHIFT_DURATION_MS] = sl::DRAG_SHIFT_DURATION_MS;
  values[CONSTANT_DRAG_DROP_DURATION_MS] = sl::DRAG_DROP_DURATION_MS;
  values[CONSTANT_SECTION_INDEX_TITLE_HEIGHT] = sl::SECTION_INDEX_TITLE_HEIGHT;
  values[CONSTANT_SECTION_INDEX_WIDTH] = sl::SECTION_INDEX_WIDTH;
  jdoubleArray array = env->NewDoubleArray(CONSTANT_SLOTS);
  if (array != nullptr) {
    env->SetDoubleArrayRegion(array, 0, CONSTANT_SLOTS, values);
  }
  return array;
}

JNIEXPORT jdouble JNICALL SLK_JNI(nativePageScrollTarget)(
  JNIEnv*, jclass, jdouble offset, jdouble windowAlong, jdouble maxOffset, jint direction) {
  return sl::pageScrollTarget(offset, windowAlong, maxOffset, direction);
}

JNIEXPORT jdouble JNICALL SLK_JNI(nativeSectionIndexTitlesTop)(
  JNIEnv*, jclass, jdouble areaHeight, jint count, jdouble scale) {
  return sl::sectionIndexTitlesTop(areaHeight, static_cast<std::size_t>(std::max(count, 0)), scale);
}

JNIEXPORT jint JNICALL SLK_JNI(nativeSectionIndexTitleAt)(
  JNIEnv*, jclass, jdouble y, jdouble areaHeight, jint count, jdouble scale) {
  return static_cast<jint>(sl::sectionIndexTitleAt(y, areaHeight, static_cast<std::size_t>(std::max(count, 0)), scale));
}

JNIEXPORT jdouble JNICALL SLK_JNI(nativeSwipeButtonSize)(JNIEnv*, jclass, jdouble fitted, jdouble scale) {
  return sl::swipeButtonSize(fitted, scale);
}

/*
 * Where the revealed side's count buttons go, into out: the revealed span's start and size,
 * then each button's start and size.
 */
JNIEXPORT void JNICALL SLK_JNI(nativeSwipeButtonSpans)(JNIEnv* env, jclass, jdoubleArray sizes, jint count,
  jdouble offset, jboolean full, jdouble crossSize, jdoubleArray out) {
  jsize given = sizes == nullptr ? 0 : env->GetArrayLength(sizes);
  std::size_t buttons = static_cast<std::size_t>(std::clamp(count, 0, given));
  std::vector<double> values(buttons);
  if (buttons > 0) {
    env->GetDoubleArrayRegion(sizes, 0, static_cast<jsize>(buttons), values.data());
  }
  std::vector<sl::SwipeSpan> spans;
  sl::swipeButtonSpans(values, offset, full, crossSize, spans);
  sl::SwipeSpan revealed = sl::swipeRevealedSpan(offset, crossSize);
  std::vector<jdouble> packed;
  packed.reserve(2 + spans.size() * 2);
  packed.push_back(revealed.start);
  packed.push_back(revealed.size);
  for (const sl::SwipeSpan& span : spans) {
    packed.push_back(span.start);
    packed.push_back(span.size);
  }
  jsize length = std::min(env->GetArrayLength(out), static_cast<jsize>(packed.size()));
  env->SetDoubleArrayRegion(out, 0, length, packed.data());
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

JNIEXPORT jboolean JNICALL SLK_JNI(nativeSwipeIsOut)(JNIEnv* env, jclass, jdoubleArray io, jdouble offset) {
  jdouble slots[SWIPE_SLOTS];
  env->GetDoubleArrayRegion(io, 0, SWIPE_SLOTS, slots);
  sl::SwipeReveal swipe;
  swipe.begin(readSwipeSpec(slots), 0.0);
  return swipe.isSwipedOut(offset) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Where a swipe let go at offset rests, into the SWIPE_OUT_* slots. Side is a SWIPE_SIDE_* value.
 */
JNIEXPORT void JNICALL SLK_JNI(nativeSwipeSettle)(
  JNIEnv* env, jclass, jdoubleArray io, jdouble offset, jdouble velocity, jdouble flingVelocity) {
  jdouble slots[SWIPE_SLOTS];
  env->GetDoubleArrayRegion(io, 0, SWIPE_SLOTS, slots);
  sl::SwipeReveal swipe;
  swipe.begin(readSwipeSpec(slots), 0.0);
  sl::SwipeRest rest = swipe.settle(offset, velocity, flingVelocity);
  SwipeSideValue side = rest.side == sl::SwipeSide::Leading ? SWIPE_SIDE_LEADING
    : rest.side == sl::SwipeSide::Trailing                  ? SWIPE_SIDE_TRAILING
                                                            : SWIPE_SIDE_NONE;
  slots[SWIPE_OUT_SIDE] = static_cast<jdouble>(side);
  slots[SWIPE_OUT_FULL] = rest.full ? 1.0 : 0.0;
  slots[SWIPE_OUT_OFFSET] = rest.offset;
  env->SetDoubleArrayRegion(io, SWIPE_OUT_SIDE, SWIPE_SLOTS - SWIPE_OUT_SIDE, slots + SWIPE_OUT_SIDE);
}

}
