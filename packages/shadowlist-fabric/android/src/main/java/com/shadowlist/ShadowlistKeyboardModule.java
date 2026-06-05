package com.shadowlist;

import android.app.Activity;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.UiThreadUtil;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.PixelUtil;

import java.util.List;

/*
 * TurboModule that streams the system keyboard (IME) frame to JS as onKeyboardMove
 * events (see NativeShadowlistKeyboard.ts / useKeyboardAnimation).
 *
 * Android reads the genuine per-frame inset via WindowInsetsAnimationCompat: onProgress
 * fires every frame of both the system open/close animation and the interactive
 * swipe-to-dismiss, so this is true frame-accurate tracking.
 *
 * Note: the IME animation callback fires reliably when the host Activity uses
 * SOFT_INPUT_ADJUST_RESIZE (and, for edge-to-edge apps, decorFitsSystemWindows=false).
 * We do not mutate the host window here (it is an app-global concern); if a host sees
 * no events, ensure its windowSoftInputMode is adjustResize.
 *
 * NativeShadowlistKeyboardSpec and emitOnKeyboardMove are generated from the JS spec.
 */
@ReactModule(name = ShadowlistKeyboardModule.NAME)
public class ShadowlistKeyboardModule extends NativeShadowlistKeyboardSpec {
  public static final String NAME = "ShadowlistKeyboard";

  private boolean mEnabled = false;
  private float mTargetDip = 0f;
  @Nullable private View mObservedView = null;

  public ShadowlistKeyboardModule(ReactApplicationContext context) {
    super(context);
  }

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public void setEnabled(boolean enabled) {
    if (enabled == mEnabled) {
      return;
    }
    mEnabled = enabled;
    UiThreadUtil.runOnUiThread(() -> {
      if (enabled) {
        attach();
      } else {
        detach();
      }
    });
  }

  private void attach() {
    Activity activity = getReactApplicationContext().getCurrentActivity();
    if (activity == null) {
      return;
    }
    mObservedView = activity.getWindow().getDecorView();
    ViewCompat.setWindowInsetsAnimationCallback(mObservedView, mCallback);
  }

  private void detach() {
    if (mObservedView != null) {
      ViewCompat.setWindowInsetsAnimationCallback(mObservedView, null);
      mObservedView = null;
    }
  }

  private final WindowInsetsAnimationCompat.Callback mCallback =
    new WindowInsetsAnimationCompat.Callback(WindowInsetsAnimationCompat.Callback.DISPATCH_MODE_STOP) {
      @NonNull
      @Override
      public WindowInsetsAnimationCompat.BoundsCompat onStart(
        @NonNull WindowInsetsAnimationCompat animation,
        @NonNull WindowInsetsAnimationCompat.BoundsCompat bounds) {
        // getUpperBound() is the fully-shown IME inset (androidx Insets); capture the
        // target height for the progress fraction.
        int targetPx = bounds.getUpperBound().bottom;
        mTargetDip = PixelUtil.toDIPFromPixel(targetPx);
        return bounds;
      }

      @NonNull
      @Override
      public WindowInsetsCompat onProgress(
        @NonNull WindowInsetsCompat insets,
        @NonNull List<WindowInsetsAnimationCompat> runningAnimations) {
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
        emitHeight(PixelUtil.toDIPFromPixel(imeBottomPx));
        return insets;
      }
    };

  private void emitHeight(float heightDip) {
    float progress = mTargetDip > 0 ? Math.min(1f, Math.max(0f, heightDip / mTargetDip)) : 0f;
    WritableMap payload = Arguments.createMap();
    payload.putDouble("height", heightDip);
    payload.putDouble("progress", progress);
    emitOnKeyboardMove(payload);
  }

  @Override
  public void invalidate() {
    UiThreadUtil.runOnUiThread(this::detach);
    super.invalidate();
  }
}
