package com.shadowlist;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;
import android.view.ViewParent;

import androidx.annotation.Nullable;

public class ShadowListElementView extends ViewGroup {
  /*
   * Copy of the index prop. Starts at the prop default, since a props diff leaves out
   * values equal to the default and index 0 would never arrive.
   */
  private int mElementIndex = 0;

  // Copy of the elementKey prop. Drag to reorder sends it so JS can move the right item.
  private String mElementKey = "";

  public ShadowListElementView(Context context) {
    super(context);
    updateOutline();
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

  /*
   * Clear what a previous mount left behind before the view is reused.
   */
  void resetForRecycle() {
    mElementIndex = 0;
    mElementKey = "";
    removeAllViews();
    ViewParent parent = getParent();
    if (parent instanceof ViewGroup) {
      ((ViewGroup) parent).removeView(this);
    }
  }

  public ShadowListElementView(Context context, AttributeSet attrs) {
    super(context, attrs);
    updateOutline();
  }

  public ShadowListElementView(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
    updateOutline();
  }

  /*
   * Drag to reorder lifts the held row with translationZ. Without a background the default
   * outline casts an invisible shadow that the renderer still processes every frame, so
   * drop the outline then. A row with a background keeps its outline and its lift shadow.
   */
  private void updateOutline() {
    setOutlineProvider(getBackground() == null ? null : ViewOutlineProvider.BACKGROUND);
  }

  @Override
  @SuppressWarnings("deprecation")
  public void setBackgroundDrawable(@Nullable Drawable background) {
    super.setBackgroundDrawable(background);
    updateOutline();
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    // The core positions the children.
  }
}
