package com.shadowlist;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;

public class ShadowlistElementView extends ViewGroup {
  /*
   * The element's flat index in the list, mirrored from the `index` prop. Drag-to-
   * reorder reads it to map a touched child back to a data index (the iOS side reads
   * the same value straight off the element's props).
   */
  private int mElementIndex = -1;

  public ShadowlistElementView(Context context) {
    super(context);
  }

  public void setElementIndex(int index) {
    mElementIndex = index;
  }

  public int getElementIndex() {
    return mElementIndex;
  }

  public ShadowlistElementView(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  public ShadowlistElementView(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    // Children are positioned by the shadowlist core, not by Android layout.
  }
}
