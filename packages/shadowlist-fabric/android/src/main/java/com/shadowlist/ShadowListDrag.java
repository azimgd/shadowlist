package com.shadowlist;

import com.facebook.soloader.SoLoader;

/*
 * One list's drag session in C++, from the host layer of shadowlist-core: host/DragReorder's
 * DragReorder, which places the held row, finds the drop spot and works out how far every
 * other row slides, its DragFeedback for the haptic ticks and its DropSettle for the wait after
 * a drop. The same session runs the iOS lists. A drag moves the same way on both. It also
 * carries the drag board's held-away and foreign sessions, see ShadowListDragBoard.
 * Rows come in arrays the caller reuses. UI thread only.
 */
final class ShadowListDrag {
  static {
    SoLoader.loadLibrary("react_codegen_ShadowListViewSpec");
  }

  /*
   * Slots of the array updateInsertion reads for each row, ROW_SLOTS values a row. Match
   * RowSlot in ShadowListDragJNI.cpp.
   */
  static final int ROW_LEADING = 0;
  static final int ROW_EXTENT = 1;
  static final int ROW_CROSS_LEADING = 2;
  static final int ROW_CROSS_EXTENT = 3;
  static final int ROW_SLOTS = 4;

  /*
   * What a drop settle frame says, see DropSettle::Status.
   */
  static final int SETTLE_IDLE = 0;
  static final int SETTLE_WAITING = 1;
  static final int SETTLE_SETTLE = 2;
  static final int SETTLE_GIVE_UP = 3;

  /*
   * The core's drag constants: how long a press holds and how far it may drift, in dp, the
   * lift's elevation in dp, and how long rows slide and a drop settles.
   */
  static final long PRESS_DURATION_MS;
  static final double PRESS_SLOP_DP;
  static final double LIFT_ELEVATION_DP;
  static final long SHIFT_DURATION_MS;
  static final long DROP_DURATION_MS;

  /*
   * Slots of the array nativeConstants fills. Match ConstantSlot in ShadowListDragJNI.cpp.
   */
  private static final int CONSTANT_PRESS_DURATION_MS = 0;
  private static final int CONSTANT_PRESS_SLOP = 1;
  private static final int CONSTANT_LIFT_ELEVATION = 2;
  private static final int CONSTANT_SHIFT_DURATION_MS = 3;
  private static final int CONSTANT_DROP_DURATION_MS = 4;
  private static final int CONSTANT_SLOTS = 5;

  static {
    double[] constants = new double[CONSTANT_SLOTS];
    nativeConstants(constants);
    PRESS_DURATION_MS = Math.round(constants[CONSTANT_PRESS_DURATION_MS]);
    PRESS_SLOP_DP = constants[CONSTANT_PRESS_SLOP];
    LIFT_ELEVATION_DP = constants[CONSTANT_LIFT_ELEVATION];
    SHIFT_DURATION_MS = Math.round(constants[CONSTANT_SHIFT_DURATION_MS]);
    DROP_DURATION_MS = Math.round(constants[CONSTANT_DROP_DURATION_MS]);
  }

  private static final ShadowListPeerReclaimer RECLAIMER = new ShadowListPeerReclaimer(ShadowListDrag::nativeDestroy);

  private final double[] mOffset = new double[2];
  private long mHandle;

  private long handle() {
    if (mHandle == 0) {
      mHandle = nativeCreate();
      RECLAIMER.register(this, mHandle);
    }
    return mHandle;
  }

  /*
   * Free the native session. The next call creates a fresh one.
   */
  void destroy() {
    if (mHandle == 0) {
      return;
    }
    nativeDestroy(mHandle);
    RECLAIMER.release(mHandle);
    mHandle = 0;
  }

  /*
   * Whether a press still lifts a row after the finger moved this far, in dp, along and across
   * the scroll axis. Along the axis it fails sooner. A slow scroll stays a scroll.
   */
  static boolean pressHolds(double alongDp, double crossDp) {
    return nativePressHolds(alongDp, crossDp);
  }

  /*
   * The offset after a frame of dtSeconds of auto scroll near the viewport edges, in pixels.
   */
  static double autoScrollOffset(
    double touch, double windowSize, double offset, double maxOffset, double pixelsPerDp, double dtSeconds) {
    return nativeAutoScrollOffset(touch, windowSize, offset, maxOffset, pixelsPerDp, dtSeconds);
  }

  /*
   * A pickup of the row at index, with its resting frame and where the finger touched in the
   * content, along and across the scroll axis.
   */
  void begin(
    int index, String key, double leading, double extent, double crossLeading, double crossExtent,
    double touchAlong, double touchCross, int columns) {
    nativeBegin(handle(), index, key, leading, extent, crossLeading, crossExtent, touchAlong, touchCross, columns);
  }

  /*
   * A data change during the drag can move the held row. -1 or an empty key keeps the one we have.
   */
  void updateOrigin(int index, String key) {
    nativeUpdateOrigin(handle(), index, key);
  }

  /*
   * Move the held row under the finger. Returns its translation from its resting frame, along
   * the scroll axis at 0 and across it at 1. The array is reused by the next call.
   */
  double[] placeRow(
    double touchAlong, double touchCross, double leading, double extent, double crossLeading, double crossExtent,
    double contentSize, double crossContentSize) {
    nativePlaceRow(handle(), touchAlong, touchCross, leading, extent, crossLeading, crossExtent, contentSize,
      crossContentSize, mOffset);
    return mOffset;
  }

  /*
   * Find the drop spot among the other mounted rows and work out their shifts.
   */
  void updateInsertion(int[] indices, String[] keys, double[] frames, int count) {
    nativeUpdateInsertion(handle(), indices, keys, frames, count);
  }

  /*
   * How far the row at index slides, along at 0 and across at 1. The array is reused.
   */
  double[] offsetFor(int index) {
    nativeOffsetFor(handle(), index, mOffset);
    return mOffset;
  }

  void setHeldAway(boolean away) {
    nativeSetHeldAway(handle(), away);
  }

  boolean isHeldAway() {
    return nativeIsHeldAway(handle());
  }

  void beginForeign(double extent, double crossExtent, int columns) {
    nativeBeginForeign(handle(), extent, crossExtent, columns);
  }

  void placeForeignCenter(double center, double crossCenter) {
    nativePlaceForeignCenter(handle(), center, crossCenter);
  }

  void endForeign() {
    nativeEndForeign(handle());
  }

  boolean isForeign() {
    return nativeIsForeign(handle());
  }

  boolean isGrid() {
    return nativeIsGrid(handle());
  }

  int getOriginIndex() {
    return nativeOriginIndex(handle());
  }

  int getInsertionIndex() {
    return nativeInsertionIndex(handle());
  }

  String getOriginKey() {
    return nativeOriginKey(handle());
  }

  String getInsertionKey() {
    return nativeInsertionKey(handle());
  }

  /*
   * The held row's leading edges where it was placed, which is where it was let go on drop.
   */
  double getLeading() {
    return nativeLeading(handle());
  }

  double getCrossLeading() {
    return nativeCrossLeading(handle());
  }

  /*
   * Haptics: start counting from this drop spot, or -1 to make the next spot tick. shouldTick
   * says whether the current drop spot ticks now.
   */
  void resetFeedback(int index) {
    nativeResetFeedback(handle(), index);
  }

  boolean shouldTick(double nowMs) {
    return nativeShouldTick(handle(), nowMs);
  }

  /*
   * The wait after a drop for the row to reach insertionIndex, see SETTLE_*.
   */
  void beginSettle(int insertionIndex, double nowMs) {
    nativeBeginSettle(handle(), insertionIndex, nowMs);
  }

  int tickSettle(int currentIndex, double nowMs) {
    return nativeTickSettle(handle(), currentIndex, nowMs);
  }

  void resetSettle() {
    if (mHandle != 0) {
      nativeResetSettle(mHandle);
    }
  }

  private static native long nativeCreate();
  private static native void nativeDestroy(long handle);
  private static native void nativeConstants(double[] out);
  private static native boolean nativePressHolds(double along, double cross);
  private static native double nativeAutoScrollOffset(
    double touch, double windowSize, double offset, double maxOffset, double pixelsPerDp, double dtSeconds);
  private static native void nativeBegin(
    long handle, int index, String key, double leading, double extent, double crossLeading, double crossExtent,
    double touchAlong, double touchCross, int columns);
  private static native void nativeUpdateOrigin(long handle, int index, String key);
  private static native void nativePlaceRow(
    long handle, double touchAlong, double touchCross, double leading, double extent, double crossLeading,
    double crossExtent, double contentSize, double crossContentSize, double[] out);
  private static native void nativeUpdateInsertion(long handle, int[] indices, String[] keys, double[] frames, int count);
  private static native void nativeOffsetFor(long handle, int index, double[] out);
  private static native void nativeSetHeldAway(long handle, boolean away);
  private static native boolean nativeIsHeldAway(long handle);
  private static native void nativeBeginForeign(long handle, double extent, double crossExtent, int columns);
  private static native void nativePlaceForeignCenter(long handle, double center, double crossCenter);
  private static native void nativeEndForeign(long handle);
  private static native boolean nativeIsForeign(long handle);
  private static native boolean nativeIsGrid(long handle);
  private static native int nativeOriginIndex(long handle);
  private static native int nativeInsertionIndex(long handle);
  private static native String nativeOriginKey(long handle);
  private static native String nativeInsertionKey(long handle);
  private static native double nativeLeading(long handle);
  private static native double nativeCrossLeading(long handle);
  private static native void nativeResetFeedback(long handle, int index);
  private static native boolean nativeShouldTick(long handle, double nowMs);
  private static native void nativeBeginSettle(long handle, int insertionIndex, double nowMs);
  private static native int nativeTickSettle(long handle, int currentIndex, double nowMs);
  private static native void nativeResetSettle(long handle);
}
