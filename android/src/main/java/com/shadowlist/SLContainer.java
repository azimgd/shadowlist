package com.shadowlist;

import android.content.Context;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.view.View;
import android.graphics.Canvas;
import android.util.AttributeSet;
import androidx.annotation.Nullable;

import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;

public class SLContainer extends ScrollView {
  LinearLayout scrollContent;
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
    scrollContent = new LinearLayout(context);
    scrollContent.setLayoutParams(new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

    mContainerChildrenManager = new SLContainerChildrenManager(scrollContent);
    super.addView(scrollContent, 0);
  }

  public void setScrollContentLayout(float width, float height) {
    // scrollContent.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  @Override
  protected void onScrollChanged(int x, int y, int oldX, int oldY) {
    WritableNativeMap mapBuffer = new WritableNativeMap();

    mapBuffer.putDouble("scrollPositionTop", PixelUtil.toDIPFromPixel(y));
    mapBuffer.putDouble("scrollPositionLeft", PixelUtil.toDIPFromPixel(x));

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

  public void setStateWrapper(StateWrapper stateWrapper) {
    if (stateWrapper.getStateDataMapBuffer().getBoolean(8)) {
      scrollContent.setOrientation(LinearLayout.HORIZONTAL);
    } else {
      scrollContent.setOrientation(LinearLayout.VERTICAL);
    }

    int visibleStartIndex = stateWrapper.getStateDataMapBuffer().getInt(0);
    int visibleEndIndex = stateWrapper.getStateDataMapBuffer().getInt(1) == 0 ?
      stateWrapper.getStateDataMapBuffer().getInt(9) :
      stateWrapper.getStateDataMapBuffer().getInt(1);

    mContainerChildrenManager.mount(
      visibleStartIndex,
      visibleEndIndex);

    mStateWrapper = stateWrapper;
  }
}
