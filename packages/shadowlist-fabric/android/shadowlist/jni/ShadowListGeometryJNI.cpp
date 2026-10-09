/*
 * JNI side of com.shadowlist.ShadowListGeometry. Plain calls into the host layer's pinning,
 * drag, snap and page scroll math. The Java host calls these on the UI thread with arrays it
 * reuses.
 */

#include <jni.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <shadowlist-core/host/DragReorder.hpp>
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

/*
 * One scratch buffer is enough because only the UI thread calls in.
 */
std::vector<std::size_t>& scratchIndices() {
  static thread_local std::vector<std::size_t> indices;
  return indices;
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

/*
 * Copies of the grid element arrays. They only grow. Drag frames reuse them.
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
    indices[row] = indexFromJint(rawIndices[row]);
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

JNIEXPORT jint JNICALL SL_GEOMETRY_JNI(nativeDragInsertionPosition)(
  JNIEnv* env,
  jclass,
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
    indices[static_cast<std::size_t>(row)] = indexFromJint(rawIndices[row]);
  }
  env->ReleasePrimitiveArrayCritical(indicesArray, rawIndices, JNI_ABORT);

  auto* leadings = static_cast<jdouble*>(env->GetPrimitiveArrayCritical(leadingsArray, nullptr));
  auto* extents = leadings != nullptr ? static_cast<jdouble*>(env->GetPrimitiveArrayCritical(extentsArray, nullptr)) : nullptr;
  std::size_t position = sl::UNDEFINED_INDEX;
  if (leadings != nullptr && extents != nullptr) {
    position = sl::dragInsertionPosition(
      indices.data(), leadings, extents, static_cast<std::size_t>(count), indexFromJint(originIndex), center);
  }
  if (extents != nullptr) {
    env->ReleasePrimitiveArrayCritical(extentsArray, extents, JNI_ABORT);
  }
  if (leadings != nullptr) {
    env->ReleasePrimitiveArrayCritical(leadingsArray, leadings, JNI_ABORT);
  }
  return jintFromIndex(position);
}

JNIEXPORT void JNICALL SL_GEOMETRY_JNI(nativeDragShifts)(
  JNIEnv* env,
  jclass,
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
  auto* shifts = indices != nullptr ? static_cast<jdouble*>(env->GetPrimitiveArrayCritical(shiftsArray, nullptr)) : nullptr;
  if (indices != nullptr && shifts != nullptr) {
    std::size_t origin = indexFromJint(originIndex);
    std::size_t insertion = indexFromJint(insertionIndex);
    for (jint row = 0; row < count; ++row) {
      shifts[row] = sl::dragShift(origin, insertion, draggedExtent, indexFromJint(indices[row]));
    }
  }
  if (shifts != nullptr) {
    env->ReleasePrimitiveArrayCritical(shiftsArray, shifts, 0);
  }
  if (indices != nullptr) {
    env->ReleasePrimitiveArrayCritical(indicesArray, indices, JNI_ABORT);
  }
}

JNIEXPORT jint JNICALL SL_GEOMETRY_JNI(nativeDragGridInsertionPosition)(
  JNIEnv* env,
  jclass,
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
  held.index = indexFromJint(heldIndex);
  held.leading = heldLeading;
  held.extent = heldExtent;
  held.crossLeading = heldCrossLeading;
  held.crossExtent = heldCrossExtent;
  return jintFromIndex(sl::dragGridInsertionPosition(cells, held, indexFromJint(insertionIndex), center, crossCenter));
}

JNIEXPORT void JNICALL SL_GEOMETRY_JNI(nativeDragGridShifts)(
  JNIEnv* env,
  jclass,
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
  held.index = indexFromJint(heldIndex);
  held.leading = heldLeading;
  held.extent = heldExtent;
  held.crossLeading = heldCrossLeading;
  held.crossExtent = heldCrossExtent;
  auto& scratch = gridScratch();
  scratch.shifts.resize(cells.count);
  scratch.crossShifts.resize(cells.count);
  sl::dragGridShifts(cells, held, indexFromJint(insertionIndex), static_cast<std::size_t>(columns), scratch.shifts.data(), scratch.crossShifts.data());
  env->SetDoubleArrayRegion(shiftsArray, 0, count, scratch.shifts.data());
  env->SetDoubleArrayRegion(crossShiftsArray, 0, count, scratch.crossShifts.data());
}

JNIEXPORT jdouble JNICALL SL_GEOMETRY_JNI(nativeDragHeldLeading)(
  JNIEnv*,
  jclass,
  jdouble touchContent,
  jdouble grabOffset,
  jdouble extent,
  jdouble contentExtent) {
  return sl::dragHeldLeading(touchContent, grabOffset, extent, contentExtent);
}

JNIEXPORT jdouble JNICALL SL_GEOMETRY_JNI(nativeDragAutoScrollOffset)(
  JNIEnv*,
  jclass,
  jdouble touch,
  jdouble windowSize,
  jdouble offset,
  jdouble maxOffset,
  jdouble pixelsPerDp) {
  // Android's edge and speed are in dp.
  sl::DragAutoScrollConfig config{
    sl::DRAG_AUTO_SCROLL_ANDROID.edge * pixelsPerDp,
    sl::DRAG_AUTO_SCROLL_ANDROID.maxSpeed * pixelsPerDp};
  return sl::dragAutoScrollOffset(config, touch, windowSize, offset, maxOffset);
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
