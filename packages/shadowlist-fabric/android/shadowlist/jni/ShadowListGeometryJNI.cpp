/*
 * JNI side of com.shadowlist.ShadowListGeometry. Plain calls into the host layer's pinning,
 * drag and snap math. The Java host calls these on the UI thread with arrays it reuses.
 */

#include <jni.h>

#include <shadowlist-core/host/DragReorder.hpp>
#include <shadowlist-core/host/Snap.hpp>
#include <shadowlist-core/host/StickyLayout.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace {

namespace sl = azimgd::shadowlist;

// Slots of ShadowListGeometry.STICKY_*.
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
  STICKY_LAST_OFFSET,
  STICKY_HEADER_TRANSLATION,
  STICKY_FOOTER_TRANSLATION,
  STICKY_SLOTS,
};

// Only the UI thread calls in, so one scratch buffer is enough.
std::vector<long>& scratchIndices() {
  static thread_local std::vector<long> indices;
  return indices;
}

/*
 * Copies of the grid cell arrays. They only grow, so drag frames reuse them.
 */
struct GridScratch {
  std::vector<double> leadings;
  std::vector<double> extents;
  std::vector<double> crossLeadings;
  std::vector<double> crossExtents;
  std::vector<double> shifts;
  std::vector<double> crossShifts;
};

GridScratch& gridScratch() {
  static thread_local GridScratch scratch;
  return scratch;
}

bool readGridCells(
  JNIEnv* env,
  jintArray indicesArray,
  jdoubleArray leadingsArray,
  jdoubleArray extentsArray,
  jdoubleArray crossLeadingsArray,
  jdoubleArray crossExtentsArray,
  jint count,
  sl::DragCells& cells) {
  if (count <= 0 || indicesArray == nullptr || leadingsArray == nullptr || extentsArray == nullptr ||
      crossLeadingsArray == nullptr || crossExtentsArray == nullptr || env->GetArrayLength(indicesArray) < count ||
      env->GetArrayLength(leadingsArray) < count || env->GetArrayLength(extentsArray) < count ||
      env->GetArrayLength(crossLeadingsArray) < count || env->GetArrayLength(crossExtentsArray) < count) {
    return false;
  }
  auto size = static_cast<std::size_t>(count);
  auto& indices = scratchIndices();
  auto& scratch = gridScratch();
  indices.resize(size);
  scratch.leadings.resize(size);
  scratch.extents.resize(size);
  scratch.crossLeadings.resize(size);
  scratch.crossExtents.resize(size);
  auto* rawIndices = static_cast<jint*>(env->GetPrimitiveArrayCritical(indicesArray, nullptr));
  if (rawIndices == nullptr) {
    return false;
  }
  for (std::size_t row = 0; row < size; ++row) {
    indices[row] = rawIndices[row];
  }
  env->ReleasePrimitiveArrayCritical(indicesArray, rawIndices, JNI_ABORT);
  env->GetDoubleArrayRegion(leadingsArray, 0, count, scratch.leadings.data());
  env->GetDoubleArrayRegion(extentsArray, 0, count, scratch.extents.data());
  env->GetDoubleArrayRegion(crossLeadingsArray, 0, count, scratch.crossLeadings.data());
  env->GetDoubleArrayRegion(crossExtentsArray, 0, count, scratch.crossExtents.data());
  cells = {indices.data(), scratch.leadings.data(), scratch.extents.data(), scratch.crossLeadings.data(),
    scratch.crossExtents.data(), size};
  return true;
}

}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListGeometry_stickyTranslations(
  JNIEnv* env,
  jclass /*clazz*/,
  jdoubleArray slotsArray) {
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
  state.lastOffset = slots[STICKY_LAST_OFFSET];

  auto translations = sl::stickyTranslations(input, state);
  slots[STICKY_HEADER_HIDDEN] = state.headerHidden;
  slots[STICKY_FOOTER_HIDDEN] = state.footerHidden;
  slots[STICKY_LAST_OFFSET] = state.lastOffset;
  slots[STICKY_HEADER_TRANSLATION] = translations.header;
  slots[STICKY_FOOTER_TRANSLATION] = translations.footer;
  env->SetDoubleArrayRegion(slotsArray, STICKY_HEADER_HIDDEN, STICKY_SLOTS - STICKY_HEADER_HIDDEN, slots + STICKY_HEADER_HIDDEN);
}

extern "C" JNIEXPORT jdouble JNICALL Java_com_shadowlist_ShadowListGeometry_sectionOverlayTranslation(
  JNIEnv* env,
  jclass /*clazz*/,
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
  auto* sizes = static_cast<jdouble*>(env->GetPrimitiveArrayCritical(sizesArray, nullptr));
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

extern "C" JNIEXPORT jint JNICALL Java_com_shadowlist_ShadowListGeometry_nearestSnapOffsetPx(
  JNIEnv* env,
  jclass /*clazz*/,
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

extern "C" JNIEXPORT jint JNICALL Java_com_shadowlist_ShadowListGeometry_dragInsertionPosition(
  JNIEnv* env,
  jclass /*clazz*/,
  jintArray indicesArray,
  jdoubleArray leadingsArray,
  jdoubleArray extentsArray,
  jint count,
  jint originIndex,
  jdouble center) {
  if (count <= 0 || indicesArray == nullptr || leadingsArray == nullptr || extentsArray == nullptr ||
      env->GetArrayLength(indicesArray) < count || env->GetArrayLength(leadingsArray) < count ||
      env->GetArrayLength(extentsArray) < count) {
    return -1;
  }
  auto& indices = scratchIndices();
  indices.resize(static_cast<std::size_t>(count));
  auto* rawIndices = static_cast<jint*>(env->GetPrimitiveArrayCritical(indicesArray, nullptr));
  if (rawIndices == nullptr) {
    return -1;
  }
  for (jint row = 0; row < count; ++row) {
    indices[static_cast<std::size_t>(row)] = rawIndices[row];
  }
  env->ReleasePrimitiveArrayCritical(indicesArray, rawIndices, JNI_ABORT);

  auto* leadings = static_cast<jdouble*>(env->GetPrimitiveArrayCritical(leadingsArray, nullptr));
  auto* extents = static_cast<jdouble*>(env->GetPrimitiveArrayCritical(extentsArray, nullptr));
  long position = -1;
  if (leadings != nullptr && extents != nullptr) {
    position = sl::dragInsertionPosition(
      indices.data(), leadings, extents, static_cast<std::size_t>(count), originIndex, center);
  }
  if (extents != nullptr) {
    env->ReleasePrimitiveArrayCritical(extentsArray, extents, JNI_ABORT);
  }
  if (leadings != nullptr) {
    env->ReleasePrimitiveArrayCritical(leadingsArray, leadings, JNI_ABORT);
  }
  return static_cast<jint>(position);
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListGeometry_dragShifts(
  JNIEnv* env,
  jclass /*clazz*/,
  jintArray indicesArray,
  jint count,
  jint originIndex,
  jint insertionIndex,
  jdouble draggedExtent,
  jdoubleArray shiftsArray) {
  if (count <= 0 || indicesArray == nullptr || shiftsArray == nullptr ||
      env->GetArrayLength(indicesArray) < count || env->GetArrayLength(shiftsArray) < count) {
    return;
  }
  auto* indices = static_cast<jint*>(env->GetPrimitiveArrayCritical(indicesArray, nullptr));
  auto* shifts = static_cast<jdouble*>(env->GetPrimitiveArrayCritical(shiftsArray, nullptr));
  if (indices != nullptr && shifts != nullptr) {
    for (jint row = 0; row < count; ++row) {
      shifts[row] = sl::dragShift(originIndex, insertionIndex, draggedExtent, indices[row]);
    }
  }
  if (shifts != nullptr) {
    env->ReleasePrimitiveArrayCritical(shiftsArray, shifts, 0);
  }
  if (indices != nullptr) {
    env->ReleasePrimitiveArrayCritical(indicesArray, indices, JNI_ABORT);
  }
}

extern "C" JNIEXPORT jint JNICALL Java_com_shadowlist_ShadowListGeometry_dragGridInsertionPosition(
  JNIEnv* env,
  jclass /*clazz*/,
  jintArray indicesArray,
  jdoubleArray leadingsArray,
  jdoubleArray extentsArray,
  jdoubleArray crossLeadingsArray,
  jdoubleArray crossExtentsArray,
  jint count,
  jint heldIndex,
  jdouble heldLeading,
  jdouble heldExtent,
  jdouble heldCrossLeading,
  jdouble heldCrossExtent,
  jint insertionIndex,
  jdouble center,
  jdouble crossCenter) {
  sl::DragCells cells;
  if (!readGridCells(env, indicesArray, leadingsArray, extentsArray, crossLeadingsArray, crossExtentsArray, count, cells)) {
    cells = {};
  }
  sl::DragRow held;
  held.index = heldIndex;
  held.leading = heldLeading;
  held.extent = heldExtent;
  held.crossLeading = heldCrossLeading;
  held.crossExtent = heldCrossExtent;
  return static_cast<jint>(sl::dragGridInsertionPosition(cells, held, insertionIndex, center, crossCenter));
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListGeometry_dragGridShifts(
  JNIEnv* env,
  jclass /*clazz*/,
  jintArray indicesArray,
  jdoubleArray leadingsArray,
  jdoubleArray extentsArray,
  jdoubleArray crossLeadingsArray,
  jdoubleArray crossExtentsArray,
  jint count,
  jint heldIndex,
  jdouble heldLeading,
  jdouble heldExtent,
  jdouble heldCrossLeading,
  jdouble heldCrossExtent,
  jint insertionIndex,
  jint columns,
  jdoubleArray shiftsArray,
  jdoubleArray crossShiftsArray) {
  sl::DragCells cells;
  if (shiftsArray == nullptr || crossShiftsArray == nullptr || env->GetArrayLength(shiftsArray) < count ||
      env->GetArrayLength(crossShiftsArray) < count || columns <= 0 ||
      !readGridCells(env, indicesArray, leadingsArray, extentsArray, crossLeadingsArray, crossExtentsArray, count, cells)) {
    return;
  }
  sl::DragRow held;
  held.index = heldIndex;
  held.leading = heldLeading;
  held.extent = heldExtent;
  held.crossLeading = heldCrossLeading;
  held.crossExtent = heldCrossExtent;
  auto& scratch = gridScratch();
  scratch.shifts.resize(cells.count);
  scratch.crossShifts.resize(cells.count);
  sl::dragGridShifts(cells, held, insertionIndex, static_cast<std::size_t>(columns), scratch.shifts.data(), scratch.crossShifts.data());
  env->SetDoubleArrayRegion(shiftsArray, 0, count, scratch.shifts.data());
  env->SetDoubleArrayRegion(crossShiftsArray, 0, count, scratch.crossShifts.data());
}

extern "C" JNIEXPORT jdouble JNICALL Java_com_shadowlist_ShadowListGeometry_dragHeldLeading(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jdouble touchContent,
  jdouble grabOffset,
  jdouble extent,
  jdouble contentExtent) {
  return sl::dragHeldLeading(touchContent, grabOffset, extent, contentExtent);
}

extern "C" JNIEXPORT jdouble JNICALL Java_com_shadowlist_ShadowListGeometry_dragAutoScrollOffset(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jdouble touch,
  jdouble windowSize,
  jdouble offset,
  jdouble maxOffset,
  jdouble pixelsPerDip) {
  // Android's edge and speed are in dp.
  sl::DragAutoScrollConfig config{
    sl::DRAG_AUTO_SCROLL_ANDROID.edge * pixelsPerDip,
    sl::DRAG_AUTO_SCROLL_ANDROID.maxSpeed * pixelsPerDip};
  return sl::dragAutoScrollOffset(config, touch, windowSize, offset, maxOffset);
}
