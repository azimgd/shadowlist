package com.shadowlist;

import android.content.Context;
import android.widget.HorizontalScrollView;
import android.widget.ScrollView;
import android.view.View;
import android.graphics.Canvas;
import android.util.AttributeSet;
import androidx.annotation.Nullable;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.common.mapbuffer.MapBuffer;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.views.view.ReactViewGroup;

public class SLContainer extends ReactViewGroup {
  boolean orientation;
  HorizontalScrollView scrollContainerHorizontal;
  ScrollView scrollContainerVertical;
  ReactViewGroup scrollContent;
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
    scrollContainerHorizontal = new HorizontalScrollView(context);
    scrollContainerVertical = new ScrollView(context);

    scrollContent = new ReactViewGroup(context);
    mContainerChildrenManager = new SLContainerChildrenManager(scrollContent);

    OnScrollChangeListener listener = (v, scrollX, scrollY, oldScrollX, oldScrollY) -> {
      WritableNativeMap mapBuffer = new WritableNativeMap();

      mapBuffer.putDouble("scrollPositionTop", PixelUtil.toDIPFromPixel(scrollY));
      mapBuffer.putDouble("scrollPositionLeft", PixelUtil.toDIPFromPixel(scrollX));

      mStateWrapper.updateState(mapBuffer);
    };
    scrollContainerVertical.setOnScrollChangeListener(listener);
    scrollContainerHorizontal.setOnScrollChangeListener(listener);
  }

  public void setScrollContainerHorizontal() {
    if (orientation) return;
    orientation = true;
    scrollContainerHorizontal.addView(scrollContent, 0);
    super.addView(scrollContainerHorizontal, 0);
  }

  public void setScrollContainerVertical() {
    if (orientation) return;
    orientation = true;
    scrollContainerVertical.addView(scrollContent, 0);
    super.addView(scrollContainerVertical, 0);
  }

  public void setScrollContentLayout(float width, float height) {
    scrollContent.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  public void setScrollContainerLayout(float width, float height) {
    scrollContainerHorizontal.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
    scrollContainerVertical.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  @Override
  protected void onLayout(boolean changed, int l, int t, int r, int b) {
    if (!orientation) {
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
    scrollContent.draw(canvas);
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
