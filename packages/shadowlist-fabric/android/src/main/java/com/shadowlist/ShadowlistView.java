package com.shadowlist;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.views.scroll.ReactScrollView;

public class ShadowlistView extends ReactScrollView {
  /*
   * Trace the native <-> C++ core state synchronization. Mirrors the
   * SHADOWLIST_DEBUG_LOG flag in shadowlist-core/Constants.hpp (and the iOS
   * mm.* logs) and shares the same [SL] tag so all layers interleave into one
   * stream. Filter with: adb logcat -s SL
   */
  private static final boolean DEBUG_LOG = true;
  private static final String LOG_TAG = "SL";

  private static void slLog(String message) {
    if (DEBUG_LOG) {
      Log.d(LOG_TAG, "[SL] " + message);
    }
  }

  private @Nullable StateWrapper mState = null;
  private ContentContainer mContentView = null;

  private static class ContentContainer extends ViewGroup {
    public ContentContainer(Context context) {
      super(context);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    }
  }

  public ShadowlistView(Context context) {
    super(context);
    init(context);
  }

  public ShadowlistView(Context context, AttributeSet attrs) {
    super(context, attrs);
    init(context);
  }

  public ShadowlistView(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
    init(context);
  }

  @Override
  public void addView(View child, int index) {
    if (child instanceof ShadowlistElementView) {
      mContentView.addView(child, index);
      return;
    }

    if (child instanceof ShadowlistTemplateView) {
      mContentView.addView(child);
      return;
    }
  }

  @Override
  public void removeView(View child) {
    mContentView.removeView(child);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
  }

  @Override
  protected void onScrollChanged(int scrollX, int scrollY, int oldScrollX, int oldScrollY) {
    super.onScrollChanged(scrollX, scrollY, oldScrollX, oldScrollY);
    updateScrollState(scrollX, scrollY);
  }

  private void updateScrollState(int scrollX, int scrollY) {
    if (mState == null) {
      return;
    }

    WritableMap map = new WritableNativeMap();

    map.putDouble("containerOffsetX", PixelUtil.toDIPFromPixel(scrollX));
    map.putDouble("containerOffsetY", PixelUtil.toDIPFromPixel(scrollY));
    map.putBoolean("containerOffsetEnabled", false);

    slLog(String.format("java.onScrollChanged: offset=(%.1f,%.1f)",
      PixelUtil.toDIPFromPixel(scrollX), PixelUtil.toDIPFromPixel(scrollY)));
    mState.updateState(map);
  }

  private void init(Context context) {
    setVerticalScrollBarEnabled(true);
    setHorizontalScrollBarEnabled(true);
    setScrollbarFadingEnabled(true);
    setScrollBarStyle(View.SCROLLBARS_INSIDE_OVERLAY);
    setFillViewport(false);
    setClipToPadding(false);

    GradientDrawable scrollbarDrawable = new GradientDrawable();
    scrollbarDrawable.setShape(GradientDrawable.RECTANGLE);
    scrollbarDrawable.setColor(Color.WHITE);
    scrollbarDrawable.setCornerRadius(8);

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
      setVerticalScrollbarThumbDrawable(scrollbarDrawable);
      setHorizontalScrollbarThumbDrawable(scrollbarDrawable);
    }

    mContentView = new ContentContainer(context);
    super.addView(mContentView, 0);
  }

  public void updateState(@Nullable StateWrapper stateWrapper) {
    mState = stateWrapper;

    if (mState == null) {
      return;
    }

    ReadableMap nextStateData = mState.getStateData();
    if (nextStateData == null) {
      return;
    }

    slLog(String.format("java.updateState: contentSize=(%.1f,%.1f) enabled=%d offset=(%.1f,%.1f) curOffset=(%.1f,%.1f)",
      nextStateData.hasKey("totalContainerWidth") ? nextStateData.getDouble("totalContainerWidth") : 0.0,
      nextStateData.hasKey("totalContainerHeight") ? nextStateData.getDouble("totalContainerHeight") : 0.0,
      (nextStateData.hasKey("containerOffsetEnabled") && nextStateData.getBoolean("containerOffsetEnabled")) ? 1 : 0,
      nextStateData.hasKey("containerOffsetX") ? nextStateData.getDouble("containerOffsetX") : 0.0,
      nextStateData.hasKey("containerOffsetY") ? nextStateData.getDouble("containerOffsetY") : 0.0,
      PixelUtil.toDIPFromPixel(getScrollX()), PixelUtil.toDIPFromPixel(getScrollY())));

    if (nextStateData.hasKey("totalContainerWidth") && nextStateData.hasKey("totalContainerHeight")) {
      float totalContainerWidth = (float) nextStateData.getDouble("totalContainerWidth");
      float totalContainerHeight = (float) nextStateData.getDouble("totalContainerHeight");

      int newContentWidth = (int) PixelUtil.toPixelFromDIP(totalContainerWidth);
      int newContentHeight = (int) PixelUtil.toPixelFromDIP(totalContainerHeight);

      mContentView.layout(0, 0, newContentWidth, newContentHeight);
    }

    if (nextStateData.hasKey("containerOffsetEnabled") && nextStateData.getBoolean("containerOffsetEnabled")) {
      if (nextStateData.hasKey("containerOffsetX") && nextStateData.hasKey("containerOffsetY")) {
        float containerOffsetX = (float) nextStateData.getDouble("containerOffsetX");
        float containerOffsetY = (float) nextStateData.getDouble("containerOffsetY");

        scrollTo(
          (int) PixelUtil.toPixelFromDIP(containerOffsetX),
          (int) PixelUtil.toPixelFromDIP(containerOffsetY)
        );
      }
    }
  }

  public void setStartReachedEnabled(boolean enabled) {
    if (mState == null) {
      return;
    }

    WritableMap map = new WritableNativeMap();

    map.putBoolean("startReachedEnabled", enabled);

    slLog("java.cmd setStartReachedEnabled: enabled=" + (enabled ? 1 : 0));
    mState.updateState(map);
  }

  public void setEndReachedEnabled(boolean enabled) {
    if (mState == null) {
      return;
    }

    WritableMap map = new WritableNativeMap();

    map.putBoolean("endReachedEnabled", enabled);

    slLog("java.cmd setEndReachedEnabled: enabled=" + (enabled ? 1 : 0));
    mState.updateState(map);
  }

  public void scrollToIndex(int index) {
    if (mState == null) {
      return;
    }

    // Bump the nonce so the core treats this as a fresh request and re-scrolls
    // even when the index is unchanged from the previous call
    double nextNonce = 0;
    ReadableMap currentStateData = mState.getStateData();
    if (currentStateData != null && currentStateData.hasKey("containerOffsetIndexNonce")) {
      nextNonce = currentStateData.getDouble("containerOffsetIndexNonce") + 1;
    }

    WritableMap map = new WritableNativeMap();
    map.putDouble("containerOffsetIndex", (double) index);
    map.putDouble("containerOffsetIndexNonce", nextNonce);
    map.putBoolean("containerOffsetEnabled", true);
    slLog("java.cmd scrollToIndex: index=" + index + " nonce=" + (long) nextNonce);
    mState.updateState(map);
  }
}
