package com.shadowlist;

import androidx.annotation.Nullable;
import android.content.Context;
import android.graphics.Canvas;
import android.view.View;
import android.widget.ScrollView;

import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.views.view.ReactViewGroup;
import com.facebook.react.uimanager.StateWrapper;

public class ShadowListContainer extends ScrollView {
  ReactViewGroup scrollContent;

  public ShadowListContainer(Context context) {
    super(context);
    scrollContent = new ReactViewGroup(context);
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
  }

  @Override
  public void addView(View child, int index) {
    scrollContent.addView(child, index);
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
}
