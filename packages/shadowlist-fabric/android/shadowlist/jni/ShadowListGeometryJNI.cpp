/*
 * JNI side of com.shadowlist.ShadowListGeometry. Plain calls into the host layer's pinning,
 * snap and page scroll math. The Java host calls these on the UI thread with arrays it
 * reuses.
 */

#include <jni.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <shadowlist-core/host/ScrollTarget.hpp>
#include <shadowlist-core/host/Snap.hpp>
#include <shadowlist-core/host/StickyLayout.hpp>

namespace {

namespace sl = azimgd::shadowlist;

/*
 * Slots of ShadowListGeometry.STICKY_*.
 */
enum StickySlot {
  STICKY_OFFSET = 0,
  STICKY_WINDOW_SIZE,
  STICKY_CONTENT_SIZE,
  STICKY_HAS_HEADER,
  STICKY_HEADER_SIZE,
  STICKY_STICKY_HEADER,
  STICKY_AUTO_HIDE_HEADER,
  STICKY_HAS_FOOTER,
  STICKY_FOOTER_SIZE,
  STICKY_FOOTER_START,
  STICKY_STICKY_FOOTER,
  STICKY_AUTO_HIDE_FOOTER,
  STICKY_ACCUMULATE,
  STICKY_HEADER_HIDDEN,
  STICKY_FOOTER_HIDDEN,
  STICKY_PREVIOUS_OFFSET,
  STICKY_HEADER_TRANSLATION,
  STICKY_FOOTER_TRANSLATION,
  STICKY_SLOTS,
};

}

#define SL_GEOMETRY_JNI(name) Java_com_shadowlist_ShadowListGeometry_##name

extern "C" {

JNIEXPORT void JNICALL SL_GEOMETRY_JNI(nativeStickyTranslations)(JNIEnv* env, jclass, jdoubleArray slotsArray) {
  if (slotsArray == nullptr || env->GetArrayLength(slotsArray) < STICKY_SLOTS) {
    return;
  }
  jdouble slots[STICKY_SLOTS];
  env->GetDoubleArrayRegion(slotsArray, 0, STICKY_SLOTS, slots);

  sl::StickyInput input;
  input.offset = slots[STICKY_OFFSET];
  input.windowSize = slots[STICKY_WINDOW_SIZE];
  input.contentSize = slots[STICKY_CONTENT_SIZE];
  input.hasHeader = slots[STICKY_HAS_HEADER] != 0.0;
  input.headerSize = slots[STICKY_HEADER_SIZE];
  input.stickyHeader = slots[STICKY_STICKY_HEADER] != 0.0;
  input.autoHideHeader = slots[STICKY_AUTO_HIDE_HEADER] != 0.0;
  input.hasFooter = slots[STICKY_HAS_FOOTER] != 0.0;
  input.footerSize = slots[STICKY_FOOTER_SIZE];
  input.footerStart = slots[STICKY_FOOTER_START];
  input.stickyFooter = slots[STICKY_STICKY_FOOTER] != 0.0;
  input.autoHideFooter = slots[STICKY_AUTO_HIDE_FOOTER] != 0.0;
  input.accumulate = slots[STICKY_ACCUMULATE] != 0.0;

  sl::StickyState state;
  state.headerHidden = slots[STICKY_HEADER_HIDDEN];
  state.footerHidden = slots[STICKY_FOOTER_HIDDEN];
  state.previousOffset = slots[STICKY_PREVIOUS_OFFSET];

  auto translations = sl::stickyTranslations(input, state);
  slots[STICKY_HEADER_HIDDEN] = state.headerHidden;
  slots[STICKY_FOOTER_HIDDEN] = state.footerHidden;
  slots[STICKY_PREVIOUS_OFFSET] = state.previousOffset;
  slots[STICKY_HEADER_TRANSLATION] = translations.header;
  slots[STICKY_FOOTER_TRANSLATION] = translations.footer;
  env->SetDoubleArrayRegion(slotsArray, STICKY_HEADER_HIDDEN, STICKY_SLOTS - STICKY_HEADER_HIDDEN, slots + STICKY_HEADER_HIDDEN);
}

JNIEXPORT jdouble JNICALL SL_GEOMETRY_JNI(nativeSectionOverlayTranslation)(
  JNIEnv* env,
  jclass,
  jdoubleArray offsetsArray,
  jdoubleArray sizesArray,
  jint count,
  jdouble offset) {
  constexpr double hidden = std::numeric_limits<double>::quiet_NaN();
  if (offsetsArray == nullptr || sizesArray == nullptr || count <= 0 ||
      env->GetArrayLength(offsetsArray) < count || env->GetArrayLength(sizesArray) < count) {
    return hidden;
  }
  auto* offsets = static_cast<jdouble*>(env->GetPrimitiveArrayCritical(offsetsArray, nullptr));
  // A failed get leaves an exception pending. Skip the second get, only releases are allowed then.
  auto* sizes = offsets != nullptr ? static_cast<jdouble*>(env->GetPrimitiveArrayCritical(sizesArray, nullptr)) : nullptr;
  sl::SectionOverlayPosition position;
  if (offsets != nullptr && sizes != nullptr) {
    position = sl::sectionOverlayPosition(offsets, sizes, static_cast<std::size_t>(count), offset);
  }
  if (sizes != nullptr) {
    env->ReleasePrimitiveArrayCritical(sizesArray, sizes, JNI_ABORT);
  }
  if (offsets != nullptr) {
    env->ReleasePrimitiveArrayCritical(offsetsArray, offsets, JNI_ABORT);
  }
  return position.visible ? position.translation : hidden;
}

JNIEXPORT jint JNICALL SL_GEOMETRY_JNI(nativeNearestSnapOffsetPx)(
  JNIEnv* env,
  jclass,
  jfloatArray offsetsArray,
  jint count,
  jint targetPx) {
  if (offsetsArray == nullptr || count <= 0 || env->GetArrayLength(offsetsArray) < count) {
    return targetPx;
  }
  std::vector<double> offsets(static_cast<std::size_t>(count));
  auto* values = static_cast<jfloat*>(env->GetPrimitiveArrayCritical(offsetsArray, nullptr));
  if (values == nullptr) {
    return targetPx;
  }
  for (jint index = 0; index < count; ++index) {
    offsets[static_cast<std::size_t>(index)] = values[index];
  }
  env->ReleasePrimitiveArrayCritical(offsetsArray, values, JNI_ABORT);
  return static_cast<jint>(sl::nearestSnapOffset(offsets, static_cast<double>(targetPx), true));
}

JNIEXPORT jdouble JNICALL SL_GEOMETRY_JNI(nativePageScrollTarget)(
  JNIEnv*,
  jclass,
  jdouble offset,
  jdouble windowAlong,
  jdouble maxOffset,
  jint direction) {
  return sl::pageScrollTarget(offset, windowAlong, maxOffset, direction);
}

}
