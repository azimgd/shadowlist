package com.shadowlist;

import android.content.Context;
import android.widget.HorizontalScrollView;
import android.widget.ScrollView;
import android.view.View;
import android.graphics.Canvas;
import android.util.AttributeSet;
import androidx.annotation.Nullable;

import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.common.mapbuffer.MapBuffer;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.views.scroll.ReactHorizontalScrollView;
import com.facebook.react.views.scroll.ReactScrollView;
import com.facebook.react.views.view.ReactViewGroup;

public class SLContainer extends ReactScrollView {
  private boolean mOrientation;
  private SLContainerManager.OnStartReachedHandler mOnStartReachedHandler;
  private SLContainerManager.OnEndReachedHandler mOnEndReachedHandler;
  private SLContainerManager.OnVisibleChangeHandler mOnVisibleChangeHandler;

  private @Nullable StateWrapper mStateWrapper = null;

  public SLContainer(Context context) {
    super(context);
    init(context);
  }

  public SLContainer(Context context, AttributeSet attrs) {
    super(context);
    init(context);
  }

  private void init(Context context) {
    OnScrollChangeListener scrollListenerVertical = (v, scrollX, scrollY, oldScrollX, oldScrollY) -> {
      WritableMap stateMapBuffer = new WritableNativeMap();
      stateMapBuffer.putDouble("scrollPositionLeft", PixelUtil.toDIPFromPixel(oldScrollX));
      stateMapBuffer.putDouble("scrollPositionTop", PixelUtil.toDIPFromPixel(scrollY));
      mStateWrapper.updateState(stateMapBuffer);
    };

    this.setOnScrollChangeListener(scrollListenerVertical);
    this.setVerticalScrollBarEnabled(true);
  }

  public void setScrollContainerHorizontal() {
  }

  public void setScrollContainerVertical() {
  }

  public void setScrollContentLayout(float width, float height) {
    getChildAt(0).layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  public void setScrollContainerLayout(float width, float height) {
    this.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  public void setScrollContainerOffset(int x, int y) {
    this.scrollTo((int)PixelUtil.toPixelFromDIP(x), (int)PixelUtil.toPixelFromDIP(y));
  }

  @Override
  protected void onLayout(boolean changed, int l, int t, int r, int b) {
    if (!mOrientation) {
      setScrollContainerVertical();
    }
  }

  @Override
  protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
    setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.getSize(heightMeasureSpec));
  }

  @Override
  public void draw(Canvas canvas) {
    super.draw(canvas);
  }

  public void setStateWrapper(
    StateWrapper stateWrapper,
    SLContainerManager.OnStartReachedHandler onStartReachedHandler,
    SLContainerManager.OnEndReachedHandler onEndReachedHandler,
    SLContainerManager.OnVisibleChangeHandler onVisibleChangeHandler) {
    MapBuffer stateMapBuffer = stateWrapper.getStateDataMapBuffer();

    if (stateMapBuffer == null) {
      return;
    }

    if (stateMapBuffer.getBoolean(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTAINER_UPDATED)) {
      this.setScrollContainerLayout(
        (int)stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH),
        (int)stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT)
      );
    }

    if (stateMapBuffer.getBoolean(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTENT_UPDATED)) {
      this.setScrollContentLayout(
        (int)stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH),
        (int)stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT)
      );
    }

    if (stateMapBuffer.getBoolean(SLContainerManager.SLCONTAINER_STATE_SCROLL_POSITION_UPDATED)) {
      this.setScrollContainerOffset(
        (int)stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_POSITION_LEFT),
        (int)stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_POSITION_TOP)
      );
    }

    mOnStartReachedHandler = onStartReachedHandler;
    mOnEndReachedHandler = onEndReachedHandler;
    mOnVisibleChangeHandler = onVisibleChangeHandler;
    mStateWrapper = stateWrapper;
  }

  public void scrollToIndex(int index, boolean animated) {
  }

  public void scrollToOffset(int offset, boolean animated) {
  }
}
