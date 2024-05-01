package com.shadowlist;

import androidx.annotation.Nullable;
import android.content.Context;

import com.facebook.react.views.view.ReactViewGroup;

public class ShadowListItem extends ReactViewGroup {

  public ShadowListItem(Context context) {
    super(context);
  }

  @Override
  protected void onLayout(boolean changed, int l, int t, int r, int b) {
  }

  @Override
  protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
    setMeasuredDimension(
      MeasureSpec.getSize(widthMeasureSpec),
      MeasureSpec.getSize(heightMeasureSpec));
  }
}
