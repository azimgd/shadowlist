package com.shadowlist;

import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.soloader.SoLoader;

/*
 * Keeps the scroll view and the core in step through the host layer's ScrollSync in C++.
 * Offsets are in dp, and results come back in mOut, reused so scroll frames allocate nothing.
 */
final class ShadowListScrollSync {
  static {
    SoLoader.loadLibrary("react_codegen_ShadowListViewSpec");
  }

  /*
   * Slots of mOut, matching OutSlot in ShadowListScrollSyncJNI.cpp.
   */
  private static final int OUT_COMMIT = 0;
  private static final int OUT_FRAME_USER_SCROLLED = 1;
  private static final int OUT_OFFSET_X = 2;
  private static final int OUT_OFFSET_Y = 3;
  private static final int OUT_OFFSET_ENABLED = 4;
  private static final int OUT_USER_SCROLLED = 5;
  private static final int OUT_SCROLL_PHASE = 6;
  private static final int OUT_COMMIT_TOKEN = 7;
  private static final int OUT_SEQUENCE = 8;
  private static final int OUT_HAS_COMMAND = 9;
  private static final int OUT_COMMAND_INDEX = 10;
  private static final int OUT_COMMAND_SEQUENCE = 11;
  private static final int OUT_COMMAND_VIEW_POSITION = 12;
  private static final int OUT_ACTION_KIND = 13;
  private static final int OUT_ACTION_X = 14;
  private static final int OUT_ACTION_Y = 15;
  private static final int OUT_ACTION_TOKEN = 16;
  private static final int OUT_ACTION_SHIFTED = 17;
  private static final int OUT_ACTION_PRESERVE_MOMENTUM = 18;
  private static final int OUT_SLOTS = 19;

  /*
   * Values of correction(), matching MountAction::Kind.
   */
  static final int ACTION_NONE = 0;
  static final int ACTION_WRITE = 1;

  private final long mNative;
  private final double[] mOut = new double[OUT_SLOTS];

  /*
   * landingTolerance is how close, in dp, our own scroll must land to count as reaching its target.
   */
  ShadowListScrollSync(double landingTolerance) {
    mNative = nativeCreate(landingTolerance);
  }

  @Override
  @SuppressWarnings("deprecation")
  protected void finalize() throws Throwable {
    try {
      nativeDestroy(mNative);
    } finally {
      super.finalize();
    }
  }

  void reset(boolean horizontal) {
    nativeReset(mNative, horizontal);
  }

  void setHorizontal(boolean horizontal) {
    nativeSetHorizontal(mNative, horizontal);
  }

  /*
   * A state mounts. Call before the content size changes.
   */
  void beginMount(
    long liveHandle,
    boolean offsetEnabled,
    double offsetX,
    double offsetY,
    double baseX,
    double baseY,
    double commitToken,
    boolean userScrolled,
    double scrollPhase,
    double concealGeneration,
    double commandSequence,
    double bandLow,
    double bandHigh) {
    nativeBeginMount(mNative, liveHandle, offsetEnabled, offsetX, offsetY, baseX, baseY, commitToken,
      userScrolled, scrollPhase, concealGeneration, commandSequence, bandLow, bandHigh);
  }

  void setApplyingContentSize(boolean applying) {
    nativeSetApplyingContentSize(mNative, applying);
  }

  /*
   * What the mounted correction does, ACTION_NONE or ACTION_WRITE. The target and flags are
   * then in actionX() and the rest. Offsets and the range are in dp along both axes.
   */
  int correction(
    double offsetX, double offsetY, double minOffset, double maxOffset,
    boolean touching, boolean moving, boolean ownsOffset) {
    return nativeCorrection(mNative, offsetX, offsetY, minOffset, maxOffset, touching, moving, ownsOffset, mOut);
  }

  double actionX() {
    return mOut[OUT_ACTION_X];
  }

  double actionY() {
    return mOut[OUT_ACTION_Y];
  }

  boolean actionPreservesMomentum() {
    return mOut[OUT_ACTION_PRESERVE_MOMENTUM] != 0.0;
  }

  /*
   * Right before writing the correction, so its scroll callback counts as ours.
   */
  void willWrite() {
    nativeWillWrite(mNative);
  }

  void didWrite(boolean moved) {
    nativeDidWrite(mNative, moved);
  }

  void endMount() {
    nativeEndMount(mNative);
  }

  /*
   * One scroll callback in dp. Returns whether it must become a state update, which is then
   * patchMap(). frameUserScrolled() tells whether the user moved the view.
   */
  boolean onScroll(double offsetX, double offsetY, double scrollPhase, boolean commitEveryFrame) {
    return nativeOnScroll(mNative, offsetX, offsetY, scrollPhase, commitEveryFrame, mOut);
  }

  boolean frameUserScrolled() {
    return mOut[OUT_FRAME_USER_SCROLLED] != 0.0;
  }

  /*
   * Mark a scroll we start, like a snap or scrollToOffset, so its frames are not the user.
   */
  void arm(double offsetX, double offsetY, boolean animated) {
    nativeArm(mNative, offsetX, offsetY, animated);
  }

  void disarm() {
    nativeDisarm(mNative);
  }

  void momentumStopped() {
    nativeMomentumStopped(mNative);
  }

  /*
   * An update with the live offset and the last echoed token, with the newest report's
   * gesture state. The result is patchMap().
   */
  void livePatch(double offsetX, double offsetY) {
    nativeLivePatch(mNative, offsetX, offsetY, true, false, 0.0, mOut);
  }

  void livePatch(double offsetX, double offsetY, boolean userScrolled, double scrollPhase) {
    nativeLivePatch(mNative, offsetX, offsetY, false, userScrolled, scrollPhase, mOut);
  }

  /*
   * The rest report after a gesture and its momentum. Returns false when none is due.
   */
  boolean clearUserScrolled(double offsetX, double offsetY) {
    return nativeClearUserScrolled(mNative, offsetX, offsetY, mOut);
  }

  void issueCommand(double index, double viewPosition, double offsetX, double offsetY, boolean momentumYielded) {
    nativeIssueCommand(mNative, index, viewPosition, offsetX, offsetY, momentumYielded, mOut);
  }

  boolean currentUserScrolled() {
    return nativeCurrentUserScrolled(mNative);
  }

  double currentScrollPhase() {
    return nativeCurrentScrollPhase(mNative);
  }

  /*
   * The last patch as a state update, matching the keys ShadowListViewState reads.
   */
  WritableMap patchMap() {
    WritableMap map = new WritableNativeMap();
    map.putDouble("containerOffsetX", mOut[OUT_OFFSET_X]);
    map.putDouble("containerOffsetY", mOut[OUT_OFFSET_Y]);
    map.putBoolean("containerOffsetEnabled", mOut[OUT_OFFSET_ENABLED] != 0.0);
    map.putBoolean("userScrolled", mOut[OUT_USER_SCROLLED] != 0.0);
    map.putDouble("scrollPhase", mOut[OUT_SCROLL_PHASE]);
    map.putDouble("commitToken", mOut[OUT_COMMIT_TOKEN]);
    map.putDouble("hostSequence", mOut[OUT_SEQUENCE]);
    if (mOut[OUT_HAS_COMMAND] != 0.0) {
      map.putDouble("containerOffsetIndex", mOut[OUT_COMMAND_INDEX]);
      map.putDouble("containerOffsetIndexSequence", mOut[OUT_COMMAND_SEQUENCE]);
      map.putDouble("containerOffsetIndexViewPosition", mOut[OUT_COMMAND_VIEW_POSITION]);
    }
    return map;
  }

  private static native long nativeCreate(double landingTolerance);
  private static native void nativeDestroy(long pointer);
  private static native void nativeReset(long pointer, boolean horizontal);
  private static native void nativeSetHorizontal(long pointer, boolean horizontal);
  private static native void nativeBeginMount(
    long pointer,
    long liveHandle,
    boolean offsetEnabled,
    double offsetX,
    double offsetY,
    double baseX,
    double baseY,
    double commitToken,
    boolean userScrolled,
    double scrollPhase,
    double concealGeneration,
    double commandSequence,
    double bandLow,
    double bandHigh);
  private static native void nativeSetApplyingContentSize(long pointer, boolean applying);
  private static native int nativeCorrection(
    long pointer,
    double offsetX,
    double offsetY,
    double minOffset,
    double maxOffset,
    boolean touching,
    boolean moving,
    boolean ownsOffset,
    double[] out);
  private static native void nativeWillWrite(long pointer);
  private static native void nativeDidWrite(long pointer, boolean moved);
  private static native void nativeEndMount(long pointer);
  private static native boolean nativeOnScroll(
    long pointer, double offsetX, double offsetY, double scrollPhase, boolean commitEveryFrame, double[] out);
  private static native void nativeArm(long pointer, double offsetX, double offsetY, boolean animated);
  private static native void nativeDisarm(long pointer);
  private static native void nativeMomentumStopped(long pointer);
  private static native void nativeLivePatch(
    long pointer, double offsetX, double offsetY, boolean current, boolean userScrolled, double scrollPhase, double[] out);
  private static native boolean nativeClearUserScrolled(long pointer, double offsetX, double offsetY, double[] out);
  private static native void nativeIssueCommand(
    long pointer, double index, double viewPosition, double offsetX, double offsetY, boolean momentumYielded, double[] out);
  private static native boolean nativeCurrentUserScrolled(long pointer);
  private static native double nativeCurrentScrollPhase(long pointer);
}
