package com.shadowlist;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

public class ShadowlistElementView extends ViewGroup {
  public ShadowlistElementView(Context context) {
    super(context);
  }

  public ShadowlistElementView(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  public ShadowlistElementView(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
  }

  @Override
  public void addView(View child, int index) {
    super.addView(child, index);
  }

  @Override
  public void removeView(View child) {
    super.removeView(child);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
  }
}
