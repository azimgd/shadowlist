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
 * Sends the keyboard height to JS on every frame as onKeyboardMove events.
 * Events only fire when the Activity uses SOFT_INPUT_ADJUST_RESIZE.
 */
@ReactModule(name = ShadowListKeyboardModule.NAME)
public class ShadowListKeyboardModule extends NativeShadowListKeyboardSpec
    implements LifecycleEventListener {
  public static final String NAME = "ShadowListKeyboard";

  /*
   * Counts enable calls so several users can't undo each other's attach or detach.
   * Volatile because it is written under the lock and read on the UI thread.
   */
  private volatile int mEnabledCount = 0;
  private float mTargetDp = 0f;
  @Nullable private View mObservedView = null;
  @Nullable private KeyboardInsetsCallback mCallback = null;
  /*
   * Set by invalidate before the detach runs, so a late callback does nothing
   * instead of touching torn down state.
   */
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
      // No Activity yet. onHostResume will try again.
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
    /*
     * Attach if we aren't watching the current Activity's window. The first enable can come
     * before any Activity exists, and a recreated Activity gets a new window.
     */
    Activity activity = getReactApplicationContext().getCurrentActivity();
    View decorView = activity != null ? activity.getWindow().getDecorView() : null;
    if (decorView != null && mObservedView != decorView) {
      attach();
    }
  }

  @Override
  public void onHostPause() {
    // The callback stays attached across a pause.
  }

  @Override
  public void onHostDestroy() {
    // onHostResume attaches to the new Activity's window.
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
      /*
       * Only keyboard animations count here, like in onProgress. A system bar animation
       * must not overwrite the target height.
       */
      if ((animation.getTypeMask() & WindowInsetsCompat.Type.ime()) == 0) {
        return bounds;
      }
      int targetPx = bounds.getUpperBound().bottom;
      module.mTargetDp = PixelUtil.toDIPFromPixel(targetPx);
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

  private void emitHeight(float heightDp) {
    float progress = mTargetDp > 0 ? Math.min(1f, Math.max(0f, heightDp / mTargetDp)) : 0f;
    WritableMap payload = Arguments.createMap();
    payload.putDouble("height", heightDp);
    payload.putDouble("progress", progress);
    emitOnKeyboardMove(payload);
  }

  @Override
  public void invalidate() {
    /*
     * Set this before posting the detach, so a callback that fires before the detach runs
     * on the UI thread does nothing instead of touching torn down state.
     */
    mInvalidated = true;
    getReactApplicationContext().removeLifecycleEventListener(this);
    UiThreadUtil.runOnUiThread(this::detach);
    super.invalidate();
  }
}
