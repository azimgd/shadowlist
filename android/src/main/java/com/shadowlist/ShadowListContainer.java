package com.shadowlist;

import androidx.annotation.Nullable;

import android.content.Context;
import android.util.AttributeSet;

import android.view.ViewGroup;

public class ShadowListContainer extends ViewGroup {

  public ShadowListContainer(Context context) {
    super(context);
  }

  public ShadowListContainer(Context context, @Nullable AttributeSet attrs) {
    super(context, attrs);
  }

  public ShadowListContainer(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
  }

  @Override
  protected void onLayout(boolean changed, int l, int t, int r, int b) {

  }
}
