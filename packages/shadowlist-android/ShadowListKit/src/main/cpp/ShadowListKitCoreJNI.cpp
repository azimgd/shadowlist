/*
 * JNI side of com.shadowlist.kit.ShadowListKitCore. Every call is a thin hop into the core's ListDriver
 * on the UI thread. Arrays the Kotlin side reuses carry values in and out. A layout allocates
 * nothing.
 */

#include <jni.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <shadowlist-core/host/ChangeAnimation.hpp>
#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListSections.hpp>
#include <shadowlist-core/host/ListSelection.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>
#include <shadowlist-core/host/ScrollTarget.hpp>
#include <shadowlist-core/host/SectionIndex.hpp>
#include <shadowlist-core/host/SwipeReveal.hpp>

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
  PASS_PREFETCH,
  PASS_MOUNT_PAD,
  PASS_LEADING_INSET,
  PASS_TRAILING_INSET,
  PASS_OUT_OFFSET,
  PASS_OUT_CONTENT,
  PASS_OUT_BAND_LOW,
  PASS_OUT_BAND_HIGH,
  PASS_OUT_SETTLING,
  PASS_OUT_REACHED_START,
  PASS_OUT_REACHED_END,
  PASS_OUT_GEOMETRY,
  PASS_OUT_MEASURED_LOW,
  PASS_OUT_MEASURED_HIGH,
  PASS_OUT_FOOTER_START,
  PASS_SLOTS,
};

/*
 * The driver and the Java object it measures and reports prefetch through. The env and target
 * are only valid during runPasses, the one call that measures. The vectors are reused by the
 * calls that run on every drag frame or mount pass.
 */
struct Peer {
  sl::ListDriver driver;
  sl::ListSections sections;
  sl::ChangeAnimation changes;
  JNIEnv* env = nullptr;
  jobject target = nullptr;
  jmethodID measureRow = nullptr;
  jmethodID prefetchRows = nullptr;
  std::vector<jint> dragInts;
  std::vector<std::size_t> dragRows;
  std::vector<jdouble> dragOut;
  std::vector<std::size_t> prefetchFound;
  std::vector<std::size_t> cancelFound;
  std::vector<jint> prefetchPacked;
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
 * Keys joined into one char array, with where each one ends. See ShadowListKitCore.pack.
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
  input.prefetch = slots[PASS_PREFETCH] != 0.0;
  input.mountPad = slots[PASS_MOUNT_PAD];
  input.leadingInset = slots[PASS_LEADING_INSET];
  input.trailingInset = slots[PASS_TRAILING_INSET];
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
  std::optional<sl::MountedRange> measured = driver.getMeasuredRange();
  slots[PASS_OUT_MEASURED_LOW] = measured ? static_cast<double>(measured->low) : -1.0;
  slots[PASS_OUT_MEASURED_HIGH] = measured ? static_cast<double>(measured->high) : -1.0;
  slots[PASS_OUT_FOOTER_START] = result.footerStart;
}

void writeRect(JNIEnv* env, const sl::RowRect& rect, jdoubleArray out) {
  jdouble values[4] = {rect.x, rect.y, rect.width, rect.height};
  env->SetDoubleArrayRegion(out, 0, 4, values);
}

bool validRow(const sl::ListDriver& driver, jint index) {
  return index >= 0 && static_cast<std::size_t>(index) < driver.getRowCount();
}

/*
 * Slots at the start of nativeDragFrame's out, ShadowListKitCore.DRAG_FRAME_* in Kotlin. Each
 * row's shift follows as an along, cross pair.
 */
enum DragFrameSlot {
  DRAG_FRAME_HELD_ALONG = 0,
  DRAG_FRAME_HELD_CROSS,
  DRAG_FRAME_INSERTION_MOVED,
  DRAG_FRAME_SLOTS,
};

/*
 * Bits of a section's flags in nativeSetSections, ShadowListKitCore.SECTION_* in Kotlin.
 */
enum SectionFlag {
  SECTION_HEADER = 1,
  SECTION_FOOTER = 2,
};

/*
 * Kinds of a row in nativePlaceOfRow and the low bits they take, ShadowListKitCore.ROW_* in Kotlin.
 */
enum RowKindValue {
  ROW_ITEM = 0,
  ROW_HEADER,
  ROW_FOOTER,
  ROW_KIND_BITS = 2,
};

jintArray makeIntArray(JNIEnv* env, const std::vector<jint>& values) {
  jintArray array = env->NewIntArray(static_cast<jsize>(values.size()));
  if (array != nullptr && !values.empty()) {
    env->SetIntArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
  }
  return array;
}

jobjectArray makeStringArray(JNIEnv* env, const std::vector<std::string>& values) {
  jclass stringClass = env->FindClass("java/lang/String");
  jobjectArray array = env->NewObjectArray(static_cast<jsize>(values.size()), stringClass, nullptr);
  for (std::size_t at = 0; array != nullptr && at < values.size(); ++at) {
    jstring value = env->NewStringUTF(values[at].c_str());
    if (value == nullptr) {
      break;
    }
    env->SetObjectArrayElement(array, static_cast<jsize>(at), value);
    env->DeleteLocalRef(value);
  }
  env->DeleteLocalRef(stringClass);
  return array;
}

std::vector<jint> jintsFromIndices(const std::vector<std::size_t>& indices) {
  std::vector<jint> values;
  values.reserve(indices.size());
  for (std::size_t index : indices) {
    values.push_back(static_cast<jint>(index));
  }
  return values;
}

/*
 * Slots of a change animation step in ShadowListKitCore.runChange, CHANGE_STEP_* in Kotlin.
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
 * Slots of ShadowListKitCore.constants, CONSTANT_* in Kotlin: kit constants of the core in dp at scale 1
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
  CONSTANT_CHANGE_DURATION_MS,
  CONSTANT_OFFSET_MOVED_THRESHOLD,
  CONSTANT_SLOTS,
};

/*
 * Kotlin carries a missing index as -1, the core as UNDEFINED_INDEX.
 */
/*
 * Prefetch changes as items, packed into the peer's buffer: the prefetch count, those items,
 * the cancel count, those items. Returns the packed length.
 */
jint packPrefetch(Peer& peer, const std::vector<std::size_t>& prefetch, const std::vector<std::size_t>& cancel) {
  std::vector<jint>& packed = peer.prefetchPacked;
  packed.clear();
  packed.push_back(0);
  if (!prefetch.empty()) {
    for (std::size_t item : peer.sections.itemsOfRows(prefetch)) {
      packed.push_back(static_cast<jint>(item));
    }
  }
  packed[0] = static_cast<jint>(packed.size() - 1);
  std::size_t cancelAt = packed.size();
  packed.push_back(0);
  if (!cancel.empty()) {
    for (std::size_t item : peer.sections.itemsOfRows(cancel)) {
      packed.push_back(static_cast<jint>(item));
    }
  }
  packed[cancelAt] = static_cast<jint>(packed.size() - cancelAt - 1);
  return static_cast<jint>(packed.size());
}

jint jintFromIndex(std::size_t index) {
  return index == sl::UNDEFINED_INDEX ? -1 : static_cast<jint>(index);
}

std::size_t indexFromJint(jint index) {
  return index < 0 ? sl::UNDEFINED_INDEX : static_cast<std::size_t>(index);
}

/*
 * Section and kind of a row: section shl ROW_KIND_BITS or a ROW_* kind.
 */
jint packedPlace(const sl::ListSections& sections, std::size_t row) {
  sl::RowPlace place = sections.placeOfRow(row);
  RowKindValue kind = place.kind == sl::RowKind::Header ? ROW_HEADER
    : place.kind == sl::RowKind::Footer                 ? ROW_FOOTER
                                                        : ROW_ITEM;
  return (static_cast<jint>(place.section) << ROW_KIND_BITS) | kind;
}

}

#define SHADOWLIST_KIT_JNI(name) Java_com_shadowlist_kit_ShadowListKitCore_##name

extern "C" {

JNIEXPORT jlong JNICALL SHADOWLIST_KIT_JNI(nativeCreate)(JNIEnv* env, jobject thiz) {
  auto* peer = new Peer();
  jclass clazz = env->GetObjectClass(thiz);
  peer->measureRow = env->GetMethodID(clazz, "measureRow", "(ID)D");
  peer->prefetchRows = env->GetMethodID(clazz, "prefetchRows", "(I)V");
  env->DeleteLocalRef(clazz);
  peer->driver.setMeasureRow([peer](std::size_t row, const std::string&, double cross) {
    // A measure that threw leaves its exception pending. No JNI call may follow until runPasses returns.
    if (peer->env == nullptr || peer->target == nullptr || peer->env->ExceptionCheck()) {
      return 0.0;
    }
    double size = peer->env->CallDoubleMethod(peer->target, peer->measureRow, static_cast<jint>(row), cross);
    return peer->env->ExceptionCheck() ? 0.0 : size;
  });
  peer->driver.setPrefetchRows([peer](const std::vector<std::size_t>& prefetch, const std::vector<std::size_t>& cancel) {
    if (peer->env == nullptr || peer->target == nullptr || peer->env->ExceptionCheck()) {
      return;
    }
    jint length = packPrefetch(*peer, prefetch, cancel);
    peer->env->CallVoidMethod(peer->target, peer->prefetchRows, length);
  });
  return reinterpret_cast<jlong>(peer);
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeDestroy)(JNIEnv*, jclass, jlong handle) {
  delete peerOf(handle);
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeSetSettings)(JNIEnv*, jclass, jlong handle, jdouble estimatedRowSize,
  jdouble overscan, jdouble startReachedThreshold, jdouble endReachedThreshold, jint numberOfColumns, jboolean inverted,
  jboolean followAppends, jboolean horizontal, jboolean snapToItem, jint snapAlignment) {
  sl::ListSettings settings;
  settings.estimatedRowSize = estimatedRowSize;
  settings.overscan = overscan;
  settings.startReachedThreshold = startReachedThreshold;
  settings.endReachedThreshold = endReachedThreshold;
  settings.numberOfColumns = static_cast<std::size_t>(numberOfColumns < 1 ? 1 : numberOfColumns);
  settings.inverted = inverted;
  settings.followAppends = followAppends;
  settings.horizontal = horizontal;
  settings.snapToItem = snapToItem;
  settings.snapAlignment = static_cast<sl::SnapAlignment>(snapAlignment);
  peerOf(handle)->driver.setSettings(settings);
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeSetStickyIndices)(JNIEnv* env, jclass, jlong handle, jintArray indices) {
  peerOf(handle)->driver.setStickyIndices(readIndices(env, indices));
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeMarkRemeasure)(JNIEnv* env, jclass, jlong handle, jintArray indices) {
  peerOf(handle)->driver.markRemeasure(readIndices(env, indices));
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeRunPasses)(JNIEnv* env, jobject thiz, jlong handle, jdoubleArray io) {
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

JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeResetKeepingPosition)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->driver.resetKeepingPosition();
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeRowCount)(JNIEnv*, jclass, jlong handle) {
  return static_cast<jint>(peerOf(handle)->driver.getRowCount());
}

/*
 * Frames of rows low to high, four values each, x y width height.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeCopyRowRects)(
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

JNIEXPORT jboolean JNICALL SHADOWLIST_KIT_JNI(nativeRowRect)(JNIEnv* env, jclass, jlong handle, jint index, jdoubleArray out) {
  const sl::ListDriver& driver = peerOf(handle)->driver;
  if (!validRow(driver, index)) {
    return JNI_FALSE;
  }
  writeRect(env, driver.getRowRect(static_cast<std::size_t>(index)), out);
  return JNI_TRUE;
}

/*
 * The rows overlapping the viewport packed as low in the high 32 bits and high in the low ones,
 * or -1 before the first layout.
 */
JNIEXPORT jlong JNICALL SHADOWLIST_KIT_JNI(nativeVisibleRange)(JNIEnv*, jclass, jlong handle) {
  std::optional<sl::MountedRange> visible = peerOf(handle)->driver.getVisibleRange();
  if (!visible) {
    return -1;
  }
  return (static_cast<jlong>(visible->low) << 32) | static_cast<jlong>(visible->high);
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeIndexOfKey)(JNIEnv* env, jclass, jlong handle, jstring key) {
  return jintFromIndex(peerOf(handle)->driver.indexOfKey(readString(env, key)));
}

/*
 * Leading edge and extent of every sticky row, two values each. Returns false when out is too short.
 */
JNIEXPORT jboolean JNICALL SHADOWLIST_KIT_JNI(nativeCopyStickyFrames)(JNIEnv* env, jclass, jlong handle, jdoubleArray out) {
  std::vector<double> frames;
  peerOf(handle)->driver.stickyFrames(frames);
  if (env->GetArrayLength(out) < static_cast<jsize>(frames.size())) {
    return JNI_FALSE;
  }
  env->SetDoubleArrayRegion(out, 0, static_cast<jsize>(frames.size()), frames.data());
  return JNI_TRUE;
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeScrollToRow)(
  JNIEnv*, jclass, jlong handle, jint row, jdouble viewPosition) {
  if (row >= 0) {
    peerOf(handle)->driver.scrollToRow(static_cast<std::size_t>(row), viewPosition);
  }
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeScrollToStart)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->driver.scrollToStart();
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeScrollToEnd)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->driver.scrollToEnd();
}

JNIEXPORT jdouble JNICALL SHADOWLIST_KIT_JNI(nativeNearestSnapOffset)(JNIEnv*, jclass, jlong handle, jdouble target) {
  return peerOf(handle)->driver.nearestSnapOffset(target);
}

/*
 * Where an animated scroll to a row aims, or NaN for a row the core has not placed yet.
 */
JNIEXPORT jdouble JNICALL SHADOWLIST_KIT_JNI(nativeAnimatedTargetOffset)(
  JNIEnv*, jclass, jlong handle, jint index, jdouble viewPosition, jdouble windowAlong, jdouble maxOffset) {
  const sl::ListDriver& driver = peerOf(handle)->driver;
  if (!validRow(driver, index)) {
    return std::nan("");
  }
  return driver.animatedTargetOffset(static_cast<std::size_t>(index), viewPosition, windowAlong, maxOffset);
}

/*
 * Target is ShadowListKitCore.LANDING_*, the same order as ScrollLanding::Target.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeSetLanding)(
  JNIEnv*, jclass, jlong handle, jint target, jint index, jdouble viewPosition) {
  sl::ScrollLanding landing;
  landing.target = static_cast<sl::ScrollLanding::Target>(target);
  landing.index = static_cast<std::size_t>(index < 0 ? 0 : index);
  landing.viewPosition = viewPosition;
  peerOf(handle)->driver.setLanding(landing);
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeCancelLanding)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->driver.cancelLanding();
}

JNIEXPORT jboolean JNICALL SHADOWLIST_KIT_JNI(nativeLand)(JNIEnv*, jclass, jlong handle) {
  return peerOf(handle)->driver.land() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeDragBegin)(
  JNIEnv*, jclass, jlong handle, jint index, jdouble touchAlong, jdouble touchCross) {
  if (index >= 0) {
    peerOf(handle)->driver.dragBegin(static_cast<std::size_t>(index), touchAlong, touchCross);
  }
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeDragEnd)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->driver.dragEnd();
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeHeldIndex)(JNIEnv*, jclass, jlong handle) {
  return jintFromIndex(peerOf(handle)->driver.getHeldIndex());
}

/*
 * One drag frame: place the held row under the touch, find the drop slot among the first count
 * rows and write the held row's offset and whether the slot moved into out's DRAG_FRAME_* slots,
 * then each row's shift as an along, cross pair. Returns the held row, or -1 when its key is gone.
 */
JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeDragFrame)(JNIEnv* env, jclass, jlong handle, jdouble touchAlong,
  jdouble touchCross, jintArray rows, jint count, jdoubleArray out) {
  Peer* peer = peerOf(handle);
  sl::ListDriver& driver = peer->driver;
  std::size_t held = driver.getHeldIndex();
  if (held == sl::UNDEFINED_INDEX || held >= driver.getRowCount()) {
    return -1;
  }
  sl::DragOffset placed = driver.placeHeld(held, touchAlong, touchCross);
  std::size_t insertion = driver.getDragInsertionIndex();
  jsize given = std::clamp(count, 0, env->GetArrayLength(rows));
  std::vector<jint>& ints = peer->dragInts;
  ints.resize(static_cast<std::size_t>(given));
  if (given > 0) {
    env->GetIntArrayRegion(rows, 0, given, ints.data());
  }
  peer->dragRows.clear();
  for (jint row : ints) {
    if (row >= 0) {
      peer->dragRows.push_back(static_cast<std::size_t>(row));
    }
  }
  driver.dragUpdateInsertion(peer->dragRows);
  std::vector<jdouble>& values = peer->dragOut;
  values.resize(DRAG_FRAME_SLOTS + ints.size() * 2);
  values[DRAG_FRAME_HELD_ALONG] = placed.leading;
  values[DRAG_FRAME_HELD_CROSS] = placed.cross;
  values[DRAG_FRAME_INSERTION_MOVED] = driver.getDragInsertionIndex() != insertion ? 1.0 : 0.0;
  for (std::size_t at = 0; at < ints.size(); ++at) {
    sl::DragOffset shift = driver.dragShiftFor(indexFromJint(ints[at]));
    values[DRAG_FRAME_SLOTS + at * 2] = shift.leading;
    values[DRAG_FRAME_SLOTS + at * 2 + 1] = shift.cross;
  }
  jsize length = std::min(env->GetArrayLength(out), static_cast<jsize>(values.size()));
  env->SetDoubleArrayRegion(out, 0, length, values.data());
  return static_cast<jint>(held);
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeDragOriginIndex)(JNIEnv*, jclass, jlong handle) {
  return jintFromIndex(peerOf(handle)->driver.getDragOriginIndex());
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeDragInsertionIndex)(JNIEnv*, jclass, jlong handle) {
  return jintFromIndex(peerOf(handle)->driver.getDragInsertionIndex());
}

JNIEXPORT jdouble JNICALL SHADOWLIST_KIT_JNI(nativeDragAutoScrollOffset)(
  JNIEnv*,
  jclass,
  jdouble touch,
  jdouble windowSize,
  jdouble offset,
  jdouble maxOffset,
  jdouble density,
  jdouble dtSeconds) {
  sl::DragAutoScrollConfig config{sl::DRAG_AUTO_SCROLL_ANDROID.edge * density,
    sl::DRAG_AUTO_SCROLL_ANDROID.maxSpeed * density};
  return sl::dragAutoScrollOffset(config, touch, windowSize, offset, maxOffset, dtSeconds);
}

/*
 * Sections over the rows. counts has each section's item count, flags its SECTION_* bits.
 * Without counts the list has no sections and count items.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeSetSections)(
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
 * The item of every row, -1 for headers and footers, into items, whether a separator follows
 * the row into separators, and its section and kind as nativePlaceOfRow packs them into places.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeCopyRows)(
  JNIEnv* env, jclass, jlong handle, jintArray items, jbooleanArray separators, jintArray places) {
  const sl::ListSections& sections = peerOf(handle)->sections;
  std::size_t count = sections.getRowCount();
  std::vector<jint> rowItems(count);
  std::vector<jboolean> rowSeparators(count);
  std::vector<jint> rowPlaces(count);
  for (std::size_t row = 0; row < count; ++row) {
    rowItems[row] = jintFromIndex(sections.itemForRow(row));
    rowSeparators[row] = sections.isItemBeforeItem(row) ? JNI_TRUE : JNI_FALSE;
    rowPlaces[row] = packedPlace(sections, row);
  }
  env->SetIntArrayRegion(items, 0, std::min(env->GetArrayLength(items), static_cast<jsize>(count)), rowItems.data());
  env->SetBooleanArrayRegion(separators, 0, std::min(env->GetArrayLength(separators), static_cast<jsize>(count)),
    rowSeparators.data());
  env->SetIntArrayRegion(places, 0, std::min(env->GetArrayLength(places), static_cast<jsize>(count)), rowPlaces.data());
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeItemForDrop)(JNIEnv*, jclass, jlong handle, jint fromRow, jint toRow) {
  if (fromRow < 0 || toRow < 0) {
    return -1;
  }
  return jintFromIndex(
    peerOf(handle)->sections.itemForDrop(static_cast<std::size_t>(fromRow), static_cast<std::size_t>(toRow)));
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeRowForItem)(JNIEnv*, jclass, jlong handle, jint item) {
  return item < 0 ? -1 : jintFromIndex(peerOf(handle)->sections.rowForItem(static_cast<std::size_t>(item)));
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeSectionForItem)(JNIEnv*, jclass, jlong handle, jint item) {
  return item < 0 ? -1 : jintFromIndex(peerOf(handle)->sections.sectionForItem(static_cast<std::size_t>(item)));
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeFirstItemInSection)(JNIEnv*, jclass, jlong handle, jint section) {
  return section < 0 ? -1
                     : jintFromIndex(peerOf(handle)->sections.firstItemInSection(static_cast<std::size_t>(section)));
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeHeaderRow)(JNIEnv*, jclass, jlong handle, jint section) {
  return section < 0 ? -1 : jintFromIndex(peerOf(handle)->sections.headerRow(static_cast<std::size_t>(section)));
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeFooterRow)(JNIEnv*, jclass, jlong handle, jint section) {
  return section < 0 ? -1 : jintFromIndex(peerOf(handle)->sections.footerRow(static_cast<std::size_t>(section)));
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeFirstRowInSection)(JNIEnv*, jclass, jlong handle, jint section) {
  return section < 0 ? -1
                     : jintFromIndex(peerOf(handle)->sections.firstRowInSection(static_cast<std::size_t>(section)));
}

/*
 * Section and kind of a row: section shl ROW_KIND_BITS or a ROW_* kind, or -1.
 */
JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativePlaceOfRow)(JNIEnv*, jclass, jlong handle, jint row) {
  const sl::ListSections& sections = peerOf(handle)->sections;
  if (row < 0 || static_cast<std::size_t>(row) >= sections.getRowCount()) {
    return -1;
  }
  return packedPlace(sections, static_cast<std::size_t>(row));
}

JNIEXPORT jintArray JNICALL SHADOWLIST_KIT_JNI(nativeStickyRows)(
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
JNIEXPORT jobjectArray JNICALL SHADOWLIST_KIT_JNI(nativeEdgeRowKeys)(
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
  return makeStringArray(env, peerOf(handle)->sections.edgeRowKeys(given, firsts));
}

/*
 * Prefetch changes for the mounted rows, as items, packed into out: the prefetch count, those
 * items, the cancel count, those items. Returns the packed length. When out is shorter, the
 * packed values wait for nativeCopyPrefetch.
 */
JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeUpdatePrefetch)(
  JNIEnv* env, jclass, jlong handle, jint low, jint high, jintArray out) {
  Peer* peer = peerOf(handle);
  peer->driver.updatePrefetch(indexFromJint(low), indexFromJint(high), peer->prefetchFound, peer->cancelFound);
  jsize length = packPrefetch(*peer, peer->prefetchFound, peer->cancelFound);
  if (env->GetArrayLength(out) >= length) {
    env->SetIntArrayRegion(out, 0, length, peer->prefetchPacked.data());
  }
  return length;
}

/*
 * The values of the last nativeUpdatePrefetch into out, which has room for them.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeCopyPrefetch)(JNIEnv* env, jclass, jlong handle, jintArray out) {
  const std::vector<jint>& packed = peerOf(handle)->prefetchPacked;
  jsize length = std::min(env->GetArrayLength(out), static_cast<jsize>(packed.size()));
  env->SetIntArrayRegion(out, 0, length, packed.data());
}

/*
 * The row at the viewport start: its key, with the distance into it in out[0], or null.
 */
JNIEXPORT jstring JNICALL SHADOWLIST_KIT_JNI(nativeAnchorState)(JNIEnv* env, jclass, jlong handle, jdouble offset, jdoubleArray out) {
  std::optional<sl::AnchorState> anchor = peerOf(handle)->driver.getAnchorState(offset);
  if (!anchor) {
    return nullptr;
  }
  jdouble distance = anchor->offset;
  env->SetDoubleArrayRegion(out, 0, 1, &distance);
  return env->NewStringUTF(anchor->key.c_str());
}

JNIEXPORT jboolean JNICALL SHADOWLIST_KIT_JNI(nativeRestoreAnchorState)(
  JNIEnv* env, jclass, jlong handle, jstring key, jdouble offset) {
  return peerOf(handle)->driver.restoreAnchorState({readString(env, key), offset}) ? JNI_TRUE : JNI_FALSE;
}

/*
 * A change is about to reach the core. The mounted rows and where they show, x and y pairs,
 * are recorded when the change starts an animation.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_JNI(nativeCaptureChange)(JNIEnv* env, jclass, jlong handle, jcharArray deletedPacked,
  jintArray deletedEnds, jcharArray insertedPacked, jintArray insertedEnds, jcharArray mountedPacked,
  jintArray mountedEnds, jdoubleArray mountedPositions) {
  sl::ChangeAnimation& changes = peerOf(handle)->changes;
  std::vector<std::string> deleted = readPackedKeys(env, deletedPacked, deletedEnds);
  std::vector<std::string> inserted = readPackedKeys(env, insertedPacked, insertedEnds);
  if (!changes.capture(deleted, inserted)) {
    return;
  }
  std::vector<std::string> mounted = readPackedKeys(env, mountedPacked, mountedEnds);
  std::vector<sl::ScreenPoint> positions = readPoints(env, mountedPositions);
  for (std::size_t at = 0; at < mounted.size() && at < positions.size(); ++at) {
    changes.recordPosition(mounted[at], positions[at]);
  }
}

/*
 * Where a deleted row showed before the change, x and y into out, or false when it does not
 * fade out.
 */
JNIEXPORT jboolean JNICALL SHADOWLIST_KIT_JNI(nativeDeletedPosition)(
  JNIEnv* env, jclass, jlong handle, jstring key, jdoubleArray out) {
  std::optional<sl::ScreenPoint> position = peerOf(handle)->changes.deletedPosition(readString(env, key));
  if (!position) {
    return JNI_FALSE;
  }
  jdouble values[2] = {position->x, position->y};
  env->SetDoubleArrayRegion(out, 0, 2, values);
  return JNI_TRUE;
}

/*
 * The animation of the first count rows, mounted after the layout pass and low to high,
 * CHANGE_STEP_SLOTS values per row, or null without a pending change.
 */
JNIEXPORT jdoubleArray JNICALL SHADOWLIST_KIT_JNI(nativeRunChange)(
  JNIEnv* env, jclass, jlong handle, jintArray rows, jint count, jdoubleArray positions) {
  Peer* peer = peerOf(handle);
  sl::ChangeAnimation& changes = peer->changes;
  if (!changes.isPending()) {
    return nullptr;
  }
  std::vector<jint> given = readInts(env, rows);
  given.resize(std::min(given.size(), static_cast<std::size_t>(std::max(count, 0))));
  std::vector<std::string> keys;
  keys.reserve(given.size());
  for (jint row : given) {
    bool valid = row >= 0 && static_cast<std::size_t>(row) < peer->driver.getKeyCount();
    keys.push_back(valid ? peer->driver.getKeyAt(static_cast<std::size_t>(row)) : std::string());
  }
  std::vector<sl::ChangeStep> steps = changes.run(keys, readPoints(env, positions));
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

JNIEXPORT jintArray JNICALL SHADOWLIST_KIT_JNI(nativeInsertionIndices)(
  JNIEnv* env, jclass, jintArray indices, jint previousCount) {
  std::size_t count = static_cast<std::size_t>(std::max(previousCount, 0));
  std::vector<jint> sorted;
  for (std::size_t index : sl::insertionIndices(readIndices(env, indices), count)) {
    sorted.push_back(static_cast<jint>(index));
  }
  return makeIntArray(env, sorted);
}

JNIEXPORT jintArray JNICALL SHADOWLIST_KIT_JNI(nativeDeletionIndices)(
  JNIEnv* env, jclass, jintArray indices, jint previousCount) {
  std::size_t count = static_cast<std::size_t>(std::max(previousCount, 0));
  std::vector<jint> sorted;
  for (std::size_t index : sl::deletionIndices(readIndices(env, indices), count)) {
    sorted.push_back(static_cast<jint>(index));
  }
  return makeIntArray(env, sorted);
}

JNIEXPORT jdoubleArray JNICALL SHADOWLIST_KIT_JNI(nativeConstants)(JNIEnv* env, jclass) {
  jdouble values[CONSTANT_SLOTS];
  values[CONSTANT_SWIPE_FLING_VELOCITY] = sl::SWIPE_FLING_VELOCITY;
  values[CONSTANT_SWIPE_SETTLE_DURATION_MS] = sl::SWIPE_SETTLE_DURATION_MS;
  values[CONSTANT_DRAG_LIFT_SCALE] = sl::DRAG_LIFT_SCALE;
  values[CONSTANT_DRAG_LIFT_DURATION_MS] = sl::DRAG_LIFT_DURATION_MS;
  values[CONSTANT_DRAG_SHIFT_DURATION_MS] = sl::DRAG_SHIFT_DURATION_MS;
  values[CONSTANT_DRAG_DROP_DURATION_MS] = sl::DRAG_DROP_DURATION_MS;
  values[CONSTANT_SECTION_INDEX_TITLE_HEIGHT] = sl::SECTION_INDEX_TITLE_HEIGHT;
  values[CONSTANT_SECTION_INDEX_WIDTH] = sl::SECTION_INDEX_WIDTH;
  values[CONSTANT_CHANGE_DURATION_MS] = sl::CHANGE_DURATION_MS;
  values[CONSTANT_OFFSET_MOVED_THRESHOLD] = sl::OFFSET_MOVED_THRESHOLD;
  jdoubleArray array = env->NewDoubleArray(CONSTANT_SLOTS);
  if (array != nullptr) {
    env->SetDoubleArrayRegion(array, 0, CONSTANT_SLOTS, values);
  }
  return array;
}

JNIEXPORT jdouble JNICALL SHADOWLIST_KIT_JNI(nativePageScrollTarget)(
  JNIEnv*, jclass, jdouble offset, jdouble windowAlong, jdouble maxOffset, jint direction) {
  return sl::pageScrollTarget(offset, windowAlong, maxOffset, direction);
}

JNIEXPORT jdouble JNICALL SHADOWLIST_KIT_JNI(nativeSectionIndexTitlesTop)(
  JNIEnv*, jclass, jdouble areaHeight, jint count, jdouble scale) {
  return sl::sectionIndexTitlesTop(areaHeight, static_cast<std::size_t>(std::max(count, 0)), scale);
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_JNI(nativeSectionIndexTitleAt)(
  JNIEnv*, jclass, jdouble y, jdouble areaHeight, jint count, jdouble scale) {
  return static_cast<jint>(sl::sectionIndexTitleAt(y, areaHeight, static_cast<std::size_t>(std::max(count, 0)), scale));
}

JNIEXPORT jdouble JNICALL SHADOWLIST_KIT_JNI(nativeSwipeButtonSize)(JNIEnv*, jclass, jdouble fitted, jdouble scale) {
  return sl::swipeButtonSize(fitted, scale);
}


}

/*
 * JNI side of com.shadowlist.kit.ShadowListKitListState: the row keys, the selection, the batch
 * being collected and the content versions of one list, which outlive the core the list drops on
 * detach. A key change reaches the state once, with the list's live core when it has one, and
 * the state hands the core its part.
 */
namespace {

struct StatePeer {
  sl::HeldKeys keys;
  sl::ListSelection selection;
  sl::BatchCollector batch;
  sl::CollectedBatch taken;
  sl::ContentVersions versions;
};

StatePeer* statePeerOf(jlong handle) {
  return reinterpret_cast<StatePeer*>(handle);
}

std::vector<std::int64_t> readLongs(JNIEnv* env, jlongArray array) {
  if (array == nullptr) {
    return {};
  }
  jsize length = env->GetArrayLength(array);
  std::vector<jlong> values(static_cast<std::size_t>(length));
  env->GetLongArrayRegion(array, 0, length, values.data());
  return std::vector<std::int64_t>(values.begin(), values.end());
}

/*
 * Values of a tap's out[0], ShadowListKitListState.TAP_* in Kotlin.
 */
enum TapOutcome {
  TAP_NOTHING = 0,
  TAP_SELECTED,
  TAP_TOGGLED_OFF,
};

/*
 * Bits of nativeCommitKeys' flags, ShadowListKitListState.COMMIT_* in Kotlin.
 */
enum CommitFlag {
  COMMIT_DIFF = 1,
  COMMIT_RECORD_VERSIONS = 2,
  COMMIT_UPDATE_VERSIONS = 4,
};

/*
 * The core of a core handle, or none for 0.
 */
Peer* livePeerOf(jlong handle) {
  return handle == 0 ? nullptr : peerOf(handle);
}

/*
 * The selected keys a key change took away drop out.
 */
void retainSelection(StatePeer& peer) {
  if (!peer.selection.isEmpty()) {
    peer.selection.retainKeys(peer.keys.rows());
  }
}

std::vector<std::string> copyKeys(sl::KeyList keys, std::size_t start, std::size_t count) {
  std::vector<std::string> copied;
  copied.reserve(count);
  for (std::size_t index = start; index < start + count; ++index) {
    copied.push_back(keys[index]);
  }
  return copied;
}

}

#define SHADOWLIST_KIT_STATE_JNI(name) Java_com_shadowlist_kit_ShadowListKitListState_##name

extern "C" {

JNIEXPORT jlong JNICALL SHADOWLIST_KIT_STATE_JNI(nativeCreate)(JNIEnv*, jclass) {
  return reinterpret_cast<jlong>(new StatePeer());
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_STATE_JNI(nativeDestroy)(JNIEnv*, jclass, jlong handle) {
  delete statePeerOf(handle);
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_STATE_JNI(nativeSetMultipleSelection)(JNIEnv*, jclass, jlong handle, jboolean multiple) {
  statePeerOf(handle)->selection.setMultiple(multiple == JNI_TRUE);
}

JNIEXPORT jint JNICALL SHADOWLIST_KIT_STATE_JNI(nativeSelectedCount)(JNIEnv*, jclass, jlong handle) {
  return static_cast<jint>(statePeerOf(handle)->selection.getCount());
}

JNIEXPORT jboolean JNICALL SHADOWLIST_KIT_STATE_JNI(nativeIsSelected)(JNIEnv* env, jclass, jlong handle, jstring key) {
  return statePeerOf(handle)->selection.contains(readString(env, key)) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Select a row. Returns the rows this deselected.
 */
JNIEXPORT jobjectArray JNICALL SHADOWLIST_KIT_STATE_JNI(nativeSelect)(JNIEnv* env, jclass, jlong handle, jstring key) {
  std::vector<std::string> deselected;
  statePeerOf(handle)->selection.select(readString(env, key), deselected);
  return makeStringArray(env, deselected);
}

JNIEXPORT jboolean JNICALL SHADOWLIST_KIT_STATE_JNI(nativeDeselect)(JNIEnv* env, jclass, jlong handle, jstring key) {
  return statePeerOf(handle)->selection.deselect(readString(env, key)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobjectArray JNICALL SHADOWLIST_KIT_STATE_JNI(nativeClearSelection)(JNIEnv* env, jclass, jlong handle) {
  std::vector<std::string> deselected;
  statePeerOf(handle)->selection.clear(deselected);
  return makeStringArray(env, deselected);
}

/*
 * The core's ListSelection.tap. shouldSelect is a ShadowListKitListState.ShouldSelect, asked
 * only before a row gets selected. out[0] gets a TAP_* outcome. Returns the rows this deselected.
 */
JNIEXPORT jobjectArray JNICALL SHADOWLIST_KIT_STATE_JNI(nativeTap)(
  JNIEnv* env, jclass, jlong handle, jstring key, jobject shouldSelect, jintArray out) {
  sl::SelectionTap tap = statePeerOf(handle)->selection.tap(readString(env, key), [&] {
    jclass clazz = env->GetObjectClass(shouldSelect);
    jmethodID method = env->GetMethodID(clazz, "shouldSelect", "()Z");
    env->DeleteLocalRef(clazz);
    jboolean answer = env->CallBooleanMethod(shouldSelect, method);
    // A callback that threw selects nothing, and its exception reaches Kotlin when this returns.
    return !env->ExceptionCheck() && answer == JNI_TRUE;
  });
  jint outcome = tap.toggledOff ? TAP_TOGGLED_OFF : tap.selected ? TAP_SELECTED : TAP_NOTHING;
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  env->SetIntArrayRegion(out, 0, 1, &outcome);
  return makeStringArray(env, tap.deselected);
}

/*
 * The positions of the selected keys among the rows held, low to high.
 */
JNIEXPORT jintArray JNICALL SHADOWLIST_KIT_STATE_JNI(nativeSelectedRows)(JNIEnv* env, jclass, jlong handle) {
  StatePeer* peer = statePeerOf(handle);
  if (peer->selection.isEmpty()) {
    return env->NewIntArray(0);
  }
  return makeIntArray(env, jintsFromIndices(peer->selection.indicesIn(peer->keys.rows())));
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_STATE_JNI(nativeBeginBatch)(JNIEnv*, jclass, jlong handle) {
  statePeerOf(handle)->batch.begin();
}

JNIEXPORT jboolean JNICALL SHADOWLIST_KIT_STATE_JNI(nativeEndBatch)(JNIEnv*, jclass, jlong handle) {
  return statePeerOf(handle)->batch.end() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL SHADOWLIST_KIT_STATE_JNI(nativeIsBatching)(JNIEnv*, jclass, jlong handle) {
  return statePeerOf(handle)->batch.isCollecting() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_STATE_JNI(nativeBatchDeleted)(JNIEnv* env, jclass, jlong handle, jintArray indices) {
  statePeerOf(handle)->batch.addDeleted(readIndices(env, indices));
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_STATE_JNI(nativeBatchInserted)(JNIEnv* env, jclass, jlong handle, jintArray indices) {
  statePeerOf(handle)->batch.addInserted(readIndices(env, indices));
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_STATE_JNI(nativeBatchMoved)(JNIEnv*, jclass, jlong handle, jint from, jint to) {
  if (from >= 0 && to >= 0) {
    statePeerOf(handle)->batch.addMoved(static_cast<std::size_t>(from), static_cast<std::size_t>(to));
  }
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_STATE_JNI(nativeBatchReloaded)(JNIEnv* env, jclass, jlong handle, jintArray indices) {
  statePeerOf(handle)->batch.addReloaded(readIndices(env, indices));
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_STATE_JNI(nativeBatchNeedsReload)(JNIEnv*, jclass, jlong handle) {
  statePeerOf(handle)->batch.markNeedsReload();
}

/*
 * Take the collected batch, kept for nativePlanTakenBatch. Returns null for an empty batch,
 * otherwise whether it needs a reload, 1 or 0, then the reloaded items.
 */
JNIEXPORT jintArray JNICALL SHADOWLIST_KIT_STATE_JNI(nativeTakeBatch)(JNIEnv* env, jclass, jlong handle) {
  StatePeer* peer = statePeerOf(handle);
  peer->taken = peer->batch.take();
  if (peer->taken.isEmpty()) {
    return nullptr;
  }
  std::vector<jint> packed;
  packed.reserve(peer->taken.update.reloaded.size() + 1);
  packed.push_back(peer->taken.needsReload ? 1 : 0);
  for (std::size_t item : peer->taken.update.reloaded) {
    packed.push_back(static_cast<jint>(item));
  }
  return makeIntArray(env, packed);
}

/*
 * The core's planBatch over the batch nativeTakeBatch took: the source of every next row, a
 * previous index or -1 for an insert, or null for a batch that does not add up.
 */
JNIEXPORT jintArray JNICALL SHADOWLIST_KIT_STATE_JNI(nativePlanTakenBatch)(
  JNIEnv* env, jclass, jlong handle, jint previousCount, jint nextCount) {
  StatePeer* peer = statePeerOf(handle);
  std::optional<sl::BatchPlan> plan = sl::planBatch(static_cast<std::size_t>(std::max(previousCount, 0)),
    static_cast<std::size_t>(std::max(nextCount, 0)), peer->taken.update);
  peer->taken = sl::CollectedBatch{};
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
 * Take the next keys: rowEnd unchanged rows at the end, the joined rows between, and with
 * sectioned the items the same way. The live core gets the row splice, versions are recorded or
 * compared against the items held, and the selection drops the rows that left. Returns the
 * COMMIT_DIFF diff of the items, packed: the delete count and previous indices, the insert count
 * and next indices, the move count and from, to pairs. The count and items whose version changed
 * under COMMIT_UPDATE_VERSIONS follow.
 */
JNIEXPORT jintArray JNICALL SHADOWLIST_KIT_STATE_JNI(nativeCommitKeys)(JNIEnv* env, jclass, jlong handle, jlong core,
  jint rowStart, jint rowEnd, jcharArray rowPacked, jintArray rowEnds, jboolean sectioned, jint itemStart, jint itemEnd,
  jcharArray itemPacked, jintArray itemEnds, jlongArray versions, jint flags) {
  StatePeer* peer = statePeerOf(handle);
  std::optional<sl::KeyChange> items;
  if (sectioned) {
    items = sl::KeyChange{indexFromJint(std::max(itemStart, 0)), indexFromJint(std::max(itemEnd, 0)),
      readPackedKeys(env, itemPacked, itemEnds)};
  }
  sl::KeyChange rows{indexFromJint(std::max(rowStart, 0)), indexFromJint(std::max(rowEnd, 0)),
    readPackedKeys(env, rowPacked, rowEnds)};
  sl::HeldKeysCommit commit = peer->keys.commit(std::move(rows), std::move(items), (flags & COMMIT_DIFF) != 0);
  if (Peer* live = livePeerOf(core); live && !commit.rows.isEmpty()) {
    live->driver.replaceKeys(
      commit.rows.start, commit.rows.deleted, copyKeys(peer->keys.rows(), commit.rows.start, commit.rows.inserted));
  }
  std::vector<std::size_t> reloaded;
  if (versions != nullptr && (flags & COMMIT_RECORD_VERSIONS) != 0) {
    peer->versions.record(peer->keys.items(), readLongs(env, versions));
  } else if (versions != nullptr && (flags & COMMIT_UPDATE_VERSIONS) != 0) {
    reloaded = peer->versions.update(peer->keys.items(), readLongs(env, versions));
  }
  retainSelection(*peer);
  const sl::KeyDiff& diff = commit.diff;
  std::vector<jint> packed;
  packed.reserve(4 + diff.deleted.size() + diff.inserted.size() + diff.moved.size() * 2 + reloaded.size());
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
  packed.push_back(static_cast<jint>(reloaded.size()));
  for (std::size_t item : reloaded) {
    packed.push_back(static_cast<jint>(item));
  }
  return makeIntArray(env, packed);
}

/*
 * Rows inserted at sorted positions of the next keys of a list without sections, with the joined
 * keys in the same order. A negative index drops its key with it, which keeps the two lists paired.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_STATE_JNI(nativeInsertRows)(
  JNIEnv* env, jclass, jlong handle, jlong core, jintArray indices, jcharArray packed, jintArray ends) {
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
  if (Peer* live = livePeerOf(core)) {
    live->driver.insertKeys(kept, keptKeys);
  }
  statePeerOf(handle)->keys.insertRows(kept, std::move(keptKeys));
}

/*
 * Rows deleted at positions of the previous keys of a list without sections.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_STATE_JNI(nativeDeleteRows)(
  JNIEnv* env, jclass, jlong handle, jlong core, jintArray indices) {
  std::vector<std::size_t> deleted = readIndices(env, indices);
  if (Peer* live = livePeerOf(core)) {
    live->driver.deleteKeys(deleted);
  }
  StatePeer* peer = statePeerOf(handle);
  peer->keys.deleteRows(deleted);
  retainSelection(*peer);
}

/*
 * A new core takes every row held.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_STATE_JNI(nativeSeedCore)(JNIEnv*, jclass, jlong handle, jlong core) {
  sl::KeyList rows = statePeerOf(handle)->keys.rows();
  if (Peer* live = livePeerOf(core); live && !rows.empty()) {
    live->driver.replaceKeys(0, live->driver.getKeyCount(), copyKeys(rows, 0, rows.size()));
  }
}

}

/*
 * JNI side of com.shadowlist.kit.ShadowListKitSwipeReveal: the core's SwipeReveal of the row swiped
 * open with both sides' button sizes. A touch move or an animation frame is one call.
 */
namespace {

struct SwipePeer {
  sl::SwipeReveal reveal;
  std::vector<double> leadingSizes;
  std::vector<double> trailingSizes;
  std::vector<sl::SwipeSpan> spans;
  std::vector<jdouble> out;
};

SwipePeer* swipePeerOf(jlong handle) {
  return reinterpret_cast<SwipePeer*>(handle);
}

/*
 * Slots of a swipe's out, ShadowListKitSwipeReveal.OUT_* in Kotlin. Each button's start and size
 * follow from SWIPE_OUT_SPANS.
 */
enum SwipeOutSlot {
  SWIPE_OUT_OFFSET = 0,
  SWIPE_OUT_PAST_FULL,
  SWIPE_OUT_REVEALED_START,
  SWIPE_OUT_REVEALED_SIZE,
  SWIPE_OUT_SPANS,
};

/*
 * Slots of a released swipe's rest, ShadowListKitSwipeReveal.REST_* in Kotlin. The side is a
 * SWIPE_SIDE_* value.
 */
enum SwipeRestSlot {
  SWIPE_REST_SIDE = 0,
  SWIPE_REST_FULL,
  SWIPE_REST_OFFSET,
  SWIPE_REST_SLOTS,
};

enum SwipeSideValue {
  SWIPE_SIDE_NONE = 0,
  SWIPE_SIDE_LEADING,
  SWIPE_SIDE_TRAILING,
};

void readSizes(JNIEnv* env, jdoubleArray array, std::vector<double>& out) {
  jsize length = array == nullptr ? 0 : env->GetArrayLength(array);
  out.resize(static_cast<std::size_t>(length));
  if (length > 0) {
    env->GetDoubleArrayRegion(array, 0, length, out.data());
  }
}

/*
 * The row at offset: whether it is past the full swipe point and where the buttons of the side
 * it reveals go across crossSize, into out.
 */
void writeSwipe(JNIEnv* env, SwipePeer& peer, double offset, double crossSize, jdoubleArray out) {
  bool past = offset != 0.0 && peer.reveal.isPastFullSwipe(offset);
  sl::swipeButtonSpans(offset > 0.0 ? peer.leadingSizes : peer.trailingSizes, offset, past, crossSize, peer.spans);
  sl::SwipeSpan revealed = sl::swipeRevealedSpan(offset, crossSize);
  std::vector<jdouble>& values = peer.out;
  values.resize(SWIPE_OUT_SPANS + peer.spans.size() * 2);
  values[SWIPE_OUT_OFFSET] = offset;
  values[SWIPE_OUT_PAST_FULL] = past ? 1.0 : 0.0;
  values[SWIPE_OUT_REVEALED_START] = revealed.start;
  values[SWIPE_OUT_REVEALED_SIZE] = revealed.size;
  for (std::size_t at = 0; at < peer.spans.size(); ++at) {
    values[SWIPE_OUT_SPANS + at * 2] = peer.spans[at].start;
    values[SWIPE_OUT_SPANS + at * 2 + 1] = peer.spans[at].size;
  }
  jsize length = std::min(env->GetArrayLength(out), static_cast<jsize>(values.size()));
  env->SetDoubleArrayRegion(out, 0, length, values.data());
}

}

#define SHADOWLIST_KIT_SWIPE_JNI(name) Java_com_shadowlist_kit_ShadowListKitSwipeReveal_##name

extern "C" {

JNIEXPORT jlong JNICALL SHADOWLIST_KIT_SWIPE_JNI(nativeCreate)(JNIEnv*, jclass) {
  return reinterpret_cast<jlong>(new SwipePeer());
}

JNIEXPORT void JNICALL SHADOWLIST_KIT_SWIPE_JNI(nativeDestroy)(JNIEnv*, jclass, jlong handle) {
  delete swipePeerOf(handle);
}

/*
 * A row with these buttons opens. Each side's width is the sum of its button sizes.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_SWIPE_JNI(nativeBegin)(JNIEnv* env, jclass, jlong handle, jdoubleArray leadingSizes,
  jdoubleArray trailingSizes, jboolean leadingFull, jboolean trailingFull, jdouble rowSize) {
  SwipePeer* peer = swipePeerOf(handle);
  readSizes(env, leadingSizes, peer->leadingSizes);
  readSizes(env, trailingSizes, peer->trailingSizes);
  sl::SwipeSpec spec;
  for (double size : peer->leadingSizes) {
    spec.leadingWidth += size;
  }
  for (double size : peer->trailingSizes) {
    spec.trailingWidth += size;
  }
  spec.leadingFullSwipe = leadingFull == JNI_TRUE;
  spec.trailingFullSwipe = trailingFull == JNI_TRUE;
  spec.rowSize = rowSize;
  peer->reveal.begin(spec, 0.0);
}

/*
 * A finger that moved translation since the swipe began at start. Writes the row it moves to
 * into out.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_SWIPE_JNI(nativeMove)(
  JNIEnv* env, jclass, jlong handle, jdouble start, jdouble translation, jdouble crossSize, jdoubleArray out) {
  SwipePeer* peer = swipePeerOf(handle);
  peer->reveal.begin(peer->reveal.getSpec(), start);
  writeSwipe(env, *peer, peer->reveal.drag(translation), crossSize, out);
}

/*
 * The row at offset, like an animation frame, into out.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_SWIPE_JNI(nativeShow)(
  JNIEnv* env, jclass, jlong handle, jdouble offset, jdouble crossSize, jdoubleArray out) {
  writeSwipe(env, *swipePeerOf(handle), offset, crossSize, out);
}

JNIEXPORT jboolean JNICALL SHADOWLIST_KIT_SWIPE_JNI(nativeIsSwipedOut)(JNIEnv*, jclass, jlong handle, jdouble offset) {
  return swipePeerOf(handle)->reveal.isSwipedOut(offset) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Where a swipe let go at offset rests, into the SWIPE_REST_* slots of out.
 */
JNIEXPORT void JNICALL SHADOWLIST_KIT_SWIPE_JNI(nativeSettle)(
  JNIEnv* env, jclass, jlong handle, jdouble offset, jdouble velocity, jdouble flingVelocity, jdoubleArray out) {
  sl::SwipeRest rest = swipePeerOf(handle)->reveal.settle(offset, velocity, flingVelocity);
  SwipeSideValue side = rest.side == sl::SwipeSide::Leading ? SWIPE_SIDE_LEADING
    : rest.side == sl::SwipeSide::Trailing                  ? SWIPE_SIDE_TRAILING
                                                            : SWIPE_SIDE_NONE;
  jdouble values[SWIPE_REST_SLOTS];
  values[SWIPE_REST_SIDE] = static_cast<jdouble>(side);
  values[SWIPE_REST_FULL] = rest.full ? 1.0 : 0.0;
  values[SWIPE_REST_OFFSET] = rest.offset;
  env->SetDoubleArrayRegion(out, 0, SWIPE_REST_SLOTS, values);
}

}
