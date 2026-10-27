/*
 * JNI side of com.shadowlist.ShadowListDragBoard. Each drag board owns one flight from the
 * host layer, reached by handle: the core's DragBoard that decides each frame whether the
 * held row lifts out, comes back home or retargets, where its floating picture goes and how
 * long a drop waits to land, and the DragBoardScroll that scrolls the view around the board
 * sideways. The last frame's step and the last release are kept in the peer and read back
 * through their getters. Only the UI thread calls in.
 */

#include <jni.h>

#include <cstddef>
#include <string>
#include <vector>

#include <shadowlist-core/host/DragBoard.hpp>
#include <shadowlist-core/host/DragReorder.hpp>

namespace {

namespace sl = azimgd::shadowlist;

struct Peer {
  sl::DragBoard board;
  sl::DragBoardScroll scroll;
  sl::DragBoardStep step;
  sl::DragBoardRelease release;
  std::vector<std::string> candidates;
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
 * Slots of ShadowListDragBoard.CONSTANT_*.
 */
enum ConstantSlot {
  CONSTANT_LAND_TIMEOUT_MS = 0,
  CONSTANT_SLOTS,
};

/*
 * Slots of the array nativeFrame fills, ShadowListDragBoard.FRAME_*.
 */
enum FrameSlot {
  FRAME_FLOATING = 0,
  FRAME_OVERLAY_X,
  FRAME_OVERLAY_Y,
  FRAME_HIDE_HELD,
  FRAME_SLOTS,
};

}

#define SL_DRAG_BOARD_JNI(name) Java_com_shadowlist_ShadowListDragBoard_##name

extern "C" {

JNIEXPORT jlong JNICALL SL_DRAG_BOARD_JNI(nativeCreate)(JNIEnv*, jclass) {
  return reinterpret_cast<jlong>(new Peer());
}

JNIEXPORT void JNICALL SL_DRAG_BOARD_JNI(nativeDestroy)(JNIEnv*, jclass, jlong handle) {
  delete peerOf(handle);
}

JNIEXPORT void JNICALL SL_DRAG_BOARD_JNI(nativeConstants)(JNIEnv* env, jclass, jdoubleArray out) {
  if (out == nullptr || env->GetArrayLength(out) < CONSTANT_SLOTS) {
    return;
  }
  jdouble values[CONSTANT_SLOTS];
  values[CONSTANT_LAND_TIMEOUT_MS] = sl::DRAG_BOARD_LAND_TIMEOUT_MS;
  env->SetDoubleArrayRegion(out, 0, CONSTANT_SLOTS, values);
}

JNIEXPORT void JNICALL SL_DRAG_BOARD_JNI(nativeBegin)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jstring homeId,
  jstring heldKey,
  jdouble fingerX,
  jdouble fingerY,
  jdouble heldCenterX,
  jdouble heldCenterY) {
  Peer* peer = peerOf(handle);
  peer->board.begin(
    stringFromJava(env, homeId), stringFromJava(env, heldKey), {fingerX, fingerY}, {heldCenterX, heldCenterY});
  peer->scroll.begin(fingerX);
  peer->step = {};
  peer->release = {};
}

/*
 * One drag frame with count candidate ids, topmost first. Returns the step's kind as
 * DragBoardStep::Kind, and writes FRAME_SLOTS values into out.
 */
JNIEXPORT jint JNICALL SL_DRAG_BOARD_JNI(nativeFrame)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jdouble fingerX,
  jdouble fingerY,
  jdouble shownX,
  jdouble shownY,
  jobjectArray candidatesArray,
  jint count,
  jdouble nowMs,
  jdoubleArray out) {
  Peer* peer = peerOf(handle);
  auto& candidates = peer->candidates;
  candidates.clear();
  if (candidatesArray != nullptr && count > 0 && env->GetArrayLength(candidatesArray) >= count) {
    for (jint position = 0; position < count; ++position) {
      auto id = static_cast<jstring>(env->GetObjectArrayElement(candidatesArray, position));
      candidates.push_back(stringFromJava(env, id));
      if (id != nullptr) {
        env->DeleteLocalRef(id);
      }
    }
  }
  sl::DragBoardFrame frame = peer->board.frame({fingerX, fingerY}, {shownX, shownY}, candidates, nowMs);
  peer->step = frame.step;
  if (out != nullptr && env->GetArrayLength(out) >= FRAME_SLOTS) {
    jdouble values[FRAME_SLOTS];
    values[FRAME_FLOATING] = frame.floating ? 1.0 : 0.0;
    values[FRAME_OVERLAY_X] = frame.overlayCenter.x;
    values[FRAME_OVERLAY_Y] = frame.overlayCenter.y;
    values[FRAME_HIDE_HELD] = frame.hideHeld ? 1.0 : 0.0;
    env->SetDoubleArrayRegion(out, 0, FRAME_SLOTS, values);
  }
  return static_cast<jint>(frame.step.kind);
}

JNIEXPORT jstring JNICALL SL_DRAG_BOARD_JNI(nativeStepNext)(JNIEnv* env, jclass, jlong handle) {
  return env->NewStringUTF(peerOf(handle)->step.next.c_str());
}

/*
 * The held row's offset after a return home, written into out as x then y.
 */
JNIEXPORT void JNICALL SL_DRAG_BOARD_JNI(nativeReturnOffset)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jdouble placedX,
  jdouble placedY,
  jdouble nowMs,
  jdoubleArray out) {
  sl::DragPoint offset = peerOf(handle)->board.returnOffset({placedX, placedY}, nowMs);
  if (out != nullptr && env->GetArrayLength(out) >= 2) {
    jdouble values[2] = {offset.x, offset.y};
    env->SetDoubleArrayRegion(out, 0, 2, values);
  }
}

/*
 * Let go. Returns DragBoardRelease::Kind. The list id, index and key are read back with
 * nativeReleaseListId, nativeReleaseIndex and nativeReleaseKey.
 */
JNIEXPORT jint JNICALL SL_DRAG_BOARD_JNI(nativeRelease)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jint insertionIndex,
  jstring insertionKey,
  jint rowCount,
  jdouble nowMs) {
  Peer* peer = peerOf(handle);
  peer->release = peer->board.release(
    indexFromJint(insertionIndex), stringFromJava(env, insertionKey), indexFromJint(rowCount), nowMs);
  return static_cast<jint>(peer->release.kind);
}

JNIEXPORT jstring JNICALL SL_DRAG_BOARD_JNI(nativeReleaseListId)(JNIEnv* env, jclass, jlong handle) {
  return env->NewStringUTF(peerOf(handle)->release.listId.c_str());
}

JNIEXPORT jint JNICALL SL_DRAG_BOARD_JNI(nativeReleaseIndex)(JNIEnv*, jclass, jlong handle) {
  return jintFromIndex(peerOf(handle)->release.index);
}

JNIEXPORT jstring JNICALL SL_DRAG_BOARD_JNI(nativeReleaseKey)(JNIEnv* env, jclass, jlong handle) {
  return env->NewStringUTF(peerOf(handle)->release.key.c_str());
}

JNIEXPORT jboolean JNICALL SL_DRAG_BOARD_JNI(nativeLanded)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jstring listId,
  jboolean keysHoldHeldKey) {
  return peerOf(handle)->board.landed(stringFromJava(env, listId), keysHoldHeldKey == JNI_TRUE) ? JNI_TRUE
                                                                                                : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL SL_DRAG_BOARD_JNI(nativeExpired)(JNIEnv*, jclass, jlong handle, jdouble nowMs) {
  return peerOf(handle)->board.expired(nowMs) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL SL_DRAG_BOARD_JNI(nativeCancel)(JNIEnv* env, jclass, jlong handle) {
  std::string target = peerOf(handle)->board.cancel();
  return env->NewStringUTF(target.c_str());
}

JNIEXPORT void JNICALL SL_DRAG_BOARD_JNI(nativeEndFlight)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->board.endFlight();
}

JNIEXPORT void JNICALL SL_DRAG_BOARD_JNI(nativeLeave)(JNIEnv* env, jclass, jlong handle, jstring listId) {
  peerOf(handle)->board.leave(stringFromJava(env, listId));
}

/*
 * The phase as DragBoardPhase: 0 Idle, 1 AtHome, 2 Floating, 3 Landing, 4 Settling.
 */
JNIEXPORT jint JNICALL SL_DRAG_BOARD_JNI(nativePhase)(JNIEnv*, jclass, jlong handle) {
  return static_cast<jint>(peerOf(handle)->board.getPhase());
}

JNIEXPORT jstring JNICALL SL_DRAG_BOARD_JNI(nativeHeldKey)(JNIEnv* env, jclass, jlong handle) {
  return env->NewStringUTF(peerOf(handle)->board.getHeldKey().c_str());
}

JNIEXPORT jstring JNICALL SL_DRAG_BOARD_JNI(nativeTargetId)(JNIEnv* env, jclass, jlong handle) {
  return env->NewStringUTF(peerOf(handle)->board.getTargetId().c_str());
}

JNIEXPORT jlong JNICALL SL_DRAG_BOARD_JNI(nativeFlight)(JNIEnv*, jclass, jlong handle) {
  return static_cast<jlong>(peerOf(handle)->board.getFlight());
}

/*
 * This frame's sideways scroll for the view around the board, in pixels. finger is in window
 * pixels, touch is the finger in that view's viewport. Android's edge, speed and arm distance
 * are in dp.
 */
JNIEXPORT jdouble JNICALL SL_DRAG_BOARD_JNI(nativeScrollDelta)(
  JNIEnv*,
  jclass,
  jlong handle,
  jdouble finger,
  jdouble touch,
  jdouble windowSize,
  jdouble pixelsPerDp,
  jdouble dtSeconds) {
  sl::DragAutoScrollConfig config{
    sl::DRAG_BOARD_SCROLL_ANDROID.edge * pixelsPerDp,
    sl::DRAG_BOARD_SCROLL_ANDROID.maxSpeed * pixelsPerDp};
  return peerOf(handle)->scroll.delta(
    config, sl::DRAG_BOARD_SCROLL_ARM_ANDROID * pixelsPerDp, finger, touch, windowSize, dtSeconds);
}

}
