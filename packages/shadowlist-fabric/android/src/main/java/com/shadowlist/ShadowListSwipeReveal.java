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
    mHandle = 0;
  }

  /*
   * A fallback for a row dropped without destroy().
   */
  @Override
  @SuppressWarnings("deprecation")
  protected void finalize() throws Throwable {
    try {
      destroy();
    } finally {
      super.finalize();
    }
  }

  private long handle() {
    if (mHandle == 0) {
      mHandle = nativeCreate();
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
