package com.shadowlist;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;

public class ShadowListTemplateView extends ViewGroup {
  // Header, footer or empty. The list reads it to pin sticky templates.
  private String mTemplateType = "";

  public ShadowListTemplateView(Context context) {
    super(context);
  }

  public ShadowListTemplateView(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  public ShadowListTemplateView(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    // The core positions the children, not Android layout.
  }

  public void setTemplateType(String templateType) {
    mTemplateType = templateType != null ? templateType : "";
  }

  public String getTemplateType() {
    return mTemplateType;
  }
}
