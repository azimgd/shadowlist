package com.shadowlist;

import android.view.Choreographer;

import com.facebook.soloader.SoLoader;

/*
 * Ticks the list's ShadowListMountDriver in C++ on Choreographer frames while it has work for a
 * frame: a paced step of the mounted range, or a viewability report waiting for minimumViewTime.
 * The view pokes it after every mount, and it stops once the driver has nothing left. UI thread
 * only.
 */
final class ShadowListMountTicker implements Choreographer.FrameCallback {
  static {
    SoLoader.loadLibrary("react_codegen_ShadowListViewSpec");
  }

  private long mHandle = 0;
  private boolean mPosted = false;

  void setHandle(long handle) {
    if (handle != mHandle) {
      stop();
      mHandle = handle;
    }
  }

  /*
   * After a mount: start ticking if the driver waits for a frame.
   */
  void poke() {
    if (mPosted || mHandle == 0 || !nativeNeedsTick(mHandle)) {
      return;
    }
    mPosted = true;
    Choreographer.getInstance().postFrameCallback(this);
  }

  void stop() {
    if (mPosted) {
      Choreographer.getInstance().removeFrameCallback(this);
      mPosted = false;
    }
  }

  @Override
  public void doFrame(long frameTimeNanos) {
    mPosted = false;
    if (mHandle != 0 && nativeTick(mHandle)) {
      mPosted = true;
      Choreographer.getInstance().postFrameCallback(this);
    }
  }

  private static native boolean nativeNeedsTick(long handle);

  private static native boolean nativeTick(long handle);
}
