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
 * Streams the per-frame keyboard (IME) height to JS as onKeyboardMove events.
 * Requires the host Activity to use SOFT_INPUT_ADJUST_RESIZE for events to fire.
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
        // Fully-shown IME height, used as the denominator for the progress fraction.
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
