package com.shadowlist;

import android.util.Log;

import com.facebook.soloader.SoLoader;

/*
 * Writes the host's scroll report into the list's C++ ShadowListLiveScroll every frame,
 * without a commit. A commit that runs for any other reason reads it, so the view only
 * sends a state update when the core needs one. See ShadowListViewState.h.
 *
 * The native side lives in the list's codegen library. If it can't be loaded, write
 * returns 0 and the view sends every frame as a state update, like before.
 */
final class ShadowListLiveScroll {
  private static final String LOG_TAG = "SL";
  private static final String LIBRARY = "react_codegen_ShadowListViewSpec";

  private static volatile boolean sAvailable = load();

  private ShadowListLiveScroll() {}

  private static boolean load() {
    try {
      SoLoader.loadLibrary(LIBRARY);
      return true;
    } catch (Throwable error) {
      Log.w(LOG_TAG, "[SL] live scroll unavailable, every frame commits: " + error);
      return false;
    }
  }

  /*
   * Store a report for the list with this handle and return its sequence, or 0 when the
   * report could not be stored and the caller must send a full state update.
   */
  static long write(
    long handle,
    double offsetX,
    double offsetY,
    boolean userScrolled,
    double scrollPhase,
    double commitToken,
    double concealGenerationAck) {
    if (!sAvailable || handle == 0) {
      return 0;
    }
    try {
      return nativeWrite(handle, offsetX, offsetY, userScrolled, scrollPhase, commitToken, concealGenerationAck);
    } catch (UnsatisfiedLinkError error) {
      Log.w(LOG_TAG, "[SL] live scroll native write missing, every frame commits: " + error);
      sAvailable = false;
      return 0;
    }
  }

  private static native long nativeWrite(
    long handle,
    double offsetX,
    double offsetY,
    boolean userScrolled,
    double scrollPhase,
    double commitToken,
    double concealGenerationAck);
}
