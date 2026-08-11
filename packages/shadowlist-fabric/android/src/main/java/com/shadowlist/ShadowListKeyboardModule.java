package com.shadowlist;

import android.app.Activity;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.LifecycleEventListener;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.UiThreadUtil;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.PixelUtil;

import java.lang.ref.WeakReference;
import java.util.List;

/*
 * Streams the per-frame keyboard (IME) height to JS as onKeyboardMove events.
 * Requires the host Activity to use SOFT_INPUT_ADJUST_RESIZE for events to fire.
 */
@ReactModule(name = ShadowListKeyboardModule.NAME)
public class ShadowListKeyboardModule extends NativeShadowListKeyboardSpec
    implements LifecycleEventListener {
  public static final String NAME = "ShadowListKeyboard";

  // Reference-counted per the TS spec so concurrent consumers can't desync each other's
  // attach/detach. volatile: written under synchronized, read on the UI thread.
  private volatile int mEnabledCount = 0;
  private float mTargetDip = 0f;
  @Nullable private View mObservedView = null;
  @Nullable private KeyboardInsetsCallback mCallback = null;
  // Set synchronously by invalidate() before its async detach is posted, so a callback
  // firing in that race window becomes a safe no-op instead of touching torn-down state.
  private volatile boolean mInvalidated = false;

  public ShadowListKeyboardModule(ReactApplicationContext context) {
    super(context);
    context.addLifecycleEventListener(this);
  }

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public synchronized void setEnabled(boolean enabled) {
    if (enabled) {
      mEnabledCount++;
      if (mEnabledCount == 1) {
        UiThreadUtil.runOnUiThread(this::attach);
      }
    } else {
      if (mEnabledCount > 0) {
        mEnabledCount--;
      }
      if (mEnabledCount == 0) {
        UiThreadUtil.runOnUiThread(this::detach);
      }
    }
  }

  private void attach() {
    Activity activity = getReactApplicationContext().getCurrentActivity();
    if (activity == null) {
      // No Activity yet: mObservedView stays null and onHostResume retries.
      return;
    }
    mObservedView = activity.getWindow().getDecorView();
    mCallback = new KeyboardInsetsCallback(this);
    ViewCompat.setWindowInsetsAnimationCallback(mObservedView, mCallback);
  }

  private void detach() {
    if (mObservedView != null) {
      ViewCompat.setWindowInsetsAnimationCallback(mObservedView, null);
      mObservedView = null;
    }
    mCallback = null;
  }

  @Override
  public void onHostResume() {
    if (mEnabledCount <= 0) {
      return;
    }
    // Attach whenever enabled but not observing the CURRENT Activity's decorView: the
    // first enable may predate any Activity, and an Activity recreation swaps the
    // decorView, which would otherwise leave the callback bound to a destroyed window.
    Activity activity = getReactApplicationContext().getCurrentActivity();
    View decorView = activity != null ? activity.getWindow().getDecorView() : null;
    if (decorView != null && mObservedView != decorView) {
      attach();
    }
  }

  @Override
  public void onHostPause() {
    // No-op: the animation callback stays attached across a pause.
  }

  @Override
  public void onHostDestroy() {
    // No-op: onHostResume re-attaches to the replacement Activity's decorView.
  }

  private static final class KeyboardInsetsCallback extends WindowInsetsAnimationCompat.Callback {
    private final WeakReference<ShadowListKeyboardModule> mModuleRef;

    KeyboardInsetsCallback(ShadowListKeyboardModule module) {
      super(WindowInsetsAnimationCompat.Callback.DISPATCH_MODE_STOP);
      mModuleRef = new WeakReference<>(module);
    }

    @NonNull
    @Override
    public WindowInsetsAnimationCompat.BoundsCompat onStart(
      @NonNull WindowInsetsAnimationCompat animation,
      @NonNull WindowInsetsAnimationCompat.BoundsCompat bounds) {
      ShadowListKeyboardModule module = mModuleRef.get();
      if (module == null || module.mInvalidated) {
        return bounds;
      }
      // Same IME-only filter as onProgress: an unrelated concurrent animation (e.g. a
      // system-bar visibility change) must not overwrite mTargetDip with its own bound.
      if ((animation.getTypeMask() & WindowInsetsCompat.Type.ime()) == 0) {
        return bounds;
      }
      int targetPx = bounds.getUpperBound().bottom;
      module.mTargetDip = PixelUtil.toDIPFromPixel(targetPx);
      return bounds;
    }

    @NonNull
    @Override
    public WindowInsetsCompat onProgress(
      @NonNull WindowInsetsCompat insets,
      @NonNull List<WindowInsetsAnimationCompat> runningAnimations) {
      ShadowListKeyboardModule module = mModuleRef.get();
      if (module == null || module.mInvalidated) {
        return insets;
      }
      boolean animatingIme = false;
      for (WindowInsetsAnimationCompat animation : runningAnimations) {
        if ((animation.getTypeMask() & WindowInsetsCompat.Type.ime()) != 0) {
          animatingIme = true;
          break;
        }
      }
      if (!animatingIme) {
        return insets;
      }
      int imeBottomPx = insets.getInsets(WindowInsetsCompat.Type.ime()).bottom;
      module.emitHeight(PixelUtil.toDIPFromPixel(imeBottomPx));
      return insets;
    }
  }

  private void emitHeight(float heightDip) {
    float progress = mTargetDip > 0 ? Math.min(1f, Math.max(0f, heightDip / mTargetDip)) : 0f;
    WritableMap payload = Arguments.createMap();
    payload.putDouble("height", heightDip);
    payload.putDouble("progress", progress);
    emitOnKeyboardMove(payload);
  }

  @Override
  public void invalidate() {
    // Set synchronously, before the async detach is even posted, so a callback firing
    // in the window between this call and detach() actually running on the UI thread
    // sees the flag and no-ops instead of touching state super.invalidate() tears down.
    mInvalidated = true;
    getReactApplicationContext().removeLifecycleEventListener(this);
    UiThreadUtil.runOnUiThread(this::detach);
    super.invalidate();
  }
}
