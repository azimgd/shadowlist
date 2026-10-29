package com.shadowlist;

import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeArray;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.soloader.SoLoader;

/*
 * Keeps the scroll view and the core in step through the host layer's ScrollSync in C++.
 * Offsets are in dp, and results come back in mOut, reused so a scroll frame that sends no state
 * update allocates nothing.
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
  private static final int OUT_COMMAND_VIEW_OFFSET = 19;
  private static final int OUT_COMMAND_ANIMATED = 20;
  private static final int OUT_HAS_ANCHOR_REQUEST = 21;
  private static final int OUT_ANCHOR_REQUEST_SEQUENCE = 22;
  private static final int OUT_FRAME_LANDED = 23;
  private static final int OUT_SLOTS = 24;

  /*
   * The mOut slot of each slot of the "patch" array, in the order of ShadowListPatchSlot in
   * ShadowListViewState.h.
   */
  private static final int[] PATCH_SLOTS = {
    OUT_OFFSET_X,
    OUT_OFFSET_Y,
    OUT_OFFSET_ENABLED,
    OUT_USER_SCROLLED,
    OUT_SCROLL_PHASE,
    OUT_COMMIT_TOKEN,
    OUT_SEQUENCE,
    OUT_HAS_COMMAND,
    OUT_COMMAND_INDEX,
    OUT_COMMAND_SEQUENCE,
    OUT_COMMAND_VIEW_POSITION,
    OUT_COMMAND_VIEW_OFFSET,
    OUT_COMMAND_ANIMATED,
    OUT_HAS_ANCHOR_REQUEST,
    OUT_ANCHOR_REQUEST_SEQUENCE,
  };

  /*
   * Values of correction(), matching MountAction::Kind.
   */
  static final int ACTION_NONE = 0;
  static final int ACTION_WRITE = 1;
  static final int ACTION_ANIMATE = 3;

  /*
   * Frees the peer of a view dropped without destroy().
   */
  private static final ShadowListPeerReclaimer RECLAIMER =
    new ShadowListPeerReclaimer(ShadowListScrollSync::nativeDestroy);

  private final double mLandingTolerance;
  private final double[] mOut = new double[OUT_SLOTS];
  private boolean mHorizontal;

  /*
   * The native Peer, 0 until first use and after destroy().
   */
  private long mHandle;

  /*
   * landingTolerance is how close, in dp, our own scroll must land to count as reaching its target.
   */
  ShadowListScrollSync(double landingTolerance) {
    mLandingTolerance = landingTolerance;
  }

  /*
   * Free the native peer. The next call creates a fresh one. The view calls this when Fabric
   * drops it.
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
      mHandle = nativeCreate(mLandingTolerance);
      RECLAIMER.register(this, mHandle);
      nativeSetHorizontal(mHandle, mHorizontal);
    }
    return mHandle;
  }

  void setHorizontal(boolean horizontal) {
    mHorizontal = horizontal;
    if (mHandle != 0) {
      nativeSetHorizontal(mHandle, horizontal);
    }
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
    double animationSequence,
    double animationOffset,
    double bandLow,
    double bandHigh) {
    nativeBeginMount(handle(), liveHandle, offsetEnabled, offsetX, offsetY, baseX, baseY, commitToken,
      userScrolled, scrollPhase, concealGeneration, commandSequence, animationSequence, animationOffset,
      bandLow, bandHigh);
  }

  void setApplyingContentSize(boolean applying) {
    nativeSetApplyingContentSize(handle(), applying);
  }

  /*
   * What the mounted correction does, ACTION_NONE, ACTION_WRITE or ACTION_ANIMATE. The target and flags are
   * then in getActionX() and the rest. Offsets and the range are in dp along both axes.
   */
  int correction(
    double offsetX, double offsetY, double minOffset, double maxOffset,
    boolean touching, boolean moving, boolean ownsOffset) {
    return nativeCorrection(handle(), offsetX, offsetY, minOffset, maxOffset, touching, moving, ownsOffset, mOut);
  }

  double getActionX() {
    return mOut[OUT_ACTION_X];
  }

  double getActionY() {
    return mOut[OUT_ACTION_Y];
  }

  boolean actionPreservesMomentum() {
    return mOut[OUT_ACTION_PRESERVE_MOMENTUM] != 0.0;
  }

  /*
   * Right before writing the correction. Its scroll callback then counts as ours.
   */
  void willWrite() {
    nativeWillWrite(handle());
  }

  void didWrite(boolean moved) {
    nativeDidWrite(handle(), moved);
  }

  void endMount() {
    nativeEndMount(handle());
  }

  /*
   * One scroll callback in dp. Returns whether it must become a state update, which is then
   * patchMap(). isFrameUserScrolled() tells whether the user moved the view.
   */
  boolean onScroll(double offsetX, double offsetY, int scrollPhase, boolean commitEveryFrame) {
    return nativeOnScroll(handle(), offsetX, offsetY, (double) scrollPhase, commitEveryFrame, mOut);
  }

  boolean isFrameUserScrolled() {
    return mOut[OUT_FRAME_USER_SCROLLED] != 0.0;
  }

  /*
   * Whether the last frame ended an animated move of ours on its target.
   */
  boolean isFrameLanded() {
    return mOut[OUT_FRAME_LANDED] != 0.0;
  }

  /*
   * Mark a scroll we start, like a snap or scrollToOffset. Its frames are not the user.
   */
  void arm(double offsetX, double offsetY, boolean animated) {
    nativeArm(handle(), offsetX, offsetY, animated);
  }

  void disarm() {
    nativeDisarm(handle());
  }

  void momentumStopped() {
    nativeMomentumStopped(handle());
  }

  /*
   * An update with the live offset and the last echoed token, with the newest report's
   * gesture state. The result is patchMap().
   */
  void livePatch(double offsetX, double offsetY) {
    nativeLivePatch(handle(), offsetX, offsetY, true, false, 0.0, mOut);
  }

  void livePatch(double offsetX, double offsetY, boolean userScrolled, int scrollPhase) {
    nativeLivePatch(handle(), offsetX, offsetY, false, userScrolled, (double) scrollPhase, mOut);
  }

  /*
   * The rest report after a gesture and its momentum. Returns false when none is due.
   */
  boolean clearUserScrolled(double offsetX, double offsetY) {
    return nativeClearUserScrolled(handle(), offsetX, offsetY, mOut);
  }

  void issueCommand(
    double index, double viewPosition, double viewOffset, boolean animated,
    double offsetX, double offsetY, boolean momentumYielded) {
    nativeIssueCommand(handle(), index, viewPosition, viewOffset, animated, offsetX, offsetY, momentumYielded, mOut);
  }

  /*
   * An animated command's animation ended. Returns false when none waits. Otherwise the
   * command without the animation is patchMap().
   */
  boolean land(double offsetX, double offsetY) {
    return nativeLand(handle(), offsetX, offsetY, mOut);
  }

  boolean isLanding() {
    return mHandle != 0 && nativeIsLanding(mHandle);
  }

  /*
   * Ask the core for the anchor at the live offset. The update is patchMap().
   */
  void requestAnchor(double offsetX, double offsetY) {
    nativeRequestAnchor(handle(), offsetX, offsetY, mOut);
  }

  boolean isCurrentUserScrolled() {
    return nativeIsCurrentUserScrolled(handle());
  }

  int getCurrentScrollPhase() {
    return (int) nativeGetCurrentScrollPhase(handle());
  }

  /*
   * The last patch as a state update: one "patch" array in the order of PATCH_SLOTS, which
   * ShadowListViewState reads as a ScrollPatch like the iOS host's.
   */
  WritableMap patchMap() {
    WritableNativeArray patch = new WritableNativeArray();
    for (int slot : PATCH_SLOTS) {
      patch.pushDouble(mOut[slot]);
    }
    WritableMap map = new WritableNativeMap();
    map.putArray("patch", patch);
    return map;
  }

  private static native long nativeCreate(double landingTolerance);
  private static native void nativeDestroy(long handle);
  private static native void nativeSetHorizontal(long handle, boolean horizontal);
  private static native void nativeBeginMount(
    long handle,
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
    double animationSequence,
    double animationOffset,
    double bandLow,
    double bandHigh);
  private static native void nativeSetApplyingContentSize(long handle, boolean applying);
  private static native int nativeCorrection(
    long handle,
    double offsetX,
    double offsetY,
    double minOffset,
    double maxOffset,
    boolean touching,
    boolean moving,
    boolean ownsOffset,
    double[] out);
  private static native void nativeWillWrite(long handle);
  private static native void nativeDidWrite(long handle, boolean moved);
  private static native void nativeEndMount(long handle);
  private static native boolean nativeOnScroll(
    long handle, double offsetX, double offsetY, double scrollPhase, boolean commitEveryFrame, double[] out);
  private static native void nativeArm(long handle, double offsetX, double offsetY, boolean animated);
  private static native void nativeDisarm(long handle);
  private static native void nativeMomentumStopped(long handle);
  private static native void nativeLivePatch(
    long handle, double offsetX, double offsetY, boolean current, boolean userScrolled, double scrollPhase, double[] out);
  private static native boolean nativeClearUserScrolled(long handle, double offsetX, double offsetY, double[] out);
  private static native void nativeIssueCommand(
    long handle, double index, double viewPosition, double viewOffset, boolean animated,
    double offsetX, double offsetY, boolean momentumYielded, double[] out);
  private static native boolean nativeLand(long handle, double offsetX, double offsetY, double[] out);
  private static native boolean nativeIsLanding(long handle);
  private static native void nativeRequestAnchor(long handle, double offsetX, double offsetY, double[] out);
  private static native boolean nativeIsCurrentUserScrolled(long handle);
  private static native double nativeGetCurrentScrollPhase(long handle);
}
