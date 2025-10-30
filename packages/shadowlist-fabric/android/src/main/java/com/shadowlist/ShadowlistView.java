package com.shadowlist;

import android.content.Context;
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
  private boolean _suspenseMvcp = false;

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
    if (_suspenseMvcp) {
      return;
    }

    if (_state == null) {
      return;
    }

    ReadableMap currentState = _state.getStateData();
    if (currentState == null) {
      return;
    }

    WritableMap map = new WritableNativeMap();

    if (currentState.hasKey("windowContainerHeight")) {
      map.putDouble("windowContainerHeight", currentState.getDouble("windowContainerHeight"));
    }
    if (currentState.hasKey("windowContainerWidth")) {
      map.putDouble("windowContainerWidth", currentState.getDouble("windowContainerWidth"));
    }
    if (currentState.hasKey("visibleStartIndex")) {
      map.putInt("visibleStartIndex", currentState.getInt("visibleStartIndex"));
    }
    if (currentState.hasKey("visibleEndIndex")) {
      map.putInt("visibleEndIndex", currentState.getInt("visibleEndIndex"));
    }
    if (currentState.hasKey("totalContainerHeight")) {
      map.putDouble("totalContainerHeight", currentState.getDouble("totalContainerHeight"));
    }
    if (currentState.hasKey("totalContainerWidth")) {
      map.putDouble("totalContainerWidth", currentState.getDouble("totalContainerWidth"));
    }

    map.putDouble("containerOffsetX", PixelUtil.toDIPFromPixel(scrollX));
    map.putDouble("containerOffsetY", PixelUtil.toDIPFromPixel(scrollY));

    _state.updateState(map);
  }

  private void init(Context context) {
    setVerticalScrollBarEnabled(true);
    setFillViewport(false);

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
  }

  public void prependElements(int size) {
    _suspenseMvcp = true;

    postDelayed(new Runnable() {
      @Override
      public void run() {
        _suspenseMvcp = false;
      }
    }, 16);
  }

  public void appendElements(int size) {
  }
}
