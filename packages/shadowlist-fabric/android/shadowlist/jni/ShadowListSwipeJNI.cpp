/*
 * JNI side of com.shadowlist.ShadowListSwipeReveal. Each swiped row owns one SwipeReveal from
 * the host layer, reached by handle. Only the UI thread calls in.
 */

#include <jni.h>

#include <shadowlist-core/host/SwipeReveal.hpp>

namespace {

namespace sl = azimgd::shadowlist;

/*
 * Slots of ShadowListSwipeReveal.OUT_*.
 */
enum OutSlot {
  OUT_SIDE = 0,
  OUT_FULL,
  OUT_OFFSET,
  OUT_SLOTS,
};

/*
 * Values of OUT_SIDE, matching ShadowListSwipeReveal.SIDE_*.
 */
enum SideValue {
  SIDE_NONE = 0,
  SIDE_LEADING,
  SIDE_TRAILING,
};

/*
 * A row's swipe, begun again at the start of every drag.
 */
struct Peer {
  sl::SwipeReveal reveal;
};

Peer* peerOf(jlong handle) {
  return reinterpret_cast<Peer*>(handle);
}

SideValue sideValue(sl::SwipeSide side) {
  switch (side) {
    case sl::SwipeSide::Leading:
      return SIDE_LEADING;
    case sl::SwipeSide::Trailing:
      return SIDE_TRAILING;
    case sl::SwipeSide::None:
      return SIDE_NONE;
  }
  return SIDE_NONE;
}

}

#define SL_SWIPE_JNI(name) Java_com_shadowlist_ShadowListSwipeReveal_##name

extern "C" {

JNIEXPORT jlong JNICALL SL_SWIPE_JNI(nativeCreate)(JNIEnv*, jclass) {
  return reinterpret_cast<jlong>(new Peer());
}

JNIEXPORT void JNICALL SL_SWIPE_JNI(nativeDestroy)(JNIEnv*, jclass, jlong handle) {
  delete peerOf(handle);
}

JNIEXPORT void JNICALL SL_SWIPE_JNI(nativeBegin)(
  JNIEnv*,
  jclass,
  jlong handle,
  jdouble leadingWidth,
  jdouble trailingWidth,
  jboolean leadingFullSwipe,
  jboolean trailingFullSwipe,
  jdouble rowSize,
  jdouble startOffset) {
  sl::SwipeSpec spec;
  spec.leadingWidth = leadingWidth;
  spec.trailingWidth = trailingWidth;
  spec.leadingFullSwipe = leadingFullSwipe == JNI_TRUE;
  spec.trailingFullSwipe = trailingFullSwipe == JNI_TRUE;
  spec.rowSize = rowSize;
  peerOf(handle)->reveal.begin(spec, startOffset);
}

JNIEXPORT jdouble JNICALL SL_SWIPE_JNI(nativeDrag)(JNIEnv*, jclass, jlong handle, jdouble translation) {
  return peerOf(handle)->reveal.drag(translation);
}

JNIEXPORT jboolean JNICALL SL_SWIPE_JNI(nativeIsPastFullSwipe)(JNIEnv*, jclass, jlong handle, jdouble offset) {
  return peerOf(handle)->reveal.isPastFullSwipe(offset) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL SL_SWIPE_JNI(nativeSettle)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jdouble offset,
  jdouble velocity,
  jdouble flingVelocity,
  jdoubleArray out) {
  sl::SwipeRest rest = peerOf(handle)->reveal.settle(offset, velocity, flingVelocity);
  jdouble slots[OUT_SLOTS];
  slots[OUT_SIDE] = sideValue(rest.side);
  slots[OUT_FULL] = rest.full ? 1.0 : 0.0;
  slots[OUT_OFFSET] = rest.offset;
  env->SetDoubleArrayRegion(out, 0, OUT_SLOTS, slots);
}

}
