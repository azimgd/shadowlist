package com.shadowlist;

import com.facebook.soloader.SoLoader;

/*
 * The list's pinning, drag to reorder and snap math, which lives in C++ in the host layer
 * of shadowlist-core (host/StickyLayout, host/DragReorder, host/Snap). Every call takes
 * primitive arrays the caller reuses. Scroll and drag frames allocate nothing.
 */
final class ShadowListGeometry {
  static {
    SoLoader.loadLibrary("react_codegen_ShadowListViewSpec");
  }

  /*
   * Slots of the array stickyTranslations reads and writes. The state slots carry the auto
   * hide amounts across frames, and the two results come back in the last slots.
   */
  static final int STICKY_OFFSET = 0;
  static final int STICKY_WINDOW_SIZE = 1;
  static final int STICKY_CONTENT_SIZE = 2;
  static final int STICKY_HAS_HEADER = 3;
  static final int STICKY_HEADER_SIZE = 4;
  static final int STICKY_STICKY_HEADER = 5;
  static final int STICKY_AUTO_HIDE_HEADER = 6;
  static final int STICKY_HAS_FOOTER = 7;
  static final int STICKY_FOOTER_SIZE = 8;
  static final int STICKY_FOOTER_START = 9;
  static final int STICKY_STICKY_FOOTER = 10;
  static final int STICKY_AUTO_HIDE_FOOTER = 11;
  static final int STICKY_ACCUMULATE = 12;
  static final int STICKY_HEADER_HIDDEN = 13;
  static final int STICKY_FOOTER_HIDDEN = 14;
  static final int STICKY_PREVIOUS_OFFSET = 15;
  static final int STICKY_HEADER_TRANSLATION = 16;
  static final int STICKY_FOOTER_TRANSLATION = 17;
  static final int STICKY_SLOTS = 18;

  private ShadowListGeometry() {}

  /*
   * Header and footer translations for one pin, see STICKY_*. Flags are 0 or 1.
   */
  static native void stickyTranslations(double[] slots);

  /*
   * Where the section header overlay goes, or NaN when no section is active.
   */
  static native double sectionOverlayTranslation(double[] offsets, double[] sizes, int count, double offset);

  /*
   * The snap offset nearest to target. Offsets are rounded to whole pixels first.
   */
  static native int nearestSnapOffsetPx(float[] offsetsPx, int count, int targetPx);

  /*
   * The position in the arrays of the row the held one would drop at, or -1 to stay put.
   */
  static native int dragInsertionPosition(
    int[] indices, double[] leadings, double[] extents, int count, int originIndex, double center);

  /*
   * How far each row slides to open the gap, written into shifts.
   */
  static native void dragShifts(
    int[] indices, int count, int originIndex, int insertionIndex, double draggedExtent, double[] shifts);

  /*
   * The position in the arrays of the grid element the held one would drop at, or -1 for its
   * own slot. Over no element it keeps insertionIndex.
   */
  static native int dragGridInsertionPosition(
    int[] indices, double[] leadings, double[] extents, double[] crossLeadings, double[] crossExtents, int count,
    int heldIndex, double heldLeading, double heldExtent, double heldCrossLeading, double heldCrossExtent,
    int insertionIndex, double center, double crossCenter);

  /*
   * How far each grid element slides along and across the scroll axis to open the gap.
   */
  static native void dragGridShifts(
    int[] indices, double[] leadings, double[] extents, double[] crossLeadings, double[] crossExtents, int count,
    int heldIndex, double heldLeading, double heldExtent, double heldCrossLeading, double heldCrossExtent,
    int insertionIndex, int columns, double[] shifts, double[] crossShifts);

  /*
   * Where the held row's leading edge goes, kept inside the content.
   */
  static native double dragHeldLeading(double touchContent, double grabOffset, double extent, double contentExtent);

  /*
   * The offset after this frame's auto scroll near the viewport edges, in pixels.
   */
  static native double dragAutoScrollOffset(
    double touch, double windowSize, double offset, double maxOffset, double pixelsPerDp);
}
