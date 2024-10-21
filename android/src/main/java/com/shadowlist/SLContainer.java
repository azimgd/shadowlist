package com.shadowlist;

import android.content.Context;
import android.widget.ScrollView;
import android.view.View;
import android.graphics.Canvas;
import android.util.AttributeSet;

import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.views.view.ReactViewGroup;

public class SLContainer extends ScrollView {
  ReactViewGroup scrollContent;
  private static final int FIXED_HEIGHT = 10000;

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
    super.addView(scrollContent, 0);
  }

  public void setScrollContentLayout(float width, float height) {
    scrollContent.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  @Override
  protected void onLayout(boolean changed, int l, int t, int r, int b) {
    scrollContent.layout(0, 0, getWidth(), FIXED_HEIGHT);
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
    int width = MeasureSpec.getSize(widthMeasureSpec);
    int height = MeasureSpec.getSize(heightMeasureSpec);
    setMeasuredDimension(width, height);
    scrollContent.measure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(FIXED_HEIGHT, MeasureSpec.EXACTLY));
  }

  @Override
  public void draw(Canvas canvas) {
    super.draw(canvas);
    scrollContent.draw(canvas);
  }
}
