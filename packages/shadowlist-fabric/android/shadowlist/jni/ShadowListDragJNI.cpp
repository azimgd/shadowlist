/*
 * JNI side of com.shadowlist.ShadowListDrag. Each Java drag controller owns one drag session
 * from the host layer, reached by handle: the DragReorder that places the held row, finds the
 * drop spot and shifts the others, the DragFeedback that decides the haptic ticks, and the
 * DropSettle that waits for a drop to land. Rows come in arrays the Java side reuses. Drag
 * frames allocate nothing once the row buffer has grown. Only the UI thread calls in.
 */

#include <jni.h>

#include <cstddef>
#include <string>
#include <vector>

#include <shadowlist-core/host/DragReorder.hpp>

namespace {

namespace sl = azimgd::shadowlist;

struct Peer {
  sl::DragReorder drag;
  sl::DragFeedback feedback;
  sl::DropSettle settle;
  std::vector<sl::DragRow> rows;
  std::vector<jint> indices;
  std::vector<jdouble> frames;
};

Peer* peerOf(jlong handle) {
  return reinterpret_cast<Peer*>(handle);
}

/*
 * Java carries a missing index as -1, the core as UNDEFINED_INDEX.
 */
std::size_t indexFromJint(jint index) {
  return index < 0 ? sl::UNDEFINED_INDEX : static_cast<std::size_t>(index);
}

jint jintFromIndex(std::size_t index) {
  return index == sl::UNDEFINED_INDEX ? -1 : static_cast<jint>(index);
}

std::string stringFromJava(JNIEnv* env, jstring value) {
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
 * Slots of ShadowListDrag.CONSTANT_*.
 */
enum ConstantSlot {
  CONSTANT_PRESS_DURATION_MS = 0,
  CONSTANT_PRESS_SLOP,
  CONSTANT_LIFT_ELEVATION,
  CONSTANT_SHIFT_DURATION_MS,
  CONSTANT_DROP_DURATION_MS,
  CONSTANT_BOARD_SCROLL_ARM,
  CONSTANT_SLOTS,
};

/*
 * Slots of ShadowListDrag.ROW_*, one row of the double array a frame passes in.
 */
enum RowSlot {
  ROW_LEADING = 0,
  ROW_EXTENT,
  ROW_CROSS_LEADING,
  ROW_CROSS_EXTENT,
  ROW_SLOTS,
};

}

#define SL_DRAG_JNI(name) Java_com_shadowlist_ShadowListDrag_##name

extern "C" {

JNIEXPORT jlong JNICALL SL_DRAG_JNI(nativeCreate)(JNIEnv*, jclass) {
  return reinterpret_cast<jlong>(new Peer());
}

JNIEXPORT void JNICALL SL_DRAG_JNI(nativeDestroy)(JNIEnv*, jclass, jlong handle) {
  delete peerOf(handle);
}

JNIEXPORT void JNICALL SL_DRAG_JNI(nativeConstants)(JNIEnv* env, jclass, jdoubleArray out) {
  if (out == nullptr || env->GetArrayLength(out) < CONSTANT_SLOTS) {
    return;
  }
  jdouble values[CONSTANT_SLOTS];
  values[CONSTANT_PRESS_DURATION_MS] = sl::DRAG_PRESS_DURATION_MS;
  values[CONSTANT_PRESS_SLOP] = sl::DRAG_PRESS_SLOP;
  values[CONSTANT_LIFT_ELEVATION] = sl::DRAG_LIFT_ELEVATION;
  values[CONSTANT_SHIFT_DURATION_MS] = sl::DRAG_SHIFT_DURATION_MS;
  values[CONSTANT_DROP_DURATION_MS] = sl::DRAG_DROP_DURATION_MS;
  values[CONSTANT_BOARD_SCROLL_ARM] = sl::DRAG_BOARD_SCROLL_ARM_ANDROID;
  env->SetDoubleArrayRegion(out, 0, CONSTANT_SLOTS, values);
}

JNIEXPORT jboolean JNICALL SL_DRAG_JNI(nativePressHolds)(JNIEnv*, jclass, jdouble along, jdouble cross) {
  return sl::dragPressHolds(along, cross) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jdouble JNICALL SL_DRAG_JNI(nativeAutoScrollOffset)(
  JNIEnv*,
  jclass,
  jdouble touch,
  jdouble windowSize,
  jdouble offset,
  jdouble maxOffset,
  jdouble pixelsPerDp,
  jdouble dtSeconds) {
  // Android's edge and speed are in dp.
  sl::DragAutoScrollConfig config{
    sl::DRAG_AUTO_SCROLL_ANDROID.edge * pixelsPerDp,
    sl::DRAG_AUTO_SCROLL_ANDROID.maxSpeed * pixelsPerDp};
  return sl::dragAutoScrollOffset(config, touch, windowSize, offset, maxOffset, dtSeconds);
}

JNIEXPORT void JNICALL SL_DRAG_JNI(nativeBegin)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jint index,
  jstring key,
  jdouble leading,
  jdouble extent,
  jdouble crossLeading,
  jdouble crossExtent,
  jdouble touchAlong,
  jdouble touchCross,
  jint columns) {
  sl::DragRow resting{indexFromJint(index), stringFromJava(env, key), leading, extent, crossLeading, crossExtent};
  Peer* peer = peerOf(handle);
  peer->drag.begin(resting, touchAlong, touchCross, columns > 0 ? static_cast<std::size_t>(columns) : 1);
  peer->feedback.reset(peer->drag.getOriginIndex());
  peer->settle.reset();
}

JNIEXPORT void JNICALL SL_DRAG_JNI(nativeUpdateOrigin)(JNIEnv* env, jclass, jlong handle, jint index, jstring key) {
  peerOf(handle)->drag.updateOrigin(indexFromJint(index), stringFromJava(env, key));
}

/*
 * The held row's translation from its resting frame, written into out as along then cross.
 */
JNIEXPORT void JNICALL SL_DRAG_JNI(nativePlaceRow)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jdouble touchAlong,
  jdouble touchCross,
  jdouble leading,
  jdouble extent,
  jdouble crossLeading,
  jdouble crossExtent,
  jdouble contentSize,
  jdouble crossContentSize,
  jdoubleArray out) {
  sl::DragRow resting;
  resting.leading = leading;
  resting.extent = extent;
  resting.crossLeading = crossLeading;
  resting.crossExtent = crossExtent;
  sl::DragOffset offset =
    peerOf(handle)->drag.placeRow(touchAlong, touchCross, resting, contentSize, crossContentSize);
  if (out != nullptr && env->GetArrayLength(out) >= 2) {
    jdouble values[2] = {offset.leading, offset.cross};
    env->SetDoubleArrayRegion(out, 0, 2, values);
  }
}

/*
 * Find the drop spot among count mounted rows, given by their indices and keys and ROW_SLOTS
 * values each in frames, and work out every row's shift for nativeOffsetFor.
 */
JNIEXPORT void JNICALL SL_DRAG_JNI(nativeUpdateInsertion)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jintArray indicesArray,
  jobjectArray keysArray,
  jdoubleArray framesArray,
  jint count) {
  Peer* peer = peerOf(handle);
  auto size = count > 0 ? static_cast<std::size_t>(count) : 0;
  if (indicesArray == nullptr || keysArray == nullptr || framesArray == nullptr ||
      env->GetArrayLength(indicesArray) < count || env->GetArrayLength(keysArray) < count ||
      env->GetArrayLength(framesArray) < count * ROW_SLOTS) {
    size = 0;
  }
  peer->rows.resize(size);
  if (size > 0) {
    auto& indices = peer->indices;
    auto& frames = peer->frames;
    indices.resize(size);
    frames.resize(size * ROW_SLOTS);
    env->GetIntArrayRegion(indicesArray, 0, count, indices.data());
    env->GetDoubleArrayRegion(framesArray, 0, count * ROW_SLOTS, frames.data());
    for (std::size_t row = 0; row < size; ++row) {
      sl::DragRow& target = peer->rows[row];
      target.index = indexFromJint(indices[row]);
      auto key = static_cast<jstring>(env->GetObjectArrayElement(keysArray, static_cast<jsize>(row)));
      target.key = stringFromJava(env, key);
      if (key != nullptr) {
        env->DeleteLocalRef(key);
      }
      const jdouble* frame = frames.data() + row * ROW_SLOTS;
      target.leading = frame[ROW_LEADING];
      target.extent = frame[ROW_EXTENT];
      target.crossLeading = frame[ROW_CROSS_LEADING];
      target.crossExtent = frame[ROW_CROSS_EXTENT];
    }
  }
  peer->drag.updateInsertion(peer->rows);
}

JNIEXPORT void JNICALL SL_DRAG_JNI(nativeOffsetFor)(JNIEnv* env, jclass, jlong handle, jint index, jdoubleArray out) {
  sl::DragOffset offset = peerOf(handle)->drag.offsetFor(indexFromJint(index));
  if (out != nullptr && env->GetArrayLength(out) >= 2) {
    jdouble values[2] = {offset.leading, offset.cross};
    env->SetDoubleArrayRegion(out, 0, 2, values);
  }
}

JNIEXPORT void JNICALL SL_DRAG_JNI(nativeSetHeldAway)(JNIEnv*, jclass, jlong handle, jboolean away) {
  peerOf(handle)->drag.setHeldAway(away == JNI_TRUE);
}

JNIEXPORT jboolean JNICALL SL_DRAG_JNI(nativeIsHeldAway)(JNIEnv*, jclass, jlong handle) {
  return peerOf(handle)->drag.isHeldAway() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL SL_DRAG_JNI(nativeBeginForeign)(
  JNIEnv*,
  jclass,
  jlong handle,
  jdouble extent,
  jdouble crossExtent,
  jint columns) {
  Peer* peer = peerOf(handle);
  peer->drag.beginForeign(extent, crossExtent, columns > 0 ? static_cast<std::size_t>(columns) : 1);
  peer->feedback.reset();
}

JNIEXPORT void JNICALL SL_DRAG_JNI(nativePlaceForeignCenter)(
  JNIEnv*,
  jclass,
  jlong handle,
  jdouble center,
  jdouble crossCenter) {
  peerOf(handle)->drag.placeForeignCenter(center, crossCenter);
}

JNIEXPORT void JNICALL SL_DRAG_JNI(nativeEndForeign)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->drag.endForeign();
}

JNIEXPORT jboolean JNICALL SL_DRAG_JNI(nativeIsForeign)(JNIEnv*, jclass, jlong handle) {
  return peerOf(handle)->drag.isForeign() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL SL_DRAG_JNI(nativeIsGrid)(JNIEnv*, jclass, jlong handle) {
  return peerOf(handle)->drag.isGrid() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL SL_DRAG_JNI(nativeOriginIndex)(JNIEnv*, jclass, jlong handle) {
  return jintFromIndex(peerOf(handle)->drag.getOriginIndex());
}

JNIEXPORT jint JNICALL SL_DRAG_JNI(nativeInsertionIndex)(JNIEnv*, jclass, jlong handle) {
  return jintFromIndex(peerOf(handle)->drag.getInsertionIndex());
}

JNIEXPORT jstring JNICALL SL_DRAG_JNI(nativeOriginKey)(JNIEnv* env, jclass, jlong handle) {
  return env->NewStringUTF(peerOf(handle)->drag.getOriginKey().c_str());
}

JNIEXPORT jstring JNICALL SL_DRAG_JNI(nativeInsertionKey)(JNIEnv* env, jclass, jlong handle) {
  return env->NewStringUTF(peerOf(handle)->drag.getInsertionKey().c_str());
}

JNIEXPORT jdouble JNICALL SL_DRAG_JNI(nativeLeading)(JNIEnv*, jclass, jlong handle) {
  return peerOf(handle)->drag.getLeading();
}

JNIEXPORT jdouble JNICALL SL_DRAG_JNI(nativeCrossLeading)(JNIEnv*, jclass, jlong handle) {
  return peerOf(handle)->drag.getCrossLeading();
}

JNIEXPORT void JNICALL SL_DRAG_JNI(nativeResetFeedback)(JNIEnv*, jclass, jlong handle, jint index) {
  peerOf(handle)->feedback.reset(indexFromJint(index));
}

JNIEXPORT jboolean JNICALL SL_DRAG_JNI(nativeShouldTick)(JNIEnv*, jclass, jlong handle, jdouble nowMs) {
  Peer* peer = peerOf(handle);
  return peer->feedback.shouldTick(peer->drag.getInsertionIndex(), nowMs) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL SL_DRAG_JNI(nativeBeginSettle)(JNIEnv*, jclass, jlong handle, jint insertionIndex, jdouble nowMs) {
  peerOf(handle)->settle.begin(indexFromJint(insertionIndex), nowMs);
}

/*
 * One settle frame. Returns DropSettle::Status as 0 Idle, 1 Waiting, 2 Settle, 3 GiveUp.
 */
JNIEXPORT jint JNICALL SL_DRAG_JNI(nativeTickSettle)(JNIEnv*, jclass, jlong handle, jint currentIndex, jdouble nowMs) {
  return static_cast<jint>(peerOf(handle)->settle.tick(indexFromJint(currentIndex), nowMs));
}

JNIEXPORT void JNICALL SL_DRAG_JNI(nativeResetSettle)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->settle.reset();
}

}
