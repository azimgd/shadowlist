package com.shadowlist;

import android.content.Context;
import android.widget.HorizontalScrollView;
import android.widget.ScrollView;
import android.view.View;
import android.graphics.Canvas;
import android.util.AttributeSet;
import androidx.annotation.Nullable;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.common.mapbuffer.MapBuffer;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.views.view.ReactViewGroup;

public class SLContainer extends ReactViewGroup {
  private boolean mOrientation;
  private SwipeRefreshLayout mScrollContainerRefreshVertical;
  private SwipeRefreshLayout mScrollContainerRefreshHorizontal;
  private HorizontalScrollView mScrollContainerHorizontal;
  private ScrollView mScrollContainerVertical;
  private ReactViewGroup mScrollContent;

  private SLContainerChildrenManager mContainerChildrenManager;
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
    mScrollContainerRefreshVertical = new SwipeRefreshLayout(context);
    mScrollContainerRefreshHorizontal = new SwipeRefreshLayout(context);
    mScrollContainerHorizontal = new HorizontalScrollView(context);
    mScrollContainerVertical = new ScrollView(context);

    mScrollContent = new ReactViewGroup(context);
    mContainerChildrenManager = new SLContainerChildrenManager(mScrollContent);

    SwipeRefreshLayout.OnRefreshListener refreshListener = () -> {
    };
    OnScrollChangeListener scrollListener = (v, scrollX, scrollY, oldScrollX, oldScrollY) -> {
      WritableNativeMap mapBuffer = new WritableNativeMap();

      mapBuffer.putDouble("scrollPositionTop", PixelUtil.toDIPFromPixel(scrollY));
      mapBuffer.putDouble("scrollPositionLeft", PixelUtil.toDIPFromPixel(scrollX));

      mStateWrapper.updateState(mapBuffer);
    };
    mScrollContainerVertical.setOnScrollChangeListener(scrollListener);
    mScrollContainerHorizontal.setOnScrollChangeListener(scrollListener);
    mScrollContainerRefreshVertical.setOnRefreshListener(refreshListener);
    mScrollContainerRefreshHorizontal.setOnRefreshListener(refreshListener);
  }

  public void setScrollContainerHorizontal() {
    if (mOrientation) return;
    mOrientation = true;
    mScrollContainerHorizontal.addView(mScrollContent, 0);
    mScrollContainerRefreshHorizontal.addView(mScrollContainerHorizontal, 0);
    super.addView(mScrollContainerRefreshHorizontal, 0);
  }

  public void setScrollContainerVertical() {
    if (mOrientation) return;
    mOrientation = true;
    mScrollContainerVertical.addView(mScrollContent, 0);
    mScrollContainerRefreshVertical.addView(mScrollContainerVertical, 0);
    super.addView(mScrollContainerRefreshVertical, 0);
  }

  public void setScrollContentLayout(float width, float height) {
    mScrollContent.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  public void setScrollContainerLayout(float width, float height) {
    mScrollContainerRefreshHorizontal.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
    mScrollContainerHorizontal.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));

    mScrollContainerRefreshVertical.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
    mScrollContainerVertical.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  public void setScrollContainerOffset(int x, int y) {
  }

  @Override
  protected void onLayout(boolean changed, int l, int t, int r, int b) {
    if (!mOrientation) {
      setScrollContainerVertical();
    }
  }

  @Override
  public void addView(View child, int index) {
    mContainerChildrenManager.mountChildComponentView(child, index);
  }

  @Override
  protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
    setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.getSize(heightMeasureSpec));
  }

  @Override
  public void draw(Canvas canvas) {
    super.draw(canvas);
    mScrollContent.draw(canvas);
  }

  public void setStateWrapper(StateWrapper stateWrapper) {
    MapBuffer stateMapBuffer = stateWrapper.getStateDataMapBuffer();

    int visibleStartIndex = stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_VISIBLE_START_INDEX);
    int visibleEndIndex = stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_VISIBLE_END_INDEX) == 0 ?
      stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_INITIAL_NUM_TO_RENDER) :
      stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_VISIBLE_END_INDEX);

    mContainerChildrenManager.mount(
      visibleStartIndex,
      visibleEndIndex);

    mStateWrapper = stateWrapper;
  }
}
