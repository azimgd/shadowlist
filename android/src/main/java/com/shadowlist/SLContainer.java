package com.shadowlist;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.common.mapbuffer.MapBuffer;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.views.scroll.ReactHorizontalScrollView;
import com.facebook.react.views.scroll.ReactScrollView;

public class SLContainer extends ReactScrollView {
  private boolean mHorizontal;
  private ReactScrollView mScrollContainerVertical;
  private ReactHorizontalScrollView mScrollContainerHorizontal;
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
    mScrollContainerVertical = new ReactScrollView(context);
    mScrollContainerHorizontal = new ReactHorizontalScrollView(context);

    OnScrollChangeListener scrollListener = (v, scrollX, scrollY, oldScrollX, oldScrollY) -> {
      WritableMap stateMapBuffer = new WritableNativeMap();
      stateMapBuffer.putDouble("scrollPositionLeft", PixelUtil.toDIPFromPixel(oldScrollX));
      stateMapBuffer.putDouble("scrollPositionTop", PixelUtil.toDIPFromPixel(scrollY));
      mStateWrapper.updateState(stateMapBuffer);
    };

    if (mHorizontal) {
      mScrollContainerHorizontal.setOnScrollChangeListener(scrollListener);
      mScrollContainerHorizontal.setHorizontalScrollBarEnabled(true);
    } else {
      mScrollContainerVertical.setOnScrollChangeListener(scrollListener);
      mScrollContainerVertical.setVerticalScrollBarEnabled(true);
    }
  }

  @Override
  public void addView(View child, int index) {
    if (mHorizontal) {
      mScrollContainerHorizontal.addView(child, index);
    } else {
      mScrollContainerVertical.addView(child, index);
    }
  }

  @Override
  public void removeView(View child) {
    if (mHorizontal) {
      mScrollContainerHorizontal.removeView(child);
    } else {
      mScrollContainerVertical.removeView(child);
    }
  }

  @Override
  protected void onLayout(boolean changed, int l, int t, int r, int b) {
    if (mHorizontal) {
      super.addView(mScrollContainerHorizontal, 0);
    } else {
      super.addView(mScrollContainerVertical, 0);
    }
  }

  public void setHorizontal(boolean horizontal) {
    mHorizontal = horizontal;
  }

  public void setScrollContentLayout(float width, float height) {
    post(new Runnable() {
      @Override
      public void run() {
        if (mHorizontal) {
          mScrollContainerHorizontal.getChildAt(0).layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
        } else {
          mScrollContainerVertical.getChildAt(0).layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
        }
        requestLayout();
      }
    });
  }

  public void setScrollContainerLayout(float width, float height) {
    if (mHorizontal) {
      mScrollContainerHorizontal.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
    } else {
      mScrollContainerVertical.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
    }
  }

  public void setScrollContainerOffset(int x, int y) {
    if (mHorizontal) {
      mScrollContainerHorizontal.scrollTo((int)PixelUtil.toPixelFromDIP(x), (int)PixelUtil.toPixelFromDIP(y));
    } else {
      mScrollContainerVertical.scrollTo((int)PixelUtil.toPixelFromDIP(x), (int)PixelUtil.toPixelFromDIP(y));
    }
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
