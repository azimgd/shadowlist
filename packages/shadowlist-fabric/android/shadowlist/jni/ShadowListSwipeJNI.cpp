/*
 * JNI side of com.shadowlist.ShadowListSwipeReveal. Each swiped row owns one SwipeReveal from
 * the host layer, reached by handle. Only the UI thread calls in.
 */

#include <jni.h>

#include <algorithm>
#include <cstddef>
#include <vector>

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
 * Slots of ShadowListSwipeReveal.CONSTANT_*.
 */
enum ConstantSlot {
  CONSTANT_FLING_VELOCITY = 0,
  CONSTANT_SETTLE_DURATION_MS,
  CONSTANT_SLOTS,
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

JNIEXPORT jdoubleArray JNICALL SL_SWIPE_JNI(nativeConstants)(JNIEnv* env, jclass) {
  jdouble values[CONSTANT_SLOTS];
  values[CONSTANT_FLING_VELOCITY] = sl::SWIPE_FLING_VELOCITY;
  values[CONSTANT_SETTLE_DURATION_MS] = sl::SWIPE_SETTLE_DURATION_MS;
  jdoubleArray array = env->NewDoubleArray(CONSTANT_SLOTS);
  if (array != nullptr) {
    env->SetDoubleArrayRegion(array, 0, CONSTANT_SLOTS, values);
  }
  return array;
}

JNIEXPORT jdouble JNICALL SL_SWIPE_JNI(nativeButtonSize)(JNIEnv*, jclass, jdouble fitted, jdouble scale) {
  return sl::swipeButtonSize(fitted, scale);
}

/*
 * Where the revealed side's count buttons go, into out: the revealed span's start and size,
 * then each button's start and size.
 */
JNIEXPORT void JNICALL SL_SWIPE_JNI(nativeButtonSpans)(
  JNIEnv* env,
  jclass,
  jdoubleArray sizes,
  jint count,
  jdouble offset,
  jboolean full,
  jdouble crossSize,
  jdoubleArray out) {
  std::size_t buttons = static_cast<std::size_t>(std::max(count, 0));
  std::vector<double> values(buttons);
  if (buttons > 0) {
    env->GetDoubleArrayRegion(sizes, 0, static_cast<jsize>(buttons), values.data());
  }
  std::vector<sl::SwipeSpan> spans;
  sl::swipeButtonSpans(values, offset, full == JNI_TRUE, crossSize, spans);
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

}
