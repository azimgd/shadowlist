package com.shadowlist;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import com.facebook.react.uimanager.StateWrapper;

public class ShadowlistView extends ViewGroup {
  private StateWrapper mStateWrapper;

  public ShadowlistView(Context context) {
    super(context);
  }

  public ShadowlistView(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  public ShadowlistView(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
  }

  @Override
  public void addView(View child) {
  }

  @Override
  public void removeView(View view) {
  }

  @Override
  protected void onLayout(boolean changed, int l, int t, int r, int b) {
  }

  @Nullable
  public StateWrapper getStateWrapper() {
    return mStateWrapper;
  }

  public void setStateWrapper(@Nullable StateWrapper stateWrapper) {
    mStateWrapper = stateWrapper;

    if (mStateWrapper != null && mStateWrapper.getStateData() != null) {
      mStateWrapper.getStateData().hasKey("asd");
    }
  }
}
