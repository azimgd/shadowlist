package com.shadowlist;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;
import android.view.ViewParent;

import androidx.annotation.Nullable;

public class ShadowListTemplateView extends ViewGroup {
  // Header, footer or empty. The list reads it to pin sticky templates.
  private String mTemplateType = "";

  // Set by the list while mounted, so sticky pinning follows this view's frame.
  private @Nullable View.OnLayoutChangeListener mListLayoutListener = null;

  // Set by the list while mounted, so it finds its sticky views again when the type changes.
  private @Nullable Runnable mListTypeListener = null;

  public ShadowListTemplateView(Context context) {
    super(context);
    updateOutline();
  }

  public ShadowListTemplateView(Context context, AttributeSet attrs) {
    super(context, attrs);
    updateOutline();
  }

  public ShadowListTemplateView(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
    updateOutline();
  }

  /*
   * The sticky controller lifts pinned templates with translationZ so they draw above the
   * rows. Without a background the default outline casts an invisible shadow that the
   * renderer still processes every frame, so drop the outline then. A styled template
   * keeps its outline and its shadow.
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
    // The core positions the children, not Android layout.
  }

  public void setTemplateType(String templateType) {
    String next = templateType != null ? templateType : "";
    boolean changed = !next.equals(mTemplateType);
    mTemplateType = next;
    if (changed && mListTypeListener != null) {
      mListTypeListener.run();
    }
  }

  public String getTemplateType() {
    return mTemplateType;
  }

  /*
   * The list attaches its listeners on mount and passes null on unmount.
   */
  void setListListeners(
      @Nullable View.OnLayoutChangeListener layoutListener, @Nullable Runnable typeListener) {
    if (mListLayoutListener != null) {
      removeOnLayoutChangeListener(mListLayoutListener);
    }
    mListLayoutListener = layoutListener;
    if (layoutListener != null) {
      addOnLayoutChangeListener(layoutListener);
    }
    mListTypeListener = typeListener;
  }

  /*
   * Clear what a previous mount left behind before the view is reused.
   */
  void resetForRecycle() {
    setListListeners(null, null);
    mTemplateType = "";
    removeAllViews();
    ViewParent parent = getParent();
    if (parent instanceof ViewGroup) {
      ((ViewGroup) parent).removeView(this);
    }
  }
}
