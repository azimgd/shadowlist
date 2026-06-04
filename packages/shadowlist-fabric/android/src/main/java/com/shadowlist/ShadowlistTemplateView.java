package com.shadowlist;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;

public class ShadowlistTemplateView extends ViewGroup {
  /* "header", "footer" or "empty" - used by the parent to pin sticky templates. */
  private String mTemplateType = "";

  public ShadowlistTemplateView(Context context) {
    super(context);
  }

  public ShadowlistTemplateView(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  public ShadowlistTemplateView(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    // Children are positioned by the shadowlist core, not by Android layout.
  }

  public void setTemplateType(String templateType) {
    mTemplateType = templateType != null ? templateType : "";
  }

  public String getTemplateType() {
    return mTemplateType;
  }
}
