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
import com.facebook.react.views.view.ReactViewGroup;

public class SLContainer extends ReactViewGroup {
  private boolean mOrientation;
  private HorizontalScrollView mScrollContainerHorizontal;
  private ScrollView mScrollContainerVertical;
  private ReactViewGroup mScrollContent;
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
    mScrollContainerHorizontal = new HorizontalScrollView(context);
    mScrollContainerVertical = new ScrollView(context);

    mScrollContent = new ReactViewGroup(context);

    OnScrollChangeListener scrollListenerVertical = (v, scrollX, scrollY, oldScrollX, oldScrollY) -> {
      WritableMap stateMapBuffer = new WritableNativeMap();
      stateMapBuffer.putDouble("scrollPositionLeft", PixelUtil.toDIPFromPixel(oldScrollX));
      stateMapBuffer.putDouble("scrollPositionTop", PixelUtil.toDIPFromPixel(scrollY));
      mStateWrapper.updateState(stateMapBuffer);
    };
    OnScrollChangeListener scrollListenerHorizontal = (v, scrollX, scrollY, oldScrollX, oldScrollY) -> {
      WritableMap stateMapBuffer = new WritableNativeMap();
      stateMapBuffer.putDouble("scrollPositionLeft", PixelUtil.toDIPFromPixel(oldScrollX));
      stateMapBuffer.putDouble("scrollPositionTop", PixelUtil.toDIPFromPixel(scrollY));
      mStateWrapper.updateState(stateMapBuffer);
    };
    mScrollContainerVertical.setOnScrollChangeListener(scrollListenerVertical);
    mScrollContainerVertical.setVerticalScrollBarEnabled(true);
    mScrollContainerHorizontal.setOnScrollChangeListener(scrollListenerHorizontal);
    mScrollContainerHorizontal.setHorizontalScrollBarEnabled(true);
  }

  public void setScrollContainerHorizontal() {
    if (mOrientation) return;
    mOrientation = true;
    mScrollContainerHorizontal.addView(mScrollContent, 0);
    super.addView(mScrollContainerHorizontal, 0);
  }

  public void setScrollContainerVertical() {
    if (mOrientation) return;
    mOrientation = true;
    mScrollContainerVertical.addView(mScrollContent, 0);
    super.addView(mScrollContainerVertical, 0);
  }

  public void setScrollContentLayout(float width, float height) {
    mScrollContent.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  public void setScrollContainerLayout(float width, float height) {
    mScrollContainerHorizontal.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
    mScrollContainerVertical.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  public void setScrollContainerOffset(int x, int y) {
    mScrollContainerVertical.scrollTo((int)PixelUtil.toPixelFromDIP(x), (int)PixelUtil.toPixelFromDIP(y));
  }

  @Override
  protected void onLayout(boolean changed, int l, int t, int r, int b) {
    if (!mOrientation) {
      setScrollContainerVertical();
    }
  }

  @Override
  public void addView(View child, int index) {
    mScrollContent.addView(child, index);
  }

  @Override
  public void removeView(View child) {
    mScrollContent.removeView(child);
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
