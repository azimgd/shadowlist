package com.shadowlist;

import android.content.Context;
import android.widget.ScrollView;
import android.view.View;
import android.graphics.Canvas;
import android.util.AttributeSet;
import androidx.annotation.Nullable;

import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.views.view.ReactViewGroup;

public class SLContainer extends ScrollView {
  ReactViewGroup scrollContent;
  private SLContainerChildrenManager mContainerChildrenManager;
  private @Nullable StateWrapper mStateWrapper = null;

  public SLContainer(Context context) {
    super(context);
    init(context);
  }

  public SLContainer(Context context, AttributeSet attrs) {
    super(context, attrs);
    init(context);
  }

  private void init(Context context) {
    scrollContent = new ReactViewGroup(context);
    mContainerChildrenManager = new SLContainerChildrenManager(scrollContent);
    super.addView(scrollContent, 0);
  }

  public void setScrollContentLayout(float width, float height) {
    scrollContent.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  @Override
  protected void onLayout(boolean changed, int l, int t, int r, int b) {
  }

  @Override
  protected void onScrollChanged(int l, int t, int oldl, int oldt) {
    WritableNativeMap mapBuffer = new WritableNativeMap();

    mapBuffer.putDouble("scrollPositionTop", t);
    mapBuffer.putDouble("scrollPositionLeft", l);

    mStateWrapper.updateState(mapBuffer);
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

  @Nullable
  public StateWrapper getStateWrapper() {
    return mStateWrapper;
  }

  public void setStateWrapper(StateWrapper stateWrapper) {
    mContainerChildrenManager.mount(
      stateWrapper.getStateDataMapBuffer().getInt(0),
      stateWrapper.getStateDataMapBuffer().getInt(1));

    mStateWrapper = stateWrapper;
  }
}
