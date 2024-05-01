package com.shadowlist;

import androidx.annotation.Nullable;
import android.content.Context;
import android.graphics.Canvas;
import android.view.View;
import android.widget.ScrollView;

import com.facebook.react.views.view.ReactViewGroup;

public class ShadowListContainer extends ScrollView {
  ReactViewGroup scrollContent;

  public ShadowListContainer(Context context) {
    super(context);
    scrollContent = new ReactViewGroup(context);
    super.addView(scrollContent, 0);
  }

  @Override
  protected void onLayout(boolean changed, int l, int t, int r, int b) {
    scrollContent.layout(l, t, r, b * 10);
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
