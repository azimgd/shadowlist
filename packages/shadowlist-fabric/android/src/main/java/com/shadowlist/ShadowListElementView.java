package com.shadowlist;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;

public class ShadowListElementView extends ViewGroup {
  // Copy of the index prop, kept for debugging. Drag to reorder uses the key instead.
  private int mElementIndex = -1;

  // Copy of the elementKey prop. Drag to reorder sends it so JS can move the right item.
  private String mElementKey = "";

  public ShadowListElementView(Context context) {
    super(context);
  }

  public void setElementIndex(int index) {
    mElementIndex = index;
  }

  public int getElementIndex() {
    return mElementIndex;
  }

  public void setElementKey(String key) {
    mElementKey = key != null ? key : "";
  }

  public String getElementKey() {
    return mElementKey;
  }

  public ShadowListElementView(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  public ShadowListElementView(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    // The core positions the children.
  }
}
