package com.shadowlist;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;

public class ShadowListElementView extends ViewGroup {
  /*
   * The element's flat index in the list, mirrored from the `index` prop. Kept as a
   * debug/fallback handle; drag-to-reorder identifies rows by mElementKey instead.
   */
  private int mElementIndex = -1;

  /*
   * The element's data key, mirrored from the `elementKey` prop. Drag-to-reorder
   * emits this so JS reorders by identity.
   */
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
    // Children are positioned by the shadowlist core.
  }
}
