package com.shadowlist;

import com.facebook.soloader.SoLoader;

/*
 * A row's swipe offset through the host layer's SwipeReveal in C++. Offsets and velocities
 * are in px along the cross axis. A positive offset shows the leading actions.
 */
final class ShadowListSwipeReveal {
  static {
    SoLoader.loadLibrary("react_codegen_ShadowListViewSpec");
  }

  /*
   * Slots of mOut, matching OutSlot in ShadowListSwipeJNI.cpp.
   */
  private static final int OUT_SIDE = 0;
  private static final int OUT_FULL = 1;
  private static final int OUT_OFFSET = 2;
  private static final int OUT_SLOTS = 3;

  /*
   * Values of restSide(), matching SideValue in ShadowListSwipeJNI.cpp.
   */
  static final int SIDE_NONE = 0;
  static final int SIDE_LEADING = 1;
  static final int SIDE_TRAILING = 2;

  /*
   * Slots of nativeConstants(), matching ConstantSlot in ShadowListSwipeJNI.cpp.
   */
  private static final int CONSTANT_FLING_VELOCITY = 0;
  private static final int CONSTANT_SETTLE_DURATION_MS = 1;

  private static final double[] CONSTANTS = nativeConstants();

  /*
   * The core's swipe constants: the fling speed in dp per second and the settle duration.
   */
  static final double FLING_VELOCITY_DP = CONSTANTS[CONSTANT_FLING_VELOCITY];
  static final long SETTLE_DURATION_MS = (long) CONSTANTS[CONSTANT_SETTLE_DURATION_MS];

  /*
   * Frees the peer of a row dropped without destroy().
   */
  private static final ShadowListPeerReclaimer RECLAIMER =
    new ShadowListPeerReclaimer(ShadowListSwipeReveal::nativeDestroy);

  private final double[] mOut = new double[OUT_SLOTS];

  // The native Peer, 0 until first use and after destroy().
  private long mHandle;

  /*
   * Free the native peer. The next begin() creates a fresh one.
   */
  void destroy() {
    if (mHandle == 0) {
      return;
    }
    nativeDestroy(mHandle);
    RECLAIMER.release(mHandle);
    mHandle = 0;
  }

  private long handle() {
    if (mHandle == 0) {
      mHandle = nativeCreate();
      RECLAIMER.register(this, mHandle);
    }
    return mHandle;
  }

  /*
   * A drag starts from startOffset. Widths are each side's total button size, 0 without actions.
   */
  void begin(
    double leadingWidth,
    double trailingWidth,
    boolean leadingFullSwipe,
    boolean trailingFullSwipe,
    double rowSize,
    double startOffset) {
    nativeBegin(handle(), leadingWidth, trailingWidth, leadingFullSwipe, trailingFullSwipe, rowSize, startOffset);
  }

  /*
   * The offset for a finger that moved translation since begin().
   */
  double drag(double translation) {
    return nativeDrag(handle(), translation);
  }

  boolean isPastFullSwipe(double offset) {
    return offset != 0.0 && nativeIsPastFullSwipe(handle(), offset);
  }

  /*
   * Where the row goes when let go. The result is restSide(), isRestFull() and restOffset().
   */
  void settle(double offset, double velocity, double flingVelocity) {
    nativeSettle(handle(), offset, velocity, flingVelocity, mOut);
  }

  int restSide() {
    return (int) mOut[OUT_SIDE];
  }

  boolean isRestFull() {
    return mOut[OUT_FULL] != 0.0;
  }

  double restOffset() {
    return mOut[OUT_OFFSET];
  }

  /*
   * The core's swipeButtonSize: the title's fitted size with room around it, at least the
   * narrowest button, scaled to pixels.
   */
  static double buttonSize(double fitted, double scale) {
    return nativeButtonSize(fitted, scale);
  }

  /*
   * The core's swipeButtonSpans and swipeRevealedSpan into out: the revealed span's start and
   * size, then each of the count buttons' start and size.
   */
  static void buttonSpans(double[] sizes, int count, double offset, boolean full, double crossSize, double[] out) {
    nativeButtonSpans(sizes, count, offset, full, crossSize, out);
  }

  private static native double[] nativeConstants();
  private static native double nativeButtonSize(double fitted, double scale);
  private static native void nativeButtonSpans(
    double[] sizes,
    int count,
    double offset,
    boolean full,
    double crossSize,
    double[] out);
  private static native long nativeCreate();
  private static native void nativeDestroy(long handle);
  private static native void nativeBegin(
    long handle,
    double leadingWidth,
    double trailingWidth,
    boolean leadingFullSwipe,
    boolean trailingFullSwipe,
    double rowSize,
    double startOffset);
  private static native double nativeDrag(long handle, double translation);
  private static native boolean nativeIsPastFullSwipe(long handle, double offset);
  private static native void nativeSettle(long handle, double offset, double velocity, double flingVelocity, double[] out);
}
