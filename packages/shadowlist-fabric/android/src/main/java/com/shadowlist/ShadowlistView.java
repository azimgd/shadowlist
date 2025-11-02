package com.shadowlist;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
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
  private @Nullable StateWrapper _state = null;
  private ContentContainer _contentView = null;

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
    super(context);
    init(context);
  }

  public ShadowlistView(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context);
    init(context);
  }

  @Override
  public void addView(View child, int index) {
    _contentView.addView(child, index);
  }

  @Override
  public void removeView(View child) {
    _contentView.removeView(child);
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
    if (_state == null) {
      return;
    }

    ReadableMap currentState = _state.getStateData();
    if (currentState == null) {
      return;
    }

    WritableMap map = new WritableNativeMap();

    map.putDouble("containerOffsetX", PixelUtil.toDIPFromPixel(scrollX));
    map.putDouble("containerOffsetY", PixelUtil.toDIPFromPixel(scrollY));

    _state.updateState(map);
  }

  private void init(Context context) {
    setVerticalScrollBarEnabled(true);
    setHorizontalScrollBarEnabled(true);
    setScrollbarFadingEnabled(true);
    setScrollBarStyle(View.SCROLLBARS_INSIDE_OVERLAY);
    setFillViewport(false);

    GradientDrawable scrollbarDrawable = new GradientDrawable();
    scrollbarDrawable.setShape(GradientDrawable.RECTANGLE);
    scrollbarDrawable.setColor(Color.WHITE);
    scrollbarDrawable.setCornerRadius(8);

    setVerticalScrollbarThumbDrawable(scrollbarDrawable);
    setHorizontalScrollbarThumbDrawable(scrollbarDrawable);

    _contentView = new ContentContainer(context);
    super.addView(_contentView, 0);
  }

  public void updateState(@Nullable StateWrapper stateWrapper) {
    _state = stateWrapper;

    if (_state == null) {
      return;
    }

    ReadableMap nextStateData = _state.getStateData();
    if (nextStateData == null) {
      return;
    }

    if (nextStateData.hasKey("totalContainerWidth") && nextStateData.hasKey("totalContainerHeight")) {
      float totalContainerWidth = (float) nextStateData.getDouble("totalContainerWidth");
      float totalContainerHeight = (float) nextStateData.getDouble("totalContainerHeight");

      int newContentWidth = (int) PixelUtil.toPixelFromDIP(totalContainerWidth);
      int newContentHeight = (int) PixelUtil.toPixelFromDIP(totalContainerHeight);

      _contentView.layout(0, 0, newContentWidth, newContentHeight);
    }

    if (nextStateData.hasKey("containerOffsetX") && nextStateData.hasKey("containerOffsetY")) {
      float containerOffsetX = (float) nextStateData.getDouble("containerOffsetX");
      float containerOffsetY = (float) nextStateData.getDouble("containerOffsetY");

      scrollTo(
        (int) PixelUtil.toPixelFromDIP(containerOffsetX),
        (int) PixelUtil.toPixelFromDIP(containerOffsetY)
      );
    }
  }

  public void setStartReachedEnabled(boolean enabled) {
    if (_state == null) {
      return;
    }

    WritableMap map = new WritableNativeMap();

    map.putBoolean("startReachedEnabled", enabled);

    _state.updateState(map);
  }

  public void setEndReachedEnabled(boolean enabled) {
    if (_state == null) {
      return;
    }

    WritableMap map = new WritableNativeMap();

    map.putBoolean("endReachedEnabled", enabled);

    _state.updateState(map);
  }
}
