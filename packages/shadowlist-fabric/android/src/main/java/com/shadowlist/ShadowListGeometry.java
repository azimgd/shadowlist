package com.shadowlist;

import com.facebook.soloader.SoLoader;

/*
 * The list's pinning, snap and page scroll math, which lives in C++ in the host layer of
 * shadowlist-core (host/StickyLayout, host/Snap, host/ScrollTarget). Every call takes
 * primitive arrays the caller reuses. Scroll frames allocate nothing. The drag runs in its
 * own session, see ShadowListDrag.
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
  static void stickyTranslations(double[] slots) {
    nativeStickyTranslations(slots);
  }

  /*
   * Where the section header overlay goes, or NaN when no section is active.
   */
  static double sectionOverlayTranslation(double[] offsets, double[] sizes, int count, double offset) {
    return nativeSectionOverlayTranslation(offsets, sizes, count, offset);
  }

  /*
   * The snap offset nearest to target. Offsets are rounded to whole pixels first.
   */
  static int nearestSnapOffsetPx(float[] offsetsPx, int count, int targetPx) {
    return nativeNearestSnapOffsetPx(offsetsPx, count, targetPx);
  }

  /*
   * Where an accessibility page scroll lands: one window toward the end for a positive
   * direction, toward the start for a negative one, inside 0 and maxOffset.
   */
  static double pageScrollTarget(double offset, double windowAlong, double maxOffset, int direction) {
    return nativePageScrollTarget(offset, windowAlong, maxOffset, direction);
  }

  private static native void nativeStickyTranslations(double[] slots);
  private static native double nativeSectionOverlayTranslation(double[] offsets, double[] sizes, int count, double offset);
  private static native int nativeNearestSnapOffsetPx(float[] offsetsPx, int count, int targetPx);
  private static native double nativePageScrollTarget(double offset, double windowAlong, double maxOffset, int direction);
}
